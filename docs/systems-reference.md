# Systems & Observers Reference

Every registered Flecs **system** and **observer**, grouped by module, with its phase, scene scope, and pausability.
This is the authoritative "what runs when" map; see each module README for behavioural detail.

Legend — **Scope**: `all` = untagged (runs in every scene pipeline); `GameScene` = tagged
`.add<scene::GameScene>()`, so it runs in any scene pipeline that does not exclude it — the Game scene **and** the
Tilemap scene (which only excludes `MenuScene`), not only the game scene. **Observers are not pipeline-filtered**: they
fire on component/structural changes regardless of the active scene, so their scope is always `—`. **Pausable**: ✔
carries `ecs::Pausable` (stops while paused), ✘ keeps running.

## input (`src/engine/input/input_module.cpp`)

| Name                   | Kind            | Phase        | Scope | Pausable |
|------------------------|-----------------|--------------|-------|----------|
| `InputPoll`            | system `.run()` | `PreUpdate`  | all   | ✘       |
| `InputResetFrameState` | system `.run()` | `PostUpdate` | all   | ✘       |

Input polls before everything (so systems see fresh state) and clears per-frame flags after everything (so a key
"pressed" edge survives exactly one frame).

## scripting (`src/engine/scripting/scripting_module.cpp`)

| Name                       | Kind     | Phase / Event | Scope | Pausable |
|----------------------------|----------|---------------|-------|----------|
| `ScriptComponent.OnSet`    | observer | `OnSet`       | —     | —        |
| `ScriptComponent.OnRemove` | observer | `OnRemove`    | —     | —        |
| `ScriptPreTick`            | system   | `PreUpdate`   | all   | ✔       |
| `ScriptOnTick`             | system   | `OnUpdate`    | all   | ✔       |
| `ScriptPostTick`           | system   | `PostUpdate`  | all   | ✔       |

The `OnSet` observer compiles/instantiates the backend script; `OnRemove` tears it down. Tick systems dispatch directly
through the cached `IScriptInstance*` (no backend lookup in the hot path).

## scene (`src/engine/scene/scene_management_module.cpp`)

| Name                    | Kind     | Phase / Event               | Scope | Pausable |
|-------------------------|----------|-----------------------------|-------|----------|
| `OnSceneComponentAdded` | observer | `OnSet`                     | —     | —        |
| `OnSceneActivate`       | observer | `OnAdd` `Active`            | —     | —        |
| `OnSceneDeactivate`     | observer | `OnRemove` `Active`         | —     | —        |
| `OnPauseStateChanged`   | observer | `OnAdd`/`OnRemove` `Paused` | —     | —        |
| `ActiveSceneTick`       | system   | `OnUpdate`                  | all   | ✘       |
| `ActiveSceneInput`      | system   | `OnUpdate`                  | all   | ✘       |
| `ActiveSceneDebugUI`    | system   | `OnUI`                      | all   | ✘       |
| `ActiveSceneUI`         | system   | `OnUI`                      | all   | ✘       |

## physics (`src/engine/physics/physics_module.cpp`)

| Name                            | Kind             | Phase / Event | Scope     | Pausable |
|---------------------------------|------------------|---------------|-----------|----------|
| `PhysicsApplyForces`            | system `.each()` | `OnUpdate`    | GameScene | ✔       |
| `PhysicsApplyImpulses`          | system `.each()` | `OnUpdate`    | GameScene | ✔       |
| `PhysicsApplyVelocityOverrides` | system `.each()` | `OnUpdate`    | GameScene | ✔       |
| `PhysicsStep`                   | system `.run()`  | `OnUpdate`    | GameScene | ✔       |
| `PhysicsSyncToBackend`          | observer         | `OnSet`       | —         | —        |
| `PhysicsRemoveBody`             | observer         | `OnRemove`    | —         | —        |
| `PhysicsSyncFromBackend`        | system `.each()` | `OnUpdate`    | GameScene | ✔       |
| `PhysicsCollisionEvents`        | system `.run()`  | `OnUpdate`    | GameScene | ✔       |
| `PhysicsDebugRender`            | system           | `OnStore`     | GameScene | ✘       |
| `PhysicsDebugRenderSync`        | system           | `OnUpdate`    | GameScene | —        |

Fixed-timestep step with accumulation; per-entity request components (`Force`/`Impulse`/
`VelocityOverride`) applied then consumed; Jolt kept in sync via `OnSet`/`OnRemove` observers.

## render (`src/engine/render/render_module.cpp`)

