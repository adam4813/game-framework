/// Unit tests for the ambient-creature placement stage (ticket 10): WorldGen::Generate places a
/// sparse set of wandering creatures (livestock near towns, wildlife in the open) into
/// WorldData::creatures. Pure value types — no ECS, no Platform (mirrors world_gen_test /
/// town_layout_test). Asserts observable outcomes: determinism, walkability, and sparse counts.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>

#include "farm/creature_catalog.hpp"
#include "farm/creature_placement.hpp"
#include "farm/world_data.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;

namespace {

constexpr std::array<int, 6> kSeeds{1, 7, 42, 1337, 90210, 2024};

std::uint64_t Key(const int x, const int y) {
	return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) << 32) | static_cast<std::uint32_t>(x);
}

// Every tile covered by a blocking world-object — creatures must never be placed on these.
std::unordered_set<std::uint64_t> BlockedTiles(const WorldData& world) {
	std::unordered_set<std::uint64_t> blocked;
	for (const WorldObject& obj : world.objects) {
		if (!obj.blocksMovement) {
			continue;
		}
		for (const glm::ivec2 t : obj.footprint) {
			blocked.insert(Key(t.x, t.y));
		}
	}
	return blocked;
}

int CountLivingTowns(const WorldData& world) {
	return static_cast<int>(std::ranges::count_if(world.towns, [](const TownSite& t) {
		return t.status == TownStatus::Living;
	}));
}

// A flat world of a single walkable biome, with explicit params/seed set (no pipeline run).
WorldData MakeFlatWorld(const int w, const int h, const Biome biome, const int seed) {
	WorldData world;
	world.seed = seed;
	world.params.width = w;
	world.params.height = h;
	world.grid.width = w;
	world.grid.height = h;
	world.grid.elevation.assign(static_cast<size_t>(w) * h, 0.5F);
	world.grid.biome.assign(static_cast<size_t>(w) * h, biome);
	return world;
}

} // namespace

// ── Invariants over the full generation pipeline, every seed ───────────────────────────────────

TEST(CreaturePlacementTest, every_creature_is_on_a_walkable_unblocked_tile) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const auto blocked = BlockedTiles(world);
		for (const CreatureSpawn& c : world.creatures) {
			ASSERT_TRUE(world.grid.InBounds(c.tile.x, c.tile.y)) << "seed " << seed;
			const Biome b = world.grid.BiomeAt(c.tile.x, c.tile.y);
			EXPECT_TRUE(IsCreatureWalkable(b)) << "seed " << seed << " creature " << c.kind << " on biome " << static_cast<int>(b);
			EXPECT_EQ(blocked.count(Key(c.tile.x, c.tile.y)), 0u) << "seed " << seed << " creature on a blocking object";
		}
	}
}

TEST(CreaturePlacementTest, no_creature_on_non_walkable_biome) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		std::array<int, static_cast<size_t>(Biome::Count_)> perBiome{};
		for (const CreatureSpawn& c : world.creatures) {
			++perBiome[static_cast<size_t>(world.grid.BiomeAt(c.tile.x, c.tile.y))];
		}
		// Only Plains (grass) and Beach (sand) are walkable terrain; every other biome must be empty.
		EXPECT_EQ(perBiome[static_cast<size_t>(Biome::Ocean)], 0) << "seed " << seed;
		EXPECT_EQ(perBiome[static_cast<size_t>(Biome::Lake)], 0) << "seed " << seed;
		EXPECT_EQ(perBiome[static_cast<size_t>(Biome::River)], 0) << "seed " << seed;
		EXPECT_EQ(perBiome[static_cast<size_t>(Biome::Forest)], 0) << "seed " << seed;
		EXPECT_EQ(perBiome[static_cast<size_t>(Biome::Hills)], 0) << "seed " << seed;
		EXPECT_EQ(perBiome[static_cast<size_t>(Biome::Mountain)], 0) << "seed " << seed;
	}
}

TEST(CreaturePlacementTest, is_deterministic_for_same_seed) {
	const WorldData a = WorldGen::Generate(WorldGenParams{}, 12345);
	const WorldData b = WorldGen::Generate(WorldGenParams{}, 12345);
	ASSERT_EQ(a.creatures.size(), b.creatures.size());
	EXPECT_EQ(a.creatures, b.creatures);
}

