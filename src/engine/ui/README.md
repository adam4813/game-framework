# UI Module

Data-driven 2D UI for Flecs ECS. A UI element is an entity that pairs a `UIRect` (position + size)
with one or more visual components (`Panel`, `Label`, `Button`, `ProgressBar`, `Spinner`) and/or layout/behaviour
components (`Stack`, `ScrollRect`, `Modal`, `OnClick`, `audio::SoundEffect`). Rendering is deferred through a
`UIDrawList` singleton so draw order is explicit: FIFO, refined by the entity hierarchy and an optional
`UIElement.z_index` layer.

## Structure

- **ui_components.hpp** — Plain-data components and the deferred-render types (`UIDrawCommand`,
  `UIDrawList` with its `Push*` helpers, including `BeginClip`/`EndClip`).
- **ui_module.hpp/cpp** — Flecs registration (reflection, scripting), the OnUI systems, and the
  `Create*` convenience factories (`CreatePanel`/`Label`/`Button`/`ProgressBar`/`Spinner`/`Stack`/
  `ScrollRect`/`Modal`/`Prompt`).
- **ui.hpp** — Public umbrella header (components + module).

## Components

| Component        | Purpose                                                                           |
|------------------|-----------------------------------------------------------------------------------|
| `UIRect`         | Screen-space position + size. **Every** UI entity carries one.                    |
| `UIElement`      | Optional `z_index` (layer) + `visible`. Inherited by descendants as a floor.      |
| `Panel`          | Filled box with optional border/rounding.                                         |
| `Label`          | Text with alignment; vertically centred in its rect.                              |
| `Button`         | Clickable; runtime `hovered`/`pressed`/`clicked` state; fires `OnClick` + sound.  |
| `ProgressBar`    | Determinate value bar (`value` in `[min, max]`).                                  |
| `Spinner`        | Indeterminate loading spinner (rotating dots), animated by the module.            |
| `Stack`          | Auto-layout: positions direct UI children vertically/horizontally.                |
| `ScrollRect`     | Clipped, scrollable viewport with a scrollbar (`-scroll` offset + scissor clip).  |
| `Modal`          | Dialog over a full-screen backdrop; blocks input to non-modal UI while open.      |
| `OnClick`        | `std::function<void(flecs::entity)>` handler run on a completed click.            |
| `UIClickRequest` | Tag; add it to a Button to fire its click programmatically (scripts/tests/tools). |

## Usage

```cpp
// Imported by EngineContext after the render module:
world.import<engine::ui::UIModule>();
```

UI is treated like any other simulation, split across the standard pipeline phases: **layout** in
`PreUpdate`, **interaction/logic** in `OnUpdate` (reading input polled in `PreUpdate`, before it is reset in
`PostUpdate`), and **rendering** in `engine::ecs::OnUI` (after 3D in `OnStore`, so 2D composites on top). All UI systems
are untagged (run in every scene pipeline) and non-Pausable (UI keeps working while the game is paused).

## Adding UI to an Entity

Use the factories, or compose components directly. Parent elements with `.child_of(parent)` to control layout and draw
order (children composite on top of parents).

```cpp
using namespace engine;

// A modal prompt built from Modal + Stack + Labels + Buttons.
ui::CreatePrompt(world, {cx - 200, cy - 130, 400, 240}, "Paused", "",
    {
        {.label = "Resume", .on_click = [](flecs::entity e) { e.world().remove<scene::Paused>(); }},
        {.label = "Quit",   .on_click = [](flecs::entity)   { /* ... */ }},
    });

// A scrollable list: a Stack of item buttons inside a ScrollRect.
auto scroll = ui::CreateScrollRect(world, {x, y, 260, 175});
auto list   = ui::CreateStack(world, {x, y, 260, 0},
    {.direction = ui::StackDirection::Vertical, .spacing = 6.0F, .padding = 6.0F});
list.child_of(scroll);
for (int i = 0; i < 10; ++i)
    ui::CreateButton(world, {0, 0, 0, 34}, "Item " + std::to_string(i)).child_of(list);
```

> **Deferred context:** a scene's `Load()` runs inside the scene-activation observer, so component
> writes are deferred. Do **not** `get_mut<>()` a component you just `set<>()` in the same `Load()`
> call — it will assert (`entity does not have component`). Pass initial state through the factory /
> `set<>()`, and guard per-frame `get_mut<>()` in `Tick()` with `has<>()`.

## Scripting

Every registered component gets `self.GetT()` / `AddT()` / `SetT()` accessors in scripts, and the getters resolve to the
host (the script entity's parent). `assets/scripts/ui_demo.as` drives a
`ProgressBar` live:

```angelscript
void Tick(Entity self, float dt) {
    ProgressBar@ bar = self.GetProgressBar();   // resolves to the host progress-bar entity
    bar.value = /* ... */;                       // written straight into ECS storage
}
```

## Driving the UI from tools

`tools/flecs-api.js` can trigger buttons over the Flecs REST API by adding the `UIClickRequest` tag:

```bash
node tools/flecs-api.js list-buttons          # id, full path, label for every Button
node tools/flecs-api.js click PlayButton       # by leaf name (resolved to #id)
node tools/flecs-api.js click "#900"           # or by raw entity id
```

## Registered Systems

Systems are distributed across phases so UI behaves like ordinary simulation:

**`PreUpdate` — layout**

- `UILayout` — top-down pass from UI roots; positions every `Stack`'s children (recursing so nested stacks lay out in
  one pass) before anything hit-tests against their rects.

**`OnUpdate` — interaction/logic** (reads input polled in `PreUpdate`; runs before `PostUpdate`
clears the per-frame flags)

- `UIScrollRectUpdate` — recomputes content bounds, applies mouse-wheel scroll and scrollbar-thumb drag, clamps
  `scroll`.
- `UISpinnerAnimate` — advances each `Spinner`'s rotation using `delta_time()`.
- `UIModalInteraction` — closes a `close_on_backdrop` modal when clicked outside.
- `UIButtonInteraction` — hover/press/click from the mouse (scroll-clip, visibility, and modal input-blocking aware);
  dispatches `OnClick` + plays the button's `SoundEffect`.
- `UIButtonClickRequest` — consumes `UIClickRequest` tags and fires clicks programmatically.

**`OnUI` — rendering** (after 3D in `OnStore`)

- `UIBuildDrawList` — walks UI roots and recurses through children (hierarchy + FIFO ordering, scroll offset, `z_index`
  inheritance, clip regions) emitting draw commands.
- `UIRenderDrawList` — stable-sorts by layer, issues the platform 2D draw calls, and maintains a clip stack for
  `ScrollRect` scissor regions.

Button state components (`hovered`/`pressed`/`clicked`) persist between phases, so rendering in
`OnUI` reflects the same-frame interaction from `OnUpdate` with no lag.

Game or scripting code may also enqueue custom overlay draws through `UIDrawList`'s `Push*` helpers before
`UIRenderDrawList` runs.
