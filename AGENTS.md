# AI Agent Instructions for Game Engine

## Project Overview

Prototype for a game using: C++23 / Raylib / imgui (debug/in-game editor) / nlohmann_json / Flecs 4.x ECS / CMake +
VCPKG.

Working examples of engine features (scenes, level JSON, particles, nav graphs, overlays, panels): `games/sandbox/src/game/README.md`.

## Coding standards

- **Read `CODING_STANDARDS.md` before writing code, and apply every rule in it when reviewing.** It holds the
  data-driven, ECS, scene, platform, style, docs and test rules.
- **Load the `flecs-systems` skill before writing or changing any Flecs code** (systems, components, singletons,
  observers, reflection, scene `Load`/`Unload`) and before debugging a Flecs assert. It holds the Flecs 4 API patterns
  and the gotchas that have crashed this project.

## Build

- Configure the desktop build with:
  - `cmake --preset desktop`
  - `cmake --build --preset desktop-release`
- Configure the WASM build with:
  - `cmake --preset wasm`
  - `cmake --build --preset wasm-release`
- On the Intel Mac, use the `macos-intel`, `macos-intel-test` presets (and their `-release`/`-debug` build and test
  presets) in place of `desktop`/`desktop-test`. They carry the toolchain environment (README, "macOS Intel").
  Emscripten isn't installed there, so the `wasm` preset is checked by CI on the pull request.
- CMake uses `GLOB_RECURSE` — new `.cpp` files in `src/` are auto-discovered.
- VCPKG manages dependencies (Raylib, nlohmann-json, Flecs).

- **Test executables live under `build/desktop-test/`**, built by `cmake --build --preset desktop-test-release`. The
  `desktop` presets build the games only, so a test exe path under `build/desktop/` doesn't exist (exit 127).

- **No guardrail runs for you.** There is no pre-commit hook, and CI (`.github/workflows/build.yml`) only runs for
  `main`, so feature and game branches get no CI. Before calling work done, build the
  `desktop` and `wasm` presets and run `ctest --preset desktop-test-release` yourself.

- **Scene equivalence output is committed.** After changing an authored component, run from the repo root
  `./build/desktop-test/tests/scene_equivalence/Release/scene_equivalence_test.exe scene_test_output` (`ctest` writes
  into the build directory instead) and commit the changed `scene_test_output/` files. See README, "Scene Equivalence
  Test".

## vcpkg baseline

`vcpkg.json` `builtin-baseline` is the single source of truth; CI reads it. When changing the vcpkg commit:

1. Check out the target tag in `$VCPKG_ROOT` and re-run `bootstrap-vcpkg`.
2. Set `builtin-baseline` in `vcpkg.json`.
3. Find every other copy of the old commit (`git grep <old-sha>`) and update it. Expect `README.md`; the workflows and
   the `setup-environment` action should hold none, since they default to `vcpkg.json`. A copy there is an
   intentional override (e.g. a game needing a different commit), so keep it and say why in a comment.
4. Rebuild and run `ctest --preset desktop-test-release`.

A bump can move other ports (flecs 4.1.0 → 4.1.6 dropped `singleton()`), so keep `overrides` in `vcpkg.json` for
anything the engine isn't migrated to yet. Docs under `docs/research/` cite historical baselines on purpose; leave them.

## Git

- Use targeted commits — only stage files associated with the change, `git add <specific-file>` for each file.
- Conventional commits.
- **Always work on a branch** — never commit directly to `main`. Open a pull request to merge.
- **Post-merge verification:** After merging an integration branch into your feature branch, rebuild and run
  `ctest --preset desktop-test-release`. The test count must equal or exceed the pre-merge baseline — a drop
  means conflict resolution silently discarded tests or source files.
- **Multi-ticket worktrees:** Work only in your assigned worktree, not the main repo checkout. Never merge your
  branch into the integration branch yourself — the coordinator does that. Verify before starting:
  ```powershell
  git rev-parse --show-toplevel   # must end with your worktree name, not the main repo
  git rev-parse --abbrev-ref HEAD  # must equal your feature branch
  ```

## Debugging and Inspection

### Flecs Remote API

**After implementing any new playable scene or gameplay loop, launch the game and verify it via
the Flecs Remote API before declaring the work done.** Build success and passing unit tests are
insufficient — crashes, wrong item IDs, broken interactions, and scale problems only appear at
runtime. Use the `flecs_*` MCP tools (preferred when available) or `tools/flecs-api.js`:

```bash
# Switch to the new scene and confirm it loads
node tools/flecs-api.js switch-scene <SceneName>

# Walk the player around; confirm the core loop works
# Query entity state to verify component values
node tools/flecs-api.js query <ComponentType>
```

For all other debugging and inspection tasks, use the **`flecs-remote-api` skill** — it covers
scene switching, UI navigation, input overrides, component queries, and ECS introspection.

For screenshots (desktop and browser), logs, the headless test harness, and finding a component's full path, see
`docs/agents/debugging-toolbelt.md`. **A native crash with no log line: read its "Crash tripwire" first.**

## Interaction style

- **Suggestions are guidance, not mandates.** When the user (or a doc) *suggests* an approach, judge whether it is the
  best fit and say so, proposing the simpler or better option with a short rationale. Treat direction as
  non-negotiable only when the user **explicitly** requires it ("you must", "do it this way").
- **Ask through your harness's structured question tool** (the form-style tool the user answers in place) for all
  questions during any skill session (grilling, planning, clarification, etc.). Bundle a full question round into a
  single call (one form, multiple fields); one round = one tool call = one user response.
- **Scope expansion during active ticket work:** if a request clearly falls outside the current ticket's acceptance
  criteria, name it. Implement it if it's small and the user explicitly accepts the scope change; otherwise note it as
  a follow-up ticket before starting work on it.

## Agent skills

### Issue tracker

Issues live as local markdown files under `.scratch/`. See `docs/agents/issue-tracker.md`.

### Triage labels

Default five-role vocabulary (`needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`). See
`docs/agents/triage-labels.md`.

### Domain docs

Multi-context: `GLOSSARY-MAP.md` lists each context's glossary and ADRs. See `docs/agents/domain.md`.
