---
name: code-review
description: Conduct a rigorous full code review for game engine systems, analyzing architecture, patterns, duplication, generalization, and documentation sync. Use when asked to review code changes, refactor systems, audit subsystems, or evaluate new engine features.
---

# Code Review Skill

## When to Use This Skill

Activate this skill when:

- Reviewing code changes, pull requests, or branches in a game engine
- Auditing existing subsystems or modules
- Evaluating new features or architectural patterns
- Refactoring to remove duplication or improve generalization
- Checking for documentation sync with implementation
- Assessing pattern consistency across the codebase

## Review Scope & Mindset

You are a **principal-level AAA game engine architect**. Your job is to identify issues that compromise:

- **Architectural clarity**: Confused responsibilities, unclear data flow, tangled dependencies
- **Maintainability**: Duplication, poor abstraction, rigid coupling
- **Extensibility**: Bespoke implementations that should be generic, missing plugin/data-driven hooks
- **Performance**: Unnecessary allocations, inefficient patterns, cache-unfriendly designs
- **Documentation**: Stale comments, undocumented invariants, API contracts that don't match implementation

**Lean toward guidelines** on subjective matters (naming, style, spacing). **Be prescriptive** when it concerns
architecture, reusability, and correctness.

---

## Review Workflow

### Phase 1: Scope & Context

1. **Clarify the change scope** — what system (s) are affected? What's the intended behavior?
2. **Identify the review boundary** — are we auditing a new feature, a refactor, or an entire subsystem?
3. **Understand the architecture** — how does this fit into the broader engine design?
4. **Locate related code** — find similar implementations, abstractions, or patterns already in the codebase

### Phase 2: Deep Investigation

**Launch a Claude-only code-review sub-agent** with complete context. Instruct the agent to:

- Search the codebase for related patterns, duplication, and abstractions
- Compare the new/changed code against similar implementations
- Trace data flow and dependencies
- Check documentation against implementation
- Report high-confidence issues with citations and severity

### Phase 3: Synthesis & Recommendations

Consolidate findings into these categories (in severity order):

1. **Critical Issues** (Must fix)

- Correctness bugs, memory safety violations, data corruption risks
- Architectural blockers (e.g., circular dependencies, missing abstraction causing rigid coupling)
- Security/safety vulnerabilities

2. **High-Priority Issues** (Strong recommend)

- Duplication that should be unified (2+ similar implementations doing the same thing)
- Bespoke features that break generalization (special-case logic that belongs in a strategy/handler)
- Missing abstractions that limit extensibility
- Out-of-sync documentation that will confuse maintainers

3. **Medium Issues** (Consider)

- Suboptimal patterns that work but miss better alternatives
- Incomplete error handling or logging
- Opportunities to reduce code verbosity
- Performance concerns (not critical, but worth noting)

4. **Low Issues** (Optional)

- Style, naming, or formatting inconsistencies
- Suggestions for clarity (comments, structure)

---

## Patterns to Flag

### Duplication of Intention

**Pattern**: Two or more pieces of code that:

- Solve the same problem in different ways
- Share identical logic with minor parameter differences
- Duplicate a data structure, component query, or algorithm

**Action**: Suggest unifying under a shared abstraction (function, strategy handler, registry entry) or data-driven
configuration.

**Example (ECS)**:

```cpp
// Bespoke: PhysicsSystem updates position based on velocity
it.each([](Transform& t, Velocity& v) { t.position += v.velocity * it.delta_time(); });

// Bespoke: ParticleSystem updates position based on direction
it.each([](Transform& t, Direction& d) { t.position += d.direction * it.delta_time(); });

// DUPLICATION: Both do position += direction * dt. Suggest: unified Motion component.
```

### Bad Patterns