TEST(CreaturePlacementTest, counts_stay_sparse_within_configured_ranges) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const WorldGenParams& p = world.params;

		// No two creatures share a tile.
		std::unordered_set<std::uint64_t> seen;
		for (const CreatureSpawn& c : world.creatures) {
			EXPECT_TRUE(seen.insert(Key(c.tile.x, c.tile.y)).second) << "seed " << seed << " duplicate tile";
		}

		// Livestock per living town stays within [0, maxLivestockPerTown]; wildlife carries town id 0.
		std::array<int, 4096> livestockPerTown{};
		int wildlife = 0;
		for (const CreatureSpawn& c : world.creatures) {
			if (c.homeTownId == 0) {
				++wildlife;
			}
			else {
				ASSERT_LT(c.homeTownId, 4096);
				++livestockPerTown[static_cast<size_t>(c.homeTownId)];
			}
		}
		for (const int n : livestockPerTown) {
			EXPECT_LE(n, p.maxLivestockPerTown) << "seed " << seed;
		}

		// Overall density stays sparse: bounded by (living towns × max livestock) + (cells × max wildlife).
		const int cellsX = (world.grid.width + p.creatureAreaSize - 1) / p.creatureAreaSize;
		const int cellsY = (world.grid.height + p.creatureAreaSize - 1) / p.creatureAreaSize;
		const int upperBound = CountLivingTowns(world) * p.maxLivestockPerTown + cellsX * cellsY * p.maxWildlifePerArea;
		EXPECT_LE(static_cast<int>(world.creatures.size()), upperBound) << "seed " << seed;
		EXPECT_LE(wildlife, cellsX * cellsY * p.maxWildlifePerArea) << "seed " << seed;
	}
}

TEST(CreaturePlacementTest, default_world_actually_spawns_some_creatures) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	EXPECT_FALSE(world.creatures.empty());
}

// ── Pure-stage tests on hand-built worlds ──────────────────────────────────────────────────────

TEST(CreaturePlacementTest, mountain_world_places_no_creatures) {
	// A world with no walkable terrain (all Mountain) must place nothing, even with living towns.
	WorldData world = MakeFlatWorld(120, 120, Biome::Mountain, 7);
	world.towns.push_back({.id = 1, .name = "Peak", .location = {60, 60}, .size = 20, .status = TownStatus::Living});
	PlaceCreatures(world, DefaultCreatureCatalog());
	EXPECT_TRUE(world.creatures.empty());
}

TEST(CreaturePlacementTest, livestock_cluster_near_living_town) {
	WorldData world = MakeFlatWorld(200, 200, Biome::Plains, 99);
	world.params.creatureTownRadius = 8;
	world.params.minLivestockPerTown = 2;
	world.params.maxLivestockPerTown = 4;
	// No wildlife, so only the near-town livestock stage runs — isolates the clustering assertion.
	world.params.maxWildlifePerArea = 0;
	world.towns.push_back({.id = 1, .name = "Greendale", .location = {100, 100}, .size = 40, .status = TownStatus::Living});

	PlaceCreatures(world, DefaultCreatureCatalog());

	ASSERT_FALSE(world.creatures.empty());
	for (const CreatureSpawn& c : world.creatures) {
		EXPECT_EQ(c.homeTownId, 1);
		EXPECT_LE(std::abs(c.tile.x - 100), world.params.creatureTownRadius);
		EXPECT_LE(std::abs(c.tile.y - 100), world.params.creatureTownRadius);
	}
	EXPECT_GE(static_cast<int>(world.creatures.size()), world.params.minLivestockPerTown);
	EXPECT_LE(static_cast<int>(world.creatures.size()), world.params.maxLivestockPerTown);
}

TEST(CreaturePlacementTest, creatures_avoid_blocking_objects) {
	WorldData world = MakeFlatWorld(60, 60, Biome::Plains, 3);
	world.params.creatureTownRadius = 20;
	world.params.minLivestockPerTown = 4;
	world.params.maxLivestockPerTown = 4;
	world.params.maxWildlifePerArea = 0;
	world.towns.push_back({.id = 1, .name = "Blockton", .location = {30, 30}, .size = 50, .status = TownStatus::Living});

	// Block a solid 11×11 wall over the whole town centre; creatures must spawn on the ring outside it.
	WorldObject wall;
	wall.kind = "wall";
	wall.blocksMovement = true;
	for (int y = 25; y <= 35; ++y) {
		for (int x = 25; x <= 35; ++x) {
			wall.footprint.push_back({x, y});
		}
	}
	world.objects.push_back(std::move(wall));

	PlaceCreatures(world, DefaultCreatureCatalog());

	ASSERT_FALSE(world.creatures.empty());
	for (const CreatureSpawn& c : world.creatures) {
		const bool insideWall = c.tile.x >= 25 && c.tile.x <= 35 && c.tile.y >= 25 && c.tile.y <= 35;
		EXPECT_FALSE(insideWall) << "creature placed on a blocking object at " << c.tile.x << "," << c.tile.y;
	}
}
