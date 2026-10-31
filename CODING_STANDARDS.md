# Coding Standards

The rules code in this repo follows. Read it before writing code and apply every rule when reviewing a diff. Each rule is
citable by its heading. Flecs API details and crash gotchas live in the `flecs-systems` skill; load it before touching
Flecs code. Decisions with history behind them are in `docs/adr/`.

## Data-driven design

- **Behaviour lives in data.** Fields, buffs, chapters, Shifts and kinds are JSON entries parsed at load time. A new
  variant is a JSON entry and zero (or minimal) C++. An `if (type == "foo") … else if (type == "bar")` chain means the
  difference belongs in data or a handler registry.
- **Open-ended taxonomies are string-keyed registries.** Procedural-generation types (biomes, terrain features, history
  events), Station/prop/Item kinds and the like get a string-keyed registry with per-entry data, so a designer adds one
  without a rebuild. A closed `enum class` fits only a set fixed by the code or an external specification (a wire
  protocol, the Actions the game's own code reads). Ask of every enum: could a designer add a variant without C++?
- **Enums cross data boundaries as strings.** Loaders read the enum's name (`"projection": "orthographic"`); a reflected
  enum registers its names with `.constant()` so the Explorer and REST API show them, and keeps the default `int`
  underlying type.
- **Tuning constants are `constexpr` in a shared header** (`constants.hpp`, a game's `*_constants.hpp`) or in a data
  file when designers tune them. Implementation constants that only one algorithm uses (iteration counts, epsilons)
  sit at the top of that `.cpp`'s anonymous namespace. Repeating a magic number across files means it wants a header.
- **JSON goes through the shared helpers.** Engine code uses `engine::core` (`LoadJsonFile`, the `J*` field parsers);
  a game routes every JSON load through one reader of its own. Load errors name the file and the bad entry.

## Simplicity and patterns

- **Simplicity is the default.** Add a pattern, abstraction layer or extension point only when it measurably pays: it
  removes code, or improves developer or authoring UX by tens of percent. Weigh it: how many variants realistically
  exist, does it shrink the total code, is the result considerably easier to work with?
- **Patterns earn their place at the third variant.** A flat `if` covers 2–3 stable cases. Reach for Strategy /
  Handler (new variant = handler + registry entry), Registry / Pool (central ownership, string-id lookup) or Factory
  (building levels from data) when a third instance appears or third-party extension is the point.
- **Abstractions have a second user.** A hook, parameter or base class with one user is premature: inline it.
- **Duplication of intention gets unified.** Two pieces of code solving the same problem (the same integration loop,
  the same query, the same algorithm) share one function, component, handler registry or data field.
- **Bespoke special cases get generalized.** Hard-coded entity or type names, asset paths, or one feature
  special-cased where the behaviour applies broadly move to data, a registry, shared components or a factory.
- **One convention per concern.** Error handling, events vs callbacks and similar choices stay consistent within a
  subsystem; a system doing many jobs is split into focused systems.

## ECS (Flecs 4.x)

### Components, tags, singletons

- **Components are plain-data structs**: no virtual methods, no logic. A method earns its place only by doing real
  work on a non-trivial member (a container), by computing a predicate, or as a factory (`WorldTransform::ComputeMatrix`,
  `Cooldown::Ready`). Prefer composition (add/remove components) over inheritance.
- **Tags are zero-size structs** used only for filtering, never for carrying data.
- **Singletons hold global state** via `world.set<T>({})` / `world.get<T>()` / `world.get_mut<T>()`, kept as
  aggregates so `set<T>({})` works. No ad-hoc mutable globals.
- **Randomness is deterministic**: draw from `world.get_mut<ecs::RngState>()`, never `rand()` or a private generator.

### Registration and reflection

- **Every component is pre-registered** with `world.component<T>()` in the module constructor (or before anything can
  reach it), tags and event-only types included. A type first reached inside a system registers lazily and asserts
  (`INVALID_WHILE_READONLY`) in a multi-stage world.
- **Reflect, then expose to scripts**: `.member<…>()` for the Explorer, then `RegisterComponentForScripts`. Use
  sequential `.member<T>("name")` when every field from first to last is listed in order; use pointer-to-member
  `.member("name", &T::field)` as soon as any field in between is skipped.
- **Components the Explorer should show are reflected**, including the gameplay state a developer will inspect
  (positions, velocities, timers, relationships).

### Systems

- **Systems are stateless** and time themselves with `it.delta_time()`, never wall-clock time.
- **`.each()` for per-entity work, `.run()` for global or singleton work.** Names are unique `PascalCase`, registered in
  a `Register*System()` function or the module constructor.
- **One `.kind(phase)` per system.** A second `.kind()` silently replaces the first. Phases: `PreUpdate` input,
  `OnUpdate` logic/physics/AI/debug panels, `OnStore` 3D render, `OnUI` 2D overlays, `PostUpdate` per-frame cleanup.
- **Scene systems are scene-scoped**, in one of two ways: `.child_of(sceneRoot)` for a system that a scene's `Load()`
  creates under the root entity `Unload()` destroys (it runs in that scene only), or a mode tag
  (`.add<scene::GameScene>()` / `.add<scene::MenuScene>()`) for a system registered once in a module constructor (it
  runs in every gameplay or every menu scene, since scene pipelines filter on these). A scene's identity tag
  (`ArenaSceneTag`) belongs on the scene entity only: no pipeline filters on it, and `FindScene` can return a system
  that carries it. An unscoped system runs in every scene.
- **Pausing is data** (ADR 0004): simulation systems add `.add<ecs::Pausable>()`; input, render, UI, audio and debug
  systems omit it.
- **Event-driven over polling.** A discrete state change reacts through an observer (`OnSet` for values, `OnAdd` /
  `OnRemove` for structure) or an event; per-frame systems are for continuous work (movement, projectiles, rendering).
- **An observer that reads component data reacts to `OnSet`, never `OnAdd`** (ADR 0007). Outside a deferred merge,
  `set<T>(v)` fires `OnAdd` with `T` default-constructed, before `v` is copied in. It must also tolerate any component
  order: level JSON applies components alphabetically, so a sibling component it reads may not exist yet.
- **Per-frame systems stay cheap**: no needless allocation, no redundant queries, no `O(n²)` scans in the hot path.
- **`modified<T>()` on an engine component fires its module's `OnSet` observer**, which may rebuild meshes or reset
  streaming state. Mutate engine data in place without `modified<>()` unless the rebuild is the point.
- **`it.world()` is a stage inside a system.** The C++ wrappers resolve it, but raw C APIs that read world storage
  (`ecs_get_entities`, ...) read garbage from a stage and abort on a Flecs assert. Pass them the real world:
  `ecs_get_world(it.world().c_ptr())`.

### Scenes

- **`Load()` registers before it spawns.** Registries, system registration and component registration come before the
  setup calls whose `OnSet` observers read them (`LoadObjectKindRegistry` before `SetupTilemap`). Spawn order is free
  once the module constructor has pre-registered every component.
- **`Load()` runs undeferred** (ADR 0007): the scene module suspends deferral around it, so a `Load()` can look up and
  `get_mut<>()` what it just created. A scene never calls `defer_suspend()` itself; a second suspend re-enables
  deferral. Guard per-frame `get_mut<>()` in `Tick()` with `has<>()`.
- **`Unload()` undoes `Load()`.** Destroy the scene's root entities; hold no `flecs::entity` handles across an
  Unload/Load cycle. A scene can activate more than once.
- **Scenes are switched by identity** (`SceneId`, name), never a runtime entity id.

### Transforms and relationships

- **Author `Transform`, never `WorldTransform`** (ADR 0005). `TransformPropagation` or the physics backend computes
  `WorldTransform`. Each `set<Transform>` seeds it from the parent's current `WorldTransform`, so during a scene `Load()`
  it is only as fresh as the parent was when the child's `Transform` was set; load-time code reads `Transform`.
  Setting a rigid body's `Transform` teleports its body.
- **Relationships, not stored copies.** Link entities with first-class relationships (`RenderWith`, `LookAt`) and
  derive dependent values each frame from the target: a camera with a target is authored as
  `"refs": {"look_at": "PlayerEntity"}`, and its aim comes from the target's `WorldTransform`, not a stored position.
- **Level JSON declares links with `"link"`** (parent→child) **and `"refs"`** (named cross-references), resolved in a
  second pass; a module registers their loaders with `RegisterLink` / `RegisterRef` in its constructor.

## Platform and dependencies

- **Every Raylib (and ImGui) call goes through `platform::Platform`** (ADR 0001). Game and engine logic use its
  interface and POD types (`Rect`, `Rgba`, glm).
- **Game logic reads input through the engine**: `InputState`, and in a game its action layer (key bindings
  mapped to game actions), never raw key codes in logic.
- **Includes point one way**: engine code never includes game code; game code includes `engine/engine.hpp`.

## Code style

Formatting (tabs, braces, line length) is `.clang-format` and `.editorconfig`; run the formatter rather than reviewing
it by eye.

- **C++23 is welcome**: `std::expected`, `std::erase_if`, ranges, structured bindings, designated initializers.
- **Names**: `snake_case` files, `PascalCase` types/functions/methods, `camelCase` locals and fields; constants
  follow the surrounding file. Identifiers use the glossary's words (`docs/GLOSSARY.md`, and a game's own `GLOSSARY.md`), never the
  synonyms it lists under _Avoid_.
- **One concern per file.** A file edited for two unrelated reasons wants splitting.
- **Shallow nesting**: early returns over deep `if`/`else` trees.
- **Single-line bodies only for** a control statement, a return, an increment/decrement or a simple assignment;
  everything else gets braces and newlines.
- **Comments say why and state contracts**: a public function or struct documents what it promises and its
  preconditions; a non-obvious algorithm is explained.

## Docs stay in sync

- **A change updates the docs it touches in the same commit**: the module's `README.md`, `docs/*.md`, JSON schema
  notes, and comments describing old behaviour. A stale doc is a defect, not a nitpick.
- **New terms go in the glossary** rather than new synonyms.

## Tests

- **Tests check behaviour a player, designer or caller would notice**, through a public seam (a harness, a module's
  API), with behaviour-named `TEST`s. Expected values come from the spec or a worked example, never recomputed the
  way the code computes them.
- **A new engine component appears in the scene-equivalence test** (`tests/scene_equivalence/`, see the
  `create-module` skill) and `COMPONENT_COVERAGE.md`.
- **Engine tests register with `add_engine_test`**, a Game Target's with its own helper (`add_<game>_test`), so
  `ctest --preset desktop-test-release` runs them all.
