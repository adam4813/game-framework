# 09: Villager spread

**What to build:** The existing villager system (schedules, dialog, behaviour trees, shops) is
complete but only spawns a handful of hard-coded villagers at the starting village. The rest of
the world is empty. This ticket scales and spreads the system across every living town in the
generated world.

Villager count per town scales with population. Each villager is drawn from a data-driven
archetype catalog (extended `villagers.json`) with a procedural name pool so no two worlds feel
identical. Archetypes that match building kinds (shopkeeper → shop, elder → hall) have their
home and workplace waypoints bound to the corresponding building tiles placed by ticket 04 —
so the daily schedule is grounded in the town's actual layout. Towns produced by the
crossroads-outpost post-pass (ticket 07) also receive inhabitants.

**Blocked by:**
- 03 — Upright-object renderer (NPC entities need dynamic upright quads)
- 04 — Town layout generation (home/work waypoints bound to building positions)

**Status:** done

- [ ] Every living town in the generated world (including crossroads outposts from ticket 07)
  has at least one villager at scene load; count scales with town population.
- [ ] Villager home and workplace waypoints are bound to building tile positions in their town's
  layout; a shopkeeper's workplace is a shop-kind building; an elder's is a hall-kind building.
- [ ] The archetype catalog and name pool are extended in `villagers.json` (or a companion JSON
  file); adding a new archetype requires no C++ rebuild.
- [ ] Each villager entity is rendered as a dynamic upright quad (ticket 03) so they are visible
  from the oblique camera.
- [ ] The existing starting-village villager behaviour (dialog, shop, schedule, interact) is
  unchanged; this ticket only adds villagers to additional towns.
- [ ] Integration test (headless via `SpawnVillagersHeadless`): after scene setup, villager
  entities exist as children of at least two distinct town locations.

## Comments
Integration branch: integration/farm-world-alive (tip ffa3a71). Key commits: 198fdf5 (villager spread), a6f533d (tests). Files: villager_system.{hpp,cpp} (TownVillagerCount, PlanTownVillagers, world-level SpawnVillagers), villager_components.hpp (TownRoot), villager_spread_test.cpp (new), assets/data/villagers.json (archetypes+names+town_villagers config). 246/246 tests pass. Runtime: 23 villagers/8 towns.