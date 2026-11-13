# Fixed Step Module

Steps the simulation on a fixed Tick, whatever the Frame rate. Every system tagged `ecs::FixedStep` runs once per
Tick, in phase order, with the fixed dt, zero or more times per Frame. Decision record: ADR 0008.

## Structure

- **fixed_step_components.hpp** — The `FixedStepConfig` (tick rate, catch-up cap) and `FixedStepState` (Tick counter,
  accumulator, alpha) singletons.
- **fixed_step_module.hpp / .cpp** — `FixedStepModule`: registers and reflects the singletons, builds the Framework's
  FrameSample and fallback fixed-step pipelines, and makes the world's default pipeline a Frame pipeline.
- **fixed_step_run.hpp / .cpp** — `RunFrame` and `RunTick`, the two ways a world advances.
- **fixed_step_pipelines.hpp** — The module's private `FixedStepPipelines` singleton.
- **fixed_step.hpp** — Index header.

The `FixedStep` and `FrameSample` tags and the pipeline builders (`FramePipeline`, `FixedStepPipeline`,
`FrameSamplePipeline`) live in `engine/ecs/pipelines.hpp`, beside `Pausable`, so scenes and modules can use them
without depending on this module. `EngineContext` imports the module after the scene and input modules.

## A Frame

`RunFrame(world, dt)` is one Frame. `App::Tick` calls it with the Platform's dt; a headless world (a server, a test)
calls it with its own measured dt, so it needs no Tick loop of its own.

1. **Sample**: the `FrameSample` systems (`InputPoll`) run once, so every Tick and the rest of the Frame see this
   Frame's input.
2. **Ticks**: `dt` goes into the accumulator, and while it holds a whole fixed dt, a Tick runs the active scene's
   fixed-step pipeline and counts itself. A Frame runs at most `max_catch_up_ticks` Ticks (default 4); time beyond them
   is dropped and logged at debug level, so a stall doesn't spiral.
3. **Frame**: `world.progress(dt)` runs the active Frame pipeline: the per-Frame update, presentation and render.

```cpp
#include "engine/fixed_step/fixed_step.hpp"

// 60 Ticks per second (the default), at most 4 per Frame.
world.set<engine::fixed_step::FixedStepConfig>({.tick_rate = 60, .max_catch_up_ticks = 4});

// A headless loop: real dt in, the accumulator decides the Ticks.
engine::fixed_step::RunFrame(world, measured_dt);

// Re-simulate one Tick on demand (say after a correction). It ignores the accumulator and pause.
engine::fixed_step::RunTick(world);

// Stamp against the Tick counter. During a Tick it is that Tick's index.
const uint64_t tick = world.get<engine::fixed_step::FixedStepState>().tick;
```

Within one Tick each system runs once, in order, so a catch-up of two Ticks runs A, B, A, B, never A, A, B, B. That is
why the fixed step is a pipeline and not Flecs `interval` or a per-system accumulator (ADR 0008).

## Writing a FixedStep system

Give it its phase and mode tag as usual, then the tag. `it.delta_time()` is the fixed dt.

```cpp
world.system<const Thrust, Velocity>("ShipThrust")
    .kind(flecs::OnUpdate)
    .each([](flecs::iter& it, size_t, const Thrust& t, Velocity& v) { v.linear += t.force * it.delta_time(); })
    .add<engine::ecs::FixedStep>()
    .add<engine::scene::GameScene>();
```

- **Simulation goes on the Tick; presentation stays per Frame.** The Framework's physics (forces, impulses, velocity
  overrides, character teleport, the character and world step, and their write-back to `Transform`) is FixedStep.
  Animation, particles, the camera and rendering are not.
- **No `Pausable`**: pause stops the whole fixed step, so a FixedStep system never needs it.
- **Never in a Frame pipeline.** `world.progress()` alone never runs a FixedStep or FrameSample system, so code that
  drives a world calls `RunFrame`, not `progress`.

## Scenes

A scene builds a fixed-step pipeline next to its Frame pipelines, with the same mode filter, so a gameplay-only
simulation system stays out of a menu (see `SceneBase::InitializePipeline`):

```cpp
pipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().build();
pausedPipeline_ = ecs::FramePipeline(world).without<scene::MenuScene>().without<ecs::Pausable>().build();
fixedStepPipeline_ = ecs::FixedStepPipeline(world).without<scene::MenuScene>().build();
```

A Tick runs the active scene's fixed-step pipeline, or every FixedStep system when no scene is active or the active
one built none.

## Pause

Pause is data (ADR 0004). With `scene::Paused` on the world, `RunFrame` runs no Tick, the accumulator doesn't fill
(paused time isn't saved up for when the world resumes) and the counter stops. The sample and the Frame pipeline (the
scene's paused pipeline) keep running, so the pause menu works.

## Alpha

After its Ticks a Frame sets `FixedStepState::alpha`, the fraction of a Tick left in the accumulator, in [0, 1). It's
there for render interpolation between the last two Ticks; nothing in the Framework uses it yet.

## Frame rate

The Frame rate is a separate lever, on the Platform: `Platform::SetFrameRate({.mode = FrameRateMode::Capped, .cap =
144})`, `{.mode = FrameRateMode::VSync}` or `{.mode = FrameRateMode::Uncapped}`. Init starts capped at 60. It changes
how often the game renders, never how fast it simulates: that is `FixedStepConfig::tick_rate`.
