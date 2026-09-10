#pragma once

#include <flecs.h>

namespace engine::spatial {

/// Spatial module for transform and world-space computation.
/// Registers Transform and WorldTransform components, and the transform propagation system.
class SpatialModule {

public:
	explicit SpatialModule(const flecs::world& world);
};

} // namespace engine::spatial
