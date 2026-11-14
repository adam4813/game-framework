# 01b: World generation preview scene and timeline viewer

**What to build:** A throwaway developer scene that makes world generation immediately
inspectable, without waiting for the full `FarmWorldScene` (ticket 04). The screen is divided
into two non-overlapping panels: a scrollable, zoomable tilemap on the left and a timeline
panel on the right.

The tilemap renders the world state at the currently selected timeline step at 1:1 resolution
(one screen tile per `WorldData` cell). The camera zooms out on load to show the entire
generated world at once; the developer can then zoom in and pan to inspect detail. The existing
8-tile tileset (`tileset_basic.png`: GRASS, FOREST, PATH, WATER, STONE_FLOOR, STONE_WALL, SAND,
DEEP_FOREST) covers the base biomes — extend `tools/generate_tilemap_textures.py` with new
tile types (e.g. HILLS, DEEP_WATER, RUINS) if additional visual distinction is needed.

The timeline panel lists all `GenerationTimeline` entries from `WorldData` as labelled buttons,
plus Previous/Next navigation buttons at the top and bottom. Clicking an entry or pressing
Previous/Next updates the tilemap to the cumulative world state at that step. Town and ruin
sites are overlaid as pins; road segments are drawn over the tile layer.

The UI is intentionally throwaway (basic Panel/Button components — whatever is fastest) since
ticket 05 supersedes it.

**Blocked by:** 01

**Status:** done

- [x] A `WorldGenPreviewScene` activates from the title screen (debug/dev builds only)
- [x] The tilemap panel and timeline panel are side by side with no overlap
- [x] Camera zooms out on load to show the entire world; pan and zoom are interactive
- [x] Biomes render using the existing tileset; new tile types added to the Python script if
  needed (run `python tools/generate_tilemap_textures.py` to regenerate)
- [x] Town sites are overlaid as pins; living and ruined are visually distinct
- [x] Road segments are drawn over the tilemap
- [x] The timeline panel lists every `GenerationTimeline` entry as a clickable button
- [x] Previous and Next buttons navigate one step at a time
- [x] Selecting any step re-renders the tilemap to the cumulative world state at that step
- [x] A seed input field lets the developer regenerate with a new seed without restarting


## Comments

**Done** — implemented on `feat/farm-prototype` (`src/game/farm/worldgen_preview_scene.*`, title-screen
debug button). Pan/zoom biome map, cumulative timeline stepping with highlighted active step and
auto-scroll-to-centre, seed field + Regenerate, Export Report / Export Image.

Note: biomes render as solid per-biome colours (immediate-mode `Platform` 2D), not the 3D tileset.
The 2D draw API cannot sub-sample a tile atlas, and solid colours are the clearest surface for tuning
generation; this throwaway dev scene is superseded by the in-game map (ticket 05). Commits: b6b51e4,
e17a2bd, 4e1a183.
