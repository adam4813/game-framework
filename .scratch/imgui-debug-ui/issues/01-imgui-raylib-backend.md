# Dear ImGui on Raylib behind a debug UI interface

Status: done
Source: raised during [AI Operator behaviour](../../signal-room/issues/08-ai-operator-behaviour.md), 2026-10-17

## Context

`imgui` is already a vcpkg dependency and links (`CMakeLists.txt`), and `src/engine/platform/platform.hpp` says
Raylib/ImGui calls live behind `Platform`, but nothing connects ImGui to Raylib yet. This is **framework work**:
land it on its own branch off `feat/signal-room` and PR it back into `feat/signal-room` (the project's base branch;
it never merges into `main`).

First consumers in the Signal Room slice: an AI tuning panel (live AI profile values next to each AI Operator's Job
board with candidate scores) and Fault/ticket debug buttons (force a Jam or Seize, spawn a Ticket, freeze the Window).
Later: a Layout editor and Collection Schedule authoring.

## Prior art (not yet read; read these first)

- Raylib interop (rlImGui), from a project with **no** debug UI interface:
  - `D:\src\prototypes\blacksmith\src\platform\vendor\rlImGui.h`
  - `D:\src\prototypes\blacksmith\src\platform\vendor\rlImGui.cpp`
- A debug UI interface wrapping ImGui:
  - `D:\src\prototypes\voxtech\src\ui\imgui_ui.h`
  - `D:\src\prototypes\voxtech\src\ui\imgui_ui.cpp`

## Acceptance criteria

- [x] rlImGui vendored into the engine and wired into the frame (begin/end around the debug draw, after the scene).
- [x] A debug UI interface (shaped after the voxtech one) so game code never includes ImGui or Raylib directly.
- [x] ImGui input capture gates the engine's input: while ImGui wants the mouse/keyboard, the game doesn't see it.
- [x] A key toggles the debug UI. It can be compiled out (CMake option) for release builds.
- [x] Desktop and WASM presets both build; WASM either works or is compiled out, documented which.
- [x] A minimal demo panel (e.g. FPS + a Flecs entity count) proves it end to end.

## Comments

**2026-10-17 — done.** Squashed to one commit, `5031f05`, based on `main` via the farm-world-alive retro `2e99e16` (branch
`feat/imgui-debug-ui`, ready to PR into `main`); `feat/signal-room` was restacked on top of it. The same
commit also carries the session's agent-environment fixes (skill/MCP symlinks, `flecs-api.js` nested
paths, AGENTS.md notes).

- Shipped: `platform::DebugUi` interface (`src/engine/platform/debug_ui.hpp`; base class is the no-op /
  compiled-out version), `ImGuiDebugUi` (`src/engine/platform/imgui/`), minimal rlImGui bridge
  (`src/engine/platform/raylib/rl_imgui.*`), `debug::DebugUiModule` (`src/engine/debug/`: `ecs::OnDebugUI`
  phase, `DebugUiDraw`, built-in `EngineDebugPanel`), `InputPoll` gating, `ENABLE_DEBUG_UI` CMake option,
  `ImGuiDebugUi` unit tests, docs (`src/engine/debug/README.md`, architecture, systems reference, glossary).
- Adjusted during implementation (agreed with the user):
  - "begin/end around the debug draw, after the scene" became: `ImGui::NewFrame` in `Platform::BeginFrame`
    (before any phase) so panels can run in `OnUpdate` and act this frame; rendering in `ecs::OnDebugUI`
    (after `OnUI`) via `Platform::DrawDebugUi`.
  - Input gating lives in `InputPoll` (the only Platform input consumer). Keyboard is captured only while
    an ImGui text field is active (`io.WantTextInput`, keyboard nav off); mouse while over/dragging a panel.
  - Toggle key is backtick (`input::kDebugUiToggleKey`), reserved from the game when compiled in.
  - The bridge is a minimal rewrite of rlImGui (no cursor shapes, HiDPI scale, or WASM clipboard).
- Verified: desktop runtime (panel renders over 2D title UI and 3D Cube scene; mouse held over the panel
  is withheld from `InputState`), WASM runtime in the browser (confirmed by the user), 11/11 unit tests,
  `ENABLE_DEBUG_UI=OFF` build. WASM works with the debug UI compiled in.
- Also fixed the root README's executable name (`sandbox`, not `game`).
