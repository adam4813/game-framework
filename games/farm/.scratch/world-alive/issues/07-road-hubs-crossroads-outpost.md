# 07: Road hubs + crossroads-outpost post-pass

**What to build:** With organic routing in place, this ticket adds two complementary forces
that make the road network feel like it grew from geography and commerce rather than a
shortest-path algorithm.

**Hubs / nexus nodes** are placed at sparse geographic funnel points — river fords and
confluences, mountain passes, isthmuses — that are naturally central to multiple towns. Each
hub applies a soft cost discount in the routing field (an attractor, never a forced waypoint)
so nearby roads tend to converge through it. A small marker world-object (signpost, shrine, or
well) is placed where roads actually use a hub, so the convergence reads as a meeting-place.

**Towns as attractors** extend the same idea: larger, better-connected towns get a larger cost
discount, so the network naturally develops "all roads lead to Rome" topology without any
special-case logic.

**Crossroads-outpost post-pass** caps the hub concept: if a hub accumulates very high road
convergence and has no real town nearby, a small young living town (a crossroads outpost) is
promoted at that location after routing completes. The outpost receives inhabitants in ticket 09.

**Blocked by:** 06 — Spatial road router

**Status:** done

- [ ] A sparse set of hub nodes is placed at geographic funnel points (river ford/confluence,
  mountain pass, isthmus) that are central to multiple towns; hub count and minimum spacing are
  `WorldGenParams` knobs.
- [ ] Each hub applies a configurable soft cost discount to the routing field; the discount makes
  routes through the hub cheaper but never mandatory.
- [ ] A marker world-object (signpost / shrine) is placed at each hub that at least one realised
  road route passes through.
- [ ] Towns contribute a routing attractor scaled by their size and road-connectivity so
  well-connected large towns attract more routes than isolated small ones.
- [ ] A post-pass promotes at most a bounded number of high-convergence hubs with no nearby
  living town into small living "crossroads outpost" towns; each promoted outpost appears in the
  `WorldData` town list and timeline.
- [ ] Two calls with the same seed produce identical hub placements and outpost promotions
  (determinism).
- [ ] Unit tests: generated hubs respect minimum spacing; at least one hub marker world-object
  is placed in a typical world; a promoted outpost is a living town present in `WorldData.towns`
  and in the generation timeline.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commits: 72abcb6 (hubs+attractors+outposts), 54ade42 (tests). Files: road_hubs.{hpp,cpp} (new), road_hubs_test.cpp (new), road_router.{hpp,cpp} (optional 6th costDiscount param), world_gen.cpp (hub/outpost stages), world_data.hpp (hub/outpost knobs). 273/273 tests pass. Note: outpost AC uses 300x300 scale (not enough space at 160x160 for crossroads).