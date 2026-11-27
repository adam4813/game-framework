# Game Engine Framework

A small, data-driven C++23 game engine built on Flecs 4.x (ECS), Raylib (platform/rendering), Jolt (physics), and AngelScript (scripting). Everything is a Module; game behaviour is expressed as components + systems + data, not hardcoded branches.

## Language

**EngineContext**:
The single root object that owns the `flecs::world`, the Window, the Renderer, and the `DeferredTaskQueue`. It imports all Modules in dependency order and exposes non-owning handles as Singletons so any system can reach them without a global.
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
One pass of the game loop, `fixed_step::RunFrame(world, dt)`: sample input (the `FrameSample` systems), run zero or more Ticks, then the per-Frame update and render (`world.progress(dt)`). How often Frames run is the Frame rate, set on the Window (a cap, vsync or uncapped), apart from the Tick rate.
_Avoid_: tick (a Tick is the simulation's step; `App::Tick` and `SceneBase::Tick` are per-Frame and predate the term), update

**Tick**:
One fixed step of the simulation: every `FixedStep` System runs once, in phase order, with the fixed dt (`1 / FixedStepConfig::tick_rate`). A Frame runs as many Ticks as its accumulated time holds, up to a catch-up cap; pause runs none. `FixedStepState::tick` counts the Ticks that ran, so things can be stamped against it.
_Avoid_: step, substep, fixed update, frame (a Frame renders; a Tick simulates)

**FixedStep**:
A Tag applied to a simulation System to run it on the Tick, in the fixed-step Pipeline, instead of once per Frame. Frame Pipelines leave it out, so `world.progress()` alone never runs it. It needs no Pausable tag, since pause stops the whole fixed step.
_Avoid_: fixed update, interval system

**FrameSample**:
A Tag applied to a System that samples the outside world (input) once per Frame, before the Frame's Ticks, so every Tick and the rest of the Frame see the same sample. Frame Pipelines leave it out; `fixed_step::RunFrame` runs it, paused or not. `InputPoll` carries it.
_Avoid_: pre-tick system, input phase

**Phase**:
An execution slot within a Pipeline that determines *when* a system runs relative to others. The engine uses the Flecs built-in phases (`PreUpdate`, `OnUpdate`, `OnStore`, `PostUpdate`) plus two custom phases: `ecs::OnUI` runs after `OnStore` and guarantees 2D overlays composite on top of 3D, and `ecs::OnDebugUI` runs after `OnUI` to render the debug UI on top of everything. Systems are assigned a phase via `.kind(phase)`.
_Avoid_: stage, step, tick phase

**Cluster**:
One cell of the view frustum for point lighting: a screen tile by a depth slice. Each Frame the Renderer lists the point lights that reach each cluster, and a fragment evaluates only its own cluster's list (ADR 0009).
_Avoid_: froxel, light tile, light cell

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
A world's own named spdlog logger (`ecs::WorldLogger` Singleton, read through `ecs::Log(world)`). It writes to the default logger's sinks, so two worlds in one process (a client and an integrated server) share one log, each line labeled with the world that wrote it. A world without one logs through the default logger.
_Avoid_: per-world sink (a World Logger shares the default sinks)

**Tag**:
A zero-size struct component used purely for filtering — carries no data. Examples: `ecs::Pausable` (marks a system as pausable), `scene::Active` (marks the one active scene), `scene::Paused` (switches the world into its paused pipeline). In this engine Tags are *always* zero-size; non-zero tags are Components.
_Avoid_: marker, flag component

**Pausable**:
A Tag applied to a per-Frame simulation System to exclude it from the paused Pipeline. Systems that should keep running during pause (input, rendering, UI, audio) simply omit this tag. Toggling pause adds/removes `scene::Paused` from the world; the scene-management Module observes this and swaps the active Pipeline, and the fixed step runs no Tick.
_Avoid_: pausable flag, pause-aware

**Window**:
The surface a game is shown in, and its link to the OS: its size and fullscreen state, how often Frames are paced, and each Frame's input, which it hands over as a sample. A headless world's Window has no surface. The Window doesn't draw (ADR 0001).
_Avoid_: Platform (the old name for everything outside the game), display, screen

**Canvas**:
The fixed-size surface a game lays out its 2D drawing and UI on, measured in its own units (1280 by 720 in every game today). It's fitted into the Window at its own aspect ratio, with bars filling the rest, and the mouse is reported in Canvas units. Its size doesn't change when the Window does.
_Avoid_: screen, window size, resolution (the Window's pixels, which the Canvas is scaled to)

**Renderer**:
The Module that draws a RenderFrame, handed to it once per Frame. It's the only code that talks to a graphics API, and it decides the passes, their order, and what each View sees (ADR 0010).
_Avoid_: render backend, graphics backend, Window (the Window doesn't draw)

**RenderFrame**:
Everything one Frame draws, as plain data: its Views, DrawItems, lights, environment, lines, 2D list, settings, and the Assets they use. Systems fill it from the world each Frame and the Renderer reads it; it's complete, so nothing is left out because the camera can't see it.
_Avoid_: frame (a Frame is the pass of the game loop), render queue, scene

**DrawItem**:
One thing to draw in a RenderFrame: a shape or mesh with its material, placed in the world, belonging to one entity. An entity with several parts (the sub-meshes of a model) gives one DrawItem per part, told apart by the part.
_Avoid_: draw call, render command, renderable

**View**:
A viewpoint the Renderer draws the DrawItems from: a camera, or a viewpoint a pass makes for itself, such as a shadow cascade or a spot light. A View keeps its identity from Frame to Frame.
_Avoid_: camera (that's the component a camera View comes from), viewport, render pass

**Hidden**:
A Tag saying an entity and everything under it in the `ChildOf` hierarchy isn't drawn: no DrawItems, no lights, so no shadows either. It changes only what's seen; Systems, physics, sound and UI carry on as before.
_Avoid_: disabled (a disabled entity is skipped by Systems, and its children aren't), invisible, culled (culling is the Renderer leaving out what a View can't see)

**Asset**:
Something loaded or made once and used by many entities: an image, mesh, shader, animation set, font, sound or music. It's an entity holding the data on the CPU, found by its file path when it came from one. The Renderer keeps its own GPU copy, which it can always rebuild (ADR 0012).
_Avoid_: resource, handle (a handle is how code used to point at one)

**Asset Role**:
The part an Asset plays for the entity that uses it, such as albedo, normal map, mesh or sound. Each role is its own relationship from the user to the Asset, so one material can hold several textures without mixing them up.
_Avoid_: slot, asset type (the type is what the Asset is, the role is how it's used)

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

**File Reference**:
A JSON string `"@file:<path>"` that stands in for the whole JSON document at `<path>`, resolved against the directory of the file holding it. Levels use them to split entity trees across files; `core::LoadJsonFileResolved` resolves them in any JSON (a save, a config). Distinct from a **Ref**, which names an entity.
_Avoid_: include, file ref sigil (the sigil is `kFileRefSigil`, the string is the reference)

**RenderWith**:
An ECS relationship from a renderable entity to a *material entity*. The material entity carries `Material`, the textures it uses (each through its Asset Role), and optionally its Surface Shader. Render systems traverse this relationship to find the material; script accessors (`GetMaterial()`) do the same transparently.
_Avoid_: material reference, material link

**Surface Shader**:
A material's own description of how its surface looks before any light reaches it: color, opacity, glow, bumps, roughness and metalness, and optionally how its vertices sway. It never sees lights, shadows, the camera or the screen, so the engine can light it, shadow it and capture its color for bounced light. A material without one uses the standard surface (ADR 0013).
_Avoid_: custom shader, `ShaderMap`, material shader

**Occluder Fade**:
An object standing between the camera and the player dissolves where it covers the player on screen, so the player stays visible behind trees and buildings. Only materials marked for it fade.
_Avoid_: ghost pass, x-ray, `TransparentPass`

**Translucent Pass**:
The render pass that draws `Translucent`-tagged entities after all opaque geometry: alpha-blended by their `Material` alpha, depth test on and depth write off, sorted back to front. Occluder Fade is not part of it: a fading occluder stays opaque and dissolves where it covers the player.
_Avoid_: transparent pass, alpha pass, blend pass

**Animation Kind**:
A family of animation that supplies its own clips and decides how a clip at a given time changes what's drawn: skeletal mesh, 2D sprite, spline, and so on. Every kind shares the same Animation Player; only its clips and how they take effect differ.
_Avoid_: animation type, animator

**Animation Player**:
An entity's playback state for one clip of its Animation Kind: which clip, the time into it, the speed, and whether it loops or is playing. It knows nothing of meshes or sprites; it only moves time through a clip of known length.
_Avoid_: animator, animation controller, animation state

**Character Body**:
An entity's physical presence as a walking character: a capsule that moves at the velocity the game asks for, falls under its Gravity, stays on the ground, climbs slopes and steps, and slides along walls. It goes where the game wants it to, but it doesn't know about input. It's distinct from a rigid body, which the simulation pushes around.
_Avoid_: character controller, player controller, kinematic body

**Inner Body**:
The rigid body that gives a Character Body presence to everything else: the same capsule, following the character, so rays hit it, rigid bodies bounce off it and other characters are blocked by it. A character can go without one, and then rays and other characters pass through it.
_Avoid_: hitbox, proxy body

**Collision Layer**:
The category a body or character sits on (default, player, enemy, obstacle, trigger). Raycasts choose which layers they hit; contacts between bodies ignore it.
_Avoid_: collision group, physics layer

**Gravity**:
The single acceleration an entity is subject to, which also defines its up. An entity without its own Gravity uses the world's. There's no blending between sources: something else decides which source applies and sets it.
_Avoid_: gravity source, gravity field, gravity well
