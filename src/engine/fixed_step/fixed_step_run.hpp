#pragma once

#include <flecs.h>

namespace engine::fixed_step {

// Runs one Frame of `world`, `dt` seconds after the last: the FrameSample systems (input), then zero to
// FixedStepConfig::max_catch_up_ticks Ticks as the accumulator decides, then the Frame pipeline (`world.progress(dt)`:
// the per-Frame update and render). A windowed game calls it through App::Tick; a headless world (a server, a test)
// calls it with real dt, so it needs no Tick loop of its own.
//
// While the world is paused (scene::Paused) no Tick runs, the accumulator doesn't fill and the Tick counter stays put;
// the sample and the Frame pipeline still run. Call it outside progress(), never from a system.
void RunFrame(flecs::world& world, float dt);

// Runs exactly one Tick now, whatever the accumulator holds and paused or not, and counts it: for re-simulating a
// Tick on demand. It leaves the accumulator and alpha alone. Call it outside progress(), never from a system.
void RunTick(flecs::world& world);

} // namespace engine::fixed_step
