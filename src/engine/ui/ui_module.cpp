#include "ui_module.hpp"

#include <algorithm>
#include <cmath>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

#include <spdlog/spdlog.h>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/audio/audio.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/input/input.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scripting/scripting.hpp"
#include "ui_components.hpp"

namespace engine::ui {

namespace {

constexpr float kTwoPi = 6.28318530717958647692F;

platform::Rect Translate(const platform::Rect r, const glm::vec2 offset) {
	return {.x = r.x + offset.x, .y = r.y + offset.y, .w = r.w, .h = r.h};
}

// Total scroll translation applied to `entity` by its ScrollRect ancestors (each contributes
// -scroll). An element authors its UIRect in unscrolled space; this yields the on-screen delta.
glm::vec2 ScrollOffset(const flecs::entity entity) {
	glm::vec2 offset{0.0F};
	for (flecs::entity p = entity.parent(); p; p = p.parent()) {
		if (const auto* sr = p.try_get<ScrollRect>()) {
			offset -= sr->scroll;
		}
	}
	return offset;
}

// The element's rect after ancestor scroll offsets — its true on-screen position for hit-testing.
platform::Rect ResolvedRect(const flecs::entity entity, const UIRect& ui) {
	return Translate(ui.rect, ScrollOffset(entity));
}

// True unless the element or an ancestor is hidden (UIElement.visible == false) or inside a closed
// Modal.
bool SubtreeVisible(const flecs::entity entity) {
	for (flecs::entity e = entity; e; e = e.parent()) {
		if (const auto* el = e.try_get<UIElement>(); el != nullptr && !el->visible) {
			return false;
		}
		if (const auto* modal = e.try_get<Modal>(); modal != nullptr && !modal->open) {
			return false;
		}
	}
	return true;
}

// True if the element is itself, or is nested under, an open Modal.
bool InsideOpenModal(const flecs::entity entity) {
	for (flecs::entity e = entity; e; e = e.parent()) {
		if (const auto* modal = e.try_get<Modal>(); modal != nullptr && modal->open) {
			return true;
		}
	}
	return false;
}

// The mouse must fall inside every ScrollRect viewport the element is nested in, otherwise it is
// scrolled out of view and must not receive clicks.
bool WithinScrollViewports(const flecs::entity entity, const glm::vec2 mouse) {
	for (flecs::entity p = entity.parent(); p; p = p.parent()) {
		if (p.has<ScrollRect>()) {
			if (const auto* ui = p.try_get<UIRect>(); ui != nullptr && !ResolvedRect(p, *ui).Contains(mouse)) {
				return false;
			}
		}
	}
	return true;
}

// Run a button's click reactions: play its SoundEffect immediately (direct platform call, not via
// the SoundEffectPlayback system) then invoke its OnClick handler. Sound is played BEFORE the
// callback so that a callback that triggers a scene switch (which will destroy this entity and its
// SoundEffect at the next deferred-command flush) doesn't silence the click.
void DispatchClick(const flecs::entity entity) {
	if (const auto* sfx = entity.try_get<audio::SoundEffect>(); sfx != nullptr && sfx->handle >= 0) {
		entity.world().get<platform::PlatformRef>().ptr->PlaySound(sfx->handle);
	}
	if (const auto* on_click = entity.try_get<OnClick>(); on_click != nullptr && on_click->callback) {
		on_click->callback(entity);
	}
}

// Centre `text` horizontally within `r` for the given alignment and vertically in all cases.
glm::vec2 PlaceText(
	const platform::Platform* platform,
	const platform::Rect r,
	const std::string_view text,
	const float font_size,
	const TextAlign align
) {
	const float text_width = platform->MeasureText(text, font_size);
	float x = r.x;
	switch (align) {
	case TextAlign::Left: x = r.x; break;
	case TextAlign::Center: x = r.x + (r.w - text_width) / 2.0F; break;
	case TextAlign::Right: x = r.x + r.w - text_width; break;
	}
	return {x, r.y + (r.h - font_size) / 2.0F};
}

// Append the draw commands for a single UI element (all its visual components) at layer `z`.
void EmitElement(
	const flecs::entity entity,
	UIDrawList& list,
	const platform::Platform* platform,
	const glm::vec2 offset,
	const int z
) {
	const auto* ui = entity.try_get<UIRect>();
	if (ui == nullptr) {
		return;
	}
	const platform::Rect r = Translate(ui->rect, offset);

	if (const auto* modal = entity.try_get<Modal>()) {
		const platform::Rect full{
			.x = 0.0F,
			.y = 0.0F,
			.w = static_cast<float>(platform->Width()),
			.h = static_cast<float>(platform->Height())
		};
		list.PushRect(full, modal->backdrop_color, z);
	}

	if (const auto* panel = entity.try_get<Panel>()) {
		if (panel->roundness > 0.0F) {
			list.PushRoundedRect(r, panel->roundness, panel->color, z);
			if (panel->border_thickness > 0.0F) {
				list.PushRoundedRectLines(r, panel->roundness, panel->border_thickness, panel->border_color, z);
			}
		}
		else {
			list.PushRect(r, panel->color, z);
			if (panel->border_thickness > 0.0F) {
				list.PushRectLines(r, panel->border_thickness, panel->border_color, z);
			}
		}
	}

	if (const auto* bar = entity.try_get<ProgressBar>()) {
		if (bar->roundness > 0.0F) {
			list.PushRoundedRect(r, bar->roundness, bar->track_color, z);
		}
		else {
			list.PushRect(r, bar->track_color, z);
		}
		if (const float frac = bar->Fraction(); frac > 0.0F) {
			const platform::Rect fill{.x = r.x, .y = r.y, .w = r.w * frac, .h = r.h};
			if (bar->roundness > 0.0F) {
				list.PushRoundedRect(fill, bar->roundness, bar->fill_color, z);
			}
			else {
				list.PushRect(fill, bar->fill_color, z);
			}
		}
		if (bar->border_thickness > 0.0F) {
			if (bar->roundness > 0.0F) {
				list.PushRoundedRectLines(r, bar->roundness, bar->border_thickness, bar->border_color, z);
			}
			else {
				list.PushRectLines(r, bar->border_thickness, bar->border_color, z);
			}
		}
	}

	if (const auto* spinner = entity.try_get<Spinner>()) {
		const glm::vec2 center{r.x + r.w / 2.0F, r.y + r.h / 2.0F};
		const int dots = std::max(1, spinner->dots);
		for (int i = 0; i < dots; ++i) {
			const float angle = spinner->phase + (static_cast<float>(i) / static_cast<float>(dots)) * kTwoPi;
			const glm::vec2 pos{
				center.x + spinner->radius * std::cos(angle),
				center.y + spinner->radius * std::sin(angle)
			};
			const float t = static_cast<float>(i) / static_cast<float>(dots);
			platform::Rgba color = spinner->color;
			color.a = static_cast<std::uint8_t>(40.0F + 215.0F * t);
			list.PushCircle(pos, spinner->dot_radius, color, z);
		}
	}

	if (const auto* button = entity.try_get<Button>()) {
		const platform::Rgba fill =
			button->pressed ? button->pressed_color : (button->hovered ? button->hover : button->normal);
		list.PushRoundedRect(r, button->roundness, fill, z);
		list.PushRoundedRectLines(r, button->roundness, 2.0F, button->border_color, z);
		if (!button->label.empty()) {
			const glm::vec2 pos = PlaceText(platform, r, button->label, button->font_size, TextAlign::Center);
			list.PushText(button->label, pos.x, pos.y, button->font_size, button->text_color, z);
		}
	}

	if (const auto* label = entity.try_get<Label>()) {
		if (!label->text.empty()) {
			const glm::vec2 pos = PlaceText(platform, r, label->text, label->font_size, label->align);
			list.PushText(label->text, pos.x, pos.y, label->font_size, label->color, z);
		}
	}
}

// Draw a ScrollRect's scrollbar(s) on top of (outside) the clipped content.
void EmitScrollbar(const platform::Rect viewport, const ScrollRect& sr, UIDrawList& list, const int z) {
	if (sr.vertical && sr.content_size.y > viewport.h) {
		const platform::Rect track{
			.x = viewport.x + viewport.w - sr.scrollbar_thickness,
			.y = viewport.y,
			.w = sr.scrollbar_thickness,
			.h = viewport.h
		};
		list.PushRect(track, sr.track_color, z);
		const float view_ratio = viewport.h / sr.content_size.y;
		const float thumb_h = std::max(24.0F, viewport.h * view_ratio);
		const float range = sr.content_size.y - viewport.h;
		const float t = range > 0.0F ? glm::clamp(sr.scroll.y / range, 0.0F, 1.0F) : 0.0F;
		const platform::Rect thumb{
			.x = track.x,
			.y = viewport.y + t * (viewport.h - thumb_h),
			.w = sr.scrollbar_thickness,
			.h = thumb_h
		};
		list.PushRoundedRect(thumb, 0.5F, sr.thumb_color, z);
	}
	if (sr.horizontal && sr.content_size.x > viewport.w) {
		const platform::Rect track{
			.x = viewport.x,
			.y = viewport.y + viewport.h - sr.scrollbar_thickness,
			.w = viewport.w,
			.h = sr.scrollbar_thickness
		};
		list.PushRect(track, sr.track_color, z);
		const float view_ratio = viewport.w / sr.content_size.x;
		const float thumb_w = std::max(24.0F, viewport.w * view_ratio);
		const float range = sr.content_size.x - viewport.w;
		const float t = range > 0.0F ? glm::clamp(sr.scroll.x / range, 0.0F, 1.0F) : 0.0F;
		const platform::Rect thumb{
			.x = viewport.x + t * (viewport.w - thumb_w),
			.y = track.y,
			.w = thumb_w,
			.h = sr.scrollbar_thickness
		};
		list.PushRoundedRect(thumb, 0.5F, sr.thumb_color, z);
	}
}

// Emit an element then its UI subtree. `inheritedZ` lets a high-z layer (e.g. a Modal) raise its
// whole subtree above other UI; ScrollRect subtrees are wrapped in clip commands and offset.
void EmitTree(
	const flecs::entity entity,
	UIDrawList& list,
	platform::Platform* platform,
	const glm::vec2 offset,
	const int inherited_z
) {
	if (const auto* el = entity.try_get<UIElement>(); el != nullptr && !el->visible) {
		return;
	}
	if (const auto* modal = entity.try_get<Modal>(); modal != nullptr && !modal->open) {
		return;
	}

	int z = inherited_z;
	if (const auto* el = entity.try_get<UIElement>()) {
		z = std::max(z, el->z_index);
	}

	EmitElement(entity, list, platform, offset, z);

	if (const auto* sr = entity.try_get<ScrollRect>()) {
		const auto* ui = entity.try_get<UIRect>();
		const platform::Rect viewport = ui != nullptr ? Translate(ui->rect, offset) : platform::Rect{};
		list.PushBeginClip(viewport, z);
		const glm::vec2 child_offset = offset - sr->scroll;
		entity.children([&list, platform, child_offset, z](const flecs::entity child) {
			if (child.has<UIRect>()) {
				EmitTree(child, list, platform, child_offset, z);
			}
		});
		list.PushEndClip(z);
		EmitScrollbar(viewport, *sr, list, z);
	}
	else {
		entity.children([&list, platform, offset, z](const flecs::entity child) {
			if (child.has<UIRect>()) {
				EmitTree(child, list, platform, offset, z);
			}
		});
	}
}

// Lay out one node then recurse into its UI children (top-down). When the node is a Stack, its
// direct children are positioned along the stack axis first; recursing afterwards means a nested
// child stack lays out using the rect its parent just assigned, so multi-level compositions (e.g.
// a prompt's vertical column containing a horizontal button row) resolve in a single pass.
void LayoutNode(const flecs::entity entity) {
	if (const auto* stack = entity.try_get<Stack>()) {
		if (const auto* ui = entity.try_get<UIRect>()) {
			const platform::Rect base = ui->rect;
			const bool vertical = stack->direction == StackDirection::Vertical;
			float cursor = (vertical ? base.y : base.x) + stack->padding;
			const float cross = (vertical ? base.x : base.y) + stack->padding;
			entity.children([&](const flecs::entity child) {
				if (!child.has<UIRect>()) {
					return;
				}
				platform::Rect& cr = child.get_mut<UIRect>().rect;
				if (vertical) {
					cr.x = cross;
					cr.y = cursor;
					if (stack->stretch) {
						cr.w = base.w - 2.0F * stack->padding;
					}
					cursor += cr.h + stack->spacing;
				}
				else {
					cr.x = cursor;
					cr.y = cross;
					if (stack->stretch) {
						cr.h = base.h - 2.0F * stack->padding;
					}
					cursor += cr.w + stack->spacing;
				}
			});
		}
	}
	entity.children([](const flecs::entity child) {
		if (child.has<UIRect>()) {
			LayoutNode(child);
		}
	});
}

} // namespace

UIModule::UIModule(const flecs::world& world) {
	// (1) Singletons.
	world.set<UIDrawList>({});
	world.set<UIConfig>({});
	world.set<UIModalState>({});

	// (2) Reflection — expose the plain-data components in the Flecs Explorer. platform::Rect is
	// registered here (it has no other Flecs home) so UIRect can reference it as a member type.
	world.component<platform::Rect>().member<float>("x").member<float>("y").member<float>("w").member<float>("h");
	world.component<UIRect>().member<platform::Rect>("rect");
	world.component<UIElement>().member<int>("z_index").member<bool>("visible");
	world.component<Panel>()
		.member<platform::Rgba>("color")
		.member<platform::Rgba>("border_color")
		.member<float>("border_thickness")
		.member<float>("roundness");
	world.component<TextAlign>();
	world.component<Label>()
		.member<std::string>("text")
		.member<float>("font_size")
		.member<platform::Rgba>("color")
		.member<TextAlign>("align");
	world.component<Button>()
		.member<std::string>("label")
		.member<float>("font_size")
		.member<platform::Rgba>("normal")
		.member<platform::Rgba>("hover")
		.member<platform::Rgba>("pressed_color")
		.member<platform::Rgba>("border_color")
		.member<platform::Rgba>("text_color")
		.member<float>("roundness")
		.member<bool>("hovered")
		.member<bool>("pressed")
		.member<bool>("clicked");
	world.component<ProgressBar>()
		.member<float>("value")
		.member<float>("min")
		.member<float>("max")
		.member<platform::Rgba>("fill_color")
		.member<platform::Rgba>("track_color")
		.member<platform::Rgba>("border_color")
		.member<float>("border_thickness")
		.member<float>("roundness");
	world.component<Spinner>()
		.member<platform::Rgba>("color")
		.member<float>("radius")
		.member<float>("dot_radius")
		.member<int>("dots")
		.member<float>("speed")
		.member<float>("phase");
	world.component<Stack>().member<float>("spacing").member<float>("padding").member<bool>("stretch");
	world.component<ScrollRect>()
		.member<glm::vec2>("scroll")
		.member<glm::vec2>("content_size")
		.member<bool>("vertical")
		.member<bool>("horizontal")
		.member<float>("scrollbar_thickness")
		.member<float>("wheel_speed")
		.member<platform::Rgba>("track_color")
		.member<platform::Rgba>("thumb_color")
		.member<bool>("dragging_v")
		.member<bool>("dragging_h");
	world.component<Modal>()
		.member<bool>("open")
		.member<platform::Rgba>("backdrop_color")
		.member<bool>("close_on_backdrop");
	world.component<UIClickRequest>();

	// Scripting — expose the POD components whose members are all script-registered so scripts can
	// read/write UI state (e.g. self.GetProgressBar().value = x). OnClick (std::function), Stack's
	// enum direction, and the draw-list types are intentionally left engine-only.
	scripting::RegisterValueTypeForScripts(world, world.component<platform::Rect>());
	scripting::RegisterComponentForScripts(world, world.component<UIRect>());
	scripting::RegisterComponentForScripts(world, world.component<UIElement>());
	scripting::RegisterComponentForScripts(world, world.component<Panel>());
	scripting::RegisterComponentForScripts(world, world.component<Button>());
	scripting::RegisterComponentForScripts(world, world.component<ProgressBar>());
	scripting::RegisterComponentForScripts(world, world.component<Spinner>());
	scripting::RegisterComponentForScripts(world, world.component<Stack>());
	scripting::RegisterComponentForScripts(world, world.component<ScrollRect>());
	scripting::RegisterComponentForScripts(world, world.component<Modal>());

	// SetOnClick(button, @OnClicked) — bind a script function as a button's click handler. The
	// callback is entity-first (script functions have no implicit receiver): `void OnClicked(Entity
	// button)`. The handle is wrapped into the button's existing OnClick std::function component —
	// the same storage C++ callers use — so DispatchClick and UIClickRequest fire it unchanged.
	scripting::RegisterCallbackFunctionForScripts(
		world,
		{
			.name = "SetOnClick",
			.fixed_params =
				{{.type = scripting::ScriptValueType::MakeObject(world.component<scripting::ScriptEntityRef>()),
				  .by_reference = false,
				  .name = "button"}},
			.callback =
				{
					.funcdef_name = "ClickCallback",
					.params =
						{{.type = scripting::ScriptValueType::MakeObject(world.component<scripting::ScriptEntityRef>()),
						  .by_reference = false,
						  .name = "button"}},
					.name = "onClick",
				},
		},
		[](scripting::ScriptCallContext& ctx, flecs::world&, scripting::ScriptFunctionHandle* handle) {
			auto* ref = static_cast<scripting::ScriptEntityRef*>(ctx.GetArgObject(0));
			if (!ref) return;
			const flecs::entity button = ref->GetEntity();
			if (!handle) {
				button.remove<OnClick>(); // SetOnClick(button, null) clears the handler
				return;
			}
			handle->AddRef();
			const auto handle_sp =
				std::shared_ptr<scripting::ScriptFunctionHandle>(handle, [](scripting::ScriptFunctionHandle* h) {
					if (h) h->Release();
				});
			button.set<OnClick>({[handle_sp](const flecs::entity clicked) {
				scripting::ScriptEntityRef clicked_ref{clicked};
				const void* args[] = {&clicked_ref};
				handle_sp->Invoke(args, 1);
			}});
		}
	);

	// Release script-backed OnClick handlers before the scripting engine is torn down (see the
	// tilemap module for the same teardown-ordering rationale). Clearing every OnClick is safe:
	// C++ handlers hold no engine references, and everything is being destroyed anyway.
	scripting::RegisterScriptShutdownCallback(world, [](flecs::world& shutdown_world) {
		shutdown_world.query<OnClick>().each([](flecs::entity, OnClick& on_click) { on_click.callback = nullptr; });
	});

	// (3) Systems — split across phases like ordinary simulation:
	//   PreUpdate  — UILayout: position stack children so hit-test rects are final.
	//   OnUpdate   — interaction/logic (reads input polled in PreUpdate, before PostUpdate resets).
	//   OnUI       — rendering: build then flush the 2D draw list (after 3D in OnStore).
	// Registration order sets execution order within a phase. None are Pausable (UI runs while
	// paused) and none are scene-scoped (UI works in every scene pipeline).

	// === SYSTEM: Layout (PreUpdate) ===
	// Top-down pass from UI roots that positions every Stack's children. Runs before interaction so
	// button/scroll hit-tests use the final laid-out rects this frame.
	const auto layout_query = world.query_builder<const UIRect>().build();
	world.system("UILayout").kind(flecs::PreUpdate).run([layout_query](const flecs::iter&) {
		layout_query.each([](const flecs::entity entity, const UIRect&) {
			if (const flecs::entity parent = entity.parent(); parent && parent.has<UIRect>()) {
				return; // laid out via its parent's recursion
			}
			LayoutNode(entity);
		});
	});

	// === SYSTEM: ScrollRect bookkeeping (OnUpdate) ===
	// Recomputes content bounds from the (laid-out) descendants, clamps scroll to range, and
	// applies mouse-wheel scrolling and thumb-drag while the viewport is hovered/dragged.
	world.system<ScrollRect, const UIRect>("UIScrollRectUpdate")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity entity, ScrollRect& sr, const UIRect& ui) {
			// --- 1. Recompute content extent from all descendants ---
			glm::vec2 extent{0.0F};
			auto accumulate_lambda = [&extent, &ui](const flecs::entity node, auto& recurse) -> void {
				node.children([&extent, &ui, &recurse](const flecs::entity child) {
					if (const auto* cui = child.try_get<UIRect>()) {
						extent.x = std::max(extent.x, (cui->rect.x + cui->rect.w) - ui.rect.x);
						extent.y = std::max(extent.y, (cui->rect.y + cui->rect.h) - ui.rect.y);
						recurse(child, recurse);
					}
				});
			};
			accumulate_lambda(entity, accumulate_lambda);
			sr.content_size = extent;

			const glm::vec2 range{std::max(0.0F, extent.x - ui.rect.w), std::max(0.0F, extent.y - ui.rect.h)};
			const auto& input = entity.world().get<input::InputState>();
			const glm::vec2 mouse{input.mouse.window_position.x, input.mouse.window_position.y};
			const platform::Rect viewport = ResolvedRect(entity, ui);
			const bool mouse_down = input.mouse.left.state;
			const bool mouse_pressed = input.mouse.left.pressed;

			// --- 2. Mouse-wheel scroll (requires cursor inside the viewport) ---
			if (viewport.Contains(mouse)) {
				if (sr.vertical && input.mouse.scroll.y != 0.0F) {
					sr.scroll.y -= input.mouse.scroll.y * sr.wheel_speed;
				}
				if (sr.horizontal && input.mouse.scroll.x != 0.0F) {
					sr.scroll.x -= input.mouse.scroll.x * sr.wheel_speed;
				}
			}

			// --- 3. Vertical thumb drag ---
			if (sr.vertical && range.y > 0.0F) {
				const float track_x = viewport.x + viewport.w - sr.scrollbar_thickness;
				const float thumb_h = std::max(24.0F, viewport.h * (viewport.h / sr.content_size.y));
				const float t = glm::clamp(sr.scroll.y / range.y, 0.0F, 1.0F);
				const float thumb_y = viewport.y + t * (viewport.h - thumb_h);
				const platform::Rect thumb{.x = track_x, .y = thumb_y, .w = sr.scrollbar_thickness, .h = thumb_h};

				if (!sr.dragging_v && mouse_pressed && thumb.Contains(mouse)) {
					sr.dragging_v = true;
					sr.drag_start_mouse = mouse.y;
					sr.drag_start_scroll = sr.scroll.y;
				}
				if (sr.dragging_v) {
					if (mouse_down) {
						const float delta_mouse = mouse.y - sr.drag_start_mouse;
						const float scroll_range = viewport.h - thumb_h;
						sr.scroll.y =
							sr.drag_start_scroll + (scroll_range > 0.0F ? delta_mouse / scroll_range * range.y : 0.0F);
					}
					else {
						sr.dragging_v = false;
					}
				}
			}

			// --- 4. Horizontal thumb drag ---
			if (sr.horizontal && range.x > 0.0F) {
				const float track_y = viewport.y + viewport.h - sr.scrollbar_thickness;
				const float thumb_w = std::max(24.0F, viewport.w * (viewport.w / sr.content_size.x));
				const float t = glm::clamp(sr.scroll.x / range.x, 0.0F, 1.0F);
				const float thumb_x = viewport.x + t * (viewport.w - thumb_w);
				const platform::Rect thumb{.x = thumb_x, .y = track_y, .w = thumb_w, .h = sr.scrollbar_thickness};

				if (!sr.dragging_h && mouse_pressed && thumb.Contains(mouse)) {
					sr.dragging_h = true;
					sr.drag_start_mouse = mouse.x;
					sr.drag_start_scroll = sr.scroll.x;
				}
				if (sr.dragging_h) {
					if (mouse_down) {
						const float delta_mouse = mouse.x - sr.drag_start_mouse;
						const float scroll_range = viewport.w - thumb_w;
						sr.scroll.x =
							sr.drag_start_scroll + (scroll_range > 0.0F ? delta_mouse / scroll_range * range.x : 0.0F);
					}
					else {
						sr.dragging_h = false;
					}
				}
			}

			// --- 5. Clamp final scroll values ---
			sr.scroll.x = glm::clamp(sr.scroll.x, 0.0F, range.x);
			sr.scroll.y = glm::clamp(sr.scroll.y, 0.0F, range.y);
		});

	// === SYSTEM: Spinner animation (OnUpdate) ===
	world.system<Spinner>("UISpinnerAnimate")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, size_t, Spinner& spinner) {
			spinner.phase += spinner.speed * it.delta_time();
			if (spinner.phase > kTwoPi) {
				spinner.phase -= kTwoPi;
			}
		});

	// === SYSTEM: Modal backdrop dismissal (OnUpdate) ===
	world.system<Modal, const UIRect>("UIModalInteraction")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity entity, Modal& modal, const UIRect& ui) {
			if (!modal.open || !modal.close_on_backdrop) {
				return;
			}
			const auto& input = entity.world().get<input::InputState>();
			const glm::vec2 mouse{input.mouse.window_position.x, input.mouse.window_position.y};
			if (input.mouse.left.pressed && !ResolvedRect(entity, ui).Contains(mouse)) {
				modal.open = false;
			}
		});

	// === SYSTEM: Update cached modal state (OnUpdate) ===
	// Runs once per frame to cache whether any modal is open, avoiding per-button queries.
	const auto modal_query = world.query_builder<const Modal>().build();
	world.system("UIUpdateModalState").kind(flecs::OnUpdate).run([modal_query](const flecs::iter& it) {
		auto& modal_state = it.world().get_mut<UIModalState>();
		modal_state.any_open = false;
		modal_query.each([&modal_state](const Modal& m) {
			if (m.open) {
				modal_state.any_open = true;
			}
		});
	});

	// === SYSTEM: Button interaction (OnUpdate) ===
	// Updates hover/press/click state from the mouse (honouring scroll clipping, visibility, and
	// modal input-blocking), then dispatches OnClick + SoundEffect on a completed click.
	// Uses cached UIModalState instead of querying modals per button.
	world.system<Button, const UIRect>("UIButtonInteraction")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity entity, Button& button, const UIRect& ui) {
			const auto& input = entity.world().get<input::InputState>();
			const glm::vec2 mouse{input.mouse.window_position.x, input.mouse.window_position.y};
			const auto& modal_state = entity.world().get<UIModalState>();

			const bool interactable = SubtreeVisible(entity)
									  && WithinScrollViewports(entity, mouse)
									  && (!modal_state.any_open || InsideOpenModal(entity));
			const bool hovering = interactable && ResolvedRect(entity, ui).Contains(mouse);

			button.hovered = hovering;
			button.clicked = false;
			// Detect the rising edge from held-down state rather than the one-frame `pressed` flag.
			// If the mouse is down now and we weren't already tracking a press on this button, start
			// one; a release while still hovering completes the click.
			const bool mouse_down = input.mouse.left.state;
			if (hovering && mouse_down && !button.pressed) {
				button.pressed = true;
			}
			if (button.pressed && !mouse_down) {
				button.clicked = hovering;
				button.pressed = false;
			}
			if (button.clicked) {
				DispatchClick(entity);
			}
		});

	// === SYSTEM: Programmatic click requests (OnUpdate) ===
	// Consumes the UIClickRequest tag (added by scripts, tests, or the flecs-api tool) and fires
	// the button's click reactions without synthesising mouse input.
	world.system<Button>("UIButtonClickRequest")
		.with<UIClickRequest>()
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity entity, Button& button) {
			button.clicked = true;
			DispatchClick(entity);
			entity.remove<UIClickRequest>();
		});

	// === SYSTEM: Build the draw list (OnUI) ===
	// Walks UI roots (elements whose parent is not itself a UI element) in FIFO order, recursing
	// through children so the hierarchy determines layering. Emits into the shared UIDrawList.
	const auto ui_query = world.query_builder<const UIRect>().build();
	world.system("UIBuildDrawList").kind<ecs::OnUI>().run([ui_query](const flecs::iter& it) {
		const flecs::world world = it.world();
		auto& list = world.get_mut<UIDrawList>();
		auto* platform = world.get<platform::PlatformRef>().ptr;
		list.Clear();
		ui_query.each([&list, platform](const flecs::entity entity, const UIRect&) {
			if (const flecs::entity parent = entity.parent(); parent && parent.has<UIRect>()) {
				return; // drawn via its parent's recursion
			}
			EmitTree(entity, list, platform, glm::vec2{0.0F}, 0);
		});
	});

	// === SYSTEM: Flush the draw list (OnUI) ===
	// Stable-sorts by order (z layer) — preserving FIFO/hierarchy order within a layer — then
	// issues the platform 2D draw calls, maintaining a clip stack for ScrollRect regions.
	world.system("UIRenderDrawList").kind<ecs::OnUI>().run([](const flecs::iter& it) {
		const flecs::world ecs_world = it.world();
		auto& list = ecs_world.get_mut<UIDrawList>();
		auto* platform = ecs_world.get<platform::PlatformRef>().ptr;

		std::ranges::stable_sort(list.commands, [](const UIDrawCommand& a, const UIDrawCommand& b) {
			return a.order < b.order;
		});

		std::vector<platform::Rect> clip_stack;
		const auto intersect = [](const platform::Rect a, const platform::Rect b) {
			const float x0 = std::max(a.x, b.x);
			const float y0 = std::max(a.y, b.y);
			const float x1 = std::min(a.x + a.w, b.x + b.w);
			const float y1 = std::min(a.y + a.h, b.y + b.h);
			return platform::Rect{.x = x0, .y = y0, .w = std::max(0.0F, x1 - x0), .h = std::max(0.0F, y1 - y0)};
		};

		for (const auto& cmd : list.commands) {
			switch (cmd.kind) {
			case UIDrawKind::Rect: platform->DrawRect(cmd.rect, cmd.color); break;
			case UIDrawKind::RoundedRect: platform->DrawRoundedRect(cmd.rect, cmd.roundness, cmd.color); break;
			case UIDrawKind::RectLines: platform->DrawRectLines(cmd.rect, cmd.thickness, cmd.color); break;
			case UIDrawKind::RoundedRectLines:
				platform->DrawRoundedRectLines(cmd.rect, cmd.roundness, cmd.thickness, cmd.color);
				break;
			case UIDrawKind::Text: platform->DrawText(cmd.text, cmd.p0.x, cmd.p0.y, cmd.font_size, cmd.color); break;
			case UIDrawKind::Line: platform->DrawLine(cmd.p0, cmd.p1, cmd.thickness, cmd.color); break;
			case UIDrawKind::Circle: platform->DrawCircle(cmd.p0, cmd.radius, cmd.color); break;
			case UIDrawKind::BeginClip:
			{
				const platform::Rect region = clip_stack.empty() ? cmd.rect : intersect(clip_stack.back(), cmd.rect);
				clip_stack.push_back(region);
				platform->BeginScissor(region);
				break;
			}
			case UIDrawKind::EndClip:
				if (!clip_stack.empty()) {
					clip_stack.pop_back();
				}
				if (clip_stack.empty()) {
					platform->EndScissor();
				}
				else {
					platform->BeginScissor(clip_stack.back());
				}
				break;
			}
		}

		list.Clear();
	});

	spdlog::info("[UIModule] Registered UI components, draw list, and OnUI systems with Flecs");
}

