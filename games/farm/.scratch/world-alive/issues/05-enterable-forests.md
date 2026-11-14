# 05: Enterable forests

**What to build:** Forest and deep-forest terrain tiles are currently `walkable: false`,
making the entire biome an impassable wall. This ticket makes forests explorable: the terrain
tile becomes walkable woodland ground, and impassable **tree world-objects** are scattered
across it. The player can walk into a forest, weave between trees, and reach clearings. Roads
and trails that cross a forest clear a navigable passage.

Tree density is driven by a secondary noise field that produces dense cores with sparse edges
and occasional clearings; deep-forest is denser and uses taller/darker tree variants. A small
JSON species catalog (keyed by biome) determines which trees appear where, so adding a new
species needs no C++ change.

Scale context: a 1000 × 1000 world has substantial forest coverage. Tree density should be
tunable via `WorldGenParams`; at the default, a player entering a forest edge should find some
open path forward rather than needing to hunt for a single passable gap.

**Blocked by:** 02 — World-object data model

**Status:** done

- [ ] Forest and deep-forest terrain tiles are set to `walkable: true` (or a new "woodland
  ground" tile); the player can enter these biomes.
- [ ] Tree world-objects are scattered across forest/deep-forest tiles, each with
  `blocksMovement: true`, so trees individually block movement while the ground between them
  is passable.
- [ ] Density varies spatially — dense cores, sparser edges — with occasional clearings (contiguous
  areas with no tree objects).
- [ ] Deep-forest tiles have noticeably higher tree density and taller/darker tree variants than
  regular forest.
- [ ] Road paths that cross forest tiles clear a passage — no blocking tree world-object is
  placed on a road tile within the forest.
- [ ] A JSON tree-species catalog maps biome kinds to species (sprite key, height range); adding
  a species requires no C++ change.
- [ ] Two calls with the same seed produce identical tree placements (determinism).
- [ ] Unit tests: in a generated world, forest tiles are marked walkable in the terrain layer;
  the fraction of forest tiles occupied by blocking trees falls within an expected density range;
  at least one clearing (contiguous un-treed area) exists in a typical forest; no tree object
  sits on a road tile.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commit: 67f7636 (enterable forests). Files: tree_species_catalog.{hpp,cpp} (new), assets/data/trees.json (new), enterable_forests_test.cpp (new), assets/data/tilesets/basic.json (forest walkable), world_gen.{hpp,cpp} (ScatterForestTrees + unified Generate overloads), world_data.hpp (tree density knobs). 226/226 tests pass.