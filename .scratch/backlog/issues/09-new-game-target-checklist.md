# Follow-up: document what a new Game Target needs

Status: needs-triage
Source: retro of Signal Room ticket 01 (game branch `feat/signal-room`), 2026-10-18

## Context

ADR-0006 says a new game lives in `games/<name>/` with its own CMake, `src/` and `tests/`. It doesn't list the
hidden requirements, which ticket 01 found only by grepping:

- **Engine assets at runtime.** The engine reads `assets/shaders/<glsl dir>` (hardcoded in `raylib_backend.cpp`) and
  `assets/config/profiling.json` (optional) from the game's asset directory. Each target must copy them (desktop
  `POST_BUILD`) or preload them (WASM `--preload-file`) next to its own assets.
- **CMake ordering.** `add_subdirectory(games/<name>)` must come after the root's `if (EMSCRIPTEN)` block. Otherwise the
  target misses the directory-scoped `CMAKE_EXECUTABLE_SUFFIX ".html"`.
- **WASM pre-js.** Both `tools/wasm_abort.js` and `tools/wasm_audio_unlock.js`, passed as `SHELL:--pre-js` so CMake
  doesn't de-duplicate them.
- **Scene `Load` outside deferral.** Booting by calling `ActivateScene` from `main` runs `Load` outside a deferred
  context, where an unguarded `defer_suspend()` asserts (the flecs-systems skill has the guard).
- **Headless harness input.** `InputPoll` rewrites `InputState` from the platform every frame, so a harness that
  writes keys must disable it first.

This comes up rarely (once per new game), so the list belongs next to the ADR, not in always-loaded steering.

## What to do

Add a "New Game Target checklist" to ADR-0006's Consequences, or a `docs/new-game-target.md` linked from the ADR.
Use `games/signal-room/CMakeLists.txt` and `games/signal-room/tests/room_harness.cpp` as the worked example. If backlog
06 (shared entry point and WASM link flags) lands first, the checklist shrinks to calling those helpers.
