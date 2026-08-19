#pragma once

#include <flecs.h>

namespace engine::physics {

/// Physics module for Flecs ECS integration
/// Registers Jolt physics system with proper system lifecycle
class PhysicsModule {
public:
	/// Initialize physics module and register all Flecs systems.
	/// Per-frame simulation systems are tagged engine::ecs::Pausable so a scene's paused
	/// pipeline can exclude them.
	explicit PhysicsModule(const flecs::world& world);
};

} // namespace engine::physics
