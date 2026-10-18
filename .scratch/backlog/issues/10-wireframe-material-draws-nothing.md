# Bug: a wireframe Material draws nothing on desktop

Status: needs-triage
Source: retro of Signal Room ticket 02 (branch `feat/sr-room-02-shift1-layout-greybox`), 2026-10-18

## What happened

Signal Room's solid-cells debug overlay first drew one `CubePrimitive` per solid cell with
`Material{.color = red, .wireframe = true, .cast_shadow = false}`: 22 entities, 0.94 m × 2 m × 0.94 m, standing over
furniture in the Room scene. The REST API showed all 22 entities with the expected components, but the desktop
screenshot showed no wire lines at all. The same boxes with `wireframe = false` (flat markers) draw. Wireframe is meant
to work on desktop (the WebGL limitation is separate and documented in `src/engine/render/README.md`).

## Where to look

- `RegisterPrimitiveEnqueuer` (`src/engine/render/render_module.cpp`) queues cubes into the render queue;
  `RaylibBackend::SubmitBatch` (`src/engine/platform/raylib/raylib_backend.cpp`) sends wireframe groups down the
  non-instanced path (`instanceable` is false when `material.wireframe`), to `DrawCustomMesh` or `DrawLitMesh`.
- `DrawLitMesh` calls `DrawMeshWithMaterial(..., true)`, which wraps `::DrawMesh` in `rlEnableWireMode()` /
  `rlDisableWireMode()`. Check whether the batch (`rlDrawRenderBatchActive`) is flushed inside the wire-mode window,
  and whether `DrawCustomMesh` (tried first) ignores `wireframe` entirely.

## Repro

Any scene: add a `CubePrimitive` with a `Material` whose `wireframe` is true and screenshot the desktop build
(`pwsh tools/screenshot.ps1 -Id <pid>`). Expect wire edges; today nothing is drawn.

## Done when

- A wireframe cube draws its edges on desktop through the batched render path.
- The fix keeps instanced batching for non-wireframe draws.
