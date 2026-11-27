/// Unit tests for the town-layout generator (ticket 04): a TownSite becomes a village — a central
/// landmark, population-scaled buildings along short streets, decorative field plots near farm
/// buildings, and walkable floor/door terrain. Pure value types + the pure tilemap seam — no ECS,
/// no Platform.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <set>
#include <utility>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "engine/engine.hpp"
#include "farm/building_catalog.hpp"
#include "farm/farm_tilemap_builder.hpp"
#include "farm/town_layout.hpp"
#include "farm/world_data.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;
using namespace engine::tilemap;

namespace {

constexpr std::array<int, 6> kSeeds{1, 7, 42, 1337, 90210, 2024};

// A flat, fully walkable grass grid of the given size.
Grid MakeGrassGrid(const int w, const int h) {
	Grid grid;
	grid.width = w;
	grid.height = h;
	grid.elevation.assign(static_cast<size_t>(w) * h, 0.5F);
	grid.biome.assign(static_cast<size_t>(w) * h, Biome::Plains);
	return grid;
}

TownSite MakeTown(const glm::ivec2 loc, const int size, const TownStatus status) {
	TownSite t;
	t.id = 1;
	t.name = "Testton";
	t.location = loc;
	t.size = size;
	t.status = status;
	return t;
}

// A TileSet covering every name the builder emits; blocking names are non-walkable.
TileSet MakeTileSet() {
	TileSet ts;
	constexpr std::pair<uint32_t, const char*> kTiles[] = {
		{0, "grass"}, {1, "forest"}, {2, "path"}, {3, "water"}, {4, "stone_floor"},
		{5, "stone_wall"}, {6, "sand"}, {7, "deep_forest"}, {8, "ruins"}, {9, "tilled_soil"},
		{10, "dirt"}, {11, "cobblestone"}, {12, "brick"}, {13, "moss"},
	};
	const auto blocked = [](const std::string_view n) {
		return n == "water" || n == "stone_wall" || n == "forest" || n == "deep_forest";
	};
	for (const auto& [id, name] : kTiles) {
		ts.RegisterTile({.id = id, .name = name, .walkable = !blocked(name)});
	}
	return ts;
}

// Assemble a WorldData from a grid + one town's layout result, for the tilemap seam.
WorldData MakeWorld(const Grid& grid, const TownLayoutResult& result) {
	WorldData world;
	world.grid = grid;
	world.objects = result.objects;
	world.townLayouts.push_back(result.layout);
	return world;
}

} // namespace

// ── Building catalog ──────────────────────────────────────────────────────────────

TEST(TownLayoutTest, ParseBuildingCatalogSchema) {
	const auto doc = nlohmann::json::parse(R"({
		"buildings": [
			{ "kind": "shop", "width": 4, "length": 3, "wallHeight": 1.6, "weight": 3, "villagerArchetype": "shopkeeper", "farm": false },
			{ "kind": "barn", "farm": true }
		]
	})");

	const BuildingCatalog cat = ParseBuildingCatalog(doc);
	ASSERT_EQ(cat.defs.size(), 2U);

	const BuildingDef* shop = cat.Find("shop");
	ASSERT_NE(shop, nullptr);
	EXPECT_EQ(shop->width, 4);
	EXPECT_EQ(shop->length, 3);
	EXPECT_FLOAT_EQ(shop->wallHeight, 1.6F);
	EXPECT_EQ(shop->villagerArchetype, "shopkeeper");
	EXPECT_FALSE(shop->farm);

	const BuildingDef* barn = cat.Find("barn");
	ASSERT_NE(barn, nullptr);
	EXPECT_TRUE(barn->farm);
	EXPECT_EQ(barn->width, 3) << "missing fields fall back to defaults";
}

TEST(TownLayoutTest, DefaultCatalogMapsKindsToVillagerArchetypes) {
	const BuildingCatalog cat = DefaultBuildingCatalog();
	ASSERT_NE(cat.Find("shop"), nullptr);
	ASSERT_NE(cat.Find("hall"), nullptr);
	EXPECT_EQ(cat.Find("shop")->villagerArchetype, "shopkeeper");
	EXPECT_EQ(cat.Find("hall")->villagerArchetype, "elder");
	EXPECT_TRUE(cat.Find("barn")->farm);
}

