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

## Decisions

Settled 2026-10-24.

- **File.** `assets/levels/crabnado/arena.level.json`, loaded by `ArenaScene::Load` as children of the scene root,
  through the engine level module. Rocks and pools are entities with a `transform` (position) plus a game component.
  Register `rock` `{ "radius": r }` and `tide_pool` `{ "radius": r }` with `level::RegisterComponentLoader` in the
  arena scene's `RegisterLoaders` (the `TilemapScene::RegisterLoaders` pattern).
- **Visuals come from code, not JSON.** A rock gets a grey flattened `SpherePrimitive` child (dodecahedrons don't
  exist). A pool tints its water tiles a darker `#1d6680` (decided when the tiles are built). The JSON stays one
  line per rock or pool.
- **Arena edge** is a singleton in the JSON, `"arena_edge": { "radius": 70, "driftwood": 60 }`. Code places the
  driftwood boxes (brown, 2–4 long, at radius + 3). If it's simpler, keep `kArenaRadius` as the constant and only
  read the driftwood count. Clamp the player at r, enemies at r + 1 and the crabnado at r − 8 (already partly done).
- **Layout.** Copy the prototype's 10 `ROCKS` and 4 `POOLS` exactly. They were playtested.
- **Caches.** After the level loads, build `Rocks` and `TidePools` singletons (vectors of `{position, radius}`), so
  per-frame code (2,300 tiles and 1,000 enemies) never queries entities. `SampleWaves` takes the `TidePools` span.
- **Pool calm (prototype).** Inside radius r, scale height and push by `1 - 0.92 * clamp((r - d) / (0.4 r), 0, 1)`.
  Only the wave field reads pools. Wind push is unchanged in pools. "The horde gathers there" needs no new logic.
- **Rock collision.** Push out of overlap (`pushOutOfRocks`) for the player (radius 0.9) and landed enemies (their
  radius). The crabnado ignores rocks in the normal phase (it's a tornado). Ticket 12 adds it in the boss phase.
- **Slam (prototype).** A tumbling enemy that hits a rock with knock speed > 5 takes `(speed - 4) * 1.6` damage and
  flashes for 0.2 s. Its knock is then multiplied by −0.2. Add a `slammed` flag and count `RunState.rock_kills`
  (show it on the HUD; ticket 13's results screen shows it too).
- **"No rebuild" means no C++ recompile.** Assets are copied into the build directory after each build, so edit the
  copied `arena.level.json` (or rebuild, which only re-copies assets) and press Title → Play. The level loads on every
  scene Load. No file watcher is needed.
