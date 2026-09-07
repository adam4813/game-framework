---
name: code-review
description: Conduct a rigorous code review for this Flecs/Raylib C++23 game engine — analyzing correctness, architecture, patterns, duplication, generalization, and documentation sync. Use when asked to review code changes, refactor systems, audit subsystems, or evaluate new engine features.
---

# Code Review Skill

## When to Use This Skill

Activate this skill when the user asks to:

- Review code changes, a pull request, a branch, or staged/unstaged edits
- Audit an existing subsystem or module
- Evaluate a new feature or architectural pattern
- Refactor to remove duplication or improve generalization
- Check documentation sync with implementation

**Do not** activate for security-only reviews (use a security-review specialist) or for a pure "does this
build/pass tests" question (just run the build/tests).

## Review Scope & Mindset

You are a **principal-level game engine architect** reviewing a small, data-driven **C++23 / Flecs 4.x /
Raylib / Jolt / AngelScript / nlohmann_json** engine. Your job is to find issues that compromise:

- **Correctness & safety**: bugs, memory/lifetime errors, UB, broken invariants — the top priority.
- **Architectural clarity**: confused responsibilities, tangled dependencies, wrong data flow.
- **Maintainability**: duplication, poor abstraction, rigid coupling.
- **Extensibility**: bespoke logic that should be data-driven or a pluggable handler.
- **Performance**: needless allocations/queries in hot paths, cache-unfriendly layouts.
- **Documentation sync**: stale comments/docs, undocumented invariants, API contracts that lie.

**Be prescriptive** on correctness, safety, and architecture. **Lean toward guidelines** on subjective
matters (naming, spacing, micro-style). Report **only high-confidence issues** — a review full of
low-confidence nitpicks buries the findings that matter. When unsure, investigate before reporting.

---

## Review Workflow

### Phase 1: Establish the change set & scope

Before reviewing, **bound the review to a concrete diff** — never review from memory. Determine what
changed:

```bash
git status --short                      # what's dirty
git --no-pager diff                     # unstaged changes
git --no-pager diff --cached            # staged changes
git --no-pager diff main...HEAD --stat  # a feature branch vs main (adjust base)
git --no-pager log --oneline -10        # recent history for context
```

Then:

1. **Read the actual diff**, not just file names. Understand the intended behavior and why.
2. **Identify the boundary** — new feature, refactor, or subsystem audit? Only pre-existing bugs *tightly
   coupled* to the change are in scope; note unrelated ones briefly but don't chase them.
3. **Locate related code** — find similar systems, existing abstractions, and the module's `README.md`
   (each `src/engine/<module>/` has one) so you compare against established patterns rather than inventing
   new ones.

### Phase 2: Investigation — self vs. sub-agent

- **Small/medium change (a few files, one subsystem): review it yourself.** Read the diff and the
  neighboring code directly. Delegating a small review adds latency and loses context.
