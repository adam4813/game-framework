#include "input_module.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

#include <flecs.h>

#include "engine/ecs/ecs.hpp"
#include "engine/platform/platform.hpp"
#include "engine/scripting/scripting.hpp"
#include "input_components.hpp"
#include "key_names.hpp"

namespace engine::input {

namespace {
void RegisterScriptConstants(const flecs::world& world) {
	// === Script KeyCode constants ===
	// Expose engine::input::KeyCode as Key_<Name> globals so scripts never need raw integers or
	// backend-specific codes (see scripting/README.md for the naming convention).
	std::vector<scripting::ScriptConstant> constants;
	for (const auto& [name, code] : KeyNames()) {
		constants.push_back({std::format("Key_{}", name), scripting::ScriptValueType::MakeInt(), std::to_string(code)});
	}
	scripting::RegisterGlobalConstantsForScripts(world, std::move(constants));
}

// The names in KeyOverride::held, split on spaces.
std::vector<std::string_view> OverrideKeyNames(const KeyOverride& keyOverride) {
	std::vector<std::string_view> names;
	for (const auto word : std::views::split(std::string_view{keyOverride.held}, ' ')) {
		if (!word.empty()) {
			names.emplace_back(word.begin(), word.end());
		}
	}
	return names;
}

// The key codes the KeyOverride singleton holds down (unknown names are skipped; OnSet reports them).
std::vector<int> OverrideKeys(const KeyOverride& keyOverride) {
	std::vector<int> codes;
	for (const auto name : OverrideKeyNames(keyOverride)) {
		if (const auto code = KeyCodeFromName(name)) {
			codes.push_back(*code);
		}
	}
	return codes;
}
} // namespace

InputModule::InputModule(const flecs::world& world) {
	// Register input components
	world.component<InputEnabled>().add(EcsSingleton);

	// Initialize InputState singleton
	// Platform will update this each frame
	world.set<InputState>({});
	// Per-frame typed-text buffer (kept separate so InputState stays a trivially-copyable value type).
	world.set<TypedText>({});
	// What the UI claimed (filled by the UI module) and the mouse before that claim; see input_components.hpp.
	world.set<UICapture>({});
	world.set<RawInput>({});
	world.component<UICapture>().member<bool>("pointer").member<bool>("keyboard").member<bool>("pointer_latched");

	world.component<MouseCoord>().member<float>("x").member<float>("y");
	world.component<KeyState>().member<bool>("pressed").member<bool>("state").member<bool>("current").member<bool>(
		"repeat"
	);
	world.component<MouseState>()
		.member<KeyState>("left")
		.member<KeyState>("right")
		.member<KeyState>("middle")
		.member<MouseCoord>("window_position")
		.member<MouseCoord>("relative")
		.member<MouseCoord>("view")
		.member<MouseCoord>("scroll");

	scripting::RegisterValueTypeForScripts(world, world.component<MouseCoord>());
	scripting::RegisterValueTypeForScripts(world, world.component<KeyState>());
	scripting::RegisterValueTypeForScripts(world, world.component<MouseState>());

	// Reflect only InputState.mouse. The member-pointer overload derives the correct offset
	// from &InputState::mouse, so we can expose mouse without reflecting the preceding keys
	// std::array.
	world.component<InputState>().member("mouse", &InputState::mouse);

	world.component<KeyOverride>().member<std::string>("held").add(EcsSingleton);
	world.set<KeyOverride>({});
	world.observer<const KeyOverride>("KeyOverrideNameCheck")
		.event(flecs::OnSet)
		.each([](const KeyOverride& keyOverride) {
			for (const auto name : OverrideKeyNames(keyOverride)) {
				if (!KeyCodeFromName(name)) {
					spdlog::warn("[InputModule] KeyOverride names unknown key '{}' (use a KeyCode name)", name);
				}
			}
		});
	scripting::RegisterSingletonForScripts(world, world.component<InputState>());
	scripting::RegisterComponentMethodForScripts<&InputState::IsKeyDown>(world, "IsKeyDown");
	scripting::RegisterComponentMethodForScripts<&InputState::WasKeyPressed>(world, "WasKeyPressed");
	scripting::RegisterComponentMethodForScripts<&InputState::WasKeyPressedOrRepeated>(
		world,
		"WasKeyPressedOrRepeated"
	);
	scripting::RegisterComponentMethodForScripts<&InputState::IsKeyUp>(world, "IsKeyUp");

	RegisterScriptConstants(world);

	// === SYSTEM: Poll input from platform each frame ===
	// Updates the InputState singleton with current keyboard and mouse input. A FrameSample system, so the fixed step
	// runs it before the Frame's Ticks and every Tick sees this Frame's input.
	world.system("InputPoll")
		.kind(flecs::OnLoad)
		.write<InputState>()
		.write<RawInput>()
		.write<UICapture>()
		.run([](const flecs::iter& it) {
		auto& input_state = it.world().get_mut<InputState>();
		auto& keys = input_state.keys;
		auto& mouse = input_state.mouse;
		const auto& platform = it.world().get<platform::PlatformRef>();
		// Input the debug UI captured this frame (pointer over a panel, text field active) is withheld
		// so the game doesn't also act on it. Mouse position still flows for hover/picking.
		const auto& debug_ui = platform.ptr->GetDebugUi();
		// The UI's own capture (UICapture) gates the same inputs, below.
		const bool debug_keyboard_free = !debug_ui.CapturesKeyboard();
		const bool keyboard_free = debug_keyboard_free && !it.world().get<UICapture>().keyboard;
		auto& raw_state = it.world().get_mut<RawInput>();
		// A locked cursor is hidden, so it can't be over a panel: the mouse gate would only zero the look delta.
		const bool mouse_free = platform.ptr->IsCursorLocked() || !debug_ui.CapturesMouse();

		// Poll keyboard input; keys the KeyOverride holds count as physically down.
		const auto overridden = OverrideKeys(it.world().get<KeyOverride>());
		for (size_t i = 0; i < keys.size(); ++i) {
			auto& key_state = keys[i];
			const bool was_pressed = key_state.state;
			const bool now_pressed = (keyboard_free && platform.ptr->IsKeyDown(static_cast<int>(i)))
									 || std::ranges::contains(overridden, static_cast<int>(i));

			// Track press events (transition from unpressed to pressed)
			key_state.pressed = !was_pressed && now_pressed;
			key_state.state = now_pressed;
			key_state.current = now_pressed;
			key_state.repeat = keyboard_free && platform.ptr->IsKeyPressedRepeat(static_cast<int>(i));

			// The UI's view: only the debug UI's gate applies, so a focused text field still gets its editing keys.
			auto& raw_key = raw_state.keys[i];
			const bool raw_now = (debug_keyboard_free && platform.ptr->IsKeyDown(static_cast<int>(i)))
								 || std::ranges::contains(overridden, static_cast<int>(i));
			raw_key.pressed = !raw_key.state && raw_now;
			raw_key.state = raw_now;
			raw_key.current = raw_now;
			raw_key.repeat = debug_keyboard_free && platform.ptr->IsKeyPressedRepeat(static_cast<int>(i));
		}

		// The mouse the UI sees: everything but what the debug UI captured. `mouse` below is the same, minus
		// what the UI itself captured (UICapture), and is what gameplay reads.
		auto& raw = raw_state.mouse;
		const bool was_any_down = raw.left.state || raw.right.state || raw.middle.state;
		const bool left_down = mouse_free && platform.ptr->IsMouseButtonDown(0);   // Left = 0
		const bool right_down = mouse_free && platform.ptr->IsMouseButtonDown(1);  // Right = 1
		const bool middle_down = mouse_free && platform.ptr->IsMouseButtonDown(2); // Middle = 2
		const bool any_down = left_down || right_down || middle_down;

		// UI capture of the pointer. A press that lands while the UI holds the pointer latches, so the drag keeps
		// going to the UI after the cursor leaves the element and the release isn't split from its press. A press
		// that began in gameplay stays there, even if it is dragged across a UI element.
		auto& capture = it.world().get_mut<UICapture>();
		if (!any_down) {
			capture.pointer_latched = false;
		}
		else if (!was_any_down) {
			capture.pointer_latched = capture.pointer;
		}
		const bool ui_has_pointer = platform.ptr->IsCursorLocked() ? false : (any_down ? capture.pointer_latched : capture.pointer);
		const bool game_mouse_free = !ui_has_pointer;

		const auto update_button = [](KeyState& state, const bool down) {
			state.pressed = !state.state && down;
			state.state = down;
			state.current = down;
		};
		update_button(raw.left, left_down);
		update_button(raw.right, right_down);
		update_button(raw.middle, middle_down);
		update_button(mouse.left, game_mouse_free && left_down);
		update_button(mouse.right, game_mouse_free && right_down);
		update_button(mouse.middle, game_mouse_free && middle_down);

		// Mouse position: always flows, for hover and picking.
		const auto mouse_pos = platform.ptr->MousePosition();
		raw.window_position = mouse.window_position = {mouse_pos.x, mouse_pos.y};

		// Mouse relative motion and scroll
		const auto delta = mouse_free ? platform.ptr->MouseDelta() : glm::vec2{0.0f};
		const auto scroll = mouse_free ? platform.ptr->MouseWheel() : glm::vec2{0.0f};
		raw.relative = {delta.x, delta.y};
		raw.scroll = {scroll.x, scroll.y};
		mouse.relative = game_mouse_free ? raw.relative : MouseCoord{};
		mouse.scroll = game_mouse_free ? raw.scroll : MouseCoord{};

		// Printable text typed this frame (drains the platform character queue exactly once per frame,
		// so focused text-input UI reads it from the TypedText singleton like any other input).
		// The platform queue is drained once; gameplay's copy is withheld under keyboard capture, the UI's isn't.
		raw_state.text = debug_keyboard_free ? platform.ptr->TextInput() : std::string{};
		it.world().get_mut<TypedText>().text = keyboard_free ? raw_state.text : std::string{};
	}).add<ecs::FrameSample>();

	// === SYSTEM: Reset frame-state input flags at end of frame ===
	// Clears the per-frame "pressed"/scroll flags so they don't persist.
	world.system("InputResetFrameState")
		.kind(flecs::PostUpdate)
		.write<InputState>()
		.write<RawInput>()
		.run([](const flecs::iter& it) {
		const auto ecs_world = it.world();
		auto& input_state = ecs_world.get_mut<InputState>();

		// Reset key pressed flags
		for (auto& key_state : input_state.keys) {
			key_state.pressed = false;
			key_state.repeat = false;
		}

		// Clear text typed this frame
		ecs_world.get_mut<TypedText>().text.clear();

		// Reset mouse button pressed flags
		input_state.mouse.left.pressed = false;
		input_state.mouse.right.pressed = false;
		input_state.mouse.middle.pressed = false;

		// Reset scroll (accumulates during frame, clear for next frame)
		input_state.mouse.scroll.x = 0.0F;
		input_state.mouse.scroll.y = 0.0F;

		// The same for the unfiltered keys, text and mouse the UI reads
		auto& raw_input = ecs_world.get_mut<RawInput>();
		for (auto& key_state : raw_input.keys) {
			key_state.pressed = false;
			key_state.repeat = false;
		}
		raw_input.text.clear();
		auto& raw = raw_input.mouse;
		raw.left.pressed = false;
		raw.right.pressed = false;
		raw.middle.pressed = false;
		raw.scroll = {};
	});

	spdlog::info("[InputModule] Registered input systems with Flecs");
}

} // namespace engine::input
