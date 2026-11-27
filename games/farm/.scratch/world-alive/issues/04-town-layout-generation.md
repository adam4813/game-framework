# 04: Town layout generation

**What to build:** Towns currently render as solid grey squares of `stone_floor` tiles scaled
by population. This ticket replaces that with a generated **village layout**: a central landmark
(well or plaza), building footprints arranged along short streets, a few decorative crop-field
plots around farm buildings, and visually distinct living vs. ruined appearances. All buildings
are world-objects (walls + roof quads rendered by ticket 03) that block the tiles they occupy;
floors and doors are terrain tiles that remain walkable.

Building kind definitions (house, shop, barn, hall) live in a JSON catalog so adding a new kind
requires no C++ rebuild. Some kinds map to villager archetypes (shopkeeper → shop, elder → hall)
so the town layout informs NPC placement in ticket 09.

Scale context: at 1000 × 1000 tiles a living town's full footprint (buildings + streets) must
cover ≤ 3% of the world area so distant towns don't bleed into each other. Building count
scales with population; the largest plausible town should still leave clear terrain around it.

**Blocked by:** 02 — World-object data model

**Status:** done

- [ ] `WorldGen::Generate` places building world-objects and floor/door terrain tiles for every
  town; the layout contains a central landmark, at least two buildings for a minimum-population
  town, and scales with population.
- [ ] Living towns use intact building variants; ruined towns use broken/rubble variants.
- [ ] Each building has at least one door tile (walkable, terrain) in its wall, so the player
  can enter.
- [ ] A few decorative `tilled_soil` field-plot tiles appear near farm-type buildings (visual
  only — not interactive farm plots).
- [ ] Building kinds (house, shop, barn, hall) are defined in a JSON catalog; adding a new kind
  requires no C++ changes.
- [ ] A living town's total footprint (buildings + streets + fields) covers ≤ 3% of a
  1000 × 1000 world so towns remain spatially distinct.
- [ ] Two calls with the same seed produce identical layouts (determinism).
- [ ] Unit tests: every living town has ≥ 1 landmark object and ≥ 1 building; every building
  world-object has a corresponding walkable door tile; ruined towns have rubble variants; layout
  scales — a larger-population town has more buildings than a smaller one.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commits: afd38c6 (generate village layouts), c6616ad (tilemap painting), 21ee32a (tests). Files: building_catalog.{hpp,cpp} (new), town_layout.{hpp,cpp} (new), assets/data/buildings.json (new), town_layout_test.cpp (new), world_data.hpp (TownLayout + TownBuilding + WorldData::townLayouts), world_gen.{hpp,cpp}, farm_tilemap_builder.cpp. 201/201 tests pass. Play-scale 1000x1000 verified.