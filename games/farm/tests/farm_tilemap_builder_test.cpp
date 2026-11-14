/// Integration test for the FarmWorldScene tilemap-builder seam.
/// Tests BuildTilemapFromWorldData in isolation — no ECS, no Platform.
#include <gtest/gtest.h>

#include <set>

#include "engine/engine.hpp"
#include "farm/farm_tilemap_builder.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;
using namespace engine::tilemap;

namespace {

// Build a minimal TileSet containing every tile name the builder can emit.
TileSet BuildTestTileSet() {
	TileSet ts;
	constexpr std::pair<uint32_t, const char*> kTiles[] = {
		{0, "grass"},
		{1, "forest"},
		{2, "path"},
		{3, "water"},
		{4, "stone_floor"},
		{5, "stone_wall"},
		{6, "sand"},
		{7, "deep_forest"},
		{8, "ruins"},
		{9, "tilled_soil"},
		{10, "dirt"},
		{11, "cobblestone"},
		{12, "brick"},
		{13, "moss"},
	};
	for (const auto& [id, name] : kTiles) ts.RegisterTile({.id = id, .name = name});
	return ts;
}

} // namespace

class FarmTilemapBuilderTest : public ::testing::Test {
protected:
	WorldData world_ = WorldGen::Generate(WorldGenParams{}, 42);
	TileSet tileset_ = BuildTestTileSet();
	Tilemap tilemap_;

	void SetUp() override { tilemap_ = BuildTilemapFromWorldData(world_, tileset_); }
};

// ── Dimensions ─────────────────────────────────────────────────────────────────

TEST_F(FarmTilemapBuilderTest, DimensionsMatchWorldGrid) {
	EXPECT_EQ(tilemap_.width, static_cast<uint32_t>(world_.grid.width));
	EXPECT_EQ(tilemap_.height, static_cast<uint32_t>(world_.grid.height));
}

TEST_F(FarmTilemapBuilderTest, TileIdsSizeEqualWidthTimesHeight) {
	EXPECT_EQ(
		tilemap_.tile_ids.size(),
		static_cast<size_t>(world_.grid.width) * static_cast<size_t>(world_.grid.height)
	);
}

// ── Tile variety ────────────────────────────────────────────────────────────────

TEST_F(FarmTilemapBuilderTest, AtLeastFiveDistinctTileTypes) {
	std::set<uint32_t> seen(tilemap_.tile_ids.begin(), tilemap_.tile_ids.end());
	EXPECT_GE(seen.size(), 5u) << "Expected >=5 distinct tile types (water/plains/forest/hills/road)";
}

// ── Required tile presence ──────────────────────────────────────────────────────

TEST_F(FarmTilemapBuilderTest, ContainsWaterTiles) {
	const auto id = tileset_.ResolveTileId("water");
	ASSERT_TRUE(id.has_value());
	const bool found = std::ranges::any_of(tilemap_.tile_ids, [&](const uint32_t t) { return t == *id; });
	EXPECT_TRUE(found) << "Ocean/Lake biomes should produce water tiles";
}

TEST_F(FarmTilemapBuilderTest, ContainsGrassTiles) {
	const auto id = tileset_.ResolveTileId("grass");
	ASSERT_TRUE(id.has_value());
	const bool found = std::ranges::any_of(tilemap_.tile_ids, [&](const uint32_t t) { return t == *id; });
	EXPECT_TRUE(found) << "Plains biome should produce grass tiles";
}

TEST_F(FarmTilemapBuilderTest, ContainsForestTiles) {
	const auto id = tileset_.ResolveTileId("forest");
	ASSERT_TRUE(id.has_value());
	const bool found = std::ranges::any_of(tilemap_.tile_ids, [&](const uint32_t t) { return t == *id; });
	EXPECT_TRUE(found) << "Forest biome should produce forest tiles";
}

// ── Roads → path tiles ──────────────────────────────────────────────────────────

TEST_F(FarmTilemapBuilderTest, ContainsPathTilesFromRoads) {
	if (world_.roads.segments.empty()) GTEST_SKIP() << "Seed 42 produced no roads; cannot test path tiles";

	const auto id = tileset_.ResolveTileId("path");
	ASSERT_TRUE(id.has_value());
	const bool found = std::ranges::any_of(tilemap_.tile_ids, [&](const uint32_t t) { return t == *id; });
	EXPECT_TRUE(found) << "Road segments should produce path tiles";
}

// ── Ruins ────────────────────────────────────────────────────────────────────────

TEST_F(FarmTilemapBuilderTest, ContainsRuinTilesWhenWorldHasRuinedTowns) {
	const bool hasRuins =
		std::ranges::any_of(world_.towns, [](const TownSite& t) { return t.status == TownStatus::Ruined; });
	if (!hasRuins) GTEST_SKIP() << "Seed 42 has no ruined towns; cannot test ruins tiles";

	const auto id = tileset_.ResolveTileId("ruins");
	ASSERT_TRUE(id.has_value());
	const bool found = std::ranges::any_of(tilemap_.tile_ids, [&](const uint32_t t) { return t == *id; });
	EXPECT_TRUE(found) << "Ruined towns should produce ruins tiles";
}

// ── Town floor tiles ─────────────────────────────────────────────────────────────

TEST_F(FarmTilemapBuilderTest, ContainsTownGroundTilesFromLivingTowns) {
	const bool hasLiving =
		std::ranges::any_of(world_.towns, [](const TownSite& t) { return t.status == TownStatus::Living; });
	if (!hasLiving) GTEST_SKIP() << "Seed 42 has no living towns; cannot test town ground tiles";

	// Living towns are paved with a tier/biome-rolled ground material (dirt/cobblestone/brick/moss).
	const auto present = [&](const char* name) {
		const auto id = tileset_.ResolveTileId(name);
		return id && std::ranges::any_of(tilemap_.tile_ids, [&](const uint32_t t) { return t == *id; });
	};
	EXPECT_TRUE(present("dirt") || present("cobblestone") || present("brick") || present("moss"))
		<< "Living towns should pave their plots with a ground material";
}
