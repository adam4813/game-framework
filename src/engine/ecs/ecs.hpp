#pragma once

#include "ecs_types.hpp"
#include "pausable.hpp"
#include "singletons.hpp"

namespace engine::ecs {

// Register the transform propagation system.
// Cascades Transform changes to WorldTransform for entities with both components.
void RegisterTransformPropagation(const flecs::world& world);

} // namespace engine::ecs