| Name                                        | Kind             | Phase / Event | Scope     | Pausable |
|---------------------------------------------|------------------|---------------|-----------|----------|
| `ResolveAlbedoMap` (templated `Resolve<T>`) | observer         | `OnSet`       | —         | —        |
| `ResolveMeshPrimitive`                      | observer         | `OnSet`       | —         | —        |
| `CameraTransformUpdate`                     | observer         | `OnSet`       | —         | —        |
| `Render3DBegin`                             | system `.run()`  | `OnStore`     | GameScene | ✘       |
| `RenderLightingUpload`                      | system           | `OnStore`     | GameScene | ✘       |
| `RenderCubes`                               | system `.each()` | `OnStore`     | GameScene | ✘       |
| `RenderSpheres`                             | system `.each()` | `OnStore`     | GameScene | ✘       |
| `RenderQuads`                               | system `.each()` | `OnStore`     | GameScene | ✘       |
| `RenderCapsules`                            | system `.each()` | `OnStore`     | GameScene | ✘       |
| `RenderMeshes`                              | system `.each()` | `OnStore`     | GameScene | ✘       |
| `Render3DEnd`                               | system `.run()`  | `OnStore`     | GameScene | ✘       |

Rendering runs in registration order within `OnStore`, and is **not** pausable (the world keeps drawing while paused).
Texture resolvers share a `RegisterTextureResolver<T>` template;
`MeshPrimitive` handles resolve through the parallel `ResolveMeshPrimitive` observer (so the draw systems never load
lazily).

## ui (`src/engine/ui/ui_module.cpp`)

| Name                   | Kind            | Phase       | Scope | Pausable |
|------------------------|-----------------|-------------|-------|----------|
| `UILayout`             | system `.run()` | `PreUpdate` | all   | ✘       |
| `UIScrollRectUpdate`   | system          | `OnUpdate`  | all   | ✘       |
| `UISpinnerAnimate`     | system          | `OnUpdate`  | all   | ✘       |
| `UIModalInteraction`   | system          | `OnUpdate`  | all   | ✘       |
| `UIButtonInteraction`  | system          | `OnUpdate`  | all   | ✘       |
| `UIButtonClickRequest` | system          | `OnUpdate`  | all   | ✘       |
| `UIBuildDrawList`      | system `.run()` | `OnUI`      | all   | ✘       |
| `UIRenderDrawList`     | system `.run()` | `OnUI`      | all   | ✘       |

UI is untagged and non-pausable so menus keep working while the game is paused. Layout → interaction → render spread
across `PreUpdate`/`OnUpdate`/`OnUI`. See [ui.md](ui.md).

## audio (`src/engine/audio/audio_module.cpp`)

| Name                  | Kind             | Phase / Event | Scope | Pausable |
|-----------------------|------------------|---------------|-------|----------|
| `ResolveSoundEffect`  | observer         | `OnSet`       | —     | —        |
| `ResolveMusic`        | observer         | `OnSet`       | —     | —        |
| `SoundEffectPlayback` | system `.each()` | `OnUpdate`    | all   | ✘       |

`SoundEffect::Fire()` sets `playing`; `SoundEffectPlayback` plays once and resets it (one-shot trigger). Handles resolve
through the asset registry on `OnSet`.

## timer (`src/engine/timer/timer_module.cpp`)

| Name              | Kind             | Phase      | Scope | Pausable |
|-------------------|------------------|------------|-------|----------|
| `TimerAdvance`    | system `.each()` | `OnUpdate` | all   | ✔       |
| `CooldownAdvance` | system `.each()` | `OnUpdate` | all   | ✔       |
| `TweenAdvance`    | system `.each()` | `OnUpdate` | all   | ✔       |

## particles (`src/engine/particles/particles_module.cpp`)

| Name             | Kind             | Phase      | Scope     | Pausable |
|------------------|------------------|------------|-----------|----------|
| `ParticleEmit`   | system `.each()` | `OnUpdate` | GameScene | ✔       |
| `ParticleUpdate` | system `.each()` | `OnUpdate` | GameScene | ✔       |

## tilemap (`src/engine/tilemap/tilemap_module.cpp`)

| Name                 | Kind             | Phase / Event | Scope     | Pausable |
|----------------------|------------------|---------------|-----------|----------|
| `TilemapMeshBuilder` | observer         | `OnSet`       | —         | —        |
| `TileCallbackSystem` | system `.each()` | `PostUpdate`  | GameScene | ✔       |

`TilemapMeshBuilder` observes when `Tilemap` is set and bakes the grid into a `DynamicMesh` using the tilemap entity's
scoped `TileRegistry`. `TileCallbackSystem` detects tile transitions for entities with `GridPosition` and fires
`onenter`/`onleave`/`within` callbacks from descriptors.

## Service modules (no systems)

`assets`, `level`, and `save` register **no systems** — they own a singleton registry and expose free functions.
See [assets](../src/engine/assets/README.md),
[level](../src/engine/level/README.md), [save](../src/engine/save/README.md).

## Registration conventions

- **Per-entity** work uses `.each()`; **global**/singleton work uses `.run()`.
- **Event reactions** use observers (`OnSet` for value changes/resolution, `OnAdd`/`OnRemove` for structural/lifecycle
  changes) rather than per-frame polling.
- Systems are registered in dedicated `Register*System()`/module-constructor code; names are
  `PascalCase` and unique.
- Scene-specific systems: single `.kind(phase)` then `.add<scene::GameScene>()`. Engine-wide systems stay untagged.
- Simulation systems add `.add<ecs::Pausable>()`; input/render/UI/audio omit it.
