/// Unit tests for the creature catalog (ticket 10): the data-driven list of wandering-creature
/// kinds. Pure value types + JSON parse — no ECS, no Platform (mirrors building_catalog_test).
#include <gtest/gtest.h>

#include <nlohmann/json.hpp>

#include "farm/creature_catalog.hpp"
#include "farm/world_data.hpp"

using namespace game::farm;

TEST(CreatureCatalogTest, default_catalog_has_livestock_and_wildlife) {
	const CreatureCatalog catalog = DefaultCreatureCatalog();
	EXPECT_FALSE(catalog.defs.empty());

	bool anyLivestock = false;
	bool anyWildlife = false;
	for (const CreatureDef& d : catalog.defs) {
		anyLivestock = anyLivestock || d.nearTown;
		anyWildlife = anyWildlife || d.wilderness;
		// Every kind must declare at least one biome and a walkable affinity, or it can never spawn.
		EXPECT_FALSE(d.biomes.empty()) << d.kind;
		EXPECT_FALSE(d.kind.empty());
		EXPECT_GT(d.wanderRadius, 0) << d.kind;
	}
	EXPECT_TRUE(anyLivestock);
	EXPECT_TRUE(anyWildlife);
}

TEST(CreatureCatalogTest, find_returns_entry_by_kind) {
	const CreatureCatalog catalog = DefaultCreatureCatalog();
	const CreatureDef* cow = catalog.Find("cow");
	ASSERT_NE(cow, nullptr);
	EXPECT_EQ(cow->kind, "cow");
	EXPECT_EQ(catalog.Find("not_a_creature"), nullptr);
}

TEST(CreatureCatalogTest, parse_reads_fields_and_biome_affinity) {
	const auto doc = nlohmann::json::parse(R"({
		"creatures": [
			{
				"kind": "cow", "sprite": "cow_sprite",
				"biomes": ["Plains", "Beach"],
				"nearTown": true, "wilderness": false,
				"wanderRadius": 3, "height": 0.7, "weight": 5
			},
			{
				"kind": "deer", "sprite": "deer_sprite",
				"biomes": ["Plains"],
				"wilderness": true,
				"wanderRadius": 6
			}
		]
	})");

	const CreatureCatalog catalog = ParseCreatureCatalog(doc);
	ASSERT_EQ(catalog.defs.size(), 2u);

	const CreatureDef* cow = catalog.Find("cow");
	ASSERT_NE(cow, nullptr);
	EXPECT_EQ(cow->sprite, "cow_sprite");
	EXPECT_TRUE(cow->nearTown);
	EXPECT_FALSE(cow->wilderness);
	EXPECT_EQ(cow->wanderRadius, 3);
	EXPECT_FLOAT_EQ(cow->height, 0.7f);
	EXPECT_EQ(cow->weight, 5);
	ASSERT_EQ(cow->biomes.size(), 2u);
	EXPECT_TRUE(cow->LikesBiome(Biome::Plains));
	EXPECT_TRUE(cow->LikesBiome(Biome::Beach));
	EXPECT_FALSE(cow->LikesBiome(Biome::Forest));

	const CreatureDef* deer = catalog.Find("deer");
	ASSERT_NE(deer, nullptr);
	EXPECT_TRUE(deer->wilderness);
	EXPECT_FALSE(deer->nearTown); // defaulted
	EXPECT_EQ(deer->wanderRadius, 6);
}

TEST(CreatureCatalogTest, parse_skips_unknown_biomes_and_missing_kind) {
	const auto doc = nlohmann::json::parse(R"({
		"creatures": [
			{ "kind": "gull", "biomes": ["Beach", "Nonsense"], "wilderness": true },
			{ "sprite": "no_kind", "biomes": ["Plains"] }
		]
	})");

	const CreatureCatalog catalog = ParseCreatureCatalog(doc);
	ASSERT_EQ(catalog.defs.size(), 1u); // the entry with no "kind" is dropped
	const CreatureDef* gull = catalog.Find("gull");
	ASSERT_NE(gull, nullptr);
	ASSERT_EQ(gull->biomes.size(), 1u); // "Nonsense" skipped
	EXPECT_TRUE(gull->LikesBiome(Biome::Beach));
}

TEST(CreatureCatalogTest, parse_missing_array_yields_empty_catalog) {
	EXPECT_TRUE(ParseCreatureCatalog(nlohmann::json::object()).defs.empty());
	EXPECT_TRUE(ParseCreatureCatalog(nlohmann::json::parse(R"({"creatures": 7})")).defs.empty());
}

TEST(CreatureCatalogTest, try_parse_biome_round_trips_names) {
	Biome b{};
	EXPECT_TRUE(TryParseBiome("Plains", b));
	EXPECT_EQ(b, Biome::Plains);
	EXPECT_TRUE(TryParseBiome("Mountain", b));
	EXPECT_EQ(b, Biome::Mountain);
	EXPECT_FALSE(TryParseBiome("Swamp", b));
}
