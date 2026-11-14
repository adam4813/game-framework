/// Unit tests for the world-object layer (ticket 02): the shared representation for placed
/// things (buildings, trees, props, creatures) that later tickets populate. Pure value types +
/// the pure tilemap/walkability seam — no ECS, no Platform.
#include <gtest/gtest.h>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "engine/engine.hpp"
#include "farm/farm_tilemap_builder.hpp"
#include "farm/object_kind_registry.hpp"
#include "farm/world_data.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;
using namespace engine::tilemap;

namespace {

// A flat, fully walkable world (all Plains → "grass") of the given size, with no roads or towns.
WorldData MakeFlatGrassWorld(const int w, const int h) {
	WorldData world;
	world.grid.width = w;
	world.grid.height = h;
	world.grid.elevation.assign(static_cast<size_t>(w) * h, 0.5F);
	world.grid.biome.assign(static_cast<size_t>(w) * h, Biome::Plains);
	return world;
}

// A TileSet covering every name BuildTilemapFromWorldData can emit; grass (id 0) is walkable.
TileSet MakeFullTileSet() {
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
	};
	for (const auto& [id, name] : kTiles) {
		ts.RegisterTile({.id = id, .name = name, .walkable = (name != std::string_view{"water"})});
	}
	return ts;
}

} // namespace

// ── WorldObject value type in WorldData ──────────────────────────────────────────

TEST(WorldObjectTest, WorldDataCarriesWorldObjectCollection) {
	WorldData world;
	EXPECT_TRUE(world.objects.empty());

	WorldObject tree;
	tree.footprint.push_back(glm::ivec2{3, 4});
	tree.kind = "tree_oak";
	tree.height = 2.5F;
	tree.blocksMovement = true;
	world.objects.push_back(tree);

	ASSERT_EQ(world.objects.size(), 1U);
	EXPECT_EQ(world.objects[0].kind, "tree_oak");
	EXPECT_FLOAT_EQ(world.objects[0].height, 2.5F);
	EXPECT_TRUE(world.objects[0].blocksMovement);
	ASSERT_EQ(world.objects[0].footprint.size(), 1U);
	EXPECT_EQ(world.objects[0].footprint[0], (glm::ivec2{3, 4}));
}

TEST(WorldObjectTest, WorldObjectHasValueEquality) {
	WorldObject a;
	a.footprint = {glm::ivec2{1, 1}};
	a.kind = "house";
	a.height = 3.0F;
	a.blocksMovement = true;

	WorldObject same = a;
	EXPECT_EQ(a, same);

	same.blocksMovement = false;
	EXPECT_NE(a, same);
}

// ── Generation stub + determinism ────────────────────────────────────────────────

TEST(WorldObjectTest, GenerateProducesWorldObjects) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	EXPECT_FALSE(world.objects.empty()) << "The stub world-object stage should place at least one object";
}

TEST(WorldObjectTest, WorldObjectsAreDeterministicForSameSeed) {
	const WorldData a = WorldGen::Generate(WorldGenParams{}, 1337);
	const WorldData b = WorldGen::Generate(WorldGenParams{}, 1337);
	ASSERT_FALSE(a.objects.empty());
	EXPECT_EQ(a.objects, b.objects) << "Same seed must produce identical WorldObject collections";
}

// ── Walkability layer respects world-object footprints ───────────────────────────

