# Game Engine Framework

A small, data-driven C++23 game engine built on Flecs 4.x (ECS), Raylib (platform/rendering), Jolt (physics), and AngelScript (scripting). Everything is a Module; game behaviour is expressed as components + systems + data, not hardcoded branches.

## Language

**EngineContext**:
The single root object that owns the `flecs::world`, the `Platform`, and the `DeferredTaskQueue`. It imports all Modules in dependency order and exposes non-owning handles as Singletons so any system can reach them without a global.
_Avoid_: engine root, engine manager, game engine

**Module**:
A C++ class registered into the Flecs world via `world.import<T>()`. A Module's constructor registers its Components, Systems, and Observers into the world. Modules declare their own dependencies; `EngineContext` imports them in the required order.
_Avoid_: plugin, subsystem, manager

**Engine Module**:
A Module that lives under `src/engine/` and carries no game-specific semantics — it could be useful to any game (render, physics, input, audio, tilemap, UI, …). Engine Modules are compiled into `engine_lib`, the shared static library every game target links against.
_Avoid_: core module, base module

**Game Target**:
An independent cmake executable under `games/<name>/` that links `engine_lib` and provides all game-specific Modules, Scenes, and tests in one self-contained subtree. Each Game Target has its own entry point, its own Flecs top-level Module, and its own test suite.
_Avoid_: game project, game executable

**Sandbox**:
The `sandbox` Game Target built from `src/game/`. It contains engine demonstration Scenes (CubeScene, NoiseScene, TilemapScene, TitleScene) used for engine feature development and manual testing. Not a production game.
_Avoid_: game, playground, demo (use Sandbox consistently)

**Scene**:
A distinct screen or game state (title screen, gameplay, pause menu, …). A Scene is a C++ class deriving from `engine::scene::Scene`, attached to a Flecs entity via `SceneComponent`. Each Scene owns its own Pipeline (and optionally a paused variant). Only one Scene is Active at a time.
_Avoid_: screen, state, level (a Level is data loaded inside a scene, not the scene itself)

**Pipeline**:
A scene-owned, filtered sequence of Systems that Flecs executes each frame. Each Scene builds its own running Pipeline and, if it supports pausing, a second paused Pipeline that excludes Pausable systems. This diverges from a single global pipeline: every scene selects which pipeline is active when it is loaded.
_Avoid_: system pipeline, execution graph

**Phase**:
An execution slot within a Pipeline that determines *when* a system runs relative to others. The engine uses the Flecs built-in phases (`PreUpdate`, `OnUpdate`, `OnStore`, `PostUpdate`) plus one custom phase (`ecs::OnUI`) that runs after `OnStore` and guarantees 2D overlays composite on top of 3D. Systems are assigned a phase via `.kind(phase)`.
_Avoid_: stage, step, tick phase

**Transform**:
The *authorable* local-space position (vec3), rotation (Euler radians, vec3), and scale (vec3) of an entity relative to its parent (or the world origin if unparented). **Only `Transform` should be written by game or engine code.** The engine derives `WorldTransform` from it automatically.
_Avoid_: local transform, position component

**WorldTransform**:
The *computed* world-space position, rotation, scale, and transformation matrix. Seeded once from `Transform` by the `SetInitialWorldTransform` observer and then owned exclusively by `TransformPropagation` (for parented/unparented entities) or the physics backend (for dynamic rigid bodies). Writing `WorldTransform` directly after initialisation is overwritten the next frame.
_Avoid_: world position, global transform

**Singleton**:
A Component stored on the world entity itself (`world.set<T>({})`) rather than on an individual entity. Accessed via `world.get<T>()` (read) or `world.get_mut<T>()` (write). Used for engine-wide state: `InputState`, `AssetRegistry`, `RngState`, etc.
_Avoid_: global component, world component

**Tag**:
A zero-size struct component used purely for filtering — carries no data. Examples: `ecs::Pausable` (marks a system as pausable), `scene::Active` (marks the one active scene), `scene::Paused` (switches the world into its paused pipeline). In this engine Tags are *always* zero-size; non-zero tags are Components.
_Avoid_: marker, flag component

**Pausable**:
A Tag applied to a simulation System to exclude it from the paused Pipeline. Systems that should keep running during pause (input, rendering, UI, audio) simply omit this tag. Toggling pause adds/removes `scene::Paused` from the world; the scene-management Module observes this and swaps the active Pipeline.
_Avoid_: pausable flag, pause-aware

**Platform**:
The abstract interface (`platform::Platform`) wrapping Raylib and all windowing/rendering/input primitives. All engine and game code calls `Platform`; no code calls Raylib directly. The desktop and WASM builds share one concrete backend; future backends swap in without touching the rest of the engine.
_Avoid_: backend, Raylib wrapper, renderer

**DeferredTaskQueue**:
A queue of callbacks that must execute *after* `world.progress()` completes — used for work that would mutate Flecs world structure mid-frame (e.g. scene switches). Drained by `App::PostTick()` each frame.
_Avoid_: deferred queue, post-frame queue, task queue

**Level**:
A JSON file (`*.level.json`) that declaratively defines an entity tree: entity names, Components, children, Links, and Refs. Loaded at runtime by the level Module. A Level is data consumed *by* a Scene, not a Scene itself.
_Avoid_: map, world file, scene file (a scene is code; a level is data)

**Link**:
In a level JSON, a `"link"` field on a child entity that declares a parent→child ECS relationship by name (e.g. `"link": "render_with"` wires `RenderWith(parent, child)`). Resolved in a second pass by the level loader's `EntityBuilder`.
_Avoid_: parent reference, relationship link

**Ref**:
In a level JSON, an entry in the `"refs"` object that maps a relationship name to a *named entity* resolved after the full entity tree is built (e.g. `"refs": { "look_at": "Player" }` wires `LookAt(camera, PlayerEntity)`). Enables cross-entity relationships without authoring-order constraints.
_Avoid_: entity reference, named reference

**RenderWith**:
An ECS relationship from a renderable entity to a *material entity*. The material entity carries `Material`, `AlbedoMap`, and `ShaderMap`. Render systems traverse this relationship to find the material; script accessors (`GetMaterial()`, `GetAlbedoMap()`) do the same transparently.
_Avoid_: material reference, material link
