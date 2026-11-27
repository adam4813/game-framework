# 05: In-game world map and minimap

**What to build:** A full-screen world map the player can open/close during gameplay, plus a
persistent minimap overlay in the HUD corner. Both are built entirely with the engine's UI
component system (not ImGui) so they ship in release builds. The map renders the `WorldData`
biome grid as a pre-generated texture with pins for living towns, ruin sites, and the player's
current position. The minimap shows the local area around the player at a fixed zoom. The
player pin updates in real time as the player moves.

The `farm-scene` branch contains a prior implementation of both map and minimap — check out
that branch (a Git worktree is useful) and bring the relevant code forward rather than
rebuilding. The user can provide knowledge of what's there if needed.

**Blocked by:** 01, 04

**Status:** done

- [x] Full-screen map opens and closes via a keybind or dedicated UI button
- [x] Map renders the biome grid as a texture with at least five distinct colours
- [x] Living town pins and ruin pins are distinct; player position pin is visible
- [x] Player position pin updates in real time as the player moves
- [x] Persistent minimap is visible in a HUD corner during normal gameplay
- [x] Minimap shows the local area around the player with matching pin types
- [x] Map and minimap use only engine UI components (no ImGui)
