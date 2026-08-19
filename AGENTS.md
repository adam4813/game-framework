# AI Agent Instructions for Game Engine

## Project Overview

Prototype for a game using: C++23 / Raylib / imgui (debug/in-game editor) / nlohmann_json / Flecs 4.x ECS / CMake +
VCPKG.

## Architecture

### Data-Driven Design

- Define behavior in JSON config and data tables, not hardcoded branches.
- Fields, buffs, chapters, etc — all in data files parsed at load time. Adding something new should require a JSON entry
  and zero (or minimal) C++ changes.
- When you find yourself writing `if (type == "foo") ... else if (type == "bar")`, the behavior difference probably
  belongs in data.
- Tuning constants live in a shared header as `constexpr`, not scattered through logic.

### ECS Conventions (Flecs 4.x)

- **Always use the `flecs-systems` skill** when creating, modifying, or registering Flecs systems, components, or
  singletons. Flecs 4 differs significantly from Flecs 3 — the skill has the correct API patterns.
- Components are plain data structs — no logic, no virtual methods.
- Systems are stateless functions that query components; use `it.delta_time()` for timing.
- Singletons via `world.get<T>()` / `world.get_mut<T>()` for global state (GameData, AudioAssets, etc.).
- Tags are zero-size structs for filtering, not for carrying data.
- Scene-specific systems use a single `.kind(phase)` for ordering, then `.add<scene::YourSceneTag>()` on the returned
  entity for pipeline filtering. Untagged systems run in every scene pipeline.
- Prefer composition (add/remove components) over inheritance.
- Register systems in dedicated `Register*System()` functions.

### Event-Driven Over Per-Frame Polling

- Use event-driven logic when a state change is triggered by a discrete event rather than a continuous condition.
- Per-frame systems are fine for continuous simulation (movement, projectiles, rendering).

### Platform Abstraction

- All Raylib calls go through a `Platform` interface — never call Raylib directly from game logic.

### Gang of Four Patterns — Use When They Help

- **Strategy / Handler** — for upgrade effects or task types. New variant = new handler + registry entry.
- **Registry / Pool** — centralized ownership, string ID-based lookup
- **Factory** - Level building from data
- Don't over-engineer: a flat `if` is fine for 2–3 cases with no expected growth. Refactor when a third instance
  appears, not before.

### Simplicity Over Complexity

- **Simplicity is the default.** Only introduce complexity (a pattern, an abstraction layer, an extensibility
  mechanism) when it *measurably* pays off: it removes code, or it improves developer/authoring UX by tens of percent —
  not single digits. A registry/strategy for 5 variants with maybe 2 more ever is usually not worth it; a plain
  `if`/`else` dispatch or a small lookup is clearer. Reach for the pattern when the variant count is genuinely open-ended
  or third-party extension is a real requirement.
- **Weigh before abstracting:** how many variants realistically exist, does this reduce total code, and does it make the
  code considerably easier to work with? If the answer is "a handful / not really / marginally," keep it simple.

### Suggestions Are Guidance, Not Mandates

- When the user (or this file) *suggests* an approach, treat it as guidance: evaluate whether it is actually the best fit
  for the situation and say so, proposing the simpler/better option with a short rationale. Do **not** implement a
  suggested design just because it was mentioned if a simpler approach serves better.
- Only treat direction as non-negotiable when the user **explicitly** states it is required (e.g. "you must do this",
  "do it this way"). Otherwise, optimize for the best outcome, not literal compliance.

## Code Style

- C++23 features welcome (`std::erase_if`, structured bindings, `std::expected`, etc.).
- `snake_case` for files, `PascalCase` for types, methods, and functions, `camelCase` for locals and fields.
- One concern per file.
- Keep nesting shallow — early returns over deep if/else trees.
- Tabs not spaces, look at clang-format and editorconfig

## Build

