# Debug UI Module

An in-game Dear ImGui overlay for tuning panels, debug buttons and (later) editors. Game code draws
through the backend-agnostic `platform::DebugUi` interface and never includes ImGui or Raylib.

Press **`** (backtick, `input::kDebugUiToggleKey`) to show/hide it. The key is reserved while the
debug UI is compiled in: the game never sees it, and nothing typed on the toggle frame reaches a text
field. The built-in **Engine** panel shows FPS, frame time and the live entity count.

## Structure

| File                                    | Purpose                                                                   |
|-----------------------------------------|---------------------------------------------------------------------------|
| `platform/debug_ui.hpp`                 | `DebugUi` interface. The base class is the no-op (compiled-out) version.  |
| `platform/imgui/imgui_debug_ui.hpp/.cpp`| ImGui implementation: context, frame, widgets, input-capture flags.       |
| `platform/raylib/rl_imgui.hpp/.cpp`     | Raylib bridge (vendored from rlImGui): font upload, input feed, rendering.|
| `debug/debug_ui_module.hpp/.cpp`        | `OnDebugUI` phase, `DebugUiDraw` system, built-in `EngineDebugPanel`.     |

## Frame

1. `Platform::BeginFrame` — toggle key, queue input on ImGui, `ImGui::NewFrame` (only while visible).
2. `InputPoll` (PreUpdate) — withholds input the UI captured (see below).
3. Panel systems (any phase up to `OnUpdate`) — call widgets. Use `OnUpdate` for panels whose buttons
   change game state so the change lands this frame.
4. `DebugUiDraw` (`OnDebugUI`, after `OnUI`) — `Platform::DrawDebugUi` renders on top of everything.

## Writing a panel

```cpp
world.system("FaultDebugPanel").kind(flecs::OnUpdate).run([](const flecs::iter& it) {
	auto& ui = it.world().get<platform::PlatformRef>().ptr->GetDebugUi();
	if (ui.BeginPanel("Faults")) {
		if (ui.Button("Force jam")) { /* ... */ }
	}
	ui.EndPanel(); // always, even when BeginPanel returned false
}).add<scene::YourSceneTag>(); // optional: scope to a scene
```

While hidden or compiled out, `BeginPanel` returns false and every widget is a no-op, so panels need
no `#ifdef`s and cost almost nothing.

Panel systems must stay on the main thread (never `.multi_threaded()`): ImGui's context is global and
not thread-safe.

## Input capture

While the pointer is over (or dragging from) a panel, `InputPoll` reports mouse buttons up and zero
delta/scroll. While an ImGui text field is active (`io.WantTextInput`), it reports all keys up and no
typed text. Mouse *position* always flows. Focusing a panel, holding a button or dragging a slider does
not take the keyboard from the game (keyboard navigation is off).

## Build

`ENABLE_DEBUG_UI` (CMake option, default ON) compiles it out when OFF: no ImGui link, and
`Platform::GetDebugUi()` returns the no-op `DebugUi`. Verified running on desktop (`sandbox`) and in the
browser (`wasm-release`, `sandbox.html`); rendering goes through rlgl, so WebGL needs nothing extra.
