# 09: Save and load world state

**What to build:** Full persistence so the player can quit and resume exactly where they left
off. The `save` module's schema-driven JSON save/load is extended with four schemas: `WorldData`
(serialized once at world-gen time, reloaded on subsequent launches so generation doesn't
re-run), `PlayerInventory` (item slots and equipped slot index), farm grid state (which tile
entities carry `TilledSoil`, `Crop`, and `GrowthStage` and with what values), and `GameTime`
(current tick, day, season, hour). On launch, if a save file exists the game loads it and skips
world generation; otherwise it generates a new world and saves it immediately.

The `farm-scene` branch contains prior save/load mechanics and UI that cover much of this
scope — check out that branch (a Git worktree is useful) and bring the relevant code forward
rather than rebuilding.

**Blocked by:** 04, 07, 08

**Status:** done

- [x] On first launch a new world is generated and immediately saved
- [x] On subsequent launches the saved `WorldData` is loaded; world generation does not re-run
- [x] `PlayerInventory` (items and equipped slot) persists across quit/resume
- [x] `GameTime` (tick, day, season, hour) persists across quit/resume
- [x] All `TilledSoil`, `Crop`, and `GrowthStage` component values persist across quit/resume
- [x] A "Continue" or "New Game" option on the title screen reflects whether a save file exists
- [x] Deleting or starting a new game correctly clears the old save and triggers fresh world gen
- [x] Save and load use the existing `save` module registration pattern
