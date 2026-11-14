/// Unit tests for the data-driven multi-town villager spread (ticket 09).
/// Seam 1: the pure TownVillagerCount scaling helper (population -> villager count).
/// Seam 2: ParseVillagerCatalog (JSON -> archetypes/name pool/scaling).
/// Seam 3: PlanTownVillagers (town + layout -> villagers with building-bound waypoints).
#include <gtest/gtest.h>

#include <algorithm>

#include <nlohmann/json.hpp>

#include "farm/villager_system.hpp"
#include "farm/world_data.hpp"

using game::farm::ParseVillagerCatalog;
using game::farm::PlannedVillager;
using game::farm::PlanTownVillagers;
using game::farm::TownBuilding;
using game::farm::TownLayout;
using game::farm::TownSite;
using game::farm::TownStatus;
using game::farm::TownVillagerCount;
using game::farm::VillagerCatalog;

namespace {

// A compact catalog document mirroring games/farm/assets/data/villagers.json's archetype sections.
nlohmann::json CatalogDoc() {
	return nlohmann::json::parse(R"({
		"name_pool": ["Rowan", "Mabel", "Cedric", "Odette", "Garrick", "Linnea"],
		"town_villagers": { "perCapita": 0.04, "min": 1, "max": 8 },
		"archetypes": [
			{ "id": "farmer", "weight": 5, "buildingKind": "",
			  "dialog": { "greeting": "hi" },
			  "schedule": [ { "startHour": 6, "place": "work" }, { "startHour": 20, "place": "home" } ] },
			{ "id": "shopkeeper", "weight": 2, "buildingKind": "shop",
			  "dialog": { "greeting": "trade?" },
			  "schedule": [ { "startHour": 8, "place": "work" }, { "startHour": 20, "place": "home" } ],
			  "shop": { "sell_items": [ { "typeId": "wheat_seed", "price": 10, "count": 5 } ],
			            "buy_items": [ { "typeId": "wheat", "price": 8 } ] } },
			{ "id": "elder", "weight": 1, "buildingKind": "hall",
			  "dialog": { "greeting": "story" },
			  "schedule": [ { "startHour": 9, "place": "work" }, { "startHour": 18, "place": "home" } ] }
		]
	})");
}

// A town with one house, one shop, and one hall at known door tiles.
TownLayout MakeLayout(int townId, glm::ivec2 centre) {
	TownLayout layout;
	layout.townId = townId;
	layout.ruined = false;
	layout.landmark = centre;
	layout.buildings = {
		TownBuilding{.kind = "house", .villagerArchetype = "", .door = {centre.x - 2, centre.y}, .isHome = true},
		TownBuilding{.kind = "shop", .villagerArchetype = "shopkeeper", .door = {centre.x + 2, centre.y}},
		TownBuilding{.kind = "hall", .villagerArchetype = "elder", .door = {centre.x, centre.y + 2}},
	};
	layout.fieldTiles = {{centre.x + 3, centre.y + 3}};
	return layout;
}

bool ScheduleHasWaypoint(const PlannedVillager& pv, glm::ivec2 wp) {
	return std::ranges::any_of(pv.schedule.entries, [&](const auto& e) {
		return e.waypoint.x == wp.x && e.waypoint.y == wp.y;
	});
}

const PlannedVillager* FindByArchetype(const std::vector<PlannedVillager>& v, const std::string& arch) {
	for (const auto& pv : v) {
		if (pv.archetype == arch) return &pv;
	}
	return nullptr;
}

} // namespace

// ── TownVillagerCount (seam 1) ─────────────────────────────────────────────────

// Population scales linearly by per-capita, rounded to the nearest whole villager.
TEST(TownVillagerCountTest, scales_linearly_with_population) {
	EXPECT_EQ(TownVillagerCount(100, 0.04, 1, 8), 4);
	EXPECT_EQ(TownVillagerCount(25, 0.04, 1, 8), 1);
}

// A tiny living town still gets at least the minimum (every living town is inhabited).
TEST(TownVillagerCountTest, floors_at_minimum) {
	EXPECT_EQ(TownVillagerCount(10, 0.04, 1, 8), 1);
	EXPECT_EQ(TownVillagerCount(1, 0.02, 2, 8), 2);
}

// A huge town is capped so the largest settlement stays bounded.
TEST(TownVillagerCountTest, caps_at_maximum) { EXPECT_EQ(TownVillagerCount(1000, 0.04, 1, 8), 8); }

// A ruined/empty town (population 0) gets no villagers.
TEST(TownVillagerCountTest, empty_town_has_no_villagers) { EXPECT_EQ(TownVillagerCount(0, 0.04, 1, 8), 0); }

// ── ParseVillagerCatalog (seam 2) ─────────────────────────────────────────────

TEST(ParseVillagerCatalogTest, reads_archetypes_name_pool_and_scaling) {
	const VillagerCatalog cat = ParseVillagerCatalog(CatalogDoc());
	EXPECT_EQ(cat.archetypes.size(), 3u);
	EXPECT_EQ(cat.namePool.size(), 6u);
	EXPECT_DOUBLE_EQ(cat.perCapita, 0.04);
	EXPECT_EQ(cat.minPerTown, 1);
	EXPECT_EQ(cat.maxPerTown, 8);
}

