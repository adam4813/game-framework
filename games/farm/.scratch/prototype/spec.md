Status: ready-for-agent

# Spec: Farming Game Prototype — Foundation

## Problem Statement

We need to establish the playable foundation for a cozy sandbox farming game set in a large,
procedurally generated world with simulated history. The player should start in a village that
feels *embedded* in that history — surrounded by working farms, trade roads connecting distant
towns, and ruins of settlements that didn't survive — rather than spawning on a generic flat
starter map. Before building breadth (multiplayer, deep crafting, combat), the prototype must
prove that the three mutually reinforcing pillars of the game — world generation, core gameplay
loop (farm → harvest → trade/gift), and community — are viable on this framework.

## Solution

Build a prototype that:

1. Runs a deterministic world-generation pipeline (terrain, waterways, history simulation, town
   placement, road network) and selects a coherent starting village from the result.
2. Loads that village into a playable tilemap scene with a grid-locked player, a day/night time
   system, and a small cast of NPCs.
3. Proves the core farming loop: player plants a seed, waters it across game days, harvests the
   crop, and can gift or trade it to a villager.
4. Lays the game-agnostic groundwork (inventory, time, world interaction) so a second prototype
   can reuse those systems if the farm-specific design pivots.

## User Stories

### World Generation

1. As a player, I want the world to be different every seed so that each playthrough feels new.
2. As a player, I want the landscape to feel natural — hills, rivers, forests, and open fields —
   so that navigation and farm-site choices are meaningful.
3. As a player, I want visible ruins of abandoned settlements so that the world feels old and
   storied rather than freshly generated.
4. As a player, I want roads connecting my village to distant towns so that I have a sense of
   where to explore.
5. As a developer, I want to preview a generated world on a full-screen map with pins so that I
   can tune generation parameters and verify town/road layout quickly.
6. As a developer, I want world generation to be deterministic from a seed so that bugs are
   reproducible.
7. As a developer, I want the history simulation to produce at least one abandoned settlement per
   world so that ruins always appear.

### Time System

8. As a player, I want days to pass as I move around and farm so that my crops grow over time.
9. As a player, I want to see what day and season it is so that I can plan which crops to plant.
10. As a player, I want the world to visibly change between day and night so that time
    progression feels tangible.
11. As a developer, I want to advance time programmatically in tests so that growth and decay
    logic is verifiable without waiting.

### Player and World Interaction

12. As a player, I want to move around the world tile by tile so that navigation feels
    deliberate.
13. As a player, I want to see what action is available when I stand next to a tile so that
    interactions are discoverable.
14. As a player, I want to dig a patch of land so that I can prepare it for planting.
15. As a player, I want to water tilled soil so that crops grow faster or at all.
16. As a player, I want to harvest a ready crop by interacting with it so that the core loop
    completes.
17. As a player, I want a tool equipped in my hand to determine which interaction fires so that
    choosing the right tool matters.

### Inventory

18. As a player, I want to carry a small set of items in a hotbar so that I can switch tools and
    hold crops without a menu.
19. As a player, I want picked-up items to fill my inventory automatically so that harvesting
    feels frictionless.
20. As a player, I want to see my current inventory on screen so that I always know what I'm
    carrying.
21. As a player, I want inventory to persist across save/load so that I don't lose items.
22. As a developer, I want inventory to be a game-agnostic ECS component so that a future
    prototype or engine module can reuse it without farming-specific coupling.

### Farming Loop

23. As a player, I want to buy or find seeds at the village shop so that starting a farm has a
    small resource gate.
24. As a player, I want to plant a seed in tilled soil so that farming begins.
25. As a player, I want crops to grow through visible stages (planted → sprouting → mature →
    harvestable) across game days so that I can see progress.
26. As a player, I want un-watered crops to grow more slowly (or wither in dry seasons) so that
    watering has consequence.
27. As a player, I want to harvest a mature crop and have it added to my inventory so that the
    loop closes.
28. As a player, I want at least three crop types with different growth durations so that I can
    make planting decisions.
29. As a player, I want a basic seasons system (spring/summer/autumn/winter) so that some crops
    are seasonal and the world visually cycles.

### Community and NPCs

30. As a player, I want to see named villagers going about their day so that the starting village
    feels alive.
31. As a player, I want to talk to a villager and receive a brief greeting or hint so that NPCs
    feel purposeful.
32. As a player, I want to give a crop to a villager so that community building has a tangible
    first step.
