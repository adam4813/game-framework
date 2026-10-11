# UI Module

Data-driven 2D UI for Flecs ECS. A UI element is an entity that pairs a `UIRect` (position + size)
with one or more visual components (`Panel`, `Label`, `Button`, `ProgressBar`, `Spinner`) and/or layout/behaviour
components (`Stack`, `ScrollRect`, `Modal`, `OnClick`, `audio::SoundEffect`).

The module separates **view** (constructing the UI tree / resolving layout + building ordered draw
commands) from **presentation** (submitting those commands to the platform). The build walk runs in
`PreStore` and fills the `UIDrawList` singleton; a flush system submits it in `OnUI`. Draw order is
a total order over `(layer, seq)`: `layer` is the inherited `UIElement.z_index` (so a `Modal`
composites above base UI), and `seq` is the tree pre-order position stamped on each pushed command.
The walk is depth-first — a parent paints before its children, and siblings paint in the parent's
`children()` order (the same traversal the layout pass uses) — so child-over-parent and intra-layer
ordering stay consistent between layout and paint. Use `UIElement.z_index` to control which elements
composite on top rather than relying on registration order.

## Structure

- **ui_components.hpp** — Plain-data components (`UIRect`, `Panel`, `Label`, `Button`, `Slider`,
  `ProgressBar`, `Spinner`, `Stack`, `ScrollRect`, `Modal`) and pure-data singletons/tags.
- **ui_draw_list.hpp** — Deferred-render types (`UIDrawCommand`, `UIDrawList` with its `Push*`
  helpers, `SetLayer`, and `BeginClip`/`EndClip`), sorted by `(layer, seq)`.
- **ui_events.hpp** — Callback/behaviour components (`OnClick`).
- **ui_factories.hpp/cpp** — The `Create*` convenience factories + `SetDefaultClickSound`.
- **ui_systems.hpp/cpp** — Internal helpers + `RegisterUISystems` (layout/interaction/build/render).
- **ui_module.hpp/cpp** — Flecs registration (reflection, scripting) and the
  `Create*` convenience factories are now in **ui_factories.hpp/cpp** (`CreatePanel`/`Label`/`Button`/
  `ProgressBar`/`Slider`/`Spinner`/`Stack`/`ScrollRect`/`Modal`/`Prompt`).
- **ui.hpp** — Public umbrella header (re-exports every header above).

## Components

| Component        | Purpose                                                                            |
|------------------|------------------------------------------------------------------------------------|
| `UIRect`         | Screen-space position + size. **Every** UI entity carries one.                     |
| `UIElement`      | Optional `z_index` (layer) + `visible`. Inherited by descendants as a floor.       |
| `Panel`          | Filled box with optional border/rounding.                                          |
| `Label`          | Text with alignment; vertically centred in its rect.                               |
| `Button`         | Clickable; runtime `hovered`/`pressed`/`clicked` state; fires `OnClick` + sound.   |
| `Slider`         | Interactive horizontal value slider; drag the thumb; fires `on_change`.            |
| `ProgressBar`    | Determinate value bar (`value` in `[min, max]`).                                   |
| `Spinner`        | Indeterminate loading spinner (rotating dots), animated by the module.             |
| `Checkbox`       | Toggleable boolean + optional label; fires `on_change(bool)`.                      |
| `Radio`          | Grouped, mutually-exclusive toggle (shared `group` string); fires `on_change`.     |
| `TextInput`      | Single-line editable text; click-to-focus, caret, Backspace; Enter → `on_submit`.  |
| `TextArea`       | Multi-line editable text; Enter inserts a newline; click-to-focus, caret.          |
| `Dropdown`       | Selector with an expandable option list; fires `on_change(index)`.                 |
| `Stack`          | Auto-layout: positions direct UI children vertically/horizontally.                 |
| `ScrollRect`     | Clipped, scrollable viewport with a scrollbar (`-scroll` offset + scissor clip).   |
| `Modal`          | Dialog over a full-screen backdrop; blocks input to non-modal UI while open.       |
| `OnClick`        | `std::function<void(flecs::entity)>` handler run on a completed click.             |
| `UIFocus`        | Singleton: the text field (`TextInput`/`TextArea`) currently receiving typed text. |
| `UIClickRequest` | Tag; add it to a Button to fire its click programmatically (scripts/tests/tools).  |

## Usage

```cpp
// Imported by EngineContext after the render module:
world.import<engine::ui::UIModule>();
```

