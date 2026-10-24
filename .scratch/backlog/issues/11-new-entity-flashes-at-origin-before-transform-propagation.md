# Bug: a new entity may render at the origin for one frame before TransformPropagation runs

Status: needs-triage
Source: Crabnado ticket 08 playtest, 2026-10-24 (branch `feat/crabnado-foundation`)

## What happened

While playing Crabnado, a crab was seen at the centre of the world, flashing, as if each thrown crab spawned at the
origin for one frame and then jumped to its throw point. Crabs are created in a system with `Transform` set last
(`ThrowCrab`, `games/crabnado/src/horde.cpp`), plus three child sphere parts with local offsets. Nothing in game code
is hard-coded to the origin, and no stuck crab was found in the live ECS. The one-frame glitch could not be caught
over the Remote API or in a still screenshot, so the cause is unconfirmed.

A game-side mitigation is in: `ThrowCrab` now sets `Transform` before `Enemy` and `Airborne`. That is only a guess at
the cause. If the glitch is gone, the order of component adds matters to the engine, which is itself worth fixing.

## Where to look

- `src/engine/spatial/spatial_module.cpp`: `Transform` has a `flecs::With` on `WorldTransform`, and the
  `SetInitialWorldTransform` observer (`OnAdd` of `WorldTransform`) copies the local `Transform` if the entity has one
  at that moment. If `Transform` is added and valued after `WorldTransform`, the seed is the default (the origin). For a
  child it is the child's local value, not composed with its parent's.
- `TransformPropagation` runs in `PreStore`, with `cascade(flecs::ChildOf)`, and is `Pausable`. Check that an entity
  created by a deferred merge in `OnUpdate` is matched in the same frame's `PreStore`, and that the render systems
  (`OnStore`) never see a `WorldTransform` the propagation hasn't touched yet. Also check entities created outside a
  frame's pipeline (a scene `Load`, or a `Tick` callback) and entities created while the scene is paused, when the
  Pausable propagation does not run.
- Whether `SetInitialWorldTransform` should compose with the parent's `WorldTransform` for children.

## Acceptance criteria

- [ ] A reproduction: a test or debug scene that creates an entity with its position set last (and a child with a local
      offset) and shows its `WorldTransform` on the first rendered frame, or shows it is already correct.
- [ ] A new entity, and its children, never render at the origin or at an uncomposed local position, whatever order its
      components are set in.
- [ ] If the spatial module's behaviour or its guarantees change, `src/engine/spatial/README.md` says so.

## Comments
