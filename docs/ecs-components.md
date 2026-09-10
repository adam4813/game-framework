# ECS Components, Singletons & Tags

Flecs 4.x conventions used throughout the engine:

- **Components are plain-data structs** — no logic, no virtual methods. A method is only justified when it does real
  work — interrogating/manipulating a non-trivial member (e.g. a container) or acting as a factory — not a trivial field
  accessor/mutator. Examples that qualify: `WorldTransform::ComputeMatrix` (composes the world matrix),
  `ecs::MakeWorldTransform` (factory), `Cooldown::Ready` (computed predicate).
- **Tags** are zero-size structs used purely for filtering.
- **Singletons** are components stored on the world (`world.set<T>({})`), kept as aggregates so
  `set<T>({})` works.
- **Systems are stateless**; they query components and use `it.delta_time()` for timing.
- **Reflection then scripting**: a module first reflects a component for the Flecs Explorer
  (`world.component<T>().member<...>(...)`) and *then* registers it for scripting (`RegisterComponentForScripts`). Core
  math types are reflected once in `EngineContext`.

## Core transforms (`engine::spatial`, `src/engine/spatial/spatial_components.hpp`)

| Component        | Fields                                          | Notes                                                                                                                                                                                                                                                                              |
|------------------|-------------------------------------------------|------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------|
| `Transform`      | `position`, `rotation` (Euler radians), `scale` | **Authored source** — author only this component; do not hand-write `WorldTransform`. Local-to-parent (plain data).                                                                                                                                                                |
| `WorldTransform` | `position`, `rotation`, `scale`, `matrix`       | **Computed/canonical** — seeded once from `Transform` via `TransformSeedWorldTransform` observer (`.without<WorldTransform>()`), then owned exclusively by `TransformPropagation` (parented/local entities) or the physics backend (dynamic bodies). Render/physics read `matrix`. |

Both are reflected in `EngineContext` and registered for scripting, so any renderable/physical entity shares one
transform vocabulary. **Important**: author only `Transform`; `WorldTransform` is auto-seeded and kept in sync by the
engine. Hand-authoring `WorldTransform` is error-prone and will be overwritten.

## Singletons

| Singleton                           | Module    | Purpose                                                                                                       |
|-------------------------------------|-----------|---------------------------------------------------------------------------------------------------------------|
| `ecs::RngState`                     | ecs       | Deterministic splitmix64 RNG (`Next`, `NextFloat`, `NextRange`, `NextFloatRange`, `Chance`, `WeightedIndex`). |
| `platform::PlatformRef`             | platform  | Non-owning `Platform*` for any system.                                                                        |
| `EngineContextRef`                  | engine    | Non-owning `EngineContext*` for world-only module ctors.                                                      |
| `input::InputState`                 | input     | Full keyboard/mouse state for the current frame.                                                              |
| `render::AmbientLight`              | render    | Global ambient term for the lit shader.                                                                       |
| `assets::AssetRegistry`             | assets    | Ref-counted asset ownership (textures/sounds/meshes).                                                         |
| `save::SaveRegistry`                | save      | Schema of what persists and how.                                                                              |
| `level::LevelRegistry`              | level     | Component/singleton JSON loader callbacks.                                                                    |
| `scripting::ScriptBackendSingleton` | scripting | Owns the active scripting backend.                                                                            |

Access pattern is uniform: `world.get<T>()` for read, `world.get_mut<T>()` for write.

## Tags

| Tag                                    | Applied to                         | Meaning                                                            |
|----------------------------------------|------------------------------------|--------------------------------------------------------------------|
| `ecs::Pausable`                        | a **system**                       | Excluded from the paused pipeline (simulation stops while paused). |
| `scene::GameScene`, `scene::MenuScene` | **systems** (filter) and pipelines | Scene scoping; a scene pipeline uses `.without<OtherScene>()`.     |
| `scene::Active`                        | a **scene entity**                 | Marks the one active scene (`flecs::Exclusive`).                   |
| `scene::Paused`                        | the **world**                      | Switches the active scene to its paused pipeline.                  |
| `scripting::ScriptHost`                | a **host entity**                  | Present when the entity has at least one script child.             |
| `ui::UIClickRequest`                   | a **Button entity**                | Fires that button's click programmatically.                        |
| `timer::TimerExpired`                  | a timer entity                     | Marker for expiry.                                                 |
| `input::InputEnabled`                  | filter                             | Gate systems that require input.                                   |

## Component catalog by module

Deep field-level docs live in each module's README; this is the map of *which components exist where*.

### render (`src/engine/render/render_components.hpp`)

