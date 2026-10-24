# 08: Tide-pool arena

Spec: [spec](../spec.md). Read: [Arena and boss](../spec/arena-and-boss.md) (arena), [Wind and waves](../spec/wind-and-waves.md) (pools).

**What to build:** The game scene loads its arena from a level JSON file: a circular shore (radius about 70 units)
edged with driftwood, with rocks and tide pools placed by hand. Rocks block the player and the horde. A
tumbling enemy carried into a rock takes damage scaled by impact speed, enough to kill crabs outright: a rock slam.
Inside a tide pool the wave field's push is near zero, so pools are calm pockets, and the horde can gather there too.
Rock and pool components load through game-registered level loaders.

**Blocked by:** 05 (Waves and tumble).

**Status:** ready-for-agent

- [ ] The arena (rocks, pools, edge) comes from level JSON, and moving a rock in the JSON moves it in game with no
      rebuild.
- [ ] Rocks stop the player and walking enemies.
- [ ] A crest that tumbles crabs into a rock kills them (a visible slam), and a crest that misses rocks only tumbles.
- [ ] Standing in a pool, crests don't move the player or enemies.
- [ ] No one can leave the arena edge.
- [ ] Checked in the running game (e.g. query rock and pool entities and watch a slam over the Remote API).
