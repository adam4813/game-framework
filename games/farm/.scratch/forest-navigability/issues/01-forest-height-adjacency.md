# 01: Forest height rescaling + 4-connected adjacency

**Status:** done
visually correct.

**Height rescaling (tile units throughout):** All `WorldObject` heights are currently stored in
world units (WU). A tree "height 2.0f" is 4 tiles tall at `tileSize = 0.5f` — far taller than
intended. This ticket converts every height-bearing catalog and code constant to tile units and
changes the mesh builder to multiply by `tileSize` when placing vertex Y. Oak and birch cap at
2 tiles, pine at 3, fir at 4. Buildings, creatures, bridge deck, and the town well are rescaled
to tile-unit equivalents. Decision recorded in `games/farm/docs/adr/0002-heights-tile-units.md`.

**4-connected adjacency constraint:** The tree scatter pass (`ScatterForestTrees`) currently
places trees independently per tile; two trees can end up directly N/S/E/W adjacent, creating
impassable walls. After this ticket, the scatter pass tracks placed tiles and skips any candidate
whose cardinal neighbor (N/S/E/W) is already occupied. Diagonal adjacency is permitted. Density
parameters are unchanged.

Scale context: 1000 × 1000 canonical world. Forest biomes cover roughly 20–35% of the land area;
at `forestTreeDensity = 0.55`, roughly 40–50% of forest tiles carry a tree before the adjacency
filter. After the filter the effective rate drops; the `TreeDensityFallsInExpectedRange` test
upper bound is calibrated from the diagnostic output after implementation.

**Status:** ready-for-agent

- [ ] The upright-object mesh builder multiplies `WorldObject::height` by `tileSize` (from
  `UprightMeshConfig`) when placing vertex Y, so a height value of `2.0` stands exactly 2 tiles
  tall in the scene.
- [ ] Default tree species heights are updated to tile units: oak 1.5–2.0, birch 1.5–2.0, pine
  2.5–3.0, fir 3.5–4.0.
- [ ] Default building wall heights are updated to tile units: house 3, shop 3, barn 4, hall 5.
- [ ] Default creature heights are updated to tile units: cow 1.5, sheep 1.2, chicken 0.8, deer
  1.6, rabbit 0.7, fox 1.0.
- [ ] Remaining `WorldObject` heights set in code (bridge deck, hub marker, well/landmark) are
  updated to tile units.
- [ ] `ScatterForestTrees` tracks a placed-tile set and skips a candidate tile when any of its
  four cardinal neighbors is already occupied by a tree.
- [ ] No two tree objects in a generated world share a 4-connected adjacent tile (new unit test).
- [ ] All seeds in the existing test suite still produce a measurable density range, at least one
  clearing, and deep forest denser than regular forest.
- [ ] `TreeHeightsRespectSpeciesRange` continues to pass with the new catalog ranges.
- [ ] `upright_object_mesh_builder_test.cpp` geometry assertions are updated to expect
  `height × tileSize` for vertex Y.
- [ ] A new mesh-builder test explicitly documents the tile-unit scaling contract (e.g.
  `height=3.0, tileSize=0.5 → vertex Y = 1.5`).
- [ ] Two calls with the same seed still produce identical tree placements (determinism preserved
  for the adjacency-filtered output).
- [ ] All existing tests pass.
