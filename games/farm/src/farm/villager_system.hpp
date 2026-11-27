#pragma once

#include <string>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>
#include <nlohmann/json_fwd.hpp>

#include "villager_components.hpp"

namespace game::farm {

struct WorldData;
struct TownSite;
struct TownLayout;

/// Number of villagers a living town of the given `population` should have: the per-capita
/// fraction rounded to the nearest whole villager, clamped to [minCount, maxCount]. A living
/// town always gets at least `minCount`; an empty/ruined town (population <= 0) gets none.
/// Pure + file-free so the scaling rule is unit-testable in isolation.
[[nodiscard]] int TownVillagerCount(int population, double perCapita, int minCount, int maxCount);

// ── Data-driven archetype catalog (ticket 09) ──────────────────────────────────

/// One priced item line in an archetype's shop (shopkeeper buy/sell tables).
struct VillagerShopItem {
	std::string typeId;
	int price = 0;
	int count = 1;
};

/// One slot of an archetype's daily routine. `place` is a symbolic location ("home" or "work")
/// resolved to a concrete town-layout tile at spawn time.
struct VillagerScheduleSlot {
	int startHour = 0;
	std::string place = "home";
};

/// A data-driven villager archetype loaded from villagers.json. Adding an archetype is a JSON
/// entry with no C++ rebuild. `buildingKind` ties the archetype's workplace to a building kind
/// ("shop" -> shopkeeper, "hall" -> elder); empty means the workplace is a field/home.
struct VillagerArchetype {
	std::string id;
	int weight = 1;
	std::string buildingKind;
	std::string dialogGreeting;
	std::vector<VillagerScheduleSlot> schedule;
	std::vector<VillagerShopItem> sellItems;
	std::vector<VillagerShopItem> buyItems;
};

/// The parsed catalog: archetypes, the procedural name pool, and the population-scaling knobs.
struct VillagerCatalog {
	std::vector<VillagerArchetype> archetypes;
	std::vector<std::string> namePool;
	double perCapita = 0.04; // villagers per unit of town population
	int minPerTown = 1;      // every living town gets at least this many
	int maxPerTown = 8;      // cap so the largest town stays bounded
};

/// Pure parse of a villagers JSON document into a catalog (archetypes / name_pool / town_villagers).
/// Missing sections fall back to defaults. File-free so it is unit-testable without Platform.
[[nodiscard]] VillagerCatalog ParseVillagerCatalog(const nlohmann::json& doc);

/// A procedural villager decided for one town: who (archetype), their name, and a resolved daily
/// schedule whose waypoints are bound to concrete town-layout tiles (home door, workplace door,
/// or field). Pure output of PlanTownVillagers so NPC placement is unit-testable without ECS.
struct PlannedVillager {
	std::string id;
	std::string archetype;
	std::string name;
	VillagerSchedule schedule;
};

/// Plan (without spawning) the procedural villagers for one living town: staff every building that
/// maps to an archetype (shop -> shopkeeper, hall -> elder) with its workplace bound to that
/// building's door, then fill with generic villagers up to the population-scaled count, each homed
/// at a house door. Deterministic in `worldSeed` + the town id. `layout` may be null (no buildings).
[[nodiscard]] std::vector<PlannedVillager>
PlanTownVillagers(int worldSeed, const TownSite& town, const TownLayout* layout, const VillagerCatalog& catalog);

/// Register level component loaders for VillagerTag, VillagerName, VillagerSchedule,
/// and DialogTrigger. Call ONCE globally (e.g. from game.cpp alongside other loader
/// registrations).
void RegisterVillagerLoaders(const flecs::world& world);

/// Register per-scene systems (BT waypoint mover, InteractWith dialog opener, dialog
/// node handlers for shop buy/sell).
/// `sceneRoot` — parent entity; all registered systems become its children so they
/// are destroyed on scene Unload.
void RegisterVillagerSystems(flecs::world& world, flecs::entity sceneRoot);

/// Spawn villagers across every living town in `data`, reading games/farm/assets/data/villagers.json.
/// The starting village (`startVillageTownId`) keeps its authored villagers unchanged; every other
/// living town gets procedural villagers drawn from the archetype catalog + name pool, bound to its
/// generated buildings. Each town's villagers are parented to a per-town TownRoot entity.
/// Call during scene Load (which already runs undeferred). Pass startVillageTownId < 0 for
/// no authored start village (all towns procedural).
void SpawnVillagers(flecs::world& world, flecs::entity sceneRoot, const WorldData& data, int startVillageTownId);

/// Headless variant of SpawnVillagers used by unit tests: spawns from an inline JSON string that
/// mirrors games/farm/assets/data/villagers.json exactly (single source of truth) so integration tests can
/// assert on the multi-town spread without file I/O.
void SpawnVillagersHeadless(
	flecs::world& world,
	flecs::entity sceneRoot,
	const WorldData& data,
	int startVillageTownId
);

} // namespace game::farm
