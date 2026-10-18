# Follow-up: reconcile the orthographic Camera with the farm branch

Status: needs-triage
Source: Signal Room ticket 01 (game branch `feat/signal-room`), 2026-10-18

## Context

At the user's request, ticket 01 cherry-picked the orthographic camera from `feat/farm-preview-first-entry`
("add orthographic projection mode to Camera" and its unit test) onto `feat/signal-room`, to reconcile later. The
cherry-picks were then changed on this branch:

- `render::Projection` lost its `: uint8_t` underlying type. Flecs enum reflection reads 4 bytes, so the narrow enum
  serialized padding (the REST API reported `projection: 32513`). The farm branch still has the bug.

Open points from review:

- The `camera` loader parses `"projection"` by string compare. An unknown or misspelled value silently becomes
  Perspective. It should warn, or parse through the reflected enum constants.
- AngelScript logs `Skipping unsupported member 'Camera.projection'` (enums aren't script-bindable yet).
- `render::Projection` and `platform::CameraProjection` duplicate each other (the Platform boundary justifies two types,
  but they now have different underlying types).

## What to do

When the branches meet, take one copy of the feature with the underlying-type fix, and decide on the loader warning.