UI is treated like any other simulation, split across the standard pipeline phases: **layout** in
`PreUpdate`, **interaction/logic** in `OnUpdate` (reading input polled in `PreUpdate`, before it is reset in
`PostUpdate`), **draw-list build** in `PreStore` (the "view": walking the settled tree into ordered draw
commands), and **rendering** in `engine::ecs::OnUI` (the "presentation": submitting the commands after 3D in
`OnStore`, so 2D composites on top). All UI systems are untagged (run in every scene pipeline) and non-Pausable (UI
keeps working while the game is paused).

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

## Data Binding

`DataBinding` (ui_binding.hpp) connects a widget's natural value — `Label`/`TextInput`/`TextArea` text,
`ProgressBar`/`Slider` value, `Checkbox`/`Radio` checked, `Dropdown` selected index — to an external *data point*, so
JSON- or script-authored UI stays in sync with game state without hand-written glue.
`UIDataBindingSync` (OnUpdate) reads the source into the widget and, for editable widgets, writes edits
back. Sources (Strategy over `BindSource`):

- **Blackboard** — a key in an entity's `ai::Blackboard` (the engine's shared key-value store; this is
  an adapter, not a new registry).
- **EntityField** — a reflected member of a component on another entity, via Flecs meta (e.g. bind a
  `Label` to `Health.current`).
- **Script** — getter/setter callbacks.

`BindMode` is `ToUI` (read-only display), `ToSource` (push edits), or `TwoWay` (source-authoritative
until edited). Values flow through `BindingValue` (bool/number/text with conversions), so a numeric
source can drive a text `Label`. Attach via `BindToBlackboard` / `BindToEntityField` / `BindToScript`:

```cpp
auto state = world.entity("HudState"); // gains an ai::Blackboard
ui::BindToBlackboard(ui::CreateSlider(world, {x, y, 200, 24}, 0.5F, 0, 1), state, "volume");
ui::BindToBlackboard(ui::CreateProgressBar(world, {x, y + 30, 200, 18}), state, "volume", ui::BindMode::ToUI);
// Dragging the slider updates "volume"; the progress bar mirrors it — no code links the two.
```

## Authoring UI in JSON

`RegisterUILevelLoaders` registers `engine::level` loaders so a UI tree can be authored in a
`.level.json` (entity tree + `components`/`children`/`refs`). Component keys: `ui_rect`, `ui_element`,
`panel`, `label`, `button`, `progress_bar`, `slider`, `spinner`, `stack`, `scroll_rect`, `modal`,
`checkbox`, `radio`, `text_input`, `text_area`, `dropdown`, and `data_binding` (whose source entity is
wired by the `binding_source` ref). See `assets/levels/ui_widgets_demo.level.json` for a bound-widget
example loaded by the title screen's "UI Demo" button.

```json
{
  "components": {
    "ui_rect": [460, 176, 360, 24],
    "slider": {"value": 50, "min": 0, "max": 100},
    "data_binding": {"source": "blackboard", "key": "volume", "mode": "two_way"}
  },
  "refs": {"binding_source": "UIDemoState"}
}
```

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

Scripts can also **build** UI via the `Create*` factory globals (`RegisterUIScriptFactories`), which take a
parent `Entity` + rect (+ widget args) and return the created `Entity` (see `assets/scripts/ui_factory_demo.as`):

```angelscript
Entity panel = CreatePanel(self.GetParent(), 20, 20, 240, 120);
Entity ok    = CreateButton(panel, 32, 70, 160, 40, "OK");
SetOnClick(ok, @OnOk);
```

Available factory globals: `CreatePanel`, `CreateLabel`, `CreateButton`, `CreateCheckbox`, `CreateRadio`,
`CreateTextInput`, `CreateTextArea`, `CreateSlider`, `CreateProgressBar`, `CreateStack`, `CreateSpinner`.

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

**`PreStore` — draw-list build** (the "view", after all `OnUpdate` mutation settles)

- `UIBuildDrawList` — walks UI roots and recurses through children in `children()` order (tree pre-order
  determines layering, plus scroll offset, `z_index` inheritance into `layer`, and clip regions), stamping each
  emitted command with `(layer, seq)`.

**`OnUI` — rendering** (the "presentation", after 3D in `OnStore`)

- `UIRenderDrawList` — sorts by `(layer, seq)`, issues the platform 2D draw calls, and maintains a clip stack for
  `ScrollRect` scissor regions.

Button state components (`hovered`/`pressed`/`clicked`) are written in `OnUpdate` and read by the `PreStore`
build, so the draw list reflects the same-frame interaction with no lag.

Game or scripting code may also enqueue custom overlay draws through `UIDrawList`'s `Push*` helpers (call
`SetLayer` first to choose the compositing layer); anything pushed after the `PreStore` build paints above it
within the same layer.
