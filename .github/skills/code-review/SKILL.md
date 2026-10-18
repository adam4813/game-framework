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

## The Rules

Apply **every rule in `CODING_STANDARDS.md`** (repo root) to the diff; it is the single source of this repo's
conventions. A breach is a concrete finding: cite the rule's heading and the `file:line`, and rank it above generic
advice. Flecs API gotchas behind a crash or assert are in the `flecs-systems` skill.

---

## Sub-Agent Execution

When the change is large enough to delegate (Phase 2), **use only Claude `code-review` sub-agents**, one
per independent area, launched in parallel. Give each **complete, self-contained context** — sub-agents are
stateless:

1. **Context to include**: the engine stack, the path to `CODING_STANDARDS.md`, the exact files/diff
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
2. **When fixing**, make minimal, surgical changes; re-apply `CODING_STANDARDS.md` to the fix.
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
- **Ground every finding in `CODING_STANDARDS.md`** rather than generic best practices.