TEST(ParseVillagerCatalogTest, shopkeeper_archetype_carries_shop_items) {
	const VillagerCatalog cat = ParseVillagerCatalog(CatalogDoc());
	const auto it = std::ranges::find_if(cat.archetypes, [](const auto& a) { return a.id == "shopkeeper"; });
	ASSERT_NE(it, cat.archetypes.end());
	EXPECT_EQ(it->buildingKind, "shop");
	ASSERT_EQ(it->sellItems.size(), 1u);
	EXPECT_EQ(it->sellItems.front().typeId, "wheat_seed");
	ASSERT_EQ(it->buyItems.size(), 1u);
	EXPECT_EQ(it->buyItems.front().typeId, "wheat");
}

TEST(ParseVillagerCatalogTest, missing_sections_fall_back_to_defaults) {
	const VillagerCatalog cat = ParseVillagerCatalog(nlohmann::json::parse(R"({ "villagers": [] })"));
	EXPECT_TRUE(cat.archetypes.empty());
	EXPECT_TRUE(cat.namePool.empty());
	EXPECT_GT(cat.minPerTown, 0); // default floor keeps every living town inhabited
}

// ── PlanTownVillagers (seam 3) ─────────────────────────────────────────────────

// A shopkeeper's workplace is bound to the shop door; an elder's to the hall door (AC #2).
TEST(PlanTownVillagersTest, binds_workplace_to_matching_building_door) {
	const VillagerCatalog cat = ParseVillagerCatalog(CatalogDoc());
	const glm::ivec2 centre{50, 50};
	const TownSite town{.id = 5, .location = centre, .size = 100, .status = TownStatus::Living};
	const TownLayout layout = MakeLayout(5, centre);

	const std::vector<PlannedVillager> planned = PlanTownVillagers(1234, town, &layout, cat);

	const PlannedVillager* shopkeeper = FindByArchetype(planned, "shopkeeper");
	ASSERT_NE(shopkeeper, nullptr) << "a shop building must be staffed by a shopkeeper";
	EXPECT_TRUE(ScheduleHasWaypoint(*shopkeeper, {52, 50})) << "shopkeeper workplace must be the shop door";
	EXPECT_TRUE(ScheduleHasWaypoint(*shopkeeper, {48, 50})) << "home must be bound to the house door";

	const PlannedVillager* elder = FindByArchetype(planned, "elder");
	ASSERT_NE(elder, nullptr) << "a hall building must be staffed by an elder";
	EXPECT_TRUE(ScheduleHasWaypoint(*elder, {50, 52})) << "elder workplace must be the hall door";
}

// Count scales with population and staffs all archetype buildings; each villager has a routine.
TEST(PlanTownVillagersTest, count_scales_and_every_villager_has_a_routine) {
	const VillagerCatalog cat = ParseVillagerCatalog(CatalogDoc());
	const glm::ivec2 centre{50, 50};
	const TownSite town{.id = 5, .location = centre, .size = 100, .status = TownStatus::Living};
	const TownLayout layout = MakeLayout(5, centre);

	const std::vector<PlannedVillager> planned = PlanTownVillagers(1234, town, &layout, cat);

	// size 100 * 0.04 = 4 target; shopkeeper + elder staffed first, then farmers fill to 4.
	EXPECT_EQ(planned.size(), 4u);
	for (const PlannedVillager& pv : planned) {
		EXPECT_GE(pv.schedule.entries.size(), 2u) << "every villager needs a >= 2 waypoint routine";
		EXPECT_FALSE(pv.name.empty());
		EXPECT_FALSE(pv.id.empty());
	}
}

// A tiny living town with no buildings still gets at least one inhabitant.
TEST(PlanTownVillagersTest, tiny_town_without_layout_still_inhabited) {
	const VillagerCatalog cat = ParseVillagerCatalog(CatalogDoc());
	const TownSite town{.id = 9, .location = {10, 10}, .size = 1, .status = TownStatus::Living};

	const std::vector<PlannedVillager> planned = PlanTownVillagers(7, town, nullptr, cat);
	EXPECT_GE(planned.size(), 1u);
}

// Planning is deterministic in (worldSeed, townId): identical inputs yield identical villagers.
TEST(PlanTownVillagersTest, is_deterministic) {
	const VillagerCatalog cat = ParseVillagerCatalog(CatalogDoc());
	const glm::ivec2 centre{50, 50};
	const TownSite town{.id = 5, .location = centre, .size = 100, .status = TownStatus::Living};
	const TownLayout layout = MakeLayout(5, centre);

	const std::vector<PlannedVillager> a = PlanTownVillagers(1234, town, &layout, cat);
	const std::vector<PlannedVillager> b = PlanTownVillagers(1234, town, &layout, cat);
	ASSERT_EQ(a.size(), b.size());
	for (size_t i = 0; i < a.size(); ++i) {
		EXPECT_EQ(a[i].id, b[i].id);
		EXPECT_EQ(a[i].name, b[i].name);
		EXPECT_EQ(a[i].archetype, b[i].archetype);
	}
}