TEST(TownLayoutTest, ParseToleratesMissingArray) {
	EXPECT_TRUE(ParseBuildingCatalog(nlohmann::json::object()).defs.empty());
}

// ── Building count scales with population ────────────────────────────────────────

TEST(TownLayoutTest, BuildingCountScalesWithPopulation) {
	const WorldGenParams p;
	EXPECT_GT(TownBuildingCount(p, 120), TownBuildingCount(p, 25));
	EXPECT_GE(TownBuildingCount(p, 25), p.minBuildingsPerTown) << "a minimum-population town has >= 2 buildings";
	EXPECT_LE(TownBuildingCount(p, 100000), p.maxBuildingsPerTown) << "building count is capped";
}

// ── A living town has a landmark and buildings ───────────────────────────────────

TEST(TownLayoutTest, LivingTownHasLandmarkAndBuildings) {
	const Grid grid = MakeGrassGrid(80, 80);
	const WorldGenParams p;
	const TownSite town = MakeTown({40, 40}, 25, TownStatus::Living);
	const TownLayoutResult r = BuildTownLayout(town, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(1, town.id));

	EXPECT_EQ(r.layout.landmarkKind, "well");
	ASSERT_FALSE(r.objects.empty());
	EXPECT_EQ(r.objects.front().kind, "well") << "the landmark object is emitted first";
	const int wells = static_cast<int>(std::ranges::count_if(r.objects, [](const WorldObject& o) { return o.kind == "well"; }));
	EXPECT_GE(wells, 1) << "every living town has >= 1 landmark object";
	EXPECT_GE(r.layout.buildings.size(), 2U) << "a minimum-population town has >= 2 buildings";
}

TEST(TownLayoutTest, LargerPopulationTownHasMoreBuildings) {
	const Grid grid = MakeGrassGrid(400, 400);
	const WorldGenParams p;
	const TownSite small = MakeTown({200, 200}, 25, TownStatus::Living);
	const TownSite big = MakeTown({200, 200}, 120, TownStatus::Living);
	const TownLayoutResult rs = BuildTownLayout(small, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(1, small.id));
	const TownLayoutResult rb = BuildTownLayout(big, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(1, big.id));
	EXPECT_GT(rb.layout.buildings.size(), rs.layout.buildings.size());
}

// ── Every building has a walkable door; walls block ──────────────────────────────

TEST(TownLayoutTest, EveryBuildingHasWalkableDoorAndBlockingWalls) {
	const Grid grid = MakeGrassGrid(80, 80);
	const WorldGenParams p;
	const TownSite town = MakeTown({40, 40}, 120, TownStatus::Living);
	const TownLayoutResult r = BuildTownLayout(town, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(7, town.id));
	ASSERT_FALSE(r.layout.buildings.empty());

	const TileSet ts = MakeTileSet();
	const Tilemap tm = BuildTilemapFromWorldData(MakeWorld(grid, r), ts);

	const std::set<std::pair<int, int>> floorSet = [&] {
		std::set<std::pair<int, int>> s;
		for (const glm::ivec2 t : r.layout.floorTiles) s.insert({t.x, t.y});
		return s;
	}();

	for (const TownBuilding& b : r.layout.buildings) {
		// The door is a walkable terrain floor tile and is NOT part of the blocking wall footprint.
		EXPECT_TRUE(floorSet.contains({b.door.x, b.door.y})) << "door is a floor tile";
		EXPECT_FALSE(std::ranges::any_of(b.footprint, [&](const glm::ivec2 w) { return w == b.door; }))
			<< "door is a gap in the wall, not a wall tile";
		EXPECT_TRUE(IsTileWalkable(tm, ts, b.door.x, b.door.y)) << "the player can walk through the door";
		// Every wall tile blocks.
		for (const glm::ivec2 w : b.footprint) {
			EXPECT_FALSE(IsTileWalkable(tm, ts, w.x, w.y)) << "wall (" << w.x << "," << w.y << ") must block";
		}
	}
}

// ── Ruined towns use rubble variants ─────────────────────────────────────────────

