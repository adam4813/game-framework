# Follow-up: share the Game Target entry point and WASM link flags

Status: needs-triage
Source: code review of Signal Room ticket 01 (game branch `feat/signal-room`), 2026-10-18

## Context

`games/signal-room/src/main.cpp` repeats the sandbox `src/main.cpp` bootstrap: the spdlog console+file sinks, the
Flecs log handler, the platform/EngineContext/remote-API setup, the emscripten `tickWrapper` and the desktop main
loop. `games/signal-room/CMakeLists.txt` also re-lists the sandbox's WASM link flags (memory sizes, GLFW, pre-js
scripts, Debug/Release options) with the memory sizes hardcoded.

ADR-0006 says a module shared by two Game Targets is promoted to `src/engine/` rather than duplicated. With two
targets the copies will drift.

## What to do

- Promote the bootstrap to an engine helper (e.g. `engine::RunApp(title, size, onWorldReady)`) that both `main`s call.
- Move the WASM link options into a CMake function (e.g. `engine_wasm_link_options(target ASSETS ...)`) that takes the
  preload list, and use it for the sandbox and Signal Room.
- Rebuild both presets and re-run the runtime checks for both executables.
