Status: done

# Spec: Farm Forest Navigability + World-Gen Profiling

## Problem Statement

Two related problems degrade the Farm experience after the `farm-world-alive` effort:

1. **Forests are hard to navigate.** Tree objects can appear directly adjacent to one another
   (N/S/E/W), leaving no guaranteed gap between trees and producing dense walls that the player
   must hunt for a way through. Trees are also significantly taller than they look visually — the
   height values in the catalog and JSON were authored in world units (WU), while the game renders
   and reasons about space in tiles, so a "2.0" height means 4 tiles tall rather than the intended
   2. This misalignment makes the forest feel even more oppressive.

2. **World loading takes a noticeable amount of time and it's unclear why.** The 1000 × 1000 farm
   world is generated synchronously as one blocking pass before the player sees anything. No
   profiling instrumentation exists, so there is no data on which pipeline stage (elevation noise,
   flood-fill water, river tracing, history simulation, town layout, tree scatter, creature
   placement, tilemap build, entity spawn) dominates load time — meaning any optimisation is a
   guess. Before chunking or parallelism can be designed responsibly, Tracy profiling data is
   needed.

## Solution

### Forest navigability

- **Convert all `WorldObject` heights to tile units.** The mesh builder scales by `tileSize`
  (0.5 world units per tile) when placing geometry, so a height value of `2.0` now means exactly
  2 tiles in the scene. All catalog defaults are rescaled accordingly (see Implementation
  Decisions). The decision is recorded in ADR `games/farm/docs/adr/0002-heights-tile-units.md`.

- **Cap tree species heights at per-species maxima.** Oak and birch cap at 2 tiles, pine at 3,
  fir at 4 — a significant reduction from the current WU values that were effectively 4–10+ tiles
  tall.

- **Enforce 4-connected adjacency.** The tree scatter pass tracks placed tiles; a candidate tile
  that shares a cardinal edge (N/S/E/W) with any already-placed tree is skipped. No two trees
  will ever be immediate N/S/E/W neighbors. The density knobs (`forestTreeDensity`,
  `deepForestTreeDensity`) are left unchanged; the adjacency rule naturally reduces the effective
  placement rate.

### World-gen profiling

Add `profiling::Zone` markers at every pipeline stage — both inside `WorldGen::Generate` and in
the tilemap-build + entity-spawn phases of `FarmWorldScene` — so a developer can open Tracy and
see an accurate flamechart of the full load path. The profiling data will drive the subsequent
design of load regions and any deferred or parallelised generation work.

## User Stories

1. As a player, I want forests to always have navigable gaps between trees, so that I can find a
   path through the forest without hunting for a single open tile.
2. As a player, I want adjacent trees to never block all four cardinal directions at once, so that
   I am never hard-walled inside a forest.
3. As a player, I want oak and birch trees to look short (2 tiles tall), so that they feel like
   deciduous woodland I can see over partially.
4. As a player, I want pine and fir trees to look clearly taller than oak, so that dense
   deep-forest biomes feel dark and imposing by contrast.
5. As a player, I want creatures and buildings to stay at their relative visual scale when tree
   heights change, so that the world looks coherent and proportional.
6. As a developer, I want height values in the tree species catalog and JSON to be expressed in
   tiles, so that authoring "a tree is 3 tiles tall" maps directly to what I see in the scene
   without WU conversion.
7. As a developer, I want all WorldObject heights (buildings, creatures, bridge deck, well/landmark)
   expressed in tile units for the same reason, so that the height convention is consistent across
   every catalog and code constant.
8. As a developer, I want to open Tracy after a world load and see per-stage flamechart entries
   for the full load path (elevation, water, rivers, history, town layouts, trees, creatures,
   tilemap build, entity spawn), so that I can identify which stage dominates.
9. As a developer, I want to compare load times across seeds in Tracy, so that I can see whether
   the bottleneck is deterministic (fixed cost) or variable (seed-dependent).
10. As a future implementer, I want concrete profiling data before designing load regions, so that
    chunking targets the actual bottleneck rather than a guess.

## Implementation Decisions

### Height units (tile units throughout)

- `WorldObject::height`, `TreeSpecies::minHeight/maxHeight`, `BuildingDef::wallHeight`, and
  per-entity creature `height` fields all store values in **tile units**. 1 tile = `tileSize`
  world units (`tileSize = 0.5f` in the Farm scene).
- The upright-object mesh builder scales to world units at render time (`height × tileSize`),
  keeping all authored values in the same coordinate space as tile positions and movement radii.
- This decision is recorded in `games/farm/docs/adr/0002-heights-tile-units.md`.

### Default catalog values after rescaling

| Object | Value (tile units) |
|--------|-------------------|
| tree_oak | 1.5 – 2.0 |
| tree_birch | 1.5 – 2.0 |
| tree_pine | 2.5 – 3.0 |
| tree_fir | 3.5 – 4.0 |
| house wall | 3.0 |
| shop wall | 3.0 |
| barn wall | 4.0 |
| hall wall | 5.0 |
| cow | 1.5 |
| sheep | 1.2 |
| chicken | 0.8 |
| deer | 1.6 |
| rabbit | 0.7 |
| fox | 1.0 |
| bridge deck | 0.4 |
| well / landmark | 2.0 |

