# 03: Upright-object renderer

**What to build:** With world-objects now in `WorldData` and the camera tilted oblique, this
ticket makes them *visible*: an upright, alpha-tested cutout sprite for each object whose
visual height is independent of its one-tile footprint. A tree placed by ticket 05 should stand
several tiles tall; a building wall placed by ticket 04 should visually rise above the ground.

A geometry builder — mirroring the existing tilemap mesh builder — bakes the static world-object
collection into upright quads anchored to each footprint tile's centre and sized by the object's
height. Quads use alpha-tested (cutout) materials so the depth buffer resolves overlaps without
any transparency sort. Moving entities (villagers, creatures) use a smaller, continuously updated
upright quad. All geometry is built on CPU, uploaded as a `DynamicMesh`, and drawn by the
existing render pipeline via the `RenderWith` relationship — no new render draw-call path needed.

**Blocked by:** 02 — World-object data model

**Status:** done

- [ ] A geometry builder converts the `WorldObject` collection in `WorldData` into a
  `DynamicMesh` of upright quads; each quad is anchored to its footprint tile's ground centre
  and has height equal to the object's height field.
- [ ] The mesh uses an alpha-tested cutout material so overlapping objects sort correctly via
  the depth buffer with no manual Y-sort.
- [ ] Placing a few test objects in the farm scene (or in a unit scene) shows them rendering
  upright at the expected height when viewed from the oblique camera.
- [ ] Moving entities (NPC/creature tag) get a dynamic upright quad that updates position each
  frame, reusing the existing `GridMover` / `Transform` pipeline.
- [ ] Unit test: given a `WorldObject` with a known footprint position and height, the geometry
  builder produces upright quad vertices at the expected world coordinates (no GPU required).
- [ ] The static mesh is rebuilt when the world-object collection changes (e.g. after fresh
  generation); there is no stale geometry from a previous world.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commits: 0aa271c (builder+cutout shader), b212f3e (farm-scene wiring). Files: upright_object_mesh_builder.{hpp,cpp} (new), cutout.fs shaders (new), upright_object_mesh_builder_test.cpp (new), farm_world_scene.{cpp,hpp}, farm_game_module.cpp, assets/levels/farm_world_map.level.json (ObjectSheet), tilesets/basic.json. 212/212 tests pass. Known followup (resolved in T11): mesh chunking for play-scale forests.