# Scene Module

Flecs module that owns scenes: registering them, switching the active one, running its lifecycle and pausing it. A
scene is a `SceneBase` instance held by a `SceneComponent` on an entity that also carries an identity tag.

## Structure

- **scene-base.hpp** — `SceneBase`, the interface a scene implements (`RegisterLoaders`, `InitializePipeline`, `Load`,
  `Unload`, `Tick`, `HandleInput`, `DrawDebugUI`, `DrawUI`) and its pipeline getters.
- **scene_components.hpp** — `SceneComponent` (holds the `shared_ptr<SceneBase>`), the `Active` and `Paused` tags and
  the `GameScene` / `MenuScene` mode tags.
- **scene_module.hpp** — `SceneManagementModule` plus the templated helpers `RegisterScene<Tag>`, `FindScene<Tag>`,
  `ActivateScene<Tag>` and `ReloadScene<Tag>`.
- **scene_module.cpp** — Constructor: registers the components, the `OnUI` phase, the `PauseScene()` / `ResumeScene()`
  script functions, the lifecycle observers and the per-frame systems.
- **scene.hpp** — Umbrella header; include this from game code.

## Usage

```cpp
world.import<engine::scene::SceneManagementModule>();

struct TitleScene {};  // identity tag, one per scene

class Title : public engine::scene::SceneBase {
public:
    void InitializePipeline(const flecs::world& world) override {
        pipeline_ = engine::ecs::FramePipeline(world).without<engine::scene::GameScene>().build();
    }
    void Load(flecs::world& world) override { /* create the scene's entities */ }
    void Unload(flecs::world& world) override { /* destroy them */ }
    void DrawUI(const flecs::world& world, const engine::input::InputState& input) override { /* HUD */ }
};

engine::scene::RegisterScene<TitleScene>(world, std::make_shared<Title>());
engine::scene::ActivateScene<TitleScene>(world);   // deactivates the current scene first
engine::scene::ReloadScene<TitleScene>(world);     // Unload -> Load, same scene
world.add<engine::scene::Paused>();                // pause; world.remove<Paused>() resumes
```

Scripts pause with `PauseScene()` and `ResumeScene()`.

### Lifecycle

- **Register**: `set<SceneComponent>` fires `OnSceneComponentAdded`, which calls `InitializePipeline`.
- **Activate**: adding `Active` fires `OnSceneActivate`, which sets the scene's pipeline and calls `Load`. `Load` runs
  undeferred so it can read back what it just created. Don't `defer_suspend` inside `Load`; the observer already does.
- **Deactivate**: removing `Active` fires `OnSceneDeactivate`, which calls `Unload`.
- **Pause**: adding or removing `Paused` on the world fires `OnPauseStateChanged`, which swaps the active scene's
  pipeline between `GetPipeline()` and `GetPausedPipeline()`. The paused pipeline leaves out `ecs::Pausable` systems.
  `GetPausedPipeline()` falls back to the running pipeline when a scene builds no paused one.
- **Fixed step**: a scene builds `fixedStepPipeline_` from `ecs::FixedStepPipeline` with the same mode filter as its
  Frame pipeline (see `engine/fixed_step/README.md`).

### Mode tags

`GameScene` and `MenuScene` mark systems for one scene category. A scene's pipeline excludes the other tag with
`.without<...>()`. A system with neither tag runs in every scene.

## Systems registered

| System               | Phase             | Runs on            | What it does                                      |
|----------------------|-------------------|--------------------|---------------------------------------------------|
| `ActiveSceneTick`    | `flecs::OnUpdate` | the `Active` scene | `SceneBase::Tick(world, dt)`                      |
| `ActiveSceneInput`   | `flecs::OnUpdate` | the `Active` scene | `SceneBase::HandleInput` with `input::InputState` |
| `ActiveSceneDebugUI` | `ecs::OnUI`       | the `Active` scene | `SceneBase::DrawDebugUI`                          |
| `ActiveSceneUI`      | `ecs::OnUI`       | the `Active` scene | `SceneBase::DrawUI` with `input::InputState`      |

`ecs::OnUI` depends on `flecs::OnStore`, so 2D UI composites over 3D output whatever the module import order.

Observers: `OnSceneComponentAdded`, `OnSceneActivate`, `OnSceneDeactivate`, `OnPauseStateChanged`.