- **Hardcoded behavior**: Config that belongs in data (levels, difficulty, balance tuning)
- **Deep if/else trees**: Multiple type checks or mode branches — suggest strategy pattern or registry
- **God classes/systems**: Single entity doing too many jobs — decompose into focused components
- **Magic numbers**: Tuning constants without explanation — move to shared header with `constexpr`
- **Tightly coupled callbacks**: Event handlers that depend on specific implementation details
- **Incorrect abstraction**: Base class that only 1–2 subclasses use, or a generic abstraction forced to special-case
- **Mutable global state**: Singletons accessed directly; prefer `world.get_mut<T>()`
- **Ad-hoc error handling**: No consistent strategy (some throw, some return nullptr, some silently fail)

### Missing Core Functionality

- No error handling or logging at critical junctures
- Incomplete state management (e.g., no teardown/cleanup, no invariant checks)
- Missing input validation
- Unhandled edge cases
- No fallback for missing or corrupted data

### Bespoke vs. Generalized

**Flag when a system is too special-case**:

- Hard-coded entity names, type names, or asset paths
- Special branching for one feature that should apply broadly
- Monolithic handlers that should be pluggable (e.g., "damage" logic that only works for enemies, not traps or
  projectiles)

**Action**: Refactor to:

- Data-driven configuration (JSON tables, property sheets)
- Strategy/handler registry (new behavior = new handler + entry, no code changes)
- Component-based generalization (shared components, reusable systems)
- Abstract interfaces (one implementation per subclass, switch via factory or registry)

**Example (Data-Driven)**:

```json
// Bespoke: Damage system has if (type == "enemy") or if (type == "player")
// Generalized: Define damage response in data
{
  "damageResponses": {
    "DamageResistance": {"factor": 0.5},
    "Invulnerable": {"factor": 0.0},
    "Reflect": {"factor": 1.0, "redirectTo": "attacker"}
  }
}
```

### Documentation Sync

- **Code comments vs. implementation**: Comments describe old behavior or contradict code
- **API documentation vs. actual signature**: Function docs promise parameters that don't exist, or omit required
  preconditions
- **Design docs vs. reality**: Architecture diagrams show a pattern the code abandoned; integration docs are missing
- **Invariant documentation**: What preconditions must hold for this function to work safely? Are they checked?
- **Data format docs**: JSON schemas, asset specs, config file formats — do they match the parser?

**Action**: Update the doc to match the code, or refactor the code to match the doc (whichever is correct). Flag stale
docs for cleanup.

### Other Principal-Level Concerns

- **Complexity creep**: Feature X was simple; now it has 5 special cases. Is a better abstraction overdue?
- **Premature generalization**: Over-engineered for extensibility that will never happen (3+ variants is a real
  threshold; 2 is not)
- **Inconsistent conventions**: Systems that bypass shared patterns (e.g., one system posts events, another uses
  callbacks; inconsistent naming style)
- **Performance cliffs**: Patterns that seem fine at scale N but explode at scale 2N (e.g., O (n²) queries, missing
  spatial partitioning)
- **Testability**: Can this system be unit-tested, or does it require a fully initialized world?

---

## Code Review Checklist

### Architecture & Design

- [ ] Responsibilities are clear and separated
- [ ] Dependencies flow in one direction (no circular deps)
- [ ] Similar components/systems are unified, not duplicated
- [ ] Bespoke logic is justified; generalization is not over-engineered
- [ ] Public APIs are minimal; internal details are hidden

### Correctness & Safety

- [ ] No memory leaks, dangling pointers, or unsafe indexing
- [ ] Preconditions and invariants are documented and checked (or asserted)
- [ ] Error paths are handled explicitly
- [ ] No uninitialized state or undefined behavior
- [ ] Concurrent access is safe (if multithreaded)

### Performance & Scalability

- [ ] No unnecessary allocations in hot paths
- [ ] Data layouts support cache efficiency (SoA for ECS, locality for traversal)
- [ ] Algorithms are appropriate for expected data sizes
- [ ] No O (n²) loops or redundant queries
- [ ] Logging and debug overhead is minimal in release builds

