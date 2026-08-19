# Scenes & Lifecycle Methods

A **scene** is a distinct screen/state of the game (title, gameplay, pause menu, …). Scenes are plain C++ objects
deriving from `engine::scene::Scene` (`src/engine/scene/scene.hpp`), attached to a Flecs entity through a
`SceneComponent`. The **scene-management module**
(`src/engine/scene/scene_management_module.cpp`) drives their lifecycle with observers and systems.

## The `Scene` interface

All lifecycle methods are virtual with empty defaults — override only what you need.

| Method                      | When it runs                                                              | Typical use                                                                                                                          |
|-----------------------------|---------------------------------------------------------------------------|--------------------------------------------------------------------------------------------------------------------------------------|
| `InitializePipeline(world)` | Once, when `SceneComponent` is **set** (`OnSceneComponentAdded` observer) | Build this scene's pipeline(s): the running `pipeline_` and, if the scene pauses, a `pausedPipeline_` that excludes `ecs::Pausable`. |
| `Load()`                    | Once, when the scene becomes **Active** (`OnSceneActivate` observer)      | Create the scene's entities (world objects, UI roots). Sets the world pipeline first, then calls `Load()`.                           |
| `Unload()`                  | Once, when the scene stops being Active (`OnSceneDeactivate` observer)    | **Destroy everything `Load()` created** so re-activation doesn't duplicate entities or leak.                                         |
| `Tick(world, dt)`           | Every frame while active (`ActiveSceneTick`, `OnUpdate`)                  | Per-frame scene logic; e.g. open the pause modal while paused.                                                                       |
| `HandleInput(world, input)` | Every frame while active (`ActiveSceneInput`, `OnUpdate`)                 | React to `InputState` (keys/mouse) sampled in `PreUpdate`.                                                                           |
| `DrawDebugUI() const`       | Every frame while active (`ActiveSceneDebugUI`, `OnUI`)                   | ImGui debug panels.                                                                                                                  |
| `DrawUI(world, input)`      | Every frame while active (`ActiveSceneUI`, `OnUI`)                        | Immediate 2D overlays that must composite on top of 3D.                                                                              |

Accessors: `GetPipeline()` returns the running pipeline; `GetPausedPipeline()` returns
`pausedPipeline_` if set, else falls back to `pipeline_` (i.e. a scene that never pauses needs no second pipeline).

### Phase vs. scene filter

A scene's own systems separate *when* they run from *which scene* they run in — a single
`.kind(phase)` for ordering, then a scene tag for filtering:

```cpp
auto sys = world.system("MySystem").kind(flecs::OnUpdate).run([...]).child_of(sceneRootEntity);
```

The scene builds its pipeline to exclude the *other* scene's tag, so untagged (engine) systems run in every scene while
scene-specific systems are isolated:

```cpp
pipeline_ = world.pipeline().with(flecs::System).without<scene::MenuScene>().build();
```

## Registration, activation, reload

Free functions in `scene_management_module.hpp`:

```cpp
// Register a named scene entity carrying a (SceneId, <name>) identity pair.
flecs::entity RegisterScene(world, SceneId id, std::shared_ptr<Scene> scene);

// Find a scene entity by its SceneId identity (stable across launches; used by tools too).
flecs::entity FindScene(world, SceneId id);

// Deactivate the current scene, then activate the target (drives Unload→Load via observers).
void ActivateScene(world, SceneId id);

// Unload → Load the same scene in place (e.g. reset after applying a save). No-op if inactive.
void ReloadScene(world, SceneId id);
```

Identity lives in ECS data: each scene entity pairs its `SceneComponent` with a `SceneId` tag, so it can be
found/switched **by identity** — in C++ and over the Flecs Remote API by name — without depending on runtime-assigned
entity ids. `Active` is an `Exclusive` relationship, so only one scene is active at a time.

`ActivateScene`/`ReloadScene` are **safe to call from inside a system**: they use deferred structural changes, so the
removal of `Active` on the old scene and the add on the new are applied in order at the next merge, firing `Unload`
(old) then `Load` (new). `ActivateScene` collects the currently-active scenes into a vector *before* mutating, so it
never removes from a query while iterating it during non-deferred startup.

Example wiring (`src/game/game.cpp`):

```cpp
auto title = std::make_shared<TitleScene>(context);
title->onPlayPressed = [w = &world]{ scene::ActivateScene(*w, scene::SceneId::Game); };
scene::RegisterScene(world, scene::SceneId::Title, title);
// ...register Game scene...
scene::ActivateScene(world, scene::SceneId::Title);   // start on the title screen
```

## Pausing a scene

See [architecture.md](architecture.md#the-pause-model). A pausable scene overrides
`InitializePipeline` to build both a running and a paused pipeline; toggling
`world.add/remove<scene::Paused>()` swaps them via the `OnPauseStateChanged` observer. `CubeScene`
does this; `TitleScene` has nothing to pause, so it relies on the `GetPausedPipeline()` fallback.

## The deferred-write hazard (read this before writing `Load()`)

`Load()` runs **inside** the scene-activation observer, so component writes are **deferred**. Do **not** `get_mut<>()` a
component you just `set<>()` in the same `Load()` — Flecs will assert (`entity does not have component`) because the add
hasn't merged yet. Rules of thumb:

- Pass initial state through the factory / `set<>()` call, not a follow-up `get_mut<>()`.
- Guard any per-frame `get_mut<>()` in `Tick()` with `has<>()`.

(See the "Deferred context" note in [ui/README.md](../src/engine/ui/README.md).)

## Cleanup contract

Because a scene can be activated more than once (title → game → title → …), **`Unload()` must undo
`Load()`**. Destroy the scene's root entities (e.g. `uiRoot_`, `hud_`) so their children are recursively deleted; don't
hold stale `flecs::entity` handles across an Unload/Load cycle without rebuilding them. `TitleScene` and `CubeScene`
both build their UI in `Load()` and tear it down in
`Unload()` — follow that pattern for new scenes.

## Reference

- Interface: `src/engine/scene/scene.hpp`
- Tags/ids: `src/engine/scene/scene_components.hpp`
- Driver: `src/engine/scene/scene_management_module.{hpp,cpp}`
- Example scenes: `src/game/title_scene.{hpp,cpp}`, `src/game/game_scene.{hpp,cpp}`