TEST(TownLayoutTest, RuinedTownUsesRubbleVariants) {
	const Grid grid = MakeGrassGrid(80, 80);
	const WorldGenParams p;
	const TownSite ruin = MakeTown({40, 40}, 0, TownStatus::Ruined);
	const TownLayoutResult r = BuildTownLayout(ruin, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(1, ruin.id));

	EXPECT_TRUE(r.layout.ruined);
	EXPECT_EQ(r.layout.landmarkKind, "rubble");
	ASSERT_FALSE(r.layout.buildings.empty());
	for (const TownBuilding& b : r.layout.buildings) {
		EXPECT_TRUE(b.ruined);
		EXPECT_TRUE(b.villagerArchetype.empty()) << "ruins have no villagers";
	}
	for (const WorldObject& o : r.objects) {
		EXPECT_EQ(o.kind, "rubble") << "ruined towns emit only rubble world-objects";
	}

	// Ruined floor paints as ruins (walkable), and the door remains walkable.
	const TileSet ts = MakeTileSet();
	const Tilemap tm = BuildTilemapFromWorldData(MakeWorld(grid, r), ts);
	const auto ruinsId = ts.ResolveTileId("ruins");
	ASSERT_TRUE(ruinsId.has_value());
	const glm::ivec2 door = r.layout.buildings.front().door;
	EXPECT_EQ(GetTileIdAt(tm, door.x, door.y), *ruinsId) << "ruined-town floor is ruins";
	EXPECT_TRUE(IsTileWalkable(tm, ts, door.x, door.y));
}

// ── Farm buildings get decorative field plots ────────────────────────────────────

TEST(TownLayoutTest, FarmBuildingsProduceWalkableFieldTiles) {
	const Grid grid = MakeGrassGrid(80, 80);
	WorldGenParams p;
	BuildingCatalog farmOnly;
	farmOnly.defs = {{.kind = "barn", .width = 4, .length = 4, .wallHeight = 2.2F, .weight = 1, .villagerArchetype = "", .farm = true}};
	const TownSite town = MakeTown({40, 40}, 60, TownStatus::Living);
	const TownLayoutResult r = BuildTownLayout(town, grid, p, farmOnly, DeriveTownSeed(1, town.id));

	ASSERT_FALSE(r.layout.fieldTiles.empty()) << "farm buildings should produce decorative field tiles";

	const TileSet ts = MakeTileSet();
	const Tilemap tm = BuildTilemapFromWorldData(MakeWorld(grid, r), ts);
	const auto tilledId = ts.ResolveTileId("tilled_soil");
	ASSERT_TRUE(tilledId.has_value());
	const glm::ivec2 f = r.layout.fieldTiles.front();
	EXPECT_EQ(GetTileIdAt(tm, f.x, f.y), *tilledId) << "field plots paint as tilled_soil";
	EXPECT_TRUE(IsTileWalkable(tm, ts, f.x, f.y)) << "field plots are walkable (visual only)";
}

// ── Housing is marked as homes and scales with population ─────────────────────────

TEST(TownLayoutTest, HousingIsMarkedAsHomesAndScalesWithPopulation) {
	const Grid grid = MakeGrassGrid(400, 400);
	const WorldGenParams p;
	const TownSite small = MakeTown({200, 200}, 25, TownStatus::Living);
	const TownSite big = MakeTown({200, 200}, 120, TownStatus::Living);
	const TownLayoutResult rs = BuildTownLayout(small, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(3, small.id));
	const TownLayoutResult rb = BuildTownLayout(big, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(3, big.id));

	const auto homes = [](const TownLayoutResult& r) {
		return std::ranges::count_if(r.layout.buildings, [](const TownBuilding& b) { return b.isHome; });
	};
	EXPECT_GT(homes(rs), 0) << "even a tiny town houses its residents";
	EXPECT_GT(homes(rb), homes(rs)) << "a larger population needs more homes";

	// Residential kinds are homes; communal service buildings are not.
	for (const TownBuilding& b : rb.layout.buildings) {
		const bool residential = (b.kind == "cottage" || b.kind == "house" || b.kind == "longhouse");
		EXPECT_EQ(b.isHome, residential) << "kind '" << b.kind << "' home flag";
	}
}

// ── Communal buildings are gated by town size tier ───────────────────────────────

