# 01: World-gen load time — next-round optimisation targets

**Status:** ready-for-agent

**Context:** Farm world-gen was profiled in the `farm-forest-navigability` effort. Starting point
was ~7.3s synchronous generation (1000×1000 world). After targeted optimisations the baseline
reached **~4.8s (34% reduction)**. This ticket records what was found, what was fixed, and what
remains, so the next effort can skip the diagnosis and go straight to implementation.

---

## What was measured (post-optimisation baseline, ~4.8s)

| Stage | Zone | Time | Notes |
|---|---|---|---|
| Elevation | `WorldGen/ElevationNoise` | 1.16s | 6-octave Perlin, 1M tiles |
| TownLayoutGen | `WorldGen/TownLayouts` | 1.6s total | see sub-breakdown |
| — | `WorldGen/RoadRouting` (RouteAStar ×6) | ~960ms | A* on 1M grid, 60–344ms/route |
| — | `WorldGen/PlaceRoadHubs/CentralityScan` | ~260ms | O(n × pairs), sqrt-free |
| — | `WorldGen/PlaceRoadHubs/GeoFunnelPass` | ~245ms | 600K tiles × 8 × 5 lookups |
| — | `WorldGen/PlaceRoadHubs/SortAndSelect` | ~7ms | ✅ resolved with partial_sort |
| Forest scatter | `WorldGen/ForestNoiseField` | ~550ms | 3-octave Perlin (was 6) |
| — | `WorldGen/ForestScatterLoop` | ~290ms | 1M tile scan, 250K woodland |
| River tracing | `WorldGen/DistanceToWater` | ~258ms | full-grid BFS |
| — | `WorldGen/RiverTrace` | ~200ms | downhill tracing |
| Scene (not gen) | `FarmWorldScene/TilemapBuild` | ~400ms | WorldData → Tilemap |

---

## Remaining targets ranked by impact

### 1. `RouteAStar` ×6 routes — ~960ms total

A* over a 1000×1000 grid. Each route takes 60–344ms depending on distance. Six routes = almost
1s. Three algorithmic options:

- **Jump Point Search (JPS)** — drops to O(jump_points), typically 5–20× faster than standard
  A* on open grids. Best fit since most of the farm world is open terrain.
- **Bidirectional A*** — start from both ends; meets in the middle. ~2× speedup with no quality
  change.
- **Hierarchical pathfinding** — build a coarse macro-graph over the world (e.g. 10-tile
  supercells), route on the macro-graph, refine only the endpoints. Larger engineering cost but
  scales with number of routes.

JPS is the recommended first attempt — it plugs into `RouteAStar` with no API change.

### 2. `ElevationNoise` — 1.16s

6-octave fractal Perlin over 1M tiles. The octave count directly controls quality vs cost; 1
octave ≈ 190ms at this scale. Options:

- **Reduce to 4 octaves** — saves ~380ms. Some reduction in small-scale terrain detail; test if
  biome variety and coastline shape remain acceptable.
- **Parallelize** — elevation, water classification, and river tracing are fully independent
  after the grid is built. Running them on worker threads would make their combined time equal to
  the longest single stage.

### 3. `ForestNoiseField` — ~550ms (down from 1.1s)

Already at 3 octaves; further octave reduction would flatten density variation. The remaining
option is **shared noise** — elevation and forest density both sample Perlin noise. With
appropriate frequency scaling, the elevation field (already computed) could be resampled or
warped to produce a usable density field at zero extra cost. Design question: does derived density
feel too correlated with terrain?

### 4. `DistanceToWater` BFS — ~258ms

Standard BFS over 1M tiles. Correct and cache-friendly; not obviously improvable without
parallelism. If worker threads are added (see Elevation above), this is a natural candidate to
run concurrently with town history simulation (which takes 1ms and is sequential).

### 5. `FarmWorldScene/TilemapBuild` — ~400ms

Scene cost, not world-gen. Converts WorldData to a streaming Tilemap. Runs after the world is
generated, on the main thread. Reducing world size or tiling the build into async chunks would
address this, but it is downstream of the Load Regions design work.

---

## Patterns to carry forward into future proc-gen

These are the root causes of the bottlenecks found in this effort. They are easy to introduce and
hard to spot without profiling. Treat them as design-time checklist items.

### P1: O(n × objects) on large grids

