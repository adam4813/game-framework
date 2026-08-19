#pragma once

#include <flecs.h>

namespace engine::input {

/// Input module for Flecs ECS integration
/// Registers input state singleton and input-related systems.
/// The platform layer populates the InputState singleton each frame.
class InputModule {
public:
	/// Initialize input module and register input components/systems with Flecs
	/// Expects world to have a platform reference for input polling
	explicit InputModule(const flecs::world& world);
};

} // namespace engine::input
