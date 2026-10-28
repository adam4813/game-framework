#include "ui_module.hpp"

#include <memory>
#include <string>

#include <flecs.h>
#include <glm/glm.hpp>
#include <spdlog/spdlog.h>

#include "engine/scripting/scripting.hpp"
#include "ui_binding.hpp"
#include "ui_components.hpp"
#include "ui_draw_list.hpp"
#include "ui_events.hpp"
#include "ui_json.hpp"
#include "ui_scripting.hpp"
#include "ui_systems.hpp"

namespace engine::ui {

UIModule::UIModule(const flecs::world& world) {
	// (1) Singletons.
	world.set<UIDrawList>({});
	world.set<UIConfig>({});
	world.set<UIModalState>({});
	world.set<UIFocus>({});

	// (2) Reflection — expose the plain-data components in the Flecs Explorer. core::Rect is
	// registered here (it has no other Flecs home) so UIRect can reference it as a member type.
	world.component<core::Rect>().member<float>("x").member<float>("y").member<float>("w").member<float>("h");
	world.component<UIRect>().member<core::Rect>("rect");
	world.component<UIElement>().member<int>("z_index").member<bool>("visible");
	world.component<Panel>()
		.member<core::Rgba>("color")
		.member<core::Rgba>("border_color")
		.member<float>("border_thickness")
		.member<float>("roundness");
	world.component<TextAlign>()
		.constant("Left", TextAlign::Left)
		.constant("Center", TextAlign::Center)
		.constant("Right", TextAlign::Right);
	// `align` is authored at the JSON deserialization stage (the loader's ParseAlign, string → enum) and
	// is deliberately not meta-reflected — the same pattern as PathFollow::mode (its enum constants are
	// registered for the Explorer, but the member itself is skipped). Reflecting the 1-byte enum member
	// trips the meta serializer, and it isn't needed for authoring.
	world.component<Label>()
		.member("text", &Label::text)
		.member("font_size", &Label::font_size)
		.member("color", &Label::color)
		.member("outline", &Label::outline)
		.member("outline_color", &Label::outline_color);
	world.component<Button>()
		.member("label", &Button::label)
		.member("font_size", &Button::font_size)
		.member("normal", &Button::normal)
		.member("hover", &Button::hover)
		.member("pressed_color", &Button::pressed_color)
		.member("border_color", &Button::border_color)
		.member("text_color", &Button::text_color)
		.member("roundness", &Button::roundness)
		.member("border_thickness", &Button::border_thickness)
		.member("hovered", &Button::hovered)
		.member("pressed", &Button::pressed)
		.member("clicked", &Button::clicked);
	world.component<Slider>()
		.member("value", &Slider::value)
		.member("min", &Slider::min)
		.member("max", &Slider::max)
		.member("step", &Slider::step)
		.member("hovered", &Slider::hovered)
		.member("dragging", &Slider::dragging);
	world.component<ProgressBar>()
		.member<float>("value")
		.member<float>("min")
		.member<float>("max")
		.member<core::Rgba>("fill_color")
		.member<core::Rgba>("track_color")
		.member<core::Rgba>("border_color")
		.member<float>("border_thickness")
		.member<float>("roundness");
	world.component<Spinner>()
		.member<core::Rgba>("color")
		.member<float>("radius")
		.member<float>("dot_radius")
		.member<int>("dots")
		.member<float>("speed")
		.member<float>("phase");
	world.component<Stack>()
		.member("spacing", &Stack::spacing)
		.member("padding", &Stack::padding)
		.member("stretch", &Stack::stretch);
	world.component<ScrollRect>()
		.member<glm::vec2>("scroll")
		.member<glm::vec2>("content_size")
		.member<bool>("vertical")
		.member<bool>("horizontal")
		.member<float>("scrollbar_thickness")
		.member<float>("wheel_speed")
		.member<core::Rgba>("track_color")
		.member<core::Rgba>("thumb_color")
		.member<bool>("dragging_v")
		.member<bool>("dragging_h");
	world.component<Modal>()
		.member<bool>("open")
		.member<core::Rgba>("backdrop_color")
		.member<bool>("close_on_backdrop");
	world.component<Checkbox>()
		.member("checked", &Checkbox::checked)
		.member("label", &Checkbox::label)
		.member("font_size", &Checkbox::font_size)
		.member("box_size", &Checkbox::box_size)
		.member("box_color", &Checkbox::box_color)
		.member("check_color", &Checkbox::check_color)
		.member("border_color", &Checkbox::border_color)
		.member("text_color", &Checkbox::text_color)
		.member("roundness", &Checkbox::roundness)
		.member("hovered", &Checkbox::hovered);
	world.component<Radio>()
		.member("group", &Radio::group)
		.member("value", &Radio::value)
		.member("selected", &Radio::selected)
		.member("label", &Radio::label)
		.member("font_size", &Radio::font_size)
		.member("radius", &Radio::radius)
		.member("circle_color", &Radio::circle_color)
		.member("dot_color", &Radio::dot_color)
		.member("border_color", &Radio::border_color)
		.member("text_color", &Radio::text_color)
		.member("hovered", &Radio::hovered);
	world.component<TextInput>()
		.member("text", &TextInput::text)
		.member("placeholder", &TextInput::placeholder)
		.member("max_length", &TextInput::max_length)
		.member("font_size", &TextInput::font_size)
		.member("bg_color", &TextInput::bg_color)
		.member("border_color", &TextInput::border_color)
		.member("focus_border_color", &TextInput::focus_border_color)
		.member("text_color", &TextInput::text_color)
		.member("placeholder_color", &TextInput::placeholder_color)
		.member("caret_color", &TextInput::caret_color)
		.member("roundness", &TextInput::roundness)
		.member("hovered", &TextInput::hovered)
		.member("caret_blink", &TextInput::caret_blink);
	world.component<TextArea>()
		.member("text", &TextArea::text)
		.member("placeholder", &TextArea::placeholder)
		.member("max_length", &TextArea::max_length)
		.member("font_size", &TextArea::font_size)
		.member("line_spacing", &TextArea::line_spacing)
		.member("bg_color", &TextArea::bg_color)
		.member("border_color", &TextArea::border_color)
		.member("focus_border_color", &TextArea::focus_border_color)
		.member("text_color", &TextArea::text_color)
		.member("placeholder_color", &TextArea::placeholder_color)
		.member("caret_color", &TextArea::caret_color)
		.member("roundness", &TextArea::roundness)
		.member("hovered", &TextArea::hovered)
		.member("caret_blink", &TextArea::caret_blink);
	world.component<Dropdown>()
		.member("selected", &Dropdown::selected)
		.member("placeholder", &Dropdown::placeholder)
		.member("open", &Dropdown::open)
		.member("font_size", &Dropdown::font_size)
		.member("row_height", &Dropdown::row_height)
		.member("bg_color", &Dropdown::bg_color)
		.member("border_color", &Dropdown::border_color)
		.member("text_color", &Dropdown::text_color)
		.member("panel_color", &Dropdown::panel_color)
		.member("hover_color", &Dropdown::hover_color)
		.member("roundness", &Dropdown::roundness)
		.member("hovered", &Dropdown::hovered)
		.member("hover_index", &Dropdown::hover_index);

	world.component<UIClickRequest>();
	world.component<OnClick>();

	// Scripting — expose the components to scripts so they can read/write UI state (e.g.
	// self.GetProgressBar().value = x). Components are registered as reference types and only their
	// meta-reflected members are exposed; unreflected members (std::function callbacks, Dropdown's
	// options vector, Stack's enum direction, Label's align) are skipped automatically. The draw-list
	// types and OnClick are intentionally left engine-only.
	scripting::RegisterValueTypeForScripts(world, world.component<core::Rect>());
	scripting::RegisterComponentForScripts(world, world.component<UIRect>());
	scripting::RegisterComponentForScripts(world, world.component<UIElement>());
	scripting::RegisterComponentForScripts(world, world.component<Panel>());
	scripting::RegisterComponentForScripts(world, world.component<Label>());
	scripting::RegisterComponentForScripts(world, world.component<Button>());
	scripting::RegisterComponentForScripts(world, world.component<Slider>());
	scripting::RegisterComponentForScripts(world, world.component<ProgressBar>());
	scripting::RegisterComponentForScripts(world, world.component<Spinner>());
	scripting::RegisterComponentForScripts(world, world.component<Stack>());
	scripting::RegisterComponentForScripts(world, world.component<ScrollRect>());
	scripting::RegisterComponentForScripts(world, world.component<Modal>());
	scripting::RegisterComponentForScripts(world, world.component<Checkbox>());
	scripting::RegisterComponentForScripts(world, world.component<Radio>());
	scripting::RegisterComponentForScripts(world, world.component<TextInput>());
	scripting::RegisterComponentForScripts(world, world.component<TextArea>());
	scripting::RegisterComponentForScripts(world, world.component<Dropdown>());

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
		[](const scripting::ScriptCallContext& ctx, flecs::world&, scripting::ScriptFunctionHandle* handle) {
			const auto* ref = static_cast<scripting::ScriptEntityRef*>(ctx.GetArgObject(0));
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

	// (3) Systems — split across phases (PreUpdate layout, OnUpdate interaction, OnUI render).
	RegisterUISystems(world);
	RegisterUIBindings(world);
	RegisterUILevelLoaders(world);
	RegisterUIScriptFactories(world);

	spdlog::info("[UIModule] Registered UI components, draw list, and systems with Flecs");
}

} // namespace engine::ui