- **Large or cross-cutting change (many files, multiple subsystems, or a whole-subsystem audit): launch a
  Claude `code-review` sub-agent** per independent area, in parallel, with **complete context** (see
  [Sub-Agent Execution](#sub-agent-execution)). Use **only Claude models** for these sub-agents.

Whichever path, the investigation must: trace data flow and lifetimes, compare against similar
implementations, check docs against code, and gather **high-confidence** findings with `file:line`
citations and a root-cause explanation.

### Phase 3: Synthesis & recommendations

Consolidate into severity order: **Critical** (must fix) → **High** (strong recommend) → **Medium**
(consider) → **Low** (optional). Lead with the single most important finding.

---

## Engine-Specific Review Checklist

These encode *this* repo's conventions (see `AGENTS.md`, `docs/`, and each module `README.md`). Violations
here are concrete, citable findings — prioritize them over generic advice.

### Flecs 4.x systems & ECS

- [ ] **Components are plain data** — no methods with logic, no virtual functions; tags are zero-size structs.
- [ ] **Systems are stateless** — query components; use `it.delta_time()` for timing, never wall-clock.
- [ ] **`.kind(phase)` is called at most once per system.** Calling `.kind()` twice is a bug: the **last
      call wins** and silently removes the prior phase's `DependsOn` + tag. Flag any double `.kind()`.
- [ ] **Phase vs. scene filter are separate concerns** — order with one `.kind(phase)`, then
      `.add<scene::YourSceneTag>()` for pipeline filtering. **An untagged system runs in *every* scene** —
      flag systems that should be scene-scoped but lack a scene tag.
- [ ] **Pausing is opt-in data** — simulation systems add `.add<engine::ecs::Pausable>()`; input, render,
      UI, audio, and scene tick/input/UI systems must **omit** it. Flag a new simulation system missing
      `Pausable`, or a UI/input system that wrongly carries it.
- [ ] **Event-driven vs. per-frame** — discrete state changes should use observers/events, not per-frame
      polling; continuous simulation (movement, tweens, rendering) stays per-frame.
- [ ] **Singletons** via `world.get<T>()` / `world.get_mut<T>()`; no ad-hoc mutable globals.
- [ ] **Determinism** — randomness pulls from `world.get_mut<ecs::RngState>()` (splitmix64), never `rand()`
      or a private generator, so seeded runs reproduce.

### Scene lifecycle (see `docs/scenes.md`)

- [ ] **Deferred-write hazard**: `Load()` runs inside the activation observer, so writes are deferred. Do
      **not** `get_mut<>()` a component you just `set<>()` in the same `Load()`. Guard per-frame
      `get_mut<>()` in `Tick()` with `has<>()`. Flag violations — Flecs will assert at runtime.
- [ ] **Cleanup contract**: `Unload()` must undo everything `Load()` created (destroy root entities); no
      stale `flecs::entity` handles held across an Unload/Load cycle. A scene can activate more than once.
- [ ] **Scene identity** is the `(SceneId, <name>)` pair — switch by identity, never a hardcoded runtime
      entity id.

### Data-driven & platform

- [ ] **New behavior belongs in data** — JSON entries / data tables, not `if (type == "foo") … else if`.
      A `if`/`else` for 2–3 stable cases is fine; a growing chain or a 3rd variant signals a missing
      strategy/handler registry or data field.
- [ ] **Tuning constants** are `constexpr` in a shared header, not scattered magic numbers.
- [ ] **JSON loading** goes through `engine::core` helpers (`LoadJsonFile`, the `J*` field parsers) rather
      than re-implementing open/parse/try-catch per loader.
- [ ] **Platform abstraction** — game/engine logic calls the `platform::Platform` interface and its POD
      types (`Rect`, `Rgba`, glm), never Raylib/ImGui directly.
- [ ] **Include direction is one-way** — engine code never `#include`s game code; game code uses
      `#include "engine/engine.hpp"`.

### Correctness, performance, docs, style

- [ ] No leaks, dangling handles, unsafe indexing, uninitialized state, or UB; error paths handled.
- [ ] Preconditions/invariants documented and asserted or checked.
- [ ] No needless allocations or `O(n²)`/redundant queries in per-frame systems.
- [ ] Public functions/structs document their contract; complex algorithms explained; **docs and module
      `README.md` updated to match the change** (stale docs are a High finding, not a nitpick).
- [ ] Style: `snake_case` files, `PascalCase` types/methods/functions, `camelCase` locals/fields, **tabs**
      (see `.clang-format`/`.editorconfig`), one concern per file, shallow nesting via early returns.

---

## Cross-Cutting Patterns to Flag

### Duplication of intention

Two+ pieces of code solving the same problem, sharing near-identical logic, or duplicating a component
query/algorithm. **Action**: unify under a shared function, a strategy/handler registry, or a data field.

```cpp
// Bespoke: PhysicsSystem does t.position += v.velocity * dt
// Bespoke: ParticleSystem does t.position += d.direction * dt
// DUPLICATION → a shared Motion component + one integration system.
```

### Bespoke vs. generalized

Flag hard-coded entity/type names or asset paths, special-casing one feature that should apply broadly, or
a monolithic handler that should be pluggable. **Action**: move to data-driven config, a `string-id →
handler` registry (new behavior = new handler + entry, no core change), shared components, or a factory.
**But respect the simplicity rule** — don't demand a registry for 2 variants with no expected growth;
3+ variants (or genuine third-party extension) is the threshold.

### Complexity / abstraction smells

- **Complexity creep**: a once-simple feature now has 5 special cases → a better abstraction is overdue.
- **Premature generalization**: an abstraction/extensibility hook that will never have a second user →
  simplify. A base class with one subclass is a smell.
- **Inconsistent conventions**: one system posts events, another uses callbacks; mixed error strategies
  (some throw, some return `nullptr`, some log-and-continue) — flag the inconsistency.
- **God systems**: one system doing many jobs → decompose into focused systems/components.

### Documentation sync

Comments describing old behavior; API docs promising parameters that don't exist or omitting
preconditions; a module `README.md`/`docs/*.md` describing a pattern the code abandoned; JSON schema docs
that don't match the parser. **Action**: fix whichever (code or doc) is wrong; flag stale docs explicitly.

---

## Sub-Agent Execution

When the change is large enough to delegate (Phase 2), **use only Claude `code-review` sub-agents**, one
per independent area, launched in parallel. Give each **complete, self-contained context** — sub-agents are
stateless:

1. **Context to include**: the engine stack and conventions (this file's checklist), the exact files/diff
   in scope, the neighboring patterns to compare against, and the specific concerns to investigate.
2. **Instruct the agent to**:
   - "Use the `code-review` agent type."
   - "Investigate the change yourself and report findings — do not just advise."
   - "Search for patterns similar to X; report duplicates/inconsistencies with `file:line`."
   - "Check whether subsystem Y's `README.md`/docs match the code."
   - "Report **only high-confidence** issues: severity, root cause, `file:line`, and a concrete fix."
3. **Consolidate** the sub-agents' findings into the single output table below; de-duplicate overlaps.

---

## Output Format

### Summary

One short paragraph: review scope (what diff/subsystem), finding counts by severity, and the single top
concern.

### Findings Table

Cite `file:line`. Include a **Confidence** (1–10); report findings at confidence ≥ 6 (raise the bar for
Low-severity items). Use the severity emojis for quick scanning.

| # | Severity     | Confidence | File:Line                 | Category    | Issue                                             | Recommendation              |
|---|--------------|-----------|---------------------------|-------------|---------------------------------------------------|-----------------------------|
| 1 | 🔴 Critical  | 9/10      | src/game/foo_system.cpp:45 | Correctness | `get_mut<Bar>()` in `Load()` after `set` — deferred-write assert | Pass Bar via the `set<>()` factory call |
| 2 | 🟠 High      | 8/10      | src/engine/x/y.hpp:120     | Duplication | Two identical update loops in systems X and Y     | Unify into a shared component/system |
| 3 | 🟡 Medium    | 7/10      | src/engine/scene/mgr.cpp   | Docs        | Lifecycle docs don't match `Load`/`Unload` order  | Update `docs/scenes.md`     |
| 4 | ⚪ Low       | 6/10      | src/game/hud.cpp:12        | Style       | Spaces instead of tabs                            | Reformat per `.editorconfig`|

Severity legend: 🔴 Critical · 🟠 High · 🟡 Medium · ⚪ Low.

### Grouped guidance (optional)

If one issue recurs (e.g. "6 hardcoded strings"), group and summarize with a single recommendation rather
than repeating rows.

---

## Verification & Follow-Through

A review isn't done at the table. After presenting findings:

1. **Offer to act** — ask whether to fix Critical/High findings now, fix all, or just record them.
2. **When fixing**, make minimal, surgical changes; re-run the review checklist on the fix.
3. **Verify** the code still builds and behaves: `cmake --preset desktop && cmake --build --preset
   desktop-release` (and the WASM preset if the change could affect it). For runtime behavior of a live
   system, the Flecs Remote API / `tools/flecs-api.js` can inspect ECS state without code changes.
4. **Confirm docs** touched by the change are updated in the same pass.

---

## Tips

- **Severity ≠ style.** Never conflate a formatting nit with an architectural defect; be explicit about
  *must-fix* vs. *consider*.
- **Cite sources.** Every finding needs `file:line`; a snippet beats a vague description.
- **Explain the "why".** Not "this is bad" but "this will assert at runtime when the scene re-activates
  because …".
- **Suggest, don't dictate** — except for correctness/safety, which are non-negotiable.
- **Patterns over one-offs.** One magic number is a slip; five signal a missing constants header.
- **Ground every finding in the repo's own conventions** above rather than generic best practices.