Shapes: `CubePrimitive`, `SpherePrimitive`, `QuadPrimitive`, `CapsulePrimitive`, `MeshPrimitive`
(its `path`/`handle` resolve via the templated `RegisterPathAsset<T>()` observer). Material/appearance:
`Material`, `AlbedoMap`, `ShaderMap` (all three use `RegisterPathAsset<T>()` for path→handle resolution). Lights:
`AmbientLight`
(singleton), `DirectionalLight`. View: `Camera` (+ an `ecs::Transform` as the eye).

**Relationships & linkage**:

- `RenderWith` — relationship from a renderable entity to a **material entity**. The material entity carries `Material`,
  `AlbedoMap`, `ShaderMap`, and is resolved transparently via scripts (see `ScriptTraversal` below).
- `LookAt` — relationship from a camera to a target entity. `Render3DBegin` reads the target's `WorldTransform` to aim
  the camera; if unset, falls back to the camera's forward vector.

**ScriptTraversal**: Material-related components (`Material`, `AlbedoMap`, `ShaderMap`) are tagged with
`(ScriptTraversal, RenderWith)`. This tells the scripting backend to navigate through `RenderWith` when `GetMaterial()`/
`GetAlbedoMap()` are called from a script and the component is not found directly on the host entity — allowing scripts
to work transparently regardless of whether material data lives on the entity or its linked material child.

### physics (`src/engine/physics/physics_components.hpp`)

`RigidBody`, `CollisionShape`, `PhysicsVelocity`, and the request/override components
`PhysicsForce`, `PhysicsImpulse`, `PhysicsVelocityOverride`. Enums (`MotionType`, `ShapeType`) live in
`physics_types.hpp`. Requires a `Transform`/`WorldTransform`.

### ui (`src/engine/ui/ui_components.hpp`)

`UIRect` (every UI entity), `UIElement` (z-index/visible), visuals `Panel`/`Label`/`Button`/
`ProgressBar`/`Spinner`, layout/behaviour `Stack`/`ScrollRect`/`Modal`/`OnClick`, and the deferred render types
(`UIDrawCommand`, `UIDrawList`). See [ui.md](ui.md).

### audio (`src/engine/audio/audio_components.hpp`)

`SoundEffect` (`path`/`handle`/`playing`, `Fire()`), `Music` (`path`/`handle`/`loop`). Handles are resolved via `OnSet`
observers through the asset registry.

### timer (`src/engine/timer/timer_components.hpp`)

`Timer` (one-shot/repeat), `Cooldown` (`Ready()`), `Tween` (`EasingType` easing), tag `TimerExpired`.

### particles (`src/engine/particles/particle_components.hpp`)

`ParticleEmitter` (rate/lifetime/speed/spread/direction/colour) and `Particle`. A particle reuses
`WorldTransform` + `SpherePrimitive` + `Material`, so the render systems draw it with no new code.

### input (`src/engine/input/input_components.hpp`)

`InputState` singleton (`keys[512]`, `MouseState`), `KeyState`, `MouseState`, the `KeyCode`
namespace, tag `InputEnabled`.

### scene (`src/engine/scene/scene_components.hpp`)

`SceneId` (enum), `SceneComponent` (holds the `Scene` instance), tags `GameScene`/`MenuScene`/
`Active`/`Paused`. See [scenes.md](scenes.md).

### scripting (`src/engine/scripting/script_component.hpp`)

`ScriptComponent` (`source_path` **xor** `inline_source`, backend-owned `instance`), tag
`ScriptHost`, enum `ScriptTickPhase`. See [scripting-api.md](scripting-api.md).

### tilemap (`src/engine/tilemap/tilemap_components.hpp`)

`Tilemap` (grid of tile IDs), `TileSet` (texture + atlas dims), `TileRegistry` (tile ID → descriptor map, scoped per
tilemap entity), `TileDescriptor` (id/name/color/tex_coords/walkable/callbacks), `GridPosition` (entity's current tile
x/z), `TileCallbackState` (internal transition tracking), `TilemapViewport` (viewport-sized ring-buffer streaming),
**tag** `TilemapFollowTarget` (marks the entity that the viewport centers on — typically the player, but decoupled from
any specific follower). See [tilemap README](../src/engine/tilemap/README.md).

## Registering a new component

1. Define a plain-data struct in the module's `*_components.hpp`.
2. Reflect it in the module constructor: `world.component<T>().member<...>("field")...`.
3. (Optional) Expose to scripts: `scripting::RegisterComponentForScripts(world, world.component<T>())`.
4. Add/consume it in a system or observer.

Because CMake uses `GLOB_RECURSE`, a new `.cpp` is auto-discovered — no build-file edit needed.
