#include "input_module.hpp"

#include <algorithm>
#include <format>
#include <ranges>
#include <string_view>
#include <vector>

#include <spdlog/spdlog.h>

#include <flecs.h>

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
	// Updates the InputState singleton with current keyboard and mouse input
	world.system("InputPoll").kind(flecs::PreUpdate).write<InputState>().run([](const flecs::iter& it) {
		auto& input_state = it.world().get_mut<InputState>();
		auto& keys = input_state.keys;
		auto& mouse = input_state.mouse;
		const auto& platform = it.world().get<platform::PlatformRef>();
		// Input the debug UI captured this frame (pointer over a panel, text field active) is withheld
		// so the game doesn't also act on it. Mouse position still flows for hover/picking.
		const auto& debug_ui = platform.ptr->GetDebugUi();
		const bool keyboard_free = !debug_ui.CapturesKeyboard();
		const bool mouse_free = !debug_ui.CapturesMouse();

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
		}

		// Mouse buttons
		{
			const bool was_left_pressed = mouse.left.state;
			const bool now_left_pressed = mouse_free && platform.ptr->IsMouseButtonDown(0); // Left = 0

			mouse.left.pressed = !was_left_pressed && now_left_pressed;
			mouse.left.state = now_left_pressed;
			mouse.left.current = now_left_pressed;
		}

		{
			const bool was_right_pressed = mouse.right.state;
			const bool now_right_pressed = mouse_free && platform.ptr->IsMouseButtonDown(1); // Right = 1

			mouse.right.pressed = !was_right_pressed && now_right_pressed;
			mouse.right.state = now_right_pressed;
			mouse.right.current = now_right_pressed;
		}

		{
			const bool was_middle_pressed = mouse.middle.state;
			const bool now_middle_pressed = mouse_free && platform.ptr->IsMouseButtonDown(2); // Middle = 2

			mouse.middle.pressed = !was_middle_pressed && now_middle_pressed;
			mouse.middle.state = now_middle_pressed;
			mouse.middle.current = now_middle_pressed;
		}

		// Mouse position
		const auto mouse_pos = platform.ptr->MousePosition();
		mouse.window_position.x = mouse_pos.x;
		mouse.window_position.y = mouse_pos.y;

		// Mouse relative motion
		const auto delta = mouse_free ? platform.ptr->MouseDelta() : glm::vec2{0.0f};
		mouse.relative.x = delta.x;
		mouse.relative.y = delta.y;

		// Mouse scroll
		const auto scroll = mouse_free ? platform.ptr->MouseWheel() : glm::vec2{0.0f};
		mouse.scroll.x = scroll.x;
		mouse.scroll.y = scroll.y;

		// Printable text typed this frame (drains the platform character queue exactly once per frame,
		// so focused text-input UI reads it from the TypedText singleton like any other input).
		it.world().get_mut<TypedText>().text = keyboard_free ? platform.ptr->TextInput() : std::string{};
	});

	// === SYSTEM: Reset frame-state input flags at end of frame ===
	// Clears the per-frame "pressed"/scroll flags so they don't persist.
	world.system("InputResetFrameState").kind(flecs::PostUpdate).write<InputState>().run([](const flecs::iter& it) {
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
	});

	spdlog::info("[InputModule] Registered input systems with Flecs");
}

} // namespace engine::input