// === Factories ===

void SetDefaultClickSound(const flecs::world& world, std::string path) {
	if (world.has<UIConfig>()) {
		world.get_mut<UIConfig>().default_click_sound_path = std::move(path);
	}
}

flecs::entity CreatePanel(const flecs::world& world, const platform::Rect rect, const Panel panel) {
	return world.entity().set<UIRect>({rect}).set<Panel>(panel);
}

flecs::entity
CreateLabel(const flecs::world& world, const platform::Rect rect, std::string text, const float font_size) {
	return world.entity().set<UIRect>({rect}).set<Label>({.text = std::move(text), .font_size = font_size});
}

flecs::entity CreateButton(
	const flecs::world& world,
	const platform::Rect rect,
	std::string label,
	std::function<void(flecs::entity)> on_click
) {
	const flecs::entity entity = world.entity().set<UIRect>({rect}).set<Button>({.label = std::move(label)});
	if (on_click) {
		entity.set<OnClick>({std::move(on_click)});
	}
	// Auto-attach the default click sound so every factory-created button sounds consistent.
	if (world.has<UIConfig>()) {
		if (const auto& cfg = world.get<UIConfig>(); !cfg.default_click_sound_path.empty()) {
			entity.set<audio::SoundEffect>({.path = cfg.default_click_sound_path});
		}
	}
	return entity;
}

