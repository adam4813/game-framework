# 04: `FarmWorldScene` and player movement

**What to build:** The first playable scene for the farm prototype. It loads the `WorldData`
produced by `WorldGen` into a tilemap entity (biome grid → tile types), places the player
entity at the starting village spawn point with `GridMover`, follows the player with a camera,
and ticks `GameTime`. A hotbar UI overlay shows the player's current inventory slots along
the bottom of the screen. The scene must reach a state where a developer can walk around the
generated village, see the surrounding terrain (forest, hills, ruins marker tiles), and
confirm the time of day is advancing. Uses the existing `RegisterGridMovementSystems` and
`GridMover` as-is; no new movement code.

The `farm-scene` branch contains prior art for the scene structure, tilemap loading, and
UI layout — bring it forward rather than starting from scratch.

**Blocked by:** 01, 02, 03

**Status:** done

- [x] Scene loads without errors and activates from the title screen
- [x] The tilemap renders the generated world with at least five distinct tile visuals
  (water, plains, forest, hills, road)
- [x] Player spawns at the starting village and can move in all four directions
- [x] Camera follows the player smoothly
- [x] `GameTime` ticks; the current day, season, and phase are visible on screen
- [x] Hotbar renders the player's inventory slots; equipped slot is highlighted
- [x] Ruin sites are marked with a distinct tile or overlay so they are discoverable
- [x] Integration test: scene loads, player entity exists, tilemap entity is non-empty
