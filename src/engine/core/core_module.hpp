#pragma once

#include <flecs.h>

namespace engine::core {
/// Core module for Flecs ECS integration.
/// Registers core types (glm::vec2/3/4, Rgba, Rect) and singletons.
/// Imported first so all other modules can use these types.
class CoreModule {
public:
	explicit CoreModule(const flecs::world& world);
};

} // namespace engine::core