33. As a player, I want a simple shop NPC where I can buy seeds and sell crops so that the
    economy bootstraps.
34. As a developer, I want NPC schedules to be data-driven (JSON) so that adding new villager
    archetypes doesn't require C++ changes.

### Map and Navigation

35. As a player, I want to open a full-screen world map with pins showing my location and key
    sites so that I can orient myself in the generated world.
36. As a player, I want a persistent minimap in the corner of the screen showing nearby terrain
    and pins so that local navigation is always available.
37. As a developer, I want the map and minimap to be built with the engine's UI component system
    (not ImGui) so that they are consistent with the rest of the game UI and ship in release
    builds.

### World Persistence

38. As a player, I want the generated world to save so that I don't regenerate it on every
    launch.
39. As a player, I want my farm state (tilled plots, planted crops, growth stages) to persist
    across sessions.
40. As a player, I want my inventory and current day to persist across sessions.

## Implementation Decisions

### New `farm` game module

All farming-specific code lives under `src/game/farm/` as its own sub-namespace (`game::farm`).
It is registered via `world.import<farm::FarmModule>()` inside `GameModule`. This isolates farm
code from the existing demo scenes and makes it trivially removable if the prototype is
abandoned.

### World generation is a pure C++ data pipeline

`farm::WorldGen` is a plain C++ class (no ECS dependency) with a single entry point:
`WorldGen::Generate(WorldGenParams, seed) → WorldData`. `WorldData` holds:

- An elevation grid (float heightmap)
- A biome grid (enum per tile: water, beach, plains, forest, hills, mountain)
- A river network (list of tile paths)
- A list of `TownSite` records (location, size, alive/ruined, founding/collapse era)
- A `RoadGraph` connecting living towns (weighted by population and trade distance)
- A `HistoryLog` of notable events (town founded, town collapsed, road built)

The noise module (`NoiseRegistry::GetSampler`) drives terrain generation. History simulation
iterates population cycles: towns grow or shrink, some collapse (producing ruin sites), roads
form between surviving towns above a population threshold.

### Starting village selection

After generation, `farm::SelectStartingVillage(WorldData) → TownSite` picks a living village
that meets minimum criteria: not coastal, not mountain, has at least one road connection, has
at least one adjacent ruined settlement within exploration range. This is the player spawn site.

### World map and minimap use the UI component system

A full-screen `WorldMapScene` and an always-on minimap overlay are built entirely with the
engine's UI components (`Panel`, `Button`, texture-mapped panels for the map surface, and
`Pin` overlays for NPC/player markers). This approach is based on an existing early prototype on
a separate branch and deliberately avoids ImGui so that the map ships in release builds and
stays consistent with the rest of the game UI. The map renders the `WorldData` biome grid as
a texture (generated once at world-gen time) and overlays pins for towns, ruins, the player,
and tracked NPCs.

### Tilemap-based world rendering

The world loads into the existing `tilemap` engine module. Tile types map from biome grid
values. `FarmWorldScene::Load()` builds the tilemap entity from `WorldData`, spawns NPC entities
at town positions, and places the player entity at the starting village spawn point.

### Player movement reuses `GridMover`

The existing `GridMover` component and `RegisterGridMovementSystems()` are used as-is.
`FarmWorldScene` calls `RegisterGridMovementSystems(world, sceneRoot, tilemapEntity)` exactly
as `TilemapScene` does.

### `GameTime` singleton (game-agnostic)

`game::GameTime` is a Flecs singleton (`world.set<GameTime>({})`) tracking: `tick`, `day`,
`season` (Spring/Summer/Autumn/Winter), `hour` (0–23), `phase` (Dawn/Day/Dusk/Night). A
`TimeAdvanceSystem` (Pausable) increments tick each frame; day/season roll over at configured
tick thresholds. The singleton and system are registered in `FarmModule` but carry no
farm-specific coupling — they are candidates to move to an engine module later.

### `PlayerInventory` component (game-agnostic)

`game::PlayerInventory` is a fixed-capacity item slot array on the player entity. An `Item` is
a plain struct: `{ std::string typeId; int count; }`. The component is registered with
`LevelRegistry` so items can be authored in JSON save data. No farm-specific item types live in
the component definition — crop type IDs are strings resolved at runtime.

### `WorldInteraction` system

A `WorldInteractionSystem` (OnUpdate, Pausable) runs each frame. It:

1. Finds the tile the player is facing (player position + facing direction).
2. Looks up what ECS components that tile entity carries (`TilledSoil`, `FarmPlot`, `Crop`,
   etc.).
