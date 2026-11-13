#pragma once

#include <flecs.h>

namespace engine::fixed_step {

// Flecs module: owns the fixed step. Registers the FixedStepConfig and FixedStepState singletons, builds the
// Framework's FrameSample and fallback fixed-step pipelines, and makes the world's default pipeline a Frame pipeline
// so FixedStep and FrameSample systems never run in it. Import after the scene and input modules.
class FixedStepModule {
public:
	explicit FixedStepModule(const flecs::world& world);
};

} // namespace engine::fixed_step
