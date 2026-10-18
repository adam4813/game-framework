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
- **Procedural generation taxonomy types** (biomes, terrain features, history event categories) are almost always
  open-ended — a designer will want to add a new variant without a C++ rebuild. Prefer a string-keyed registry or
  table with per-entry data over a closed `enum class`. A closed enum earns its place only when the full set is fixed
  by an external specification (e.g. a wire protocol). When you see `enum class Biome { ... }` in a procgen module,
  ask: *could a designer add a new variant without touching C++?*

### ECS Conventions (Flecs 4.x)

- **Always use the `flecs-systems` skill** when creating, modifying, or registering Flecs systems, components, or
  singletons. Flecs 4 differs significantly from Flecs 3 — the skill has the correct API patterns.
- Components are generally plain data structs, only use methods for complex datatypes properties (e.g. std::map).
- Systems are stateless functions that query components; use `it.delta_time()` for timing.
- Singletons via `world.get<T>()` / `world.get_mut<T>()` for global state (GameData, AudioAssets, etc.).
- Tags are zero-size structs for filtering, not for carrying data.
- Scene-specific systems use `.add<scene::YourSceneTag>()` on the returned entity for pipeline filtering.
- Prefer composition (add/remove components) over inheritance.
- **Flecs template/parameter parity:** If you include a component or singleton type in the system template arguments
  (e.g., `world.system<const MyComponent>()` or `.term_at<MySingleton>().singleton()`), you MUST include it as a
  parameter in your `.each()` or `.run()` callback. The parameter list must match the template list exactly. For
  example:
  ```cpp
  world.system<const InputState>("MySystem")
    .term_at<InputState>()
    .singleton()
    .each([](const flecs::iter&, size_t, const InputState& input) { ... });  // ✓ correct
    // .each([](const flecs::iter&, size_t) { ... });  // ✗ wrong — InputState in template but not in params

  // OR Spin through the iterator to ensure ECS systems are aware of the iteration
  while (it.next());
  ```
- **Singleton access inside `.each()`:** use `it.world().get<T>()` inside the lambda body. Adding T as a template
  argument forces it into params and causes a compile error when combined with `.term_at<T>().singleton()` on some
  system shapes. The safe pattern:
  ```cpp
  world.system<MyComponent>("MySystem")
      .kind(flecs::OnUpdate)
      .each([](const flecs::iter& it, size_t, MyComponent& c) {
          const auto& singleton = it.world().get<MySingleton>(); // ✓ correct
      });
  ```
- **Component pre-registration in multi-stage worlds:** Any component type first reached inside a system via
  `try_get<T>()`, `has<T>()`, `entity.add<T>()` etc. will be lazily registered by Flecs — this calls
  `ecs_new_low_id` which **crashes** (`INVALID_WHILE_READONLY`) when the world is readonly and the physics module
  has set `stage_count > 1`. Pre-register every farm/game component type in a non-system context (e.g., module
  constructor or `Load()`) via `world.component<T>()` before systems run.
- **`Load()` initialization order:** In a scene `Load()`, registries and system pre-registration must come *before*
  the setup calls that trigger `OnSet` observers reading them. Example: `LoadObjectKindRegistry` must precede
  `SetupTilemap` because the `OnSet<WorldObjectLayer>` observer reads `ObjectKindRegistry` synchronously;
  `RegisterXxxSystems` must precede `SpawnXxx` so `world.component<T>()` calls genuinely pre-register types before
  entities create them.

### Transform Authoring

- **Author only `Transform`** (local position/rotation/scale) — never hand-write `WorldTransform`. The keeps it in sync
  via `TransformPropagation`  or the physics backend.

### Entity Relationships and Linkage

- Use first-class relationships (e.g., `RenderWith`, `LookAt`) to decouple entity structure from hardcoded
  dependencies.
- In JSON levels, declare relationships with `"link"` (for parent→child) and `"refs"` (for named-entity
  cross-references). The level loader resolves them in a second pass.
- Register relationship loaders in your module's constructor via `RegisterLink` and `RegisterRef`.
- Example: a camera with a target is authored as `"refs": { "look_at": "PlayerEntity" }`, not as a stored position
  field. The camera's aim is derived from the target's `WorldTransform` on-demand each frame.

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
  mechanism) when it *measurably* pays off: it removes code, or it improves developer/authoring UX by tens of
  percent. Reach for the pattern when the variant count is genuinely open-ended or third-party extension is best.
- **Weigh before abstracting:** how many variants realistically exist, does this reduce total code, and does it make the
  code considerably easier to work with? If the answer is "a handful / not really / marginally," keep it simple.

### Suggestions Are Guidance, Not Mandates

- When the user (or this file) *suggests* an approach, treat it as guidance: evaluate whether it is actually the best
  fit for the situation and say so, proposing the simpler/better option with a short rationale.
- Only treat direction as non-negotiable when the user **explicitly** states it is required (e.g. "you must do this",
  "do it this way"). Otherwise, optimize for the best outcome, not literal compliance.

## Code Style

- C++23 features welcome (`std::erase_if`, structured bindings, `std::expected`, designated initializers, etc.).
- `snake_case` for files, `PascalCase` for types, methods, and functions, `camelCase` for locals and fields.
- One concern per file.
- Keep nesting shallow — early returns over deep if/else trees.
- Tabs not spaces, look at clang-format and editorconfig
- Single line statements only if the expression is a control statement, a return, increment/decrement, or a simple
  assignment. Otherwise, use braces and newlines.

## Build

- Configure the desktop build with:
  - `cmake --preset desktop`
  - `cmake --build --preset desktop-release`
- Configure the WASM build with:
  - `cmake --preset wasm`
  - `cmake --build --preset wasm-release`
- CMake uses `GLOB_RECURSE` — new `.cpp` files in `src/` are auto-discovered.
- VCPKG manages dependencies (Raylib, nlohmann-json, Flecs).

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

## Interaction style

- **Use the `ask_user` tool** for all questions during any skill session (grilling, planning, clarification, etc.) —
  never ask via plain text. Bundle a full question round into a single `ask_user` call (one form, multiple fields); one
  round = one tool call = one user response.
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

Single-context: `docs/GLOSSARY.md` and `docs/adr/`. See `docs/agents/domain.md`.
