#include <gtest/gtest.h>

#include <cstdio>
#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

#include "farm/generation_log.hpp"
#include "farm/road_hubs.hpp" // kHubMarkerKind
#include "farm/world_data.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;
using nlohmann::json;

namespace {

// A hand-built world exercising every log section with known values, so the schema and the derived
// fields (bridge reason/year, isOutpost, hub promotion) can be asserted exactly without seed-hunting.
WorldData MakeSyntheticWorld() {
	WorldData w;
	w.seed = 999;
	w.grid.width = 10;
	w.grid.height = 10;

	w.history.push_back(
		{.year = -5000, .type = HistoryEventType::TownFounded, .a = 1, .b = 0, .description = "Riverford founded"}
	);
	w.history.push_back(
		{.year = 0,
		 .type = HistoryEventType::TownFounded,
		 .a = 2,
		 .b = 0,
		 .description = "crossroads outpost Crossgate grew where the roads meet"}
	);
	w.history.push_back(
		{.year = -100,
		 .type = HistoryEventType::BridgeBuilt,
		 .a = 1,
		 .b = 2,
		 .description = "Riverford funded a bridge to reach Crossgate"}
	);

	w.towns.push_back(
		{.id = 1,
		 .name = "Riverford",
		 .location = {3, 4},
		 .size = 80,
		 .status = TownStatus::Living,
		 .foundingYear = -5000}
	);
	w.towns.push_back(
		{.id = 2, .name = "Crossgate", .location = {6, 6}, .size = 20, .status = TownStatus::Living, .foundingYear = 0}
	);

	TownLayout layout;
	layout.townId = 1;
	layout.landmarkKind = "well";
	layout.landmark = {3, 4};
	TownBuilding building;
	building.kind = "house";
	building.villagerArchetype = "farmer";
	building.footprint = {{3, 4}, {4, 4}};
	building.door = {3, 5};
	layout.buildings.push_back(building);
	layout.floorTiles = {{3, 5}};
	layout.fieldTiles = {{2, 4}};
	w.townLayouts.push_back(layout);

	w.objects.push_back(
		{.footprint = {{1, 1}}, .kind = "tree_oak", .height = 2.5f, .blocksMovement = true}
	);
	// A hub signpost ON the promoted outpost's tile (6,6) => promotedToOutpost should be true.
	w.objects.push_back(
		{.footprint = {{6, 6}}, .kind = std::string{kHubMarkerKind}, .height = 1.0f, .blocksMovement = false}
	);
	// A hub signpost in the wilderness (8,8) with no town => promotedToOutpost should be false.
	w.objects.push_back(
		{.footprint = {{8, 8}}, .kind = std::string{kHubMarkerKind}, .height = 1.0f, .blocksMovement = false}
	);

	w.creatures.push_back({.tile = {5, 5}, .kind = "cow", .wanderRadius = 3, .homeTownId = 1});

	Bridge bridge;
	bridge.townA = 1;
	bridge.townB = 2;
	bridge.deck = {{4, 5}, {4, 6}};
	bridge.reason = BridgeReason::HistoricalInvestment;
	bridge.year = -100;
	bridge.description = "Riverford funded a bridge to reach Crossgate";
	w.bridges.push_back(bridge);

	return w;
}

} // namespace

TEST(GenerationLogTest, emits_valid_json_with_all_required_top_level_keys) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 1337);
	const std::string text = FormatGenerationLog(world);

	const json log = json::parse(text); // throws on invalid JSON — the parse itself is the check

	for (const char* key :
		 {"schemaVersion", "schema", "seed", "grid", "params", "history", "towns", "townLayouts", "objects", "hubs",
		  "creatures", "bridges"}) {
		EXPECT_TRUE(log.contains(key)) << "missing top-level key " << key;
	}

	EXPECT_EQ(log["seed"].get<int>(), 1337);
	EXPECT_TRUE(log["history"].is_array());
	EXPECT_TRUE(log["towns"].is_array());
	EXPECT_TRUE(log["townLayouts"].is_array());
	EXPECT_TRUE(log["objects"].is_array());
	EXPECT_TRUE(log["creatures"].is_array());
	EXPECT_TRUE(log["bridges"].is_array());
	EXPECT_TRUE(log["hubs"].is_array());

	// The log is complete: every WorldData collection is represented one-for-one.
	EXPECT_EQ(log["history"].size(), world.history.size());
	EXPECT_EQ(log["towns"].size(), world.towns.size());
	EXPECT_EQ(log["townLayouts"].size(), world.townLayouts.size());
	EXPECT_EQ(log["objects"].size(), world.objects.size());
	EXPECT_EQ(log["creatures"].size(), world.creatures.size());
	EXPECT_EQ(log["bridges"].size(), world.bridges.size());
}