- Configure the desktop build with:
  - `cmake --preset desktop`
  - `cmake --build --preset desktop-release`
- Configure the WASM build with:
  - `cmake --preset wasm`
  - `cmake --build --preset wasm-release`
- To serve the WASM build locally:
  - `cd build/wasm/Release && python -m http.server 8080`
- CMake uses `GLOB_RECURSE` — new `.cpp` files in `src/` are auto-discovered.
- VCPKG manages dependencies (Raylib, nlohmann-json, Flecs).

## Planning

- Use a root `plan.md` file for the implementation plan.
- Keep per-phase detail files in `plan/` and link them from the root plan's Implementation Progress table.

## Git

- Use targeted commits — only stage files associated with the change, `git add <specific-file>` for each file. Verify
  with `git diff --cached --stat`.
- Conventional commits: `feat:`, `fix:`, `refactor:`, `chore:`, `test:`
- Every commit: `Co-authored-by: Copilot <223556219+Copilot@users.noreply.github.com>`

## Debugging and Inspection

### Flecs Remote API

The Flecs Remote API is integrated and runs automatically. Use it to inspect the running ECS state without modifying
code:

- **Explorer:** https://flecs.dev/explorer — auto-connects to `localhost:27750`
- **Script helper:** `tools/flecs-api.js` — Node.js wrapper (no deps) for querying and mutating the running game. Full
  usage documented in the file header.

#### Switching from Title to Game scene

Each scene entity carries a stable `SceneId` identity pair (e.g. `(SceneId, Game)`), so switch by identity **name** — no
need to discover the runtime-assigned entity id:

```bash
# List scenes with their identity name, runtime id, and active flag
node tools/flecs-api.js list-scenes

# Switch active scene by SceneId identity name (stable across launches)
node tools/flecs-api.js switch-scene Game
```

`switch-scene` resolves the identity via the scene's `(SceneId, <name>)` pair, removes the `Active`
tag from the current scene, and adds it to the target — mirrors `engine::scene::ActivateScene`. It waits until the
target is actually active before returning; give the new scene's `Load()` a moment before querying the entities it
creates. A raw `#<id>` is still accepted if you need it.

#### Navigating the UI instead of "cheating"

`switch-scene` forces the active scene directly. Often it is better to drive the **actual UI flow** — clicking the
same buttons a player would — so the scene's own transition logic, teardown, and side effects run exactly as shipped.
This exercises real code paths (e.g. a pause menu's "Quit to Title" `OnClick`) rather than bypassing them:

```bash
# List UI buttons (id, name, label), then fire one's OnClick by name or #id
node tools/flecs-api.js list-buttons
node tools/flecs-api.js click PlayTilemapButton   # or: click "#1006"
```

`click` fires the button's `OnClick` via a `UIClickRequest` (same path as a real click, including script-assigned
handlers). Prefer this UI navigation when validating end-to-end flows; fall back to `switch-scene` when you just need
to jump directly. You can also drive raw input with `override-input` / `restore-input`, and set component values via
the REST API (`PUT /component/<path>?component=<c>&value=<json>`) — e.g. to place an entity on a specific tile.

> **MCP:** `tools/flecs-mcp.js` exposes the same capabilities as MCP tools (`flecs_switch_scene`,
> `flecs_list_scenes`, `flecs_pause`, `flecs_query`, …). When those `flecs_*` tools are available in
> your session, prefer them over shelling out to the CLI; otherwise use the `tools/flecs-api.js` CLI
> above. They are not loaded by default in Copilot CLI — if they're absent and would help, tell the
> **user** they can restart the CLI with `copilot --additional-mcp-config @.github/mcp-config.json`
> (from the repo root). Client setup snippets (Copilot CLI, Claude Desktop, Cursor) live in the
> `tools/flecs-mcp.js` header.

See [Flecs Remote API docs](https://www.flecs.dev/flecs/md_docs_2FlecsRemoteApi.html) for full reference.
