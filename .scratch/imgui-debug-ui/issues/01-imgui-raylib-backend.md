# Dear ImGui on Raylib behind a debug UI interface

Status: ready-for-human
Source: raised during [AI Operator behaviour](../../signal-room/issues/08-ai-operator-behaviour.md), 2026-10-17

## Context

`imgui` is already a vcpkg dependency and links (`CMakeLists.txt`), and `src/engine/platform/platform.hpp` says
Raylib/ImGui calls live behind `Platform`, but nothing connects ImGui to Raylib yet. This is **framework work**:
land it on its own branch off `main`, then rebase `feat/signal-room` onto it.

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

- [ ] rlImGui vendored into the engine and wired into the frame (begin/end around the debug draw, after the scene).
- [ ] A debug UI interface (shaped after the voxtech one) so game code never includes ImGui or Raylib directly.
- [ ] ImGui input capture gates the engine's input: while ImGui wants the mouse/keyboard, the game doesn't see it.
- [ ] A key toggles the debug UI. It can be compiled out (CMake option) for release builds.
- [ ] Desktop and WASM presets both build; WASM either works or is compiled out, documented which.
- [ ] A minimal demo panel (e.g. FPS + a Flecs entity count) proves it end to end.
