# 07: `FarmPlot` and crop growth

**What to build:** The farming half of the core loop, building on the interaction seam (ticket
06) and the time system (ticket 02). Three ECS components compose a growable tile:
`TilledSoil` (added by the hoe interaction), `Crop { typeId; dayPlanted; watered; }` (added
when a seed is planted into `TilledSoil`), and `GrowthStage { stage; daysAtStage; }` (driven
by `CropGrowthSystem`). `CropGrowthSystem` (Pausable, OnUpdate) advances `stage` based on
elapsed `GameTime::day` values, the crop type's per-stage growth durations (from `crops.json`),
and whether the crop was watered the previous day; `watered` resets to false each morning.
`TilledSoil` that holds no `Crop` for a full season is reclaimed (component removed).

`assets/data/crops.json` defines at least three crop types with distinct growth durations,
seasonal validity, and `harvestTool` (the tool required to harvest, or empty for bare-hand).

**Blocked by:** 02, 06

**Status:** done

- [x] Planting a seed item into `TilledSoil` adds `Crop` and `GrowthStage` (stage: Planted)
- [x] `CropGrowthSystem` advances stage when the required number of days has elapsed
  (watered crops advance on schedule; unwatered crops advance more slowly or not at all,
  per crop type definition)
- [x] `watered` flag resets to false at the start of each new day
- [x] A harvestable crop is correctly identified and the harvest interaction becomes available
  with the right tool equipped (or no tool for bare-hand crops)
- [x] Harvesting a mature crop adds the typeId item to `PlayerInventory` and removes the
  crop components
- [x] `TilledSoil` with no `Crop` for a full season reverts to bare ground
- [x] `crops.json` contains at least three crop types covering: short growth (a few days),
  medium growth (a season), and a seasonal restriction (only valid in one season)
- [x] Unit tests: watered vs unwatered growth rate, stage advancement, season-invalid
  planting is blocked, bare-ground reclaim after a full season
