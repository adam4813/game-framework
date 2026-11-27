# 01: Orthographic oblique camera (Engine Module)

**What to build:** The farm world scene currently renders from a near-vertical perspective
camera, producing a flat Dwarf-Fortress-like view. This ticket makes the engine's `Camera`
component support orthographic projection and tilts the farm follow-camera to a fixed oblique
angle, so the world reads as a cozy, foreshortened 2.5D place — the intended Stardew-style look.
No per-entity changes are needed: depth testing handles overlap order automatically.

The `Camera` component gains a `projection` mode (perspective / orthographic) and an
`ortho_size` / view-height field. The raylib Platform backend routes to `CAMERA_ORTHOGRAPHIC`
and builds the view/projection matrix with `glm::ortho`. The frustum-cull matrices used by the
render pipeline update accordingly. The farm follow-camera's local `Transform` is re-authored
in its Level JSON to sit at a fixed oblique offset (not rotated freely). Per ADR-0005, only
`Transform` is written — `WorldTransform` stays engine-computed.

**Blocked by:** None (can start immediately)

**Status:** done

- [ ] The engine `Camera` component accepts `projection: "orthographic"` and an orthographic
  view-size parameter alongside the existing `fov` / `aspect` / `near` / `far` fields.
- [ ] The raylib Platform backend selects `CAMERA_ORTHOGRAPHIC` when the component requests it
  and computes view/projection matrices with `glm::ortho`.
- [ ] The render pipeline's frustum-cull matrices are updated to use the orthographic projection
  so culling stays correct.
- [ ] The farm follow-camera is re-authored to a fixed oblique tilt (authored on local
  `Transform`, not `WorldTransform`); the scene continues to load and the camera follows the
  player smoothly.
- [ ] Unit test: an orthographic `Camera` component with a given view size produces the expected
  `glm::ortho` view/projection matrix (no GPU required).
- [ ] Manual / runtime check (Flecs Remote API): the farm scene renders at an oblique angle with
  no visual corruption; tiles and any existing geometry appear foreshortened as expected.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commits: 1829694 (Camera orthographic mode), 260e81e (matrix unit test), 3f1b1b8 (oblique farm camera). Files: src/engine/render/camera_projection.hpp (new), render_components.hpp, render_module.cpp, platform.hpp, raylib_backend.cpp, assets/levels/farm_world_player.level.json, tests/unit/camera_projection_test.cpp. 179/179 tests pass. Live Flecs oblique-render check deferred to ticket 11.