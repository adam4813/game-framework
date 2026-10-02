#include "ui_systems.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "engine/audio/audio.hpp"
#include "engine/ecs/ecs.hpp"
#include "engine/input/input.hpp"
#include "engine/platform/platform.hpp"
#include "ui_components.hpp"
#include "ui_draw_list.hpp"
#include "ui_events.hpp"

namespace engine::ui {

namespace {

constexpr float kTwoPi = 6.28318530717958647692F;

core::Rect Translate(const core::Rect r, const glm::vec2 offset) {
	return {.x = r.x + offset.x, .y = r.y + offset.y, .w = r.w, .h = r.h};
}

// Creation-order key for an entity: the low 32 bits of a Flecs id are its index. NOTE: this is NOT
// a safe ordering key for UI siblings, because Flecs recycles entity ids (destroying and recreating
// a subtree — as the system-stats panel does on expand/collapse — can hand back ids whose index
// order no longer matches visual creation order). UI paint/layout therefore follow the parent's
// `children()` iteration order (tree pre-order), the same traversal used by the layout pass, and
// rely on `UIElement.z_index` for explicit layering rather than any id-derived order.

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
core::Rect ResolvedRect(const flecs::entity entity, const UIRect& ui) {
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

// Shared gate for whether a control may receive pointer interaction this frame: it must be visible,
// not clipped out of a ScrollRect, inside the open modal (if any), and the pointer press must not
// have already been consumed by an overlay on top.
bool ElementInteractable(const flecs::entity entity, const glm::vec2 mouse, const UIModalState& modal_state) {
	return SubtreeVisible(entity)
		   && WithinScrollViewports(entity, mouse)
		   && !modal_state.pointer_consumed
		   && (!modal_state.any_open || InsideOpenModal(entity));
}

// Play an element's audio::SoundEffect, if present (direct platform call). Shared by click
// dispatch and the toggle/select controls so every interactive widget can click audibly.
void PlayElementSound(const flecs::entity entity) {
	if (const auto* sfx = entity.try_get<audio::SoundEffect>(); sfx != nullptr && sfx->handle >= 0) {
		entity.world().get<platform::PlatformRef>().ptr->PlaySound(sfx->handle);
	}
}

// Run a button's click reactions: play its SoundEffect immediately (direct platform call, not via
// the SoundEffectPlayback system) then invoke its OnClick handler. Sound is played BEFORE the
// callback so that a callback that triggers a scene switch (which will destroy this entity and its
// SoundEffect at the next deferred-command flush) doesn't silence the click.
void DispatchClick(const flecs::entity entity) {
	PlayElementSound(entity);
	if (const auto* on_click = entity.try_get<OnClick>(); on_click != nullptr && on_click->callback) {
		on_click->callback(entity);
	}
}

// Centre `text` horizontally within `r` for the given alignment and vertically in all cases.
glm::vec2 PlaceText(
	const platform::Platform* platform,
	const core::Rect r,
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

// Split `text` into lines that fit within `max_width` (in pixels at `font_size`), breaking on spaces
// and honouring explicit '\n'. A single word wider than max_width is left on its own (overflowing)
// line rather than dropped. Used by wrapped Labels.
std::vector<std::string>
WrapText(const platform::Platform* platform, const std::string& text, const float max_width, const float font_size) {
	std::vector<std::string> lines;
	std::string line;
	std::string word;
	const auto commit_word = [&]() {
		if (word.empty()) {
			return;
		}
		const std::string trial = line.empty() ? word : line + ' ' + word;
		if (line.empty() || platform->MeasureText(trial, font_size) <= max_width) {
			line = trial;
		}
		else {
			lines.push_back(line);
			line = word;
		}
		word.clear();
	};
	for (const char c : text) {
		if (c == '\n') {
			commit_word();
			lines.push_back(line);
			line.clear();
		}
		else if (c == ' ') {
			commit_word();
		}
		else {
			word.push_back(c);
		}
	}
	commit_word();
	if (!line.empty() || lines.empty()) {
		lines.push_back(line);
	}
	return lines;
}

// Append the draw commands for a single UI element (all its visual components) at layer `z`. The
// draw list stamps `z` (via SetLayer) and an auto-incrementing sequence onto each pushed command,
// so the paint order is (layer, push-order) with no reliance on iteration order.
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
	list.SetLayer(z);
	const core::Rect r = Translate(ui->rect, offset);

	if (const auto* modal = entity.try_get<Modal>()) {
		const core::Rect full{
			.x = 0.0F,
			.y = 0.0F,
			.w = static_cast<float>(platform->Width()),
			.h = static_cast<float>(platform->Height())
		};
		list.PushRect(full, modal->backdrop_color);
	}

	if (const auto* panel = entity.try_get<Panel>()) {
		if (panel->texture >= 0) {
			list.PushTexturedRect(panel->texture, r, panel->color);
			if (panel->border_thickness > 0.0F) {
				if (panel->roundness > 0.0F) {
					list.PushRoundedRectLines(r, panel->roundness, panel->border_thickness, panel->border_color);
				}
				else {
					list.PushRectLines(r, panel->border_thickness, panel->border_color);
				}
			}
		}
		else if (panel->roundness > 0.0F) {
			list.PushRoundedRect(r, panel->roundness, panel->color);
			if (panel->border_thickness > 0.0F) {
				list.PushRoundedRectLines(r, panel->roundness, panel->border_thickness, panel->border_color);
			}
		}
		else {
			list.PushRect(r, panel->color);
			if (panel->border_thickness > 0.0F) {
				list.PushRectLines(r, panel->border_thickness, panel->border_color);
			}
		}
	}

	if (const auto* slider = entity.try_get<Slider>()) {
		const float cy = r.y + r.h / 2.0F;
		const float track_y = cy - slider->track_height / 2.0F;
		const float thumb_r = slider->thumb_radius;
		const core::Rect track{.x = r.x + thumb_r, .y = track_y, .w = r.w - 2.0F * thumb_r, .h = slider->track_height};
		list.PushRoundedRect(track, 0.5F, slider->track_color);
		if (const float frac = slider->Fraction(); frac > 0.0F) {
			const core::Rect fill{.x = track.x, .y = track_y, .w = track.w * frac, .h = slider->track_height};
			list.PushRoundedRect(fill, 0.5F, slider->fill_color);
		}
		const glm::vec2 thumb{r.x + thumb_r + slider->Fraction() * track.w, cy};
		const core::Rgba thumb_color = slider->hovered ? slider->thumb_hover_color : slider->thumb_color;
		list.PushCircle(thumb, thumb_r, thumb_color);
	}

	if (const auto* bar = entity.try_get<ProgressBar>()) {
		if (bar->roundness > 0.0F) {
			list.PushRoundedRect(r, bar->roundness, bar->track_color);
		}
		else {
			list.PushRect(r, bar->track_color);
		}
		if (const float frac = bar->Fraction(); frac > 0.0F) {
			const core::Rect fill{.x = r.x, .y = r.y, .w = r.w * frac, .h = r.h};
			if (bar->roundness > 0.0F) {
				list.PushRoundedRect(fill, bar->roundness, bar->fill_color);
			}
			else {
				list.PushRect(fill, bar->fill_color);
			}
		}
		if (bar->border_thickness > 0.0F) {
			if (bar->roundness > 0.0F) {
				list.PushRoundedRectLines(r, bar->roundness, bar->border_thickness, bar->border_color);
			}
			else {
				list.PushRectLines(r, bar->border_thickness, bar->border_color);
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
			core::Rgba color = spinner->color;
			color.a = static_cast<std::uint8_t>(40.0F + 215.0F * t);
			list.PushCircle(pos, spinner->dot_radius, color);
		}
	}

	if (const auto* button = entity.try_get<Button>()) {
		const core::Rgba fill =
			button->pressed ? button->pressed_color : (button->hovered ? button->hover : button->normal);
		if (button->texture >= 0) {
			// Tint the texture by brightness for hover/press feedback, decoupled from the color fields
			// so any button art reads consistently.
			const core::Rgba tint = button->pressed
										? core::Rgba{.r = 170, .g = 170, .b = 170, .a = 255}
										: (button->hovered ? core::Rgba{.r = 255, .g = 255, .b = 255, .a = 255}
														   : core::Rgba{.r = 225, .g = 225, .b = 225, .a = 255});
			list.PushTexturedRect(button->texture, r, tint);
		}
		else {
			list.PushRoundedRect(r, button->roundness, fill);
		}
		list.PushRoundedRectLines(r, button->roundness, 2.0F, button->border_color);
		if (!button->label.empty()) {
			const glm::vec2 pos = PlaceText(platform, r, button->label, button->font_size, TextAlign::Center);
			list.PushText(button->label, pos.x, pos.y, button->font_size, button->text_color);
		}
	}

	if (const auto* label = entity.try_get<Label>()) {
		if (!label->text.empty()) {
			if (label->wrap) {
				const float line_height = label->font_size + label->line_spacing;
				float ly = r.y;
				for (const auto& line : WrapText(platform, label->text, r.w, label->font_size)) {
					if (!line.empty()) {
						const float tw = platform->MeasureText(line, label->font_size);
						float lx = r.x;
						switch (label->align) {
						case TextAlign::Left: lx = r.x; break;
						case TextAlign::Center: lx = r.x + (r.w - tw) / 2.0F; break;
						case TextAlign::Right: lx = r.x + r.w - tw; break;
						}
						list.PushText(line, lx, ly, label->font_size, label->color);
					}
					ly += line_height;
				}
			}
			else {
				const glm::vec2 pos = PlaceText(platform, r, label->text, label->font_size, label->align);
				list.PushText(label->text, pos.x, pos.y, label->font_size, label->color);
			}
		}
	}

	if (const auto* cb = entity.try_get<Checkbox>()) {
		const core::Rect box{.x = r.x, .y = r.y + (r.h - cb->box_size) / 2.0F, .w = cb->box_size, .h = cb->box_size};
		list.PushRoundedRect(box, cb->roundness, cb->box_color);
		list.PushRoundedRectLines(box, cb->roundness, 2.0F, cb->hovered ? cb->check_color : cb->border_color);
		if (cb->checked) {
			const core::Rect
				tick{.x = box.x + box.w * 0.25F, .y = box.y + box.h * 0.25F, .w = box.w * 0.5F, .h = box.h * 0.5F};
			list.PushRoundedRect(tick, 0.3F, cb->check_color);
		}
		if (!cb->label.empty()) {
			const float tx = box.x + box.w + 8.0F;
			list.PushText(cb->label, tx, r.y + (r.h - cb->font_size) / 2.0F, cb->font_size, cb->text_color);
		}
	}

	if (const auto* rb = entity.try_get<Radio>()) {
		const glm::vec2 center{r.x + rb->radius, r.y + r.h / 2.0F};
		list.PushCircle(center, rb->radius, rb->hovered ? rb->dot_color : rb->border_color);
		list.PushCircle(center, rb->radius - 2.0F, rb->circle_color);
		if (rb->selected) {
			list.PushCircle(center, rb->radius * 0.5F, rb->dot_color);
		}
		if (!rb->label.empty()) {
			const float tx = center.x + rb->radius + 8.0F;
			list.PushText(rb->label, tx, r.y + (r.h - rb->font_size) / 2.0F, rb->font_size, rb->text_color);
		}
	}

	if (const auto* ti = entity.try_get<TextInput>()) {
		const bool focused = entity.world().get<UIFocus>().focused == entity;
		list.PushRoundedRect(r, ti->roundness, ti->bg_color);
		list.PushRoundedRectLines(r, ti->roundness, 2.0F, focused ? ti->focus_border_color : ti->border_color);
		constexpr float pad = 8.0F;
		const core::Rect inner{.x = r.x + pad, .y = r.y, .w = r.w - 2.0F * pad, .h = r.h};
		list.PushBeginClip(inner);
		const float ty = r.y + (r.h - ti->font_size) / 2.0F;
		if (!ti->text.empty()) {
			list.PushText(ti->text, inner.x, ty, ti->font_size, ti->text_color);
		}
		else if (!focused && !ti->placeholder.empty()) {
			list.PushText(ti->placeholder, inner.x, ty, ti->font_size, ti->placeholder_color);
		}
		if (focused && std::fmod(ti->caret_blink, 1.0F) < 0.5F) {
			const float caret_x = inner.x + platform->MeasureText(ti->text, ti->font_size) + 1.0F;
			list.PushLine({caret_x, r.y + r.h * 0.2F}, {caret_x, r.y + r.h * 0.8F}, 1.5F, ti->caret_color);
		}
		list.PushEndClip();
	}

	if (const auto* ta = entity.try_get<TextArea>()) {
		const bool focused = entity.world().get<UIFocus>().focused == entity;
		list.PushRoundedRect(r, ta->roundness, ta->bg_color);
		list.PushRoundedRectLines(r, ta->roundness, 2.0F, focused ? ta->focus_border_color : ta->border_color);
		constexpr float pad = 8.0F;
		const core::Rect inner{.x = r.x + pad, .y = r.y + pad, .w = r.w - 2.0F * pad, .h = r.h - 2.0F * pad};
		list.PushBeginClip(inner);
		const float line_height = ta->font_size + ta->line_spacing;
		if (ta->text.empty() && !focused && !ta->placeholder.empty()) {
			list.PushText(ta->placeholder, inner.x, inner.y, ta->font_size, ta->placeholder_color);
		}
		else {
			const std::vector<std::string> lines = WrapText(platform, ta->text, inner.w, ta->font_size);
			float ly = inner.y;
			for (const auto& line : lines) {
				if (!line.empty()) {
					list.PushText(line, inner.x, ly, ta->font_size, ta->text_color);
				}
				ly += line_height;
			}
			if (focused && std::fmod(ta->caret_blink, 1.0F) < 0.5F) {
				const float last_w = lines.empty() ? 0.0F : platform->MeasureText(lines.back(), ta->font_size);
				const float caret_x = inner.x + last_w + 1.0F;
				const float caret_y = ly - line_height;
				list.PushLine({caret_x, caret_y}, {caret_x, caret_y + ta->font_size}, 1.5F, ta->caret_color);
			}
		}
		list.PushEndClip();
	}

	if (const auto* dd = entity.try_get<Dropdown>()) {
		list.PushRoundedRect(r, dd->roundness, dd->bg_color);
		list.PushRoundedRectLines(r, dd->roundness, 2.0F, dd->hovered ? core::colors::Accent : dd->border_color);
		constexpr float pad = 8.0F;
		const core::Rgba label_color = dd->selected >= 0 ? dd->text_color : core::colors::Subtle;
		list.PushText(dd->DisplayText(), r.x + pad, r.y + (r.h - dd->font_size) / 2.0F, dd->font_size, label_color);
		const float ax = r.x + r.w - 20.0F;
		const float ay = r.y + r.h / 2.0F - 2.0F;
		list.PushLine({ax, ay}, {ax + 5.0F, ay + 5.0F}, 2.0F, dd->text_color);
		list.PushLine({ax + 5.0F, ay + 5.0F}, {ax + 10.0F, ay}, 2.0F, dd->text_color);
		if (dd->open && !dd->options.empty()) {
			// Raise the option list above sibling UI at this layer so it overlays rather than being
			// painted under later-walked siblings.
			list.SetLayer(z + 500);
			const auto count = static_cast<int>(dd->options.size());
			for (int i = 0; i < count; ++i) {
				const core::Rect row{
					.x = r.x,
					.y = r.y + r.h + static_cast<float>(i) * dd->row_height,
					.w = r.w,
					.h = dd->row_height
				};
				list.PushRect(row, i == dd->hover_index ? dd->hover_color : dd->panel_color);
				list.PushText(
					dd->options[static_cast<size_t>(i)],
					row.x + pad,
					row.y + (dd->row_height - dd->font_size) / 2.0F,
					dd->font_size,
					dd->text_color
				);
			}
			const core::Rect
				list_rect{.x = r.x, .y = r.y + r.h, .w = r.w, .h = static_cast<float>(count) * dd->row_height};
			list.PushRectLines(list_rect, 2.0F, dd->border_color);
			list.SetLayer(z);
		}
	}
}

// Draw a ScrollRect's scrollbar(s) on top of (outside) the clipped content, at layer `z`.
void EmitScrollbar(const core::Rect viewport, const ScrollRect& sr, UIDrawList& list, const int z) {
	list.SetLayer(z);
	if (sr.vertical && sr.content_size.y > viewport.h) {
		const core::Rect track{
			.x = viewport.x + viewport.w - sr.scrollbar_thickness,
			.y = viewport.y,
			.w = sr.scrollbar_thickness,
			.h = viewport.h
		};
		list.PushRect(track, sr.track_color);
		const float view_ratio = viewport.h / sr.content_size.y;
		const float thumb_h = std::max(24.0F, viewport.h * view_ratio);
		const float range = sr.content_size.y - viewport.h;
		const float t = range > 0.0F ? glm::clamp(sr.scroll.y / range, 0.0F, 1.0F) : 0.0F;
		const core::Rect thumb{
			.x = track.x,
			.y = viewport.y + t * (viewport.h - thumb_h),
			.w = sr.scrollbar_thickness,
			.h = thumb_h
		};
		list.PushRoundedRect(thumb, 0.5F, sr.thumb_color);
	}
	if (sr.horizontal && sr.content_size.x > viewport.w) {
		const core::Rect track{
			.x = viewport.x,
			.y = viewport.y + viewport.h - sr.scrollbar_thickness,
			.w = viewport.w,
			.h = sr.scrollbar_thickness
		};
		list.PushRect(track, sr.track_color);
		const float view_ratio = viewport.w / sr.content_size.x;
		const float thumb_w = std::max(24.0F, viewport.w * view_ratio);
		const float range = sr.content_size.x - viewport.w;
		const float t = range > 0.0F ? glm::clamp(sr.scroll.x / range, 0.0F, 1.0F) : 0.0F;
		const core::Rect thumb{
			.x = viewport.x + t * (viewport.w - thumb_w),
			.y = track.y,
			.w = thumb_w,
			.h = sr.scrollbar_thickness
		};
		list.PushRoundedRect(thumb, 0.5F, sr.thumb_color);
	}
}

// Emit an element then its UI subtree (pre-order: parent before children, siblings in the parent's
// `children()` iteration order — the same traversal the layout pass uses). `inherited_z` lets a
// high-z layer (e.g. a Modal) raise its whole subtree above other UI; ScrollRect subtrees are
// wrapped in clip commands and offset.
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
		const core::Rect viewport = ui != nullptr ? Translate(ui->rect, offset) : core::Rect{};
		list.SetLayer(z);
		list.PushBeginClip(viewport);
		const glm::vec2 child_offset = offset - sr->scroll;
		entity.children([&list, platform, child_offset, z](const flecs::entity child) {
			if (child.has<UIRect>()) {
				EmitTree(child, list, platform, child_offset, z);
			}
		});
		list.SetLayer(z);
		list.PushEndClip();
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
			const core::Rect base = ui->rect;
			const bool vertical = stack->direction == StackDirection::Vertical;
			float cursor = (vertical ? base.y : base.x) + stack->padding;
			const float cross = (vertical ? base.x : base.y) + stack->padding;
			entity.children([&](const flecs::entity child) {
				if (!child.has<UIRect>()) {
					return;
				}
				core::Rect& cr = child.get_mut<UIRect>().rect;
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

void RegisterUISystems(const flecs::world& world) {
	// Systems — split across phases like ordinary simulation, and splitting "view" (tree/layout +
	// draw-command construction) from "presentation" (submitting those commands to the platform):
	//   PreUpdate  — UILayout: position stack children so hit-test rects are final.
	//   OnUpdate   — interaction/logic (reads input polled in PreUpdate, before PostUpdate resets).
	//   PreStore   — UIBuildDrawList: walk the resolved UI tree into the ordered draw list (the
	//                "view"), after all OnUpdate mutation (interaction + scene Tick) has settled.
	//   OnUI       — UIRenderDrawList: submit the draw list to the platform (after 3D in OnStore so
	//                2D composites on top).
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

	// === SYSTEM: Update cached UI input state (OnUpdate) ===
	// Runs first each frame (within OnUpdate, after any scene Tick that toggles a modal): clears the
	// pointer-consume flag and recomputes whether any modal is open, so later widget interactions share
	// one cheap snapshot and overlays can capture the press. Caching any_open here (before
	// UIModalInteraction may close a modal) means a backdrop-dismiss click leaves the underlying widgets
	// blocked until the next frame.
	const auto modal_query = world.query_builder<const Modal>().build();
	world.system("UIUpdateModalState").kind(flecs::OnUpdate).run([modal_query](const flecs::iter& it) {
		auto& modal_state = it.world().get_mut<UIModalState>();
		modal_state.any_open = false;
		modal_state.pointer_consumed = false;
		modal_query.each([&modal_state](const Modal& m) {
			if (m.open) {
				modal_state.any_open = true;
			}
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
			const core::Rect viewport = ResolvedRect(entity, ui);
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
				const core::Rect thumb{.x = track_x, .y = thumb_y, .w = sr.scrollbar_thickness, .h = thumb_h};

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

				if (const core::Rect thumb{.x = thumb_x, .y = track_y, .w = thumb_w, .h = sr.scrollbar_thickness};
					!sr.dragging_h && mouse_pressed && thumb.Contains(mouse)) {
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
	// Closes a close_on_backdrop modal clicked outside, and consumes that press so the dismissing click
	// is not also delivered to whatever sits beneath the backdrop.
	world.system<Modal, const UIRect>("UIModalInteraction")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity entity, Modal& modal, const UIRect& ui) {
			if (!modal.open || !modal.close_on_backdrop) {
				return;
			}
			const auto& input = entity.world().get<input::InputState>();
			if (const glm::vec2 mouse{input.mouse.window_position.x, input.mouse.window_position.y};
				input.mouse.left.pressed && !ResolvedRect(entity, ui).Contains(mouse)) {
				modal.open = false;
				entity.world().get_mut<UIModalState>().pointer_consumed = true;
			}
		});

	// === SYSTEM: Dropdown interaction (OnUpdate) ===
	// Runs before the regular widgets: toggles the option list open on a press over the closed box;
	// while open, tracks the hovered row and, on a press, selects a row (firing on_change) or closes
	// when clicking the box/outside. Any press it handles is consumed so the click doesn't also reach
	// the widgets its elevated option list overlaps.
	world.system<Dropdown, const UIRect>("UIDropdownInteraction")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity entity, Dropdown& dd, const UIRect& ui) {
			const auto& input = entity.world().get<input::InputState>();
			const glm::vec2 mouse{input.mouse.window_position.x, input.mouse.window_position.y};
			const auto& modal_state = entity.world().get<UIModalState>();
			const core::Rect box = ResolvedRect(entity, ui);
			dd.hovered = ElementInteractable(entity, mouse, modal_state) && box.Contains(mouse);
			dd.hover_index = -1;
			const bool pressed = input.mouse.left.pressed;
			if (dd.open) {
				const auto count = static_cast<int>(dd.options.size());
				for (int i = 0; i < count; ++i) {
					const core::Rect row{
						.x = box.x,
						.y = box.y + box.h + static_cast<float>(i) * dd.row_height,
						.w = box.w,
						.h = dd.row_height
					};
					if (row.Contains(mouse)) {
						dd.hover_index = i;
					}
				}
				if (pressed) {
					if (dd.hover_index >= 0) {
						dd.selected = dd.hover_index;
						dd.open = false;
						PlayElementSound(entity);
						if (dd.on_change) {
							dd.on_change(dd.selected);
						}
					}
					else {
						dd.open = false; // clicking the box or outside the list closes it
					}
					entity.world().get_mut<UIModalState>().pointer_consumed = true;
				}
			}
			else if (dd.hovered && pressed) {
				dd.open = true;
				PlayElementSound(entity);
				entity.world().get_mut<UIModalState>().pointer_consumed = true;
			}
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

			const bool interactable = ElementInteractable(entity, mouse, modal_state);
			const bool hovering = interactable && ResolvedRect(entity, ui).Contains(mouse);

			button.hovered = hovering;
			button.clicked = false;
			// Start a press only on the rising edge over the button, so a press cannot begin mid-hold
			// (e.g. the frame after a modal backdrop closes and unblocks the button). A release while
			// still hovering completes the click.
			const bool mouse_down = input.mouse.left.state;
			if (hovering && input.mouse.left.pressed) {
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

	// === SYSTEM: Slider interaction (OnUpdate) ===
	// Tracks hover and drag state, maps mouse X to value, fires on_change when value changes.
	world.system<Slider, const UIRect>("UISliderInteraction")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity entity, Slider& slider, const UIRect& ui) {
			const auto& input = entity.world().get<input::InputState>();
			const glm::vec2 mouse{input.mouse.window_position.x, input.mouse.window_position.y};
			const core::Rect r = ResolvedRect(entity, ui);
			const auto& modal_state = entity.world().get<UIModalState>();

			slider.hovered = ElementInteractable(entity, mouse, modal_state) && r.Contains(mouse);

			const bool mouse_down = input.mouse.left.state;
			if (slider.hovered && input.mouse.left.pressed) {
				slider.dragging = true;
			}
			if (!mouse_down) {
				slider.dragging = false;
			}

			if (slider.dragging) {
				const float track_start = r.x + slider.thumb_radius;
				const float track_width = r.w - 2.0F * slider.thumb_radius;
				const float t = glm::clamp((mouse.x - track_start) / track_width, 0.0F, 1.0F);
				float new_value = slider.min + t * (slider.max - slider.min);
				if (slider.step > 0.0F) {
					new_value = std::round((new_value - slider.min) / slider.step) * slider.step + slider.min;
				}
				new_value = glm::clamp(new_value, slider.min, slider.max);
				if (new_value != slider.value) {
					slider.value = new_value;
					if (slider.on_change) {
						slider.on_change(new_value);
					}
				}
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

	// === SYSTEM: Checkbox interaction (OnUpdate) ===
	// Toggles `checked` on a left-press while hovered, plays the element's click sound, and fires
	// on_change with the new state.
	world.system<Checkbox, const UIRect>("UICheckboxInteraction")
		.kind(flecs::OnUpdate)
		.each([](const flecs::entity entity, Checkbox& cb, const UIRect& ui) {
			const auto& input = entity.world().get<input::InputState>();
			const glm::vec2 mouse{input.mouse.window_position.x, input.mouse.window_position.y};
			const auto& modal_state = entity.world().get<UIModalState>();
			cb.hovered = ElementInteractable(entity, mouse, modal_state) && ResolvedRect(entity, ui).Contains(mouse);
			if (cb.hovered && input.mouse.left.pressed) {
				cb.checked = !cb.checked;
				PlayElementSound(entity);
				if (cb.on_change) {
					cb.on_change(cb.checked);
				}
			}
		});

	// === SYSTEM: Radio interaction (OnUpdate) ===
	// Selects the clicked radio, clears every other radio sharing its `group`, and fires on_change.
	const auto radio_query = world.query_builder<Radio>().build();
	world.system<Radio, const UIRect>("UIRadioInteraction")
		.kind(flecs::OnUpdate)
		.each([radio_query](const flecs::entity entity, Radio& rb, const UIRect& ui) {
			const auto& input = entity.world().get<input::InputState>();
			const glm::vec2 mouse{input.mouse.window_position.x, input.mouse.window_position.y};
			const auto& modal_state = entity.world().get<UIModalState>();
			rb.hovered = ElementInteractable(entity, mouse, modal_state) && ResolvedRect(entity, ui).Contains(mouse);
			if (rb.hovered && input.mouse.left.pressed && !rb.selected) {
				rb.selected = true;
				const std::string group = rb.group;
				PlayElementSound(entity);
				if (rb.on_change) {
					rb.on_change(rb.value);
				}
				radio_query.each([&entity, &group](const flecs::entity other, Radio& other_rb) {
					if (other != entity && other_rb.group == group) {
						other_rb.selected = false;
					}
				});
			}
		});

	// === SYSTEM: Text field focus (OnUpdate) ===
	// On a left-press, sets UIFocus.focused to the text field under the cursor, or clears it when the
	// click lands elsewhere — so typed text and editing keys route to at most one field.
	const auto text_input_query = world.query_builder<const TextInput, const UIRect>().build();
	const auto text_area_query = world.query_builder<const TextArea, const UIRect>().build();
	world.system("UITextFieldFocus")
		.kind(flecs::OnUpdate)
		.run([text_input_query, text_area_query](const flecs::iter& it) {
			const flecs::world w = it.world();
			const auto& input = w.get<input::InputState>();
			if (!input.mouse.left.pressed) {
				return;
			}
			const glm::vec2 mouse{input.mouse.window_position.x, input.mouse.window_position.y};
			const auto& modal_state = w.get<UIModalState>();
			flecs::entity hit{};
			text_input_query.each([&](const flecs::entity e, const TextInput&, const UIRect& ui) {
				if (!hit && ElementInteractable(e, mouse, modal_state) && ResolvedRect(e, ui).Contains(mouse)) {
					hit = e;
				}
			});
			if (!hit) {
				text_area_query.each([&](const flecs::entity e, const TextArea&, const UIRect& ui) {
					if (!hit && ElementInteractable(e, mouse, modal_state) && ResolvedRect(e, ui).Contains(mouse)) {
						hit = e;
					}
				});
			}
			w.get_mut<UIFocus>().focused = hit;
		});

	// === SYSTEM: Single-line text editing (OnUpdate) ===
	// Applies typed text + Backspace (held-to-repeat) to the focused TextInput; Enter fires on_submit.
	world.system<TextInput>("UITextInputEdit")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, TextInput& ti) {
			const flecs::entity entity = it.entity(i);
			const flecs::world w = entity.world();
			if (w.get<UIFocus>().focused != entity) {
				ti.caret_blink = 0.0F;
				return;
			}
			ti.caret_blink += it.delta_time();
			const auto& input = w.get<input::InputState>();
			bool changed = false;
			for (const char c : w.get<input::TypedText>().text) {
				if (c == '\n' || c == '\t') {
					continue; // single-line: ignore newlines/tabs
				}
				if (ti.max_length > 0 && static_cast<int>(ti.text.size()) >= ti.max_length) {
					break;
				}
				ti.text.push_back(c);
				changed = true;
			}
			if (input.WasKeyPressedOrRepeated(input::KeyCode::Backspace) && !ti.text.empty()) {
				ti.text.pop_back();
				changed = true;
			}
			if (input.WasKeyPressed(input::KeyCode::Return) && ti.on_submit) {
				ti.on_submit(ti.text);
			}
			if (changed) {
				ti.caret_blink = 0.0F;
				if (ti.on_change) {
					ti.on_change(ti.text);
				}
			}
		});

	// === SYSTEM: Multi-line text editing (OnUpdate) ===
	// Like UITextInputEdit for the focused TextArea, but Enter inserts a newline instead of submitting.
	world.system<TextArea>("UITextAreaEdit")
		.kind(flecs::OnUpdate)
		.each([](const flecs::iter& it, const size_t i, TextArea& ta) {
			const flecs::entity entity = it.entity(i);
			const flecs::world w = entity.world();
			if (w.get<UIFocus>().focused != entity) {
				ta.caret_blink = 0.0F;
				return;
			}
			ta.caret_blink += it.delta_time();
			const auto& input = w.get<input::InputState>();
			const auto at_limit = [&ta] {
				return ta.max_length > 0 && static_cast<int>(ta.text.size()) >= ta.max_length;
			};
			bool changed = false;
			for (const char c : w.get<input::TypedText>().text) {
				if (c == '\t') {
					continue;
				}
				if (at_limit()) {
					break;
				}
				ta.text.push_back(c);
				changed = true;
			}
			if (input.WasKeyPressedOrRepeated(input::KeyCode::Return) && !at_limit()) {
				ta.text.push_back('\n');
				changed = true;
			}
			if (input.WasKeyPressedOrRepeated(input::KeyCode::Backspace) && !ta.text.empty()) {
				ta.text.pop_back();
				changed = true;
			}
			if (changed) {
				ta.caret_blink = 0.0F;
				if (ta.on_change) {
					ta.on_change(ta.text);
				}
			}
		});

	// === SYSTEM: Build the draw list (PreStore) ===
	// The "view" construction pass: walks the UI roots (elements whose parent is not itself a UI
	// element), recursing through children in `children()` (tree pre-order) so the hierarchy — not
	// a fragile id/archetype order — determines layering, with `UIElement.z_index` as the explicit
	// layer. Emits ordered commands into the shared UIDrawList. Runs in PreStore, after all OnUpdate
	// mutation (interaction + scene Tick) settles.
	const auto ui_query = world.query_builder<const UIRect>().build();
	world.system("UIBuildDrawList").kind(flecs::PreStore).run([ui_query](const flecs::iter& it) {
		const flecs::world ecs_world = it.world();
		auto& list = ecs_world.get_mut<UIDrawList>();
		auto* platform = ecs_world.get<platform::PlatformRef>().ptr;
		list.Clear();
		ui_query.each([&list, platform](const flecs::entity entity, const UIRect&) {
			if (const flecs::entity parent = entity.parent(); parent && parent.has<UIRect>()) {
				return; // drawn via its parent's recursion
			}
			EmitTree(entity, list, platform, glm::vec2{0.0F}, 0);
		});
	});

	// === SYSTEM: Flush the draw list (OnUI) ===
	// The "presentation" pass: sorts by (layer, seq) — a total order fixed by the build walk, not by
	// iteration order — then issues the platform 2D draw calls, maintaining a clip stack for
	// ScrollRect regions. Runs in OnUI (after 3D in OnStore) so 2D composites on top.
	world.system("UIRenderDrawList").kind<ecs::OnUI>().run([](const flecs::iter& it) {
		const flecs::world ecs_world = it.world();
		auto& list = ecs_world.get_mut<UIDrawList>();
		auto* platform = ecs_world.get<platform::PlatformRef>().ptr;

		std::ranges::stable_sort(list.commands, [](const UIDrawCommand& a, const UIDrawCommand& b) {
			if (a.layer != b.layer) {
				return a.layer < b.layer;
			}
			return a.seq < b.seq;
		});

		std::vector<core::Rect> clip_stack;
		const auto intersect = [](const core::Rect a, const core::Rect b) {
			const float x0 = std::max(a.x, b.x);
			const float y0 = std::max(a.y, b.y);
			const float x1 = std::min(a.x + a.w, b.x + b.w);
			const float y1 = std::min(a.y + a.h, b.y + b.h);
			return core::Rect{.x = x0, .y = y0, .w = std::max(0.0F, x1 - x0), .h = std::max(0.0F, y1 - y0)};
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
			case UIDrawKind::TexturedRect: platform->DrawTexturedRect(cmd.texture, cmd.rect, cmd.color); break;
			case UIDrawKind::BeginClip:
			{
				const core::Rect region = clip_stack.empty() ? cmd.rect : intersect(clip_stack.back(), cmd.rect);
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

	spdlog::info("[UIModule] Registered UI interaction/layout/build/render systems with Flecs");
}

} // namespace engine::ui
