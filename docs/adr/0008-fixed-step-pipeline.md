# Simulation Steps on a Framework-Owned Fixed-Step Pipeline, Not `interval` or Per-System Accumulators

Simulation systems carry the `ecs::FixedStep` tag and run on a fixed Tick: the fixed step (`engine/fixed_step/`) runs
an ordinary Flecs pipeline over them once per Tick, with the fixed dt, as many times per Frame as an accumulator owned
by the Framework says (at most a catch-up cap). Each Frame samples input first, then runs its Ticks, then the per-Frame
pipeline. The Framework owns the accumulator, the Tick counter, the fixed dt and the fixed-step config, and pause
(ADR 0004) stops the whole fixed step.

The point is order. When a Frame catches up two Ticks, every Tick must run each simulation system once, in order, so
the system that applies a Tick's input runs before physics steps on that Tick: A, B, A, B.

Rejected:

- **Flecs `interval`** (a rate or interval on each system). When catching up, Flecs runs each system N times on its own
  before the next system, so A, A, B, B: one input per step can't be guaranteed.
- **Per-system accumulators**, as physics kept (`PhysicsAccumulator`, now gone). They break the same way, and each
  system that needs a fixed clock grows its own copy of the loop and its own spiral guard.

Consequences: Frame pipelines (`ecs::FramePipeline`) leave FixedStep systems out, so `world.progress()` alone never
steps the simulation; code that drives a world calls `fixed_step::RunFrame`. A headless world (a server) calls it with
real dt and needs no Tick loop of its own. The Frame rate (`Platform::SetFrameRate`) is a separate lever from the Tick
rate. The fixed step promises a fixed dt and a fixed system order only, not determinism across machines.