flecs::entity CreateProgressBar(const flecs::world& world, const platform::Rect rect, const ProgressBar& bar) {
	return world.entity().set<UIRect>({rect}).set<ProgressBar>(bar);
}

flecs::entity CreateSpinner(const flecs::world& world, const platform::Rect rect, const Spinner& spinner) {
	return world.entity().set<UIRect>({rect}).set<Spinner>(spinner);
}

flecs::entity CreateStack(const flecs::world& world, const platform::Rect rect, const Stack& stack) {
	return world.entity().set<UIRect>({rect}).set<Stack>(stack);
}

flecs::entity CreateScrollRect(const flecs::world& world, const platform::Rect rect, const ScrollRect& scroll) {
	return world.entity().set<UIRect>({rect}).set<Panel>({.color = platform::colors::PanelBg}).set<ScrollRect>(scroll);
}

flecs::entity CreateModal(const flecs::world& world, const platform::Rect rect, const Modal& modal) {
	return world.entity()
		.set<UIRect>({rect})
		.set<UIElement>({.z_index = 1000})
		.set<Modal>(modal)
		.set<Panel>({.color = platform::colors::Panel, .roundness = 0.1F});
}

flecs::entity CreatePrompt(
	const flecs::world& world,
	const platform::Rect rect,
	std::string title,
	std::string message,
	std::vector<PromptButton> buttons
) {
	const flecs::entity modal = CreateModal(world, rect);

	// Vertical stack filling the dialog with padding: title, message, then a button
	const flecs::entity column = CreateStack(
									 world,
									 {.x = rect.x, .y = rect.y, .w = rect.w, .h = rect.h},
									 {.direction = StackDirection::Vertical, .spacing = 12.0F, .padding = 18.0F}
	)
									 .child_of(modal);

	std::ignore =
		CreateLabel(world, {.x = 0.0F, .y = 0.0F, .w = 0.0F, .h = 34.0F}, std::move(title), 26.0F).child_of(column);
	std::ignore =
		CreateLabel(world, {.x = 0.0F, .y = 0.0F, .w = 0.0F, .h = 48.0F}, std::move(message), 18.0F).child_of(column);

	// Vertical stack of the action buttons.
	const flecs::entity row = CreateStack(
								  world,
								  {.x = 0.0F, .y = 0.0F, .w = 0.0F, .h = 48.0F},
								  {.direction = StackDirection::Vertical, .spacing = 12.0F, .padding = 0.0F}
	)
								  .child_of(column);

	for (auto& [label, on_click] : buttons) {
		std::ignore =
			CreateButton(world, {.x = 0.0F, .y = 0.0F, .w = 140.0F, .h = 48.0F}, std::move(label), std::move(on_click))
				.child_of(row);
	}

	return modal;
}

} // namespace engine::ui