**What happened:** `PlaceRoadHubs/CandidateScan` evaluated every tile (1M) against every
town pair (~105 pairs) with 2 `sqrt` calls per combination = 210M `sqrt` calls → 2.8s.

**Pattern:** any algorithm of the form "for each tile, check all N objects" is O(tiles × N). On
a 1000×1000 world with N=100 objects, that's 100M iterations before any work is done.

**Rule:** before writing a per-tile loop that references a list of objects, ask whether a
per-object loop that marks tiles would be O(objects × footprint) instead. Spatial indexing or
bounding-box pre-filtering can reduce the constant dramatically when objects are clustered.

### P2: Parameter reuse across different concerns

**What happened:** `GeoFunnel` used `probeR = max(2, params.hubDiscountRadius)`. The
`hubDiscountRadius` (70 tiles) controls how far the route-cost discount radiates from a hub. The
probe radius controls how far to walk to detect a geographic choke-point. Mountain passes are 1–5
tiles wide; 70 tiles of walking was 7–14× too expensive. Fixing it to 5 tiles saved ~400ms.

**Rule:** if a parameter physically means one thing (discount attenuation radius) and is reused
for something else (feature detection radius), give the second use its own named parameter or
constant with its own rationale. Sharing saves a field but hides a 14× latency bug.

### P3: Fractal noise octaves vs consumer quality requirement

**What happened:** `GenerateHeightMap` was called for both elevation and forest density with 6
octaves. Elevation needs fine detail (octave 5–6 adds ridges and texture). Forest density is a
smooth modulation field — 3 octaves is more than sufficient. Using the same `FractalNoiseParams`
struct for both meant the density field was 2× too expensive (saved ~550ms with 3 octaves).

**Rule:** every `GenerateHeightMap` call costs approximately 190ms/octave at 1M tiles. Assign
the minimum octave count that satisfies each consumer's quality requirement. A density/weighting
field never needs as many octaves as a terrain-shape field.

### P4: Sorting when top-K is all that's needed

**What happened:** `SortAndSelect` sorted ~600K candidates with `std::ranges::sort` → 156ms.
Only 8 hubs are needed; the spacing filter needed at most ~100–200 candidates in order.
`std::partial_sort` with topK=800 reduced it to 7ms (22×).

**Rule:** whenever the downstream consumer reads only the first K elements of a sorted list,
use `partial_sort(begin, begin+K, end, cmp)` instead of full sort. For K ≪ N, `O(n log K)` vs
`O(n log n)` is a significant constant-factor improvement.

### P5: Float math rejection before integer rejection

**What happened:** the ellipse-corridor test used 2 `sqrt` calls per tile per pair regardless
of whether the tile was near the corridor. The algebraic identity `di+dj ≤ T ⟺ 4·di²·dj² ≤
(T²−di²−dj²)²` eliminates both sqrts using only integer arithmetic for tiles outside the
ellipse. Combined with a conservative AABB check for short-distance pairs, sqrt is now paid only
for tiles actually inside a corridor.

**Rule:** sort rejection checks cheapest-first. Integer/boolean comparisons → integer
multiplication → float multiply-add → sqrt → transcendental. A rejection that can be expressed
in integer arithmetic should precede any float operation in a tight inner loop.

### P6: Instrument proc-gen stages before the first commit

**What happened:** profiling zones were added after noticing slow load time. The diagnosis
required several iterations of zone refinement before the real bottleneck (`CandidateScan`)
was visible — it was hidden inside a parent zone that also contained `GeoFunnel`.

**Rule:** add `profiling::Zone` blocks to every pipeline stage (elevation, water, rivers,
history, layout, scatter, placement) when first authoring the stage. The cost is 3–5 lines per
stage; the diagnostic value is immediate. Nested sub-zones (e.g. separating the noise field from
the scatter loop) cost the same and reveal the structure on the first profiling run.

---

## Out of scope here

- **Load Regions / progressive loading** — chunked entity spawn and deferred generation.
  Awaiting profiling data to choose the right target. (**This ticket provides that data.**)
- **Parallelising world-gen stages** — elevation + water + rivers are independent and could run
  concurrently. Requires a threading model decision (std::async, job system, or Flecs worker
  threads).
- **Jump Point Search** — JPS for `RouteAStar` is the highest-value single algorithmic change.
  File a dedicated ticket before implementing.
