# UI Components

Data-driven 2D UI for Flecs. A UI element is an entity pairing a `UIRect` (position + size) with one or more visual
components (`Panel`/`Label`/`Button`/`ProgressBar`/`Slider`/`Spinner`), input widgets (`Checkbox`/`Radio`/`TextInput`/
`TextArea`/`Dropdown`) and/or layout/behaviour components (`Stack`/`ScrollRect`/`Modal`/`OnClick`/`audio::SoundEffect`).
The module splits **view** (building the UI tree into an ordered draw list in `PreStore`) from **presentation**
(submitting it in `OnUI`); draw order is a total order over `(layer, seq)` — `UIElement.z_index` layer, then tree
pre-order — so children paint over parents independent of archetype/iteration order. UI can be bound to data points
(`DataBinding`) and authored from C++, JSON, or scripts.

Authoritative deep-dive: [ui/README.md](../src/engine/ui/README.md). This page is the framework-level summary; see
also [systems-reference.md](systems-reference.md#ui-srcengineuiui_modulecpp) for the system table.

## Components

| Component        | Purpose                                                                                           |
|------------------|---------------------------------------------------------------------------------------------------|
| `UIRect`         | Screen-space position + size. **Every** UI entity carries one.                                    |
| `UIElement`      | Optional `z_index` (layer) + `visible`; inherited by descendants as a floor.                      |
| `Panel`          | Filled box with optional border/rounding.                                                         |
| `Image`          | A texture in the rect: `fit` (stretch/fit/fill/tile), atlas `source` rect, `tint`. See "Image" below. |
| `Label`          | Text with alignment; vertically centred in its rect.                                              |
| `Button`         | Clickable; runtime `hovered`/`pressed`/`clicked`; fires `OnClick` + sound.                        |
| `ProgressBar`    | Determinate value bar (`value` in `[min, max]`).                                                  |
| `Slider`         | Horizontal interactive slider (`value`, `min`, `max`, `step`, `on_change`). Drag thumb to update. |
| `Spinner`        | Indeterminate loading spinner, animated by the module.                                            |
| `Checkbox`       | Toggleable boolean + optional label; fires `on_change(bool)`.                                     |
| `Radio`          | Grouped, mutually-exclusive toggle (shared `group` string); fires `on_change`.                    |
| `TextInput`      | Single-line editable text; click-to-focus, caret, Backspace; Enter → `on_submit`.                 |
| `TextArea`       | Multi-line editable text; Enter inserts a newline; click-to-focus, caret.                         |
| `Dropdown`       | Selector with an expandable option list; fires `on_change(index)`.                                |
| `Stack`          | Auto-layout: positions direct UI children vertically/horizontally.                                |
| `UIAnchor`       | Places the element against its parent's rect (or the canvas); see below.                          |
| `ScrollRect`     | Clipped, scrollable viewport with a scrollbar.                                                    |
| `Modal`          | Dialog over a full-screen backdrop; blocks input to non-modal UI while open.                      |
| `OnClick`        | `std::function<void(flecs::entity)>` run on a completed click.                                    |
| `DataBinding`    | Binds a widget's value to a blackboard key / component field / script callback.                   |
| `UIFocus`        | Singleton: the text field currently receiving typed text.                                         |
| `UIClickRequest` | Tag; add to a Button to fire its click programmatically (scripts/tests/tools).                    |

Deferred-render types (`UIDrawCommand`, `UIDrawList` with `Push*`/`BeginClip`/`EndClip`) live in
`ui_draw_list.hpp` (`SetLayer` sets the compositing layer); game/scripting code may enqueue custom overlay draws.

## Factories

`ui_factories.hpp` provides `Create*` convenience factories: `CreatePanel`, `CreateLabel`,
`CreateButton`, `CreateProgressBar`, `CreateSlider`, `CreateSpinner`, `CreateStack`, `CreateScrollRect`,
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

## Anchoring

`UIAnchor { anchor, pivot, offset }` places an element without a game-side system. `UILayout` resolves it first, before
Stack layout, against a reference rect: the parent's `UIRect` when it has one, otherwise the canvas (fixed logical size,
so a window resize never moves it). The element's size is untouched.

```
x = ref.x + anchor.x * ref.w + inward(anchor.x) * offset.x - pivot.x * rect.w     (same for y)
```

- `anchor` and `pivot` are fractions of the reference rect and of the element. A negative `pivot` component (default
  `-1`) means "same as the anchor" on that axis.
- `offset` is in pixels and moves **inward** from the anchored edge: `inward(a)` is `-1` when `a > 0.5` and `+1`
  otherwise, so `anchor {1, 1}, offset {12, 12}` sits 12 px in from the right and bottom edges.
- A Stack child ignores its `UIAnchor` (the Stack owns its position). An anchored Stack is placed first, then lays out
  its children.

```json
"ui_rect": [0, 0, 400, 60],
"ui_anchor": {"anchor": [0.5, 1.0], "offset": [0, 20]}
```

Hotbar centered at the bottom: `anchor {0.5, 1}, offset {0, 20}`. Top-left row: `offset {10, 10}`. Top-center label:
`anchor {0.5, 0}`. Left-edge column: `anchor {0, 0.5}`. Bottom-right label: `anchor {1, 1}`.

