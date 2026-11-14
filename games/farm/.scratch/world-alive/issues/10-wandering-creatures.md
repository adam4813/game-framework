# 10: Wandering creatures

**What to build:** The world has no ambient wildlife — no animals near farms, no deer in
clearings, nothing to suggest the land is lived-in beyond the player and villagers. This ticket
adds a light layer of wandering creatures: livestock near towns, and a small variety of wildlife
in forests and plains.

Creatures are placed sparsely from a JSON catalog keyed by biome/proximity. Each placed
creature spawns as an entity using the existing `bt_wanderer` behaviour tree for movement and
the ticket 03 upright-sprite renderer for visuals. Creatures are non-interactive for now —
no dialog, no harvesting, no combat.

Because creature counts are small and placement is purely a `WorldGen::Generate` output stage
(added to the `WorldData` world-object collection), this ticket can proceed in parallel with
the road and town tickets once the data model and renderer exist.

**Blocked by:**
- 02 — World-object data model (creature placements live in `WorldData`)
- 03 — Upright-object renderer (creatures need dynamic upright quads)

**Status:** done

- [ ] A JSON creature catalog defines kinds with biome affinity, proximity rules (near-town /
  wilderness), wander radius, and sprite key; adding a new kind requires no C++ change.
- [ ] `WorldGen::Generate` places a small number of creature spawn records in `WorldData` (the
  world-object or a companion collection) distributed by biome/proximity rules.
- [ ] At scene load, each creature record spawns an entity with the `bt_wanderer` behaviour tree
  and an upright-sprite quad (ticket 03).
- [ ] Creatures are non-interactive: the player can walk past them; no dialog, no harvesting,
  no damage.
- [ ] Creature counts remain sparse — a typical town has 1–4 livestock nearby; a forest area
  has 0–3 wildlife — so the world does not feel overcrowded.
- [ ] Two calls with the same seed produce identical creature placements (determinism).
- [ ] Unit test: generated creature placement counts per biome fall within the expected sparse
  range; no creature is placed on a non-walkable tile.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commits: a1dc298 (worldgen placement), 8dbdea1 (scene spawn), cbe7b44 (tests); merge required manual repair (coordinator). Files: creature_catalog.{hpp,cpp} (new), creature_placement.{hpp,cpp} (new), creature_system.{hpp,cpp} (new), creature_components.hpp (new), assets/data/creatures.json (new), world_data.hpp (CreatureSpawn + WorldData::creatures + creature params), world_gen.{hpp,cpp} (5-arg Generate), farm_world_scene.cpp. 289/289 tests pass.