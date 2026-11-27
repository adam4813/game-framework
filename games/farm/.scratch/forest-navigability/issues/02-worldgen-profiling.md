# 02: World-gen profiling instrumentation

**Status:** done
to the tilemap-build + entity-spawn phases in `FarmWorldScene`, so a developer can open Tracy
and see a flamechart of the full load path from seed-to-first-frame.

The profiling infrastructure (`engine::profiling::Zone`, `Plot`) is already wired into the build
and in use by the render and pathfinding modules. No new build machinery is required.

**Background:** The canonical Farm world is 1000 × 1000 tiles and is generated synchronously as
a single blocking pass before the player sees anything. Without per-stage timing data, any
optimisation (deferred tree scatter, chunked tilemap build, Load Region phasing) is speculative.
This ticket produces the data needed to make that decision.

**Status:** ready-for-agent

**Blocked by:** none

- [ ] A `profiling::Zone` is placed at the start of each of the following named scopes inside
  `WorldGen::Generate`:
  - Elevation (fractal Perlin heightmap generation)
  - Water classification (flood-fill water bodies)
  - River tracing
  - History simulation (town founding, roads, collapses)
  - Town layout generation (building/field placement per town)
  - Forest tree scatter (`ScatterForestTrees`)
  - Creature placement
- [ ] A `profiling::Zone` is placed around the tilemap-build pass inside `FarmWorldScene` (the
  call that converts `WorldData` to a `Tilemap`).
- [ ] A `profiling::Zone` is placed around the entity-spawn / object-streaming setup pass in
  `FarmWorldScene` (the phase that creates ECS entities for trees, buildings, creatures, etc.).
- [ ] Zone names follow the naming convention used by the existing render and pathfinding zones
  (e.g. `"WorldGen/ForestTreeScatter"`, `"FarmWorldScene/TilemapBuild"`).
- [ ] The farm scene loads without crash or assertion failure after the change.
- [ ] All existing tests pass.
- [ ] Runtime verification: launch the farm scene with Tracy connected; all expected zone names
  appear in the flamechart with non-zero durations.
