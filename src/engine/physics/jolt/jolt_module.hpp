#pragma once

#include <flecs.h>

namespace engine::physics::jolt {

/// Jolt physics backend module for Flecs ECS integration.
/// Registers all Jolt-specific systems and manages the Jolt physics backend.
/// Must be imported AFTER PhysicsModule.
class JoltModule {
public:
	/// Initialize Jolt backend and register all Jolt systems.
	/// Per-frame simulation systems are tagged engine::ecs::Pausable so a scene's paused
	/// pipeline can exclude them.
	explicit JoltModule(const flecs::world& world);
};

} // namespace engine::physics::jolt
