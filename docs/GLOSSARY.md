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
The `sandbox` Game Target built from `games/sandbox/src/game/`. It contains engine demonstration Scenes (CubeScene, NoiseScene, TilemapScene, TitleScene) used for engine feature development and manual testing. Not a production game.
_Avoid_: game, playground, demo (use Sandbox consistently)

**Scene**:
A distinct screen or game state (title screen, gameplay, pause menu, …). A Scene is a C++ class deriving from `engine::scene::Scene`, attached to a Flecs entity via `SceneComponent`. Each Scene owns its own Pipeline (and optionally a paused variant). Only one Scene is Active at a time.
_Avoid_: screen, state, level (a Level is data loaded inside a scene, not the scene itself)

**Pipeline**:
A scene-owned, filtered sequence of Systems that Flecs executes. Each Scene builds its own running Frame Pipeline (from `ecs::FramePipeline`), a paused Pipeline that excludes Pausable systems if it supports pausing, and a fixed-step Pipeline (from `ecs::FixedStepPipeline`) that each Tick runs. This diverges from a single global pipeline: every scene selects which pipelines are active when it is loaded.
_Avoid_: system pipeline, execution graph

**Frame**:
One pass of the game loop, `fixed_step::RunFrame(world, dt)`: sample input (the `FrameSample` systems), run zero or more Ticks, then the per-Frame update and render (`world.progress(dt)`). How often Frames run is the Frame rate, set on the Platform (a cap, vsync or uncapped), apart from the Tick rate.
_Avoid_: tick (a Tick is the simulation's step; `App::Tick` and `SceneBase::Tick` are per-Frame and predate the term), update

**Tick**:
One fixed step of the simulation: every `FixedStep` System runs once, in phase order, with the fixed dt (`1 / FixedStepConfig::tick_rate`). A Frame runs as many Ticks as its accumulated time holds, up to a catch-up cap; pause runs none. `FixedStepState::tick` counts the Ticks that ran, so things can be stamped against it.
_Avoid_: step, substep, fixed update, frame (a Frame renders; a Tick simulates)

**FixedStep**:
A Tag applied to a simulation System to run it on the Tick, in the fixed-step Pipeline, instead of once per Frame. Frame Pipelines leave it out, so `world.progress()` alone never runs it. It needs no Pausable tag, since pause stops the whole fixed step.
_Avoid_: fixed update, interval system

**Phase**:
An execution slot within a Pipeline that determines *when* a system runs relative to others. The engine uses the Flecs built-in phases (`PreUpdate`, `OnUpdate`, `OnStore`, `PostUpdate`) plus two custom phases: `ecs::OnUI` runs after `OnStore` and guarantees 2D overlays composite on top of 3D, and `ecs::OnDebugUI` runs after `OnUI` to render the debug UI on top of everything. Systems are assigned a phase via `.kind(phase)`.
_Avoid_: stage, step, tick phase

**Transform**:
The *authorable* local-space position (vec3), rotation (Euler radians, vec3), and scale (vec3) of an entity relative to its parent (or the world origin if unparented). **Only `Transform` should be written by game or engine code.** The engine derives `WorldTransform` from it automatically.
_Avoid_: local transform, position component

**WorldTransform**:
The *computed* world-space position, rotation, scale, and transformation matrix. Seeded from `Transform` whenever it is set, by the `TransformSeedWorldTransform` observer, and then owned exclusively by `TransformPropagation` (for parented/unparented entities) or the physics backend (for dynamic rigid bodies). Writing `WorldTransform` directly after initialisation is overwritten the next frame.
_Avoid_: world position, global transform

**Singleton**:
A Component stored on the world entity itself (`world.set<T>({})`) rather than on an individual entity. Accessed via `world.get<T>()` (read) or `world.get_mut<T>()` (write). Used for engine-wide state: `InputState`, `AssetRegistry`, `RngState`, etc.
_Avoid_: global component, world component

**World Logger**:
A world's own named spdlog logger (`ecs::WorldLogger` Singleton, read through `ecs::WorldLog(world)`). It writes to the default logger's sinks, so two worlds in one process (a client and an integrated server) share one log, each line labeled with the world that wrote it. A world without one logs through the default logger.
_Avoid_: per-world sink (a World Logger shares the default sinks)

**Tag**:
A zero-size struct component used purely for filtering — carries no data. Examples: `ecs::Pausable` (marks a system as pausable), `scene::Active` (marks the one active scene), `scene::Paused` (switches the world into its paused pipeline). In this engine Tags are *always* zero-size; non-zero tags are Components.
_Avoid_: marker, flag component

**Pausable**:
A Tag applied to a per-Frame simulation System to exclude it from the paused Pipeline. Systems that should keep running during pause (input, rendering, UI, audio) simply omit this tag. Toggling pause adds/removes `scene::Paused` from the world; the scene-management Module observes this and swaps the active Pipeline, and the fixed step runs no Tick.
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

**Animation Kind**:
A family of animation that supplies its own clips and decides how a clip at a given time changes what's drawn: skeletal mesh, 2D sprite, spline, and so on. Every kind shares the same Animation Player; only its clips and how they take effect differ.
_Avoid_: animation type, animator

**Animation Player**:
An entity's playback state for one clip of its Animation Kind: which clip, the time into it, the speed, and whether it loops or is playing. It knows nothing of meshes or sprites; it only moves time through a clip of known length.
_Avoid_: animator, animation controller, animation state

**Character Body**:
An entity's physical presence as a walking character: a capsule that moves at the velocity the game asks for, falls under its Gravity, stays on the ground, climbs slopes and steps, and slides along walls. It goes where the game wants it to, but it doesn't know about input. It's distinct from a rigid body, which the simulation pushes around.
_Avoid_: character controller, player controller, kinematic body

**Gravity**:
The single acceleration an entity is subject to, which also defines its up. An entity without its own Gravity uses the world's. There's no blending between sources: something else decides which source applies and sets it.
_Avoid_: gravity source, gravity field, gravity well
