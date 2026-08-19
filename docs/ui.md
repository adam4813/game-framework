# UI Components

Data-driven 2D UI for Flecs. A UI element is an entity pairing a `UIRect` (position + size) with one or more visual
components (`Panel`/`Label`/`Button`/`ProgressBar`/`Spinner`) and/or layout/behaviour components (`Stack`/`ScrollRect`/
`Modal`/`OnClick`/`audio::SoundEffect`). Rendering is **deferred**
through a `UIDrawList` singleton so draw order is explicit: FIFO, refined by the entity hierarchy and an optional
`UIElement.z_index`.

Authoritative deep-dive: [ui/README.md](../src/engine/ui/README.md). This page is the framework-level summary; see
also [systems-reference.md](systems-reference.md#ui-srcengineuiui_modulecpp) for the system table.

## Components

| Component        | Purpose                                                                        |
|------------------|--------------------------------------------------------------------------------|
| `UIRect`         | Screen-space position + size. **Every** UI entity carries one.                 |
| `UIElement`      | Optional `z_index` (layer) + `visible`; inherited by descendants as a floor.   |
| `Panel`          | Filled box with optional border/rounding.                                      |
| `Label`          | Text with alignment; vertically centred in its rect.                           |
| `Button`         | Clickable; runtime `hovered`/`pressed`/`clicked`; fires `OnClick` + sound.     |
| `ProgressBar`    | Determinate value bar (`value` in `[min, max]`).                               |
| `Spinner`        | Indeterminate loading spinner, animated by the module.                         |
| `Stack`          | Auto-layout: positions direct UI children vertically/horizontally.             |
| `ScrollRect`     | Clipped, scrollable viewport with a scrollbar.                                 |
| `Modal`          | Dialog over a full-screen backdrop; blocks input to non-modal UI while open.   |
| `OnClick`        | `std::function<void(flecs::entity)>` run on a completed click.                 |
| `UIClickRequest` | Tag; add to a Button to fire its click programmatically (scripts/tests/tools). |

Deferred-render types (`UIDrawCommand`, `UIDrawList` with `Push*`/`BeginClip`/`EndClip`) live in
`ui_components.hpp`; game/scripting code may enqueue custom overlay draws before `UIRenderDrawList`.

## Factories

`ui_module.hpp` provides `Create*` convenience factories: `CreatePanel`, `CreateLabel`,
`CreateButton`, `CreateProgressBar`, `CreateSpinner`, `CreateStack`, `CreateScrollRect`,
`CreateModal`, `CreatePrompt`. Parent elements with `.child_of(parent)` to control layout and draw order (children
composite on top of parents).

```cpp
using namespace engine;

ui::CreatePrompt(world, {cx - 200, cy - 130, 400, 240}, "Paused", "",
    {
        {.label = "Resume", .on_click = [](flecs::entity e){ e.world().remove<scene::Paused>(); }},
        {.label = "Quit",   .on_click = [](flecs::entity){ /* ... */ }},
    });

auto scroll = ui::CreateScrollRect(world, {x, y, 260, 175});
auto list   = ui::CreateStack(world, {x, y, 260, 0},
    {.direction = ui::StackDirection::Vertical, .spacing = 6.0F, .padding = 6.0F});
list.child_of(scroll);
for (int i = 0; i < 10; ++i)
    ui::CreateButton(world, {0, 0, 0, 34}, "Item " + std::to_string(i)).child_of(list);
```

`ui::SetDefaultClickSound(world, path)` makes every `CreateButton` auto-attach a click
`SoundEffect`.

## Phases

UI is treated like ordinary simulation, split across phases so there is no one-frame input lag:

- **Layout** — `UILayout` in `PreUpdate` positions each `Stack`'s children (recursing into nested stacks) before
  anything hit-tests.
- **Interaction/logic** — `OnUpdate` reads input sampled in `PreUpdate` (before `PostUpdate` clears it):
  `UIScrollRectUpdate`, `UISpinnerAnimate`, `UIModalInteraction`, `UIButtonInteraction`,
  `UIButtonClickRequest`.
- **Render** — `UIBuildDrawList` then `UIRenderDrawList` in `ecs::OnUI` (after 3D in `OnStore`), so 2D always composites
  on top.

All UI systems are **untagged** (run in every scene pipeline) and **non-Pausable** (UI keeps working while the game is
paused). Button state persists between phases, so `OnUI` rendering reflects the same-frame `OnUpdate` interaction.

## Deferred context

A scene's `Load()` runs inside the scene-activation observer, so component writes are deferred. Do **not** `get_mut<>()`
a component you just `set<>()` in the same `Load()` (it will assert). Pass initial state through the factory /
`set<>()`, and guard per-frame `get_mut<>()` in `Tick()` with
`has<>()`. See [scenes.md](scenes.md#the-deferred-write-hazard-read-this-before-writing-load).

## Scripting & tooling

Every registered component gets `self.GetT()`/`AddT()`/`SetT()` in scripts (resolving to the host);
`assets/scripts/ui_demo.as` drives a `ProgressBar` live. Buttons can be triggered over the Flecs REST API by adding the
`UIClickRequest` tag:

```bash
node tools/flecs-api.js list-buttons      # id, path, label for every Button
node tools/flecs-api.js click PlayButton   # by leaf name
```
