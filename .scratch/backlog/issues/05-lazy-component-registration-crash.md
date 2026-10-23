# Follow-up: lazy component registration crash in multi-stage worlds

Status: needs-triage
Source: raised while writing [Signal Room spec 1: Room & bootstrap](../../signal-room-room-bootstrap/spec.md), 2026-10-18

## Context

`AGENTS.md` (Component pre-registration) and `.github/skills/code-review/SKILL.md` say a component type first
reached inside a system (`try_get<T>()`, `has<T>()`, `add<T>()`) is registered lazily, and that this crashes with
`INVALID_WHILE_READONLY` because "the physics module has set `stage_count > 1`". The profiling module has a comment
about the same crash.

The physics attribution looks wrong. Nothing in `src/engine/` sets a stage count, and Flecs and Jolt run together fine
in the sample scene. Desktop `main` calls `set_threads(4)`, which makes the world multi-stage on its own, so the cause
is probably threading (or readonly merges in general), not physics.

**Physics and threading (from the user, 2026-10-18).** If Jolt physics really needs single-threaded execution, make
that explicit: the physics module should declare it rather than leave it as a side note about stage counts. The user
believes the physics update path isn't tied to per-entity components: only the reconciliation between ECS and Jolt is.
A first look at the Jolt module agrees. `PhysicsStep` works from the `JoltBackend` / `PhysicsWorldConfig` /
`PhysicsAccumulator` singletons. The force, impulse and velocity-override systems and `PhysicsSyncToBackend` /
`PhysicsSyncFromBackend` are the parts that touch entity components, along with the add/remove body observers. None of
them is marked `.multi_threaded()`. So physics itself probably isn't what makes the world multi-stage.

## To do

- [ ] Reproduce the crash in a minimal case (a system that lazily registers a type, with and without `set_threads`,
      with and without physics) and record the real trigger.
- [ ] Correct the explanation in `AGENTS.md` and the code-review skill to name the verified cause.
- [ ] Find out whether Jolt needs single-threaded execution. If it does, make that explicit in the physics module
      (for example, keep the step and the ECS↔Jolt reconciliation on the main thread and document why), not as a
      stage-count side effect.
- [ ] Decide whether pre-registration is still the rule, or whether the engine can catch it (for example, an assert
      or a log naming the unregistered type).
