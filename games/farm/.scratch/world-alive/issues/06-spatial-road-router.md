# 06: Spatial road router

**What to build:** Roads are currently drawn as Bresenham straight lines between town centres,
ignoring terrain and other towns. Because the road graph is just a set of `(townA, townB)` id
pairs with no spatial awareness, a near-collinear connection can be routed straight through a
third town sitting between the endpoints, producing bisected settlements and geometrically
implausible routes.

This ticket replaces the Bresenham realisation with a **least-cost tile router** that produces
organic, terrain-respecting routes. History still decides which town pairs want connections; the
router decides *how* each connection is laid.

The router applies: biome-weighted movement costs (cheap on plains/beach, costlier through
forest — which it clears — blocked on mountain and deep water); an existing-road-tile discount
so new routes merge onto shared trunks and junctions; a town-footprint avoidance cost so roads
approach from the edge, never through the centre; and a redundancy filter that skips a connection
when the already-realised network links those towns within a configurable detour factor.

**Blocked by:** 04 — Town layout generation (town footprints must be known to route around them)

**Status:** done

- [ ] Each road segment's tile path is produced by a least-cost (A*-style) router, not Bresenham;
  paths curve to follow low-cost terrain.
- [ ] Biome movement costs are data-configurable via `WorldGenParams`; mountain tiles and deep
  water tiles are blocked (no road crosses them without a bridge).
- [ ] Road tiles already laid by earlier segments cost significantly less to traverse, so new
  routes naturally share trunks and junctions rather than running as parallels.
- [ ] Town footprint tiles (building walls, floors, streets) are assigned a high avoidance cost;
  roads approach from the town's edge and do not pass through the interior.
- [ ] A redundancy filter skips a wanted connection when the realised road network already links
  those two towns within a detour-factor multiple of the direct tile distance; the detour factor
  is a `WorldGenParams` knob.
- [ ] Two calls with the same seed produce identical road paths (determinism).
- [ ] Unit tests: generated road paths contain no mountain or deep-water tiles; no road path
  passes through a town's building footprint; when towns A, B, C are collinear with B between
  A and C, an A–C road does not bisect B; a connection redundant within the detour factor is
  absent from the road graph.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commits: e8bd23a (A* router), f5520e4 (tests). Files: road_router.{hpp,cpp} (new), road_router_test.cpp (new), world_data.hpp (road cost knobs in operator==), world_gen.cpp (Bresenham removed, BuildRoadNetwork). 233/233 tests pass. Design deviation: shortest-first ordering (documented in code, rationale: triangle-inequality makes longest-first redundancy inert).