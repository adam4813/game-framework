#pragma once

#include <flecs.h>

namespace engine::physics {

/// Physics core module for Flecs ECS integration.
/// Defines physics components, types, and level loaders.
/// Does NOT register any backend-specific systems.
/// 
/// Backend implementations (e.g., JoltModule) must be imported separately
/// to register their own systems and initialize the physics backend.
class PhysicsModule {
public:
	/// Initialize physics core components, reflection, and level loaders.
	/// Backend implementations register their own systems separately.
	explicit PhysicsModule(const flecs::world& world);
};

} // namespace engine::physics
