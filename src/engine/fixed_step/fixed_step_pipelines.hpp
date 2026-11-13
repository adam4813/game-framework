#pragma once

#include <flecs.h>

#include "engine/scene/scene.hpp"

namespace engine::fixed_step {

// Singleton, private to the fixed step: the pipelines and query RunFrame and RunTick need each Frame, built once by
// FixedStepModule so a Frame neither builds nor looks anything up by name.
struct FixedStepPipelines {
	// The FrameSample systems, run at the start of each Frame.
	flecs::entity sample;
	// Every FixedStep system, run on a Tick when no active scene has its own fixed-step pipeline.
	flecs::entity fallback;
	// The active scene, whose fixed-step pipeline a Tick runs.
	flecs::query<const scene::SceneComponent> active_scene;
};

} // namespace engine::fixed_step
