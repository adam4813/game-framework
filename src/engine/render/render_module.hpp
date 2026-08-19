#pragma once

#include <flecs.h>

namespace engine::render {

/// Rendering module for Flecs ECS integration.
/// Registers primitive render components (cube, sphere, quad, capsule, mesh) and the systems
/// that draw entities carrying those components plus an ecs::WorldTransform through the
/// platform layer's 3D API.
class RenderModule {
public:
	explicit RenderModule(const flecs::world& world);
};

} // namespace engine::render