TEST(GenerationLogTest, is_deterministic_for_same_seed) {
	const WorldGenParams params{};
	// Two independent generations with the same seed produce byte-identical logs.
	EXPECT_EQ(FormatGenerationLog(WorldGen::Generate(params, 42)), FormatGenerationLog(WorldGen::Generate(params, 42)));
	// And formatting the same world twice is stable.
	const WorldData world = WorldGen::Generate(params, 90210);
	EXPECT_EQ(FormatGenerationLog(world), FormatGenerationLog(world));
}

TEST(GenerationLogTest, differs_for_different_seeds) {
	const WorldGenParams params{};
	EXPECT_NE(FormatGenerationLog(WorldGen::Generate(params, 1)), FormatGenerationLog(WorldGen::Generate(params, 2)));
}

TEST(GenerationLogTest, records_buildings_fields_bridges_creatures_and_hub_promotion) {
	const WorldData world = MakeSyntheticWorld();
	const json log = json::parse(FormatGenerationLog(world));

	// Per-town building + field placement.
	ASSERT_EQ(log["townLayouts"].size(), 1u);
	const json& layout = log["townLayouts"][0];
	EXPECT_EQ(layout["townId"].get<int>(), 1);
	EXPECT_EQ(layout["landmarkKind"].get<std::string>(), "well");
	ASSERT_EQ(layout["buildings"].size(), 1u);
	EXPECT_EQ(layout["buildings"][0]["kind"].get<std::string>(), "house");
	EXPECT_EQ(layout["buildings"][0]["villagerArchetype"].get<std::string>(), "farmer");
	EXPECT_EQ(layout["buildings"][0]["door"], json::array({3, 5}));
	EXPECT_EQ(layout["fieldTiles"], json::array({json::array({2, 4})}));

	// Object scatter (type + position).
	ASSERT_FALSE(log["objects"].empty());
	EXPECT_EQ(log["objects"][0]["kind"].get<std::string>(), "tree_oak");
	EXPECT_EQ(log["objects"][0]["footprint"], json::array({json::array({1, 1})}));

	// Bridges carry construction reason + year.
	ASSERT_EQ(log["bridges"].size(), 1u);
	EXPECT_EQ(log["bridges"][0]["reason"].get<std::string>(), "HistoricalInvestment");
	EXPECT_EQ(log["bridges"][0]["year"].get<int>(), -100);
	EXPECT_EQ(log["bridges"][0]["span"].get<int>(), 2);

	// Creature placements.
	ASSERT_EQ(log["creatures"].size(), 1u);
	EXPECT_EQ(log["creatures"][0]["kind"].get<std::string>(), "cow");
	EXPECT_EQ(log["creatures"][0]["homeTownId"].get<int>(), 1);

	// Town outpost flag derived from history.
	bool town1Outpost = true;
	bool town2Outpost = false;
	for (const json& t : log["towns"]) {
		if (t["id"].get<int>() == 1) {
			town1Outpost = t["isOutpost"].get<bool>();
		}
		if (t["id"].get<int>() == 2) {
			town2Outpost = t["isOutpost"].get<bool>();
		}
	}
	EXPECT_FALSE(town1Outpost);
	EXPECT_TRUE(town2Outpost);

	// Hub nodes: the signpost on the outpost tile is flagged promoted; the wilderness one is not.
	ASSERT_EQ(log["hubs"].size(), 2u);
	bool promotedAt66 = false;
	bool promotedAt88 = true;
	for (const json& h : log["hubs"]) {
		if (h["position"] == json::array({6, 6})) {
			promotedAt66 = h["promotedToOutpost"].get<bool>();
		}
		if (h["position"] == json::array({8, 8})) {
			promotedAt88 = h["promotedToOutpost"].get<bool>();
		}
	}
	EXPECT_TRUE(promotedAt66);
	EXPECT_FALSE(promotedAt88);
}

TEST(GenerationLogTest, write_produces_a_parseable_file_matching_the_formatter) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 7);
	const std::string path = "generation_log_unit_test_seed7.json";

	ASSERT_TRUE(WriteGenerationLog(world, path));

	std::ifstream in(path, std::ios::binary);
	ASSERT_TRUE(in.good());
	std::ostringstream buffer;
	buffer << in.rdbuf();
	in.close();
	const std::string fileText = buffer.str();

	// The file is valid JSON and byte-identical to the pure formatter (same BuildLog serialization).
	EXPECT_NO_THROW((void)json::parse(fileText));
	EXPECT_EQ(fileText, FormatGenerationLog(world));

	std::remove(path.c_str());
}
