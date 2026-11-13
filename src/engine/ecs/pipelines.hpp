#pragma once

#include <flecs.h>

namespace engine::ecs {

// Tag applied to a *system* that advances the simulation on the fixed Tick (physics forces, the physics step and
// its write-back). The fixed step runs every FixedStep system once per Tick, in phase order, with the fixed dt as
// `it.delta_time()`, zero or more times per Frame (see `engine/fixed_step/README.md`). A FixedStep system never runs
// in a Frame pipeline. Give it the same `.kind(phase)` and mode tag (`scene::GameScene` / `scene::MenuScene`) a
// per-Frame system would: the phase orders it within the Tick, the mode tag picks the scenes it runs in. It doesn't
// need `Pausable`: pause stops the whole fixed step.
struct FixedStep {};

// Tag applied to a *system* that samples the outside world (input) once per Frame, before the Frame's Ticks, so every
// Tick and the rest of the Frame see this Frame's sample. It runs in every scene, paused or not, and never in a Frame
// pipeline.
struct FrameSample {};

// Starts a Frame pipeline: every system in a phase, in phase order, minus disabled systems and the FixedStep and
// FrameSample systems the fixed step runs on its own. A scene adds its filters and builds it:
//
//   pipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().build();
//   pausedPipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().without<ecs::Pausable>().build();
[[nodiscard]] flecs::pipeline_builder<> FramePipeline(const flecs::world& world);

// Starts a fixed-step pipeline: the FixedStep systems only, in phase order, minus disabled systems. A scene adds the
// same mode filter its Frame pipeline has, so a gameplay-only simulation system stays out of a menu scene:
//
//   fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::MenuScene>().build();
[[nodiscard]] flecs::pipeline_builder<> FixedStepPipeline(const flecs::world& world);

// Starts the Frame sample pipeline: the FrameSample systems only, in phase order, minus disabled systems.
[[nodiscard]] flecs::pipeline_builder<> FrameSamplePipeline(const flecs::world& world);

} // namespace engine::ecs
