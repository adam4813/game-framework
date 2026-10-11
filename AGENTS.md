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

## Debugging and Inspection

### Flecs Remote API

Use the **`flecs-remote-api` skill** to inspect and control a running Flecs game without modifying code. The skill
guides you through scene switching, UI navigation, input overrides, component queries, and other debugging/introspection
tasks.