Creature heights are intentionally fractional — a chicken at 0.8 tiles is just under one grid
cell tall, which is correct visually.

### 4-connected adjacency constraint

- `ScatterForestTrees` maintains a placed-tile set during its tile-by-tile pass.
- Before accepting a candidate at `(x, y)`, it checks whether any of `(x±1, y)` or `(x, y±1)`
  are in the placed set. If any neighbor is occupied, the candidate is skipped (no RNG drawn for
  the placement decision itself; the per-species RNG roll still occurs before the check to keep
  the RNG stream stable for determinism — or the check is moved before the roll and the stream
  accepted as changed for this iteration).
- The adjacency check does **not** apply to diagonal neighbors (8-connected is not required).
- `forestTreeDensity` and `deepForestTreeDensity` parameters are not changed; the adjacency
  rule is the sole density lever in this ticket.

### Profiling zone placement

- `profiling::Zone` entries are added to each of the following named scopes:
  - Inside `WorldGen::Generate`: elevation generation, water classification, river tracing,
    history simulation, town layout generation, forest tree scatter, creature placement.
  - Inside `FarmWorldScene::Load` (or its helpers): tilemap build, entity spawn / object
    streaming setup.
- The profiling module (`engine::profiling`) is already wired into the build and used by other
  systems (render, pathfinding). No new infrastructure is needed.
- Zone names follow the pattern used by the render and pathfinding modules.

### World scale

The canonical Farm world is 1000 × 1000 tiles. Profiling results will be produced at this scale.
The preview scene uses a 160 × 160 world and will show proportionally smaller numbers; profiling
targets the full-scale farm load path.

## Testing Decisions

**What makes a good test here:** test the external output of `WorldGen::Generate` (the `WorldData`
value it returns) and the external output of `BuildUprightObjectMesh` (the vertex positions it
produces). Do not test the internal structure of `ScatterForestTrees` or how the placed-set is
managed.

### Tests for forest navigability (extend `enterable_forests_test.cpp`)

- **New: `NoTwoTreesAreCardinallyAdjacent`** — for each seed in `kSeeds`, iterate every tree in
  `world.objects` and assert that no other tree has a footprint tile at any of its four cardinal
  neighbors. This is a pure property test on the `WorldData` output.

- **Update: `TreeDensityFallsInExpectedRange`** — lower the upper-bound expectation to reflect
  the adjacency rule's natural reduction; the exact new bound can be calibrated from the
  diagnostic output of the existing `DiagnosticDensityReport` test after implementation.

- **Update: `TreeHeightsRespectSpeciesRange`** — no logic change needed; the test already
  validates against the catalog's own `minHeight/maxHeight`. Updating the catalog values is
  sufficient.

Prior art: `enterable_forests_test.cpp` (full file visible in codebase; uses `kSeeds`,
`TreeMask`, `ForestStats` infrastructure already in place).

### Tests for height-unit conversion (update `upright_object_mesh_builder_test.cpp`)

- **Update existing geometry tests** — the test currently passes `height = 2.0F` and expects
  vertex Y at `2.0F`. After the change it should expect `2.0F × tileSize = 1.0F`. Update the
  `constexpr float h` and any related assertions to use `height * tileSize`.

- **New: explicit tile-unit documentation test** — a `WorldObject` with `height = 3.0` (tiles)
  and a config with `tileSize = 0.5f` produces vertex Y at `1.5f` WU; with `tileSize = 1.0f` it
  produces `3.0f`. This documents the scaling contract rather than just the fixed values.

Prior art: `upright_object_mesh_builder_test.cpp` (pure CPU geometry, no ECS or GPU).

### Profiling instrumentation

No unit test. Verified at runtime by launching the farm scene and observing Tracy: all expected
zone names appear in the flamechart, each with a non-zero duration.

## Out of Scope

- **Load Regions / progressive loading.** The chunked entity-spawn design (player enters
  immediately; entities phase in outward) is explicitly deferred until profiling data reveals
  which stage to target. Neither the design nor the implementation of Load Regions is part of
  this effort.
- **Density parameter tuning.** `forestTreeDensity` and `deepForestTreeDensity` are not changed;
  the adjacency rule is the sole density lever.
- **8-connected adjacency.** Diagonal adjacency is not enforced; trees may share corners.
- **Parallelising or async-deferring any world-gen stage.** This is downstream of profiling.
- **New tree species.** Height rescaling is applied to the existing four species only.

## Further Notes

- The grilling session established that "2 tiles" (the player-facing description of tree height)
  directly maps to `height = 2.0` in tile units. "Trees should be 2 tiles high at most" is
  satisfied by the per-species `maxHeight` caps.
- The `DeepForestIsDenserThanForest` test must continue to pass after the adjacency rule is
  applied; deep forest uses `deepForestClearingThreshold = 0.12` (much lower than forest's
  `0.32`), which should maintain a density advantage even after adjacency filtering.
- If the adjacency rule breaks `DeepForestIsDenserThanForest`, the fix is to lower
  `deepForestClearingThreshold` slightly rather than raising `deepForestTreeDensity` (keeping the
  density params as the single control).
