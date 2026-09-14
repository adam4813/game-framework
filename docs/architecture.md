# Architecture

## The `EngineContext`

`engine::EngineContext` (`src/engine/engine_context.{hpp,cpp}`) owns the three things every subsystem needs and wires
them together in a fixed order:

- a `platform::Platform` (the Raylib backend on desktop/WASM),
- the single `flecs::world`,
- a `DeferredTaskQueue` for work that must run *outside* the ECS progress step.

It publishes non-owning handles as Flecs singletons so world-only code can reach them without a global:

```cpp
world.set<platform::PlatformRef>({platform.get()});   // platform->... from any system
world.set<EngineContextRef>({this});                  // context->... from a world-only module ctor
world.set<ecs::RngState>({});                          // deterministic RNG (splitmix64)
```

### Module import order (this order matters)

The constructor imports modules in dependency order. Scripting is **imported first** — all other modules register their
components for scripting from inside their constructors, so the backend must be live before they import. Core reflected
math types (`vec2`, `vec3`, `Rgba`, `Transform`, `WorldTransform`) are also registered before any module that references
them, at construction time (not inside an observer) so the structural changes are not deferred.

For the complete module order, see `src/engine/engine_context.cpp`.

`ecs::InitializeRemoteAPI(world)` is called last on desktop (the Flecs REST API needs TCP sockets, which browsers lack,
so it is compiled out under `__EMSCRIPTEN__`).

The **game** layer is a separate Flecs module (`game::GameModule`, `src/game/game.cpp`) imported on top of the engine.
It reads `EngineContextRef`, registers the save schema, registers scenes, and activates the title scene.

## The frame loop

`engine::App` (`src/engine/app/app.cpp`) is intentionally tiny — the whole frame is one ECS progress step bracketed by
the platform's frame begin/end:

```cpp
void App::Tick() const {
    const float dt = platform->DeltaTime();
    platform->BeginFrame(platform::colors::Background);
    world->progress(dt);      // runs the active scene's pipeline across all phases
    platform->EndFrame();
}
void App::PostTick() const { context_->GetDeferredTaskQueue().ProcessTasks(); }
```

`world->progress(dt)` executes whichever **pipeline** is currently active (see
[scenes.md](scenes.md) for how the active scene selects its pipeline). `PostTick()` drains the deferred task queue for
actions that cannot safely run mid-progress.

## Pipeline phases

Systems are ordered by the Flecs built-in phases plus one engine-defined phase, `ecs::OnUI`. Within a phase,
registration order and explicit dependencies decide ordering.

| Phase                  | Purpose                        | Representative systems                                                                                                                     |
|------------------------|--------------------------------|--------------------------------------------------------------------------------------------------------------------------------------------|
| `flecs::PreUpdate`     | Sample inputs, lay out UI      | `InputPoll`, `UILayout`, `ScriptPreTick`                                                                                                   |
| `flecs::OnUpdate`      | Simulation + interaction/logic | physics step & sync, `TimerAdvance`, `ParticleEmit/Update`, UI interaction, `SoundEffectPlayback`, `ActiveSceneTick/Input`, `ScriptOnTick` |
| `flecs::OnStore`       | 3D rendering, debug draw       | `Render3DBegin` → lighting → primitives → `Render3DEnd`, `PhysicsDebugRender`                                                              |
| `ecs::OnUI` *(custom)* | 2D overlays on top of 3D       | `UIBuildDrawList`, `UIRenderDrawList`, `ActiveSceneDebugUI`, `ActiveSceneUI`                                                               |
| `flecs::PostUpdate`    | Clear per-frame state          | `InputResetFrameState`, `ScriptPostTick`                                                                                                   |

`ecs::OnUI` is registered in the scene module as a real Flecs phase that **depends on**
`flecs::OnStore`, guaranteeing 2D composites on top of 3D regardless of module import order:

```cpp
world.component<ecs::OnUI>().add(flecs::Phase).add(flecs::DependsOn, flecs::OnStore);
```

The deliberate split of UI across phases — **layout** in `PreUpdate`, **interaction** in `OnUpdate`
(reading input sampled in `PreUpdate`, before `PostUpdate` clears it), **render** in `OnUI` — means UI behaves like
ordinary simulation with no one-frame input lag.

See [systems-reference.md](systems-reference.md) for the full per-module system/observer table.

## Transform and WorldTransform

**Design principle**: author `Transform` (local position/rotation/scale), never `WorldTransform` (world space). The
engine computes and maintains `WorldTransform` exclusively.

- **`SetInitialWorldTransform` observer**: When a new entity gets a `WorldTransform` component,
  this observer seeds its `WorldTransform` once from `Transform`. This is the **only**  place `WorldTransform` is
  hand-authored.
- **`TransformPropagation` system** (`OnUpdate`): For parented and unparented entities carrying `Transform`, this system
  computes the parent's `WorldTransform` matrix (if parented) or uses `Transform` directly (if root), and applies it.
  This is the canonical writer for `WorldTransform` in simulation.
- **Physics backend**: For dynamic rigid bodies, Jolt owns `WorldTransform` after each physics step (via
  `PhysicsSyncFromBackend`).

Hand-writing `set<WorldTransform>()` after initialization is error-prone and will be overwritten by propagation/physics.
Keep `Transform` in sync with gameplay intent; the engine keeps `WorldTransform` consistent automatically.

## Entity linkage and relationships

The level loader supports first-class entity **relationships** in JSON:

- **`link`** — declares an implicit parent→child relationship. E.g., a material entity nested under a renderable
  declares `"link": "render_with"`, and the render module's `LinkLoader` wires `RenderWith(parent → material)`.
- **`refs`** — declares named-entity relationships. E.g., a camera declares `"refs": { "look_at": "Player" }`, and the
  render module's `RefLoader` wires `LookAt(camera → Player)` once all named entities are built.

Both are resolved by the level loader's `EntityBuilder`, which builds the tree first, then resolves links and refs in a
second pass. This decouples the definition order and allows cross-references (e.g., a camera can reference a player
entity defined anywhere in the level).

## The pause model

Pausing is **data**, not a branch in every system. A system that advances the simulation carries the zero-size tag
`engine::ecs::Pausable` (`src/engine/ecs/pausable.hpp`). A scene builds a second
"paused" pipeline that filters those systems out:

```cpp
pausedPipeline_ = world.pipeline().with(flecs::System).without<engine::ecs::Pausable>()./*...*/.build();
```

Toggling pause is a world tag:

```cpp
world.add<engine::scene::Paused>();     // switch active scene to its paused pipeline
world.remove<engine::scene::Paused>();  // restore the running pipeline
```

The scene-management module observes `Paused` (`OnAdd`/`OnRemove`) and calls
`world.set_pipeline(scene->GetPausedPipeline() | GetPipeline())` for the active scene. Systems that must keep running
while paused — input polling, rendering, audio, and menu/UI logic — simply **omit**
the `Pausable` tag.

**Which systems pause** (carry `Pausable`): physics, particles, timers, and script ticks. **Which keep running** (no
`Pausable`): input, render, UI, scene tick/input/UI, audio playback.

## Determinism

`ecs::RngState` is a splitmix64 generator seeded to a constant, exposed as a world singleton so runs are reproducible
from a seed. Systems that need randomness (e.g. particle emission) pull from
`world.get_mut<ecs::RngState>()` rather than `rand()`.

## Related

- Bootstrapping & DI: `src/engine/engine_context.{hpp,cpp}`
- Frame loop: `src/engine/app/app.cpp`, `src/main.cpp`
- Tuning constants: `constants.hpp` (all `constexpr`, per the data-driven convention)