TEST(TownLayoutTest, CommunalBuildingsAreTierGated) {
	const Grid grid = MakeGrassGrid(400, 400);
	const WorldGenParams p;
	const TownSite hamlet = MakeTown({200, 200}, 20, TownStatus::Living);
	const TownSite city = MakeTown({200, 200}, 120, TownStatus::Living);
	const TownLayoutResult rh = BuildTownLayout(hamlet, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(5, hamlet.id));
	const TownLayoutResult rc = BuildTownLayout(city, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(5, city.id));

	const auto hasKind = [](const TownLayoutResult& r, const std::string& kind) {
		return std::ranges::any_of(r.layout.buildings, [&](const TownBuilding& b) { return b.kind == kind; });
	};

	// A hamlet (tier 0) has no village/town/city-tier services.
	EXPECT_FALSE(hasKind(rh, "shop")) << "a hamlet has no shop (village-tier)";
	EXPECT_FALSE(hasKind(rh, "hall")) << "a hamlet has no hall (town-tier)";

	// A city has the higher-tier civic buildings unlocked.
	EXPECT_TRUE(hasKind(rc, "hall")) << "a city has a town-tier hall";
	EXPECT_TRUE(hasKind(rc, "shop")) << "a city has shops";
}

// ── A living town has walkable street paths connecting its buildings ──────────────

TEST(TownLayoutTest, LivingTownHasStreetPaths) {
	const Grid grid = MakeGrassGrid(200, 200);
	const WorldGenParams p;
	const TownSite town = MakeTown({100, 100}, 90, TownStatus::Living);
	const TownLayoutResult r = BuildTownLayout(town, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(11, town.id));

	ASSERT_FALSE(r.layout.pathTiles.empty()) << "a town lays streets between its buildings";

	// Street tiles are walkable terrain the player can use.
	const TileSet ts = MakeTileSet();
	const Tilemap tm = BuildTilemapFromWorldData(MakeWorld(grid, r), ts);
	const auto pathId = ts.ResolveTileId("path");
	ASSERT_TRUE(pathId.has_value());
	const glm::ivec2 street = r.layout.pathTiles.front();
	EXPECT_EQ(GetTileIdAt(tm, street.x, street.y), *pathId) << "street tiles paint as path";
	EXPECT_TRUE(IsTileWalkable(tm, ts, street.x, street.y)) << "streets are walkable";
}

// ── Determinism ──────────────────────────────────────────────────────────────────

TEST(TownLayoutTest, BuildTownLayoutIsDeterministic) {
	const Grid grid = MakeGrassGrid(120, 120);
	const WorldGenParams p;
	const TownSite town = MakeTown({60, 60}, 90, TownStatus::Living);
	const TownLayoutResult a = BuildTownLayout(town, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(42, town.id));
	const TownLayoutResult b = BuildTownLayout(town, grid, p, DefaultBuildingCatalog(), DeriveTownSeed(42, town.id));
	EXPECT_EQ(a.layout, b.layout);
	EXPECT_EQ(a.objects, b.objects);
}

// ── End-to-end: Generate lays out every town ─────────────────────────────────────

TEST(TownLayoutTest, GenerateGivesEveryTownALayoutWithLandmarkAndBuilding) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		ASSERT_EQ(world.townLayouts.size(), world.towns.size()) << "seed " << seed;
		EXPECT_FALSE(world.objects.empty()) << "seed " << seed;

		for (const TownSite& town : world.towns) {
			if (town.status != TownStatus::Living) {
				continue;
			}
			const auto it = std::ranges::find_if(world.townLayouts, [&](const TownLayout& l) { return l.townId == town.id; });
			ASSERT_NE(it, world.townLayouts.end()) << "seed " << seed << " town " << town.id;
			EXPECT_EQ(it->landmarkKind, "well") << "living town landmark, seed " << seed;
			EXPECT_GE(it->buildings.size(), 1U) << "living town has >= 1 building, seed " << seed << " town " << town.id;
		}

		// A living town's well is an actual world-object the renderer can draw.
		const bool hasWellObject = std::ranges::any_of(world.objects, [](const WorldObject& o) { return o.kind == "well"; });
		EXPECT_TRUE(hasWellObject) << "seed " << seed;
	}
}

TEST(TownLayoutTest, GenerateIsDeterministicIncludingLayouts) {
	const WorldData a = WorldGen::Generate(WorldGenParams{}, 2024);
	const WorldData b = WorldGen::Generate(WorldGenParams{}, 2024);
	EXPECT_EQ(a.townLayouts, b.townLayouts);
	EXPECT_EQ(a.objects, b.objects);
}
