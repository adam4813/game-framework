# 08: Bridges

**What to build:** The spatial router (ticket 06) blocks routes on deep water and ocean. Most
towns are reachable by land. But islands and water-locked regions may genuinely have no land
path. This ticket adds bridges: walkable world-objects placed on a narrow water crossing, making
the crossing passable.

Bridges arise two ways: (1) **routing fallback** — when the router can find no land path for a
wanted connection, it bridges the narrowest viable crossing found by a secondary search; (2)
**historical investment** — a new prosperity-gated `HistoryEventKind` entry fires when a
sufficiently thriving town or well-connected network chooses to build a bridge that meaningfully
shortens a route or reaches an isolated place. The event is recorded in the generation timeline
so the bridge reads as a deliberate act of civilisation.

A `maxBridgeSpan` parameter caps crossing length. A forced crossing that exceeds the cap is
left unbuilt — the town or region stays isolated. This prevents implausible ocean-spanning
bridges.

**Blocked by:** 06 — Spatial road router (parallel with 07 — no dependency between hubs and bridges)

**Status:** done

- [ ] When the router finds no land A* path for a wanted connection, it attempts to locate the
  narrowest water crossing (in tiles) between the two landmasses and places bridge world-objects
  across it.
- [ ] The bridge span does not exceed the `maxBridgeSpan` parameter; connections requiring a
  longer crossing are left unbuilt and the place remains isolated.
- [ ] A prosperity-gated `HistoryEventKind` can trigger a deliberate bridge investment; the
  event is recorded in the `WorldData` generation timeline with a year and description.
- [ ] Bridge world-objects placed on water tiles make those tiles walkable so the player and
  NPCs can cross.
- [ ] Two calls with the same seed produce identical bridge placements (determinism).
- [ ] Unit tests: no bridge is placed where a land path exists (bridge is truly a last resort or
  deliberate investment); no bridge spans more tiles than `maxBridgeSpan`; a world seeded to
  include an island town contains a bridge if the crossing is within the cap, or the town is
  isolated if the crossing exceeds the cap; a bridge created by the history event appears in the
  generation timeline.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commits: 62d8351 (bridges), 6e3fa2d (tests). Files: bridge_builder.{hpp,cpp} (new), bridges_test.cpp (new), world_data.hpp (Bridge + BridgeReason + WorldData::bridges + maxBridgeSpan), world_gen.cpp (BuildBridges stage + HistoryEventKind bridge). 257/257 tests pass.