## Image

`Image` draws a texture into its element's `UIRect`: an icon, a crosshair, a portrait. Fields:

| Field     | Meaning                                                                                                   |
|-----------|-----------------------------------------------------------------------------------------------------------|
| `texture` | Handle from `assets::LoadTexture` (deduped by path, so sharing one is cheap). `-1` draws nothing.        |
| `fit`     | `Stretch` (fill, distorting), `Fit` (uniform scale, letterboxed), `Fill` (uniform scale, cropped), `Tile` (1:1 texel-to-pixel repeat, what `Panel.texture`/`Button.texture` do). Default `Stretch`. |
| `source`  | Texel sub-rectangle, for an atlas cell. A zero width or height means the whole texture. `Fit` and `Fill` use its aspect ratio. |
| `tint`    | Multiplies the texels; white (the default) leaves them unchanged.                                         |

The fit mode is resolved into a source and destination rectangle when the draw list is built
(`ResolveImageRects`, `ui_image.hpp`), so the `TexturedRect` draw command and `Platform::DrawTexturedRect(texture,
src, dest, tint)` carry the final rects and tests can assert on them headlessly. An `Image` clips under a `ScrollRect`
and paints on a `Modal`'s layer like any other element. `UIDrawList::PushTexturedRect(texture, dest, tint)` keeps its
tiling behavior; an overload takes an explicit `src`.

JSON: `"image": {"texture": "ui/icons.png", "fit": "fit", "source": [32, 0, 32, 32], "tint": [255, 255, 255, 255]}`
(`fit` is `stretch`, `fit`, `fill` or `tile`; `texture` resolves like Panel and Button textures). Scripts read and write
`Image` fields; `fit` is an int named by `FitMode_Stretch`, `FitMode_Fit`, `FitMode_Fill` and `FitMode_Tile`.

Not yet: nine-slice for `Panel`/`Button`, and an icon slot on `Button` (tracked as follow-ups).

## Phases

UI is treated like ordinary simulation, split across phases so there is no one-frame input lag:

- **Layout** — `UILayout` in `PreUpdate` resolves `UIAnchor` placements, then positions each `Stack`'s children (recursing into nested stacks) before
  anything hit-tests.
- **Interaction/logic** — `OnUpdate` reads input sampled in `PreUpdate` (before `PostUpdate` clears it):
  `UIScrollRectUpdate`, `UISpinnerAnimate`, `UIModalInteraction`, `UIButtonInteraction`,
  `UIButtonClickRequest`, then `UIPointerCapture` (see below).
- **Render** — `UIBuildDrawList` then `UIRenderDrawList` in `ecs::OnUI` (after 3D in `OnStore`), so 2D always composites
  on top.

## Pointer capture

A click on a `Button` is in the mouse input like any other, so gameplay (Build mode placing a Part) would act on it
too. The UI claims the pointer through the `input::UICapture` singleton, `{ pointer, keyboard }`:

- `UIPointerCapture` (last of the `OnUpdate` interaction systems) sets `pointer` when an interactive element is
  hovered or held (`Button`, `Slider`, `Checkbox`, `Radio`, `Dropdown`, a text field) or a `Modal` is open, from the
  hit-tests the interaction systems just ran. It also sets `keyboard` (see Keyboard capture).
- `InputPoll` honors it as it does the debug UI's capture: mouse buttons up, zero delta and scroll for gameplay, the
  position still flows. A locked cursor skips the gate.
- A press that lands while the pointer is captured latches until every button is released, so a drag that starts on a
  UI element stays with the UI and a press/release pair isn't split. A press that began in gameplay stays gameplay's
  even when dragged across a UI element.
- **One Frame of lag.** The UI pass runs after `InputPoll`, so the capture a Frame's input sees comes from the
  previous Frame's layout. It shows only when the layout moves under a still cursor: the Frame a `Modal` opens leaks
  that Frame's press to gameplay.

Since a captured press is hidden from `InputState`, the UI reads the mouse from `input::RawInput` (the mouse before
the UI's own gate; the debug UI's still applies). Gameplay reads `InputState`. Don't read `RawInput` outside UI code.

## Keyboard capture

A focused `TextInput`/`TextArea` (`UIFocus.focused`) takes the keyboard from gameplay, so typing W/A/S/D doesn't also
move a player controller:

- `UIPointerCapture` sets `UICapture.keyboard` whenever `UIFocus.focused` is a live entity. `InputPoll` then reports
  every key up, no repeats and no `TypedText` in `InputState`.
- The field edits from `RawInput`, not `InputState`: `RawInput.keys` (with `WasKeyPressed`/`WasKeyPressedOrRepeated`)
  and `RawInput.text` hold the keys and typed text before the `UICapture` gate (the debug UI's gate still applies).
  `UITextInputEdit` and `UITextAreaEdit` read them, so the field still gets typed text, Backspace and Return.
- Clicking away clears `UIFocus` in the same pass, so gameplay's keys return the next Frame. The same one Frame of
  lag as the pointer applies: the click that focuses a field leaks at most one Frame of keys. Destroying the focused
  field also frees the keyboard.

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