### Documentation

- [ ] Public functions/classes have contracts (what they do, preconditions, postconditions)
- [ ] Complex algorithms are explained
- [ ] Data structures and format specs are documented
- [ ] Invariants and assumptions are stated
- [ ] Docs are current with the code

### Testing

- [ ] Critical paths have unit or integration tests
- [ ] Edge cases are covered
- [ ] Error cases are tested
- [ ] Tests are readable and maintain the intended behavior

### Consistency

- [ ] Code follows project conventions (naming, formatting, patterns)
- [ ] Similar problems use similar solutions
- [ ] Abstraction levels are consistent throughout

---

## Sub-Agent Execution

**Use ONLY Claude sub-agents for deep investigation.** When delegating:

1. **Provide complete context** in the prompt:

- Project architecture (ECS, subsystem boundaries, key abstractions)
- Relevant code excerpts and files
- Specific concerns to investigate (duplication, pattern consistency, documentation)

2. **Instruct the agent**:

- "Use the code-review agent type"
- "Search for patterns similar to [pattern]. Are there duplicates or inconsistencies?"
- "Check if [subsystem] is documented; does the design doc match the code?"
- "Identify all occurrences of [problem] and assess severity"

3. **Gather findings** with citations (file:line):

- High-confidence issues only
- Root cause, not surface symptoms
- Recommended fix with rationale

---

## Output Format

### Summary

Brief overview: scope of review, key findings count, top concern.

### Findings Table

| # | Severity | File:Line                 | Category    | Issue                                            | Recommendation              |
|---|----------|---------------------------|-------------|--------------------------------------------------|-----------------------------|
| 1 | CRITICAL | src/physics/system.cpp:45 | Pattern     | Hardcoded damping value; should be data-driven   | Move to config JSON         |
| 2 | HIGH     | src/ecs/world.h:120       | Duplication | Two identical update loops in Systems X and Y    | Unify into shared component |
| 3 | MEDIUM   | src/scene/manager.cpp     | Docs        | Lifecycle docs don't match OnLoad/OnUnload order | Update docs/design.md       |

### Guidance by Category (Optional)

If many issues exist in one area (e.g., "6 instances of hardcoded strings"), group and summarize:

**Generalization Opportunities** (5 instances):

- `src/enemy/ai.cpp:32` — hardcoded "patrol_speed"
- `src/player/controller.cpp:18` — hardcoded "max_health"
- *Recommendation:* Extract tuning constants to a centralized config schema

---

## Example Review Summary

**Scope**: New damage system integration with armor mechanics

**Key Findings**:

- **1 Critical**: Armor damage reduction applied twice (damage + armor system both reduce, not multiplicative as
  intended)
- **2 High**: Duplication of type-based dispatch (same if/else in 3 places) → suggest handler registry
- **1 Medium**: Missing documentation of DamageResponse JSON schema in design doc

**Recommendation**: Fix critical bug first. Then refactor type dispatch into a single, centralized handler registry.
Update design.md with the new data structure.

---

## Tips for This Skill

- **Severity matters**: Don't conflate style issues with architectural problems. Be clear about what *must* be fixed vs.
  what *should* be considered.
- **Cite your sources**: Always reference file:line for findings. Code snippet > vague description.
- **Explain the "why"**: Not just "this is bad," but "this will cause [problem] when [scenario]" or "this
  duplicates [existing pattern]."
- **Suggest, don't dictate**: "Consider unifying these" vs. "You must refactor this." Except for correctness and safety
  issues, which are non-negotiable.
- **Look for patterns, not one-offs**: A single hardcoded value is a slip. Five hardcoded values suggests a missing
  abstraction.
- **Check consistency**: If the codebase uses event handlers in one place and callbacks in another, flag it as
  inconsistency, not necessarily wrong.