3. Determines the available interaction given the player's equipped tool and the tile state.
4. On player confirm input, dispatches the interaction as a component mutation or event.

This is the single seam for all "player does something to the world" logic. New interaction
types add a new component and a new branch in the dispatcher — no new systems.

### `FarmPlot` cluster

Three components compose a farmable tile:

- `TilledSoil` — marks the tile as prepared; present after digging, removed if neglected for a
  full season.
- `Crop { std::string typeId; int dayPlanted; bool watered; }` — present after planting.
- `GrowthStage { Stage stage; int daysAtStage; }` — driven by `CropGrowthSystem` which
  advances stages based on `GameTime::day`, the crop type definition, and whether watered.

Crop type definitions live in `assets/data/crops.json` (typeId, name, growth days per stage,
seasonal validity, sell price). Adding a new crop is a JSON entry only.

### NPC archetypes are data-driven

Villager archetypes (Farmer, Shopkeeper, Elder) are JSON level entities. Each carries a
`VillagerSchedule` component (authored in JSON) and uses the `ai` module's behavior tree for
movement. Dialog trees reuse the existing `dialog` module (`src/game/dialog/`). Gifting fires
a `GiftReceived` Flecs event; the NPC's dialog responds to it.

### Save/load uses existing `save` module

The `save` module's schema-driven JSON save/load is extended with farm-specific schemas:
`WorldData` (serialized once at world gen), `PlayerInventory`, `FarmPlot` grid state, and
`GameTime`. Registration follows the `RegisterMetaSaveExample` pattern already in `game.cpp`.

## Testing Decisions

**What makes a good test here:** test observable state transitions and data-pipeline outputs,
not internal implementation steps. A test should express a farming rule ("a watered crop
advances one stage per day") not an implementation detail ("CropGrowthSystem calls
AdvanceStage()").

### Seams and test types

| Seam | Test type | Prior art |
|---|---|---|
| `WorldGen::Generate` | Unit — pure function, no ECS required | `tests/` existing test harness |
| `GameTime` singleton | ECS component state — advance ticks, assert day/season rollover | `meta_save_example` pattern |
| `PlayerInventory` | ECS component state — add/remove items, assert slot counts | `meta_save_example` pattern |
| `FarmPlot` growth | ECS time-advance — set components, tick `GameTime::day`, assert `GrowthStage` | `meta_save_example` pattern |
| `FarmWorldScene` load | Integration — scene loads, starting village entity exists, tilemap non-empty | `TilemapScene` pattern |

### Modules under test

- `farm::WorldGen` (pure C++ unit tests)
- `farm::GameTime` + `farm::TimeAdvanceSystem`
- `farm::PlayerInventory`
- `farm::FarmPlot` + `farm::CropGrowthSystem`
- `farm::FarmWorldScene` (integration, scene-load smoke test)

## Out of Scope

- **Multiplayer** — drop-in, seamless travel between player worlds; this is the largest
  unknown and warrants its own `/grill-with-docs` session once the single-player foundation is
  stable, because the session/world-ownership model is a hard-to-reverse architectural decision.
- **Combat** — enemies, health, weapons; exists in the wishlist but is not a prototype risk.
- **Full automation** — pipes, conveyor belts, machines.
- **Deep crafting tree** — more than a handful of recipes at a workbench.
- **Interior / dungeon exploration** — ruins are surface-visible only in this prototype.
- **Full relationship system** — gift-giving is present; friendship meters, marriage, and NPC
  events are not.
- **Weather system** — beyond seasonal growth-rate modifiers.
- **Full quest system** — villager hints are dialog only; no tracked objectives.
- **Scene editor** — world gen preview uses the existing UI component map; no visual placement
  editor.
- **Engine-layer promotion** of `GameTime` / `PlayerInventory` — noted as candidates but left
  in the game layer for the prototype.

## Further Notes

- The `plan/data-driven-levels.md` note applies here: farm entities that are tricky to
  hand-author in JSON should start as C++ scene setup code and migrate to JSON once a scene
  editor exists.
- The `graph` engine module is a natural fit for the `RoadGraph` inside `WorldData`; use its
  ECS entity-component representation if the road network needs to be queryable at runtime
  (e.g. NPC travel routing), or keep it as a plain C++ value type inside `WorldData` if it only
  drives tilemap tile selection at load time.
- The world map / minimap prototype lives on a separate branch; bring that branch's relevant
  code forward before implementing map UI rather than rebuilding it from scratch.