TEST(WorldObjectTest, BlockingWorldObjectMakesTileNonWalkable) {
	WorldData world = MakeFlatGrassWorld(8, 8);
	WorldObject rock;
	rock.footprint = {glm::ivec2{3, 4}};
	rock.kind = "landmark";
	rock.blocksMovement = true;
	world.objects.push_back(rock);

	const TileSet ts = MakeFullTileSet();
	const Tilemap tm = BuildTilemapFromWorldData(world, ts);

	// The terrain tile under the object is grass — walkable on its own.
	const auto grassId = ts.ResolveTileId("grass");
	ASSERT_TRUE(grassId.has_value());
	ASSERT_EQ(GetTileIdAt(tm, 3, 4), *grassId);
	const TileDescriptor* descriptor = ts.GetTile(GetTileIdAt(tm, 3, 4));
	ASSERT_NE(descriptor, nullptr);
	EXPECT_TRUE(descriptor->walkable) << "the terrain tile itself must stay flat, walkable ground";

	// But the walkability layer blocks it because a blocking object covers it.
	EXPECT_FALSE(IsTileWalkable(tm, ts, 3, 4)) << "a blocking world-object footprint must be non-walkable";

	// A neighbouring tile with no object remains walkable.
	EXPECT_TRUE(IsTileWalkable(tm, ts, 4, 4));
}

TEST(WorldObjectTest, NonBlockingWorldObjectLeavesTileWalkable) {
	WorldData world = MakeFlatGrassWorld(8, 8);
	WorldObject flower;
	flower.footprint = {glm::ivec2{2, 2}};
	flower.kind = "landmark";
	flower.blocksMovement = false;
	world.objects.push_back(flower);

	const TileSet ts = MakeFullTileSet();
	const Tilemap tm = BuildTilemapFromWorldData(world, ts);
	EXPECT_TRUE(IsTileWalkable(tm, ts, 2, 2)) << "a non-blocking object must not affect walkability";
}

TEST(WorldObjectTest, MultiTileFootprintBlocksEveryCoveredTile) {
	WorldData world = MakeFlatGrassWorld(8, 8);
	WorldObject building;
	building.footprint = {glm::ivec2{1, 1}, glm::ivec2{2, 1}, glm::ivec2{1, 2}, glm::ivec2{2, 2}};
	building.kind = "house";
	building.blocksMovement = true;
	world.objects.push_back(building);

	const TileSet ts = MakeFullTileSet();
	const Tilemap tm = BuildTilemapFromWorldData(world, ts);
	for (const auto& tile : world.objects[0].footprint) {
		EXPECT_FALSE(IsTileWalkable(tm, ts, tile.x, tile.y)) << "footprint tile (" << tile.x << "," << tile.y << ")";
	}
	EXPECT_TRUE(IsTileWalkable(tm, ts, 3, 3)) << "a tile outside the footprint stays walkable";
}

// ── Data-driven object-kind registry ─────────────────────────────────────────────

TEST(WorldObjectTest, ObjectKindRegistryParsesSchema) {
	const auto doc = nlohmann::json::parse(R"({
		"objectKinds": [
			{ "kind": "tree_oak", "height": 2.5, "sprite": "tree_oak", "blocksMovement": true },
			{ "kind": "flower", "height": 0.2, "sprite": "flower_small" }
		]
	})");

	const ObjectKindRegistry reg = ParseObjectKindRegistry(doc);
	ASSERT_EQ(reg.defs.size(), 2U);

	const ObjectKindDef* oak = FindObjectKindDef(reg, "tree_oak");
	ASSERT_NE(oak, nullptr);
	EXPECT_EQ(oak->kind, "tree_oak");
	EXPECT_FLOAT_EQ(oak->height, 2.5F);
	EXPECT_EQ(oak->sprite, "tree_oak");
	EXPECT_TRUE(oak->blocksMovement);

	const ObjectKindDef* flower = FindObjectKindDef(reg, "flower");
	ASSERT_NE(flower, nullptr);
	EXPECT_EQ(flower->sprite, "flower_small");
	EXPECT_FALSE(flower->blocksMovement) << "blocksMovement defaults to false when absent";

	EXPECT_EQ(FindObjectKindDef(reg, "missing"), nullptr);
}

TEST(WorldObjectTest, ObjectKindRegistryToleratesMissingArray) {
	const ObjectKindRegistry reg = ParseObjectKindRegistry(nlohmann::json::object());
	EXPECT_TRUE(reg.defs.empty());
}
