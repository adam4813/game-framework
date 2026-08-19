#include "input_module.hpp"

#include <spdlog/spdlog.h>

#include <flecs.h>

#include "engine/platform/platform.hpp"
#include "engine/scripting/scripting.hpp"
#include "input_components.hpp"

namespace engine::input {

namespace {
void RegisterScriptConstants(const flecs::world& world) {
	// === Script KeyCode constants ===
	// Expose engine::input::KeyCode as Key_<Name> globals so scripts never need raw integers or
	// backend-specific codes (see scripting/README.md for the naming convention).

	// clang-format off
#define K(Name) scripting::ScriptConstant{"Key_" #Name, scripting::ScriptValueType::MakeInt(), std::to_string(KeyCode::Name)}

	scripting::RegisterGlobalConstantsForScripts(world, {
		// Special
		K(Return), K(Escape), K(Backspace), K(Tab), K(Space), K(Delete),
		// Letters
		K(A), K(B), K(C), K(D), K(E), K(F), K(G), K(H), K(I), K(J), K(K), K(L), K(M),
		K(N), K(O), K(P), K(Q), K(R), K(S), K(T), K(U), K(V), K(W), K(X), K(Y), K(Z),
		// Numbers
		K(Key0), K(Key1), K(Key2), K(Key3), K(Key4),
		K(Key5), K(Key6), K(Key7), K(Key8), K(Key9),
		// Symbols
		K(Minus), K(Equals), K(LeftBracket), K(RightBracket), K(Backslash),
		K(Semicolon), K(Apostrophe), K(Comma), K(Period), K(Slash), K(Grave),
		// Arrows
		K(Right), K(Left), K(Down), K(Up),
		// Navigation
		K(Home), K(End), K(PageUp), K(PageDown), K(Insert),
		// Function keys
		K(F1), K(F2), K(F3), K(F4), K(F5), K(F6),
		K(F7), K(F8), K(F9), K(F10), K(F11), K(F12),
		// Numpad
		K(NumPad0), K(NumPad1), K(NumPad2), K(NumPad3), K(NumPad4),
		K(NumPad5), K(NumPad6), K(NumPad7), K(NumPad8), K(NumPad9),
		K(NumPadMultiply), K(NumPadAdd), K(NumPadSubtract),
		K(NumPadDecimal), K(NumPadDivide), K(NumPadEnter),
		// Modifiers
		K(LeftCtrl), K(LeftShift), K(LeftAlt), K(LeftCmd),
		K(RightCtrl), K(RightShift), K(RightAlt), K(RightCmd),
		// Locks / misc
		K(CapsLock), K(NumLock), K(ScrollLock), K(PrintScreen), K(Pause),
	});

#undef K
	// clang-format on
}
} // namespace

InputModule::InputModule(const flecs::world& world) {
	// Register input components
	world.component<InputEnabled>().add(EcsSingleton);

	// Initialize InputState singleton
	// Platform will update this each frame
	world.set<InputState>({});

	world.component<MouseCoord>().member<float>("x").member<float>("y");
	world.component<KeyState>().member<bool>("pressed").member<bool>("state").member<bool>("current");
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
	scripting::RegisterSingletonForScripts(world, world.component<InputState>());
	scripting::RegisterComponentMethodForScripts<&InputState::IsKeyDown>(world, "IsKeyDown");
	scripting::RegisterComponentMethodForScripts<&InputState::WasKeyPressed>(world, "WasKeyPressed");
	scripting::RegisterComponentMethodForScripts<&InputState::IsKeyUp>(world, "IsKeyUp");

	RegisterScriptConstants(world);

	// === SYSTEM: Poll input from platform each frame ===
	// Updates the InputState singleton with current keyboard and mouse input
	world.system("InputPoll").kind(flecs::PreUpdate).write<InputState>().run([](const flecs::iter& it) {
		auto& [keys, mouse] = it.world().get_mut<InputState>();
		const auto& platform = it.world().get<platform::PlatformRef>();

		// Poll keyboard input
		for (size_t i = 0; i < keys.size(); ++i) {
			auto& key_state = keys[i];
			const bool was_pressed = key_state.state;
			const bool now_pressed = platform.ptr->IsKeyDown(static_cast<int>(i));

			// Track press events (transition from unpressed to pressed)
			key_state.pressed = !was_pressed && now_pressed;
			key_state.state = now_pressed;
			key_state.current = now_pressed;
		}

		// Mouse buttons
		{
			const bool was_left_pressed = mouse.left.state;
			const bool now_left_pressed = platform.ptr->IsMouseButtonDown(0); // Left = 0

			mouse.left.pressed = !was_left_pressed && now_left_pressed;
			mouse.left.state = now_left_pressed;
			mouse.left.current = now_left_pressed;
		}

		{
			const bool was_right_pressed = mouse.right.state;
			const bool now_right_pressed = platform.ptr->IsMouseButtonDown(1); // Right = 1

			mouse.right.pressed = !was_right_pressed && now_right_pressed;
			mouse.right.state = now_right_pressed;
			mouse.right.current = now_right_pressed;
		}

		{
			const bool was_middle_pressed = mouse.middle.state;
			const bool now_middle_pressed = platform.ptr->IsMouseButtonDown(2); // Middle = 2

			mouse.middle.pressed = !was_middle_pressed && now_middle_pressed;
			mouse.middle.state = now_middle_pressed;
			mouse.middle.current = now_middle_pressed;
		}

		// Mouse position
		const auto mouse_pos = platform.ptr->MousePosition();
		mouse.window_position.x = mouse_pos.x;
		mouse.window_position.y = mouse_pos.y;

		// Mouse relative motion
		const auto delta = platform.ptr->MouseDelta();
		mouse.relative.x = delta.x;
		mouse.relative.y = delta.y;

		// Mouse scroll
		const auto scroll = platform.ptr->MouseWheel();
		mouse.scroll.x = scroll.x;
		mouse.scroll.y = scroll.y;
	});

	// === SYSTEM: Reset frame-state input flags at end of frame ===
	// Clears the per-frame "pressed"/scroll flags so they don't persist.
	world.system("InputResetFrameState").kind(flecs::PostUpdate).write<InputState>().run([](const flecs::iter& it) {
		const auto ecs_world = it.world();
		auto& input_state = ecs_world.get_mut<InputState>();

		// Reset key pressed flags
		for (auto& key_state : input_state.keys) {
			key_state.pressed = false;
		}

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
