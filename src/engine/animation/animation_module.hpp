#pragma once

#include <flecs.h>

namespace engine::animation {

// Flecs module: kind-agnostic animation playback (AnimationPlayer, AdvanceAnimation) plus the skeletal
// mesh kind's clip set asset and binding. Drawing the skinned pose lives in the render module.
class AnimationModule {
public:
	explicit AnimationModule(const flecs::world& world);
};

} // namespace engine::animation
