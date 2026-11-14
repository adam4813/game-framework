/// Unit tests for enterable forests (ticket 05): walkable woodland ground scattered with blocking
/// tree world-objects whose density varies spatially (dense cores, sparse edges, clearings), driven
/// by a secondary noise field and a data-driven species catalog. Pure seams only — the pure
/// WorldGen::Generate output, the pure ParseTreeSpeciesCatalog, and the pure tilemap/walkability
/// builder. No ECS, no Platform.
#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <iostream>
#include <queue>
#include <unordered_set>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>

#include "engine/engine.hpp"
#include "farm/farm_tilemap_builder.hpp"
#include "farm/tree_species_catalog.hpp"
#include "farm/world_data.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;
using namespace engine::tilemap;

namespace {

// Seeds every forest guarantee must hold for (matching world_gen_test.cpp).
constexpr std::array<int, 6> kSeeds{1, 7, 42, 1337, 90210, 2024};

// Minimum woodland tiles for a seed to be a meaningful statistical sample.
constexpr int kMinWoodlandForStats = 150;

[[nodiscard]] bool IsWoodland(const Biome b) { return b == Biome::Forest || b == Biome::Hills; }

// A blocking tree object (kind "tree_*"). Filtering by kind excludes ticket-04 building objects,
// which also block movement, so the forest-density measurements count trees only.
[[nodiscard]] bool IsTree(const WorldObject& obj) { return obj.blocksMovement && obj.kind.rfind("tree", 0) == 0; }

// Index set of tiles covered by a blocking tree object (trees have single-tile footprints).
[[nodiscard]] std::vector<uint8_t> TreeMask(const WorldData& world) {
	std::vector<uint8_t> mask(static_cast<size_t>(world.grid.width) * world.grid.height, 0);
	for (const WorldObject& obj : world.objects) {
		if (!IsTree(obj)) {
			continue;
		}
		for (const glm::ivec2& t : obj.footprint) {
			if (world.grid.InBounds(t.x, t.y)) {
				mask[static_cast<size_t>(t.y) * world.grid.width + t.x] = 1;
			}
		}
	}
	return mask;
}

// Index set of tiles occupied by a town's layout (ground plots, streets, fields, building walls).
// These forest tiles are a settlement, not open woodland, so the density measurements exclude them:
// a village carved out of the forest should not count as "sparse forest".
[[nodiscard]] std::vector<uint8_t> TownTileMask(const WorldData& world) {
	std::vector<uint8_t> mask(static_cast<size_t>(world.grid.width) * world.grid.height, 0);
	const auto mark = [&](const glm::ivec2 t) {
		if (world.grid.InBounds(t.x, t.y)) {
			mask[static_cast<size_t>(t.y) * world.grid.width + t.x] = 1;
		}
	};
	for (const TownLayout& layout : world.townLayouts) {
		for (const glm::ivec2 t : layout.floorTiles) mark(t);
		for (const glm::ivec2 t : layout.pathTiles) mark(t);
		for (const glm::ivec2 t : layout.fieldTiles) mark(t);
		for (const TownBuilding& b : layout.buildings) {
			for (const glm::ivec2 t : b.footprint) mark(t);
		}
	}
	return mask;
}

struct ForestStats {
	int woodland = 0; // forest + deep-forest biome tiles
	int treed = 0;    // woodland tiles covered by a blocking tree
	int forest = 0;
	int forestTreed = 0;
	int deep = 0;
	int deepTreed = 0;
};

[[nodiscard]] ForestStats MeasureForest(const WorldData& world) {
	ForestStats s;
	const std::vector<uint8_t> trees = TreeMask(world);
	const std::vector<uint8_t> town = TownTileMask(world);
	for (int y = 0; y < world.grid.height; ++y) {
		for (int x = 0; x < world.grid.width; ++x) {
			const Biome b = world.grid.BiomeAt(x, y);
			if (!IsWoodland(b)) {
				continue;
			}
			const size_t idx = static_cast<size_t>(y) * world.grid.width + x;
			if (town[idx] != 0) {
				continue; // settlement ground, not open woodland — excluded from the density sample
			}
			const bool treed = trees[idx] != 0;
			++s.woodland;
			s.treed += treed ? 1 : 0;
			if (b == Biome::Forest) {
				++s.forest;
				s.forestTreed += treed ? 1 : 0;
			}
			else {
				++s.deep;
				s.deepTreed += treed ? 1 : 0;
			}
		}
	}
	return s;
}

// Largest contiguous (4-connected) region of woodland tiles that carry no tree — a clearing.
[[nodiscard]] int LargestClearing(const WorldData& world) {
	const int w = world.grid.width;
	const int h = world.grid.height;
	const std::vector<uint8_t> trees = TreeMask(world);
	const auto open = [&](const int x, const int y) {
		return world.grid.InBounds(x, y)
			   && IsWoodland(world.grid.BiomeAt(x, y))
			   && trees[static_cast<size_t>(y) * w + x] == 0;
	};
	std::vector<uint8_t> seen(static_cast<size_t>(w) * h, 0);
	int best = 0;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			if (!open(x, y) || seen[static_cast<size_t>(y) * w + x]) {
				continue;
			}
			int size = 0;
			std::queue<glm::ivec2> q;
			q.push({x, y});
			seen[static_cast<size_t>(y) * w + x] = 1;
			while (!q.empty()) {
				const glm::ivec2 p = q.front();
				q.pop();
				++size;
				for (const glm::ivec2 d : {glm::ivec2{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
					const int nx = p.x + d.x;
					const int ny = p.y + d.y;
					if (open(nx, ny) && !seen[static_cast<size_t>(ny) * w + nx]) {
						seen[static_cast<size_t>(ny) * w + nx] = 1;
						q.push({nx, ny});
					}
				}
			}
			best = std::max(best, size);
		}
	}
	return best;
}

// TileSet mirroring assets/data/tilesets/basic.json walkability — forest/deep_forest are walkable
// woodland ground; water/stone_wall block.
[[nodiscard]] TileSet WoodlandTileSet() {
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
		const bool walkable = name != std::string_view{"water"} && name != std::string_view{"stone_wall"};
		ts.RegisterTile({.id = id, .name = name, .walkable = walkable});
	}
	return ts;
}

} // namespace

// ── Catalog parsing (data-driven species, no C++ change to extend) ───────────────

TEST(EnterableForestsTest, CatalogParsesBiomeKeyedSpecies) {
	const auto doc = nlohmann::json::parse(R"({
		"treeSpecies": {
			"forest": [
				{ "kind": "tree_oak", "sprite": "oak", "minHeight": 2.0, "maxHeight": 3.0 },
				{ "kind": "tree_birch", "sprite": "birch", "minHeight": 1.5, "maxHeight": 2.5 }
			],
			"deep_forest": [
				{ "kind": "tree_pine", "sprite": "pine_dark", "minHeight": 3.0, "maxHeight": 4.5 }
			]
		}
	})");

	const TreeSpeciesCatalog cat = ParseTreeSpeciesCatalog(doc);
	const std::vector<TreeSpecies>* forest = cat.SpeciesForBiome("forest");
	ASSERT_NE(forest, nullptr);
	ASSERT_EQ(forest->size(), 2U);
	EXPECT_EQ((*forest)[0].kind, "tree_oak");
	EXPECT_FLOAT_EQ((*forest)[0].maxHeight, 3.0F);

	const std::vector<TreeSpecies>* deep = cat.SpeciesForBiome("deep_forest");
	ASSERT_NE(deep, nullptr);
	ASSERT_EQ(deep->size(), 1U);
	EXPECT_EQ((*deep)[0].kind, "tree_pine");

	EXPECT_EQ(cat.SpeciesForBiome("plains"), nullptr) << "unlisted biome yields no species";
}

TEST(EnterableForestsTest, CatalogMaxHeightDefaultsToMinHeight) {
	const auto doc = nlohmann::json::parse(R"({
		"treeSpecies": { "forest": [ { "kind": "stump", "minHeight": 1.2 } ] }
	})");
	const TreeSpeciesCatalog cat = ParseTreeSpeciesCatalog(doc);
	const std::vector<TreeSpecies>* forest = cat.SpeciesForBiome("forest");
	ASSERT_NE(forest, nullptr);
	ASSERT_EQ(forest->size(), 1U);
	EXPECT_FLOAT_EQ((*forest)[0].minHeight, 1.2F);
	EXPECT_FLOAT_EQ((*forest)[0].maxHeight, 1.2F);
}

TEST(EnterableForestsTest, CatalogToleratesMissingObject) {
	EXPECT_TRUE(ParseTreeSpeciesCatalog(nlohmann::json::object()).Empty());
}

TEST(EnterableForestsTest, DefaultCatalogCoversBothWoodlands) {
	const TreeSpeciesCatalog cat = DefaultTreeSpeciesCatalog();
	EXPECT_NE(cat.SpeciesForBiome("forest"), nullptr);
	EXPECT_NE(cat.SpeciesForBiome("deep_forest"), nullptr);
}

// ── Walkable woodland ground ─────────────────────────────────────────────────────

TEST(EnterableForestsTest, ForestTerrainTilesAreWalkable) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const TileSet ts = WoodlandTileSet();
	const Tilemap tm = BuildTilemapFromWorldData(world, ts);
	const std::vector<uint8_t> trees = TreeMask(world);

	// Check the actual woodland GROUND tiles ("forest"/"deep_forest"). Town/road passes may overpaint
	// some woodland-biome tiles with floor/path tiles; those are walkable too, but here we assert the
	// woodland ground itself became walkable (the core of "forests are enterable").
	const auto forestId = ts.ResolveTileId("forest");
	const auto deepId = ts.ResolveTileId("deep_forest");
	ASSERT_TRUE(forestId.has_value());
	ASSERT_TRUE(deepId.has_value());

	int woodlandGround = 0;
	int walkableOpenWoodland = 0;
	for (int y = 0; y < world.grid.height; ++y) {
		for (int x = 0; x < world.grid.width; ++x) {
			const uint32_t id = GetTileIdAt(tm, x, y);
			if (id != *forestId && id != *deepId) {
				continue;
			}
			++woodlandGround;
			const TileDescriptor* desc = ts.GetTile(id);
			ASSERT_NE(desc, nullptr);
			EXPECT_TRUE(desc->walkable) << "woodland ground tile must be walkable at (" << x << "," << y << ")";
			// An open (un-treed) woodland tile is actually walkable end-to-end.
			if (trees[static_cast<size_t>(y) * world.grid.width + x] == 0) {
				walkableOpenWoodland += IsTileWalkable(tm, ts, x, y) ? 1 : 0;
			}
		}
	}
	ASSERT_GT(woodlandGround, 0) << "seed 42 should contain woodland ground tiles";
	EXPECT_GT(walkableOpenWoodland, 0) << "forests must be enterable (open woodland tiles are walkable)";
}

TEST(EnterableForestsTest, TreeFootprintsBlockMovement) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const TileSet ts = WoodlandTileSet();
	const Tilemap tm = BuildTilemapFromWorldData(world, ts);

	ASSERT_FALSE(world.objects.empty());
	for (const WorldObject& obj : world.objects) {
		if (!IsTree(obj)) {
			continue;
		}
		for (const glm::ivec2& t : obj.footprint) {
			EXPECT_FALSE(IsTileWalkable(tm, ts, t.x, t.y)) << "tree tile (" << t.x << "," << t.y << ") must block";
		}
	}
}

// ── Trees are placed, data-driven, and deterministic ─────────────────────────────

TEST(EnterableForestsTest, TreesAreScatteredAcrossForests) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const bool anyTree = std::ranges::any_of(world.objects, [](const WorldObject& o) {
		return o.blocksMovement && (o.kind.rfind("tree", 0) == 0);
	});
	EXPECT_TRUE(anyTree) << "forests should contain blocking tree world-objects";
}

TEST(EnterableForestsTest, DeterministicForSameSeed) {
	for (const int seed : kSeeds) {
		const WorldData a = WorldGen::Generate(WorldGenParams{}, seed);
		const WorldData b = WorldGen::Generate(WorldGenParams{}, seed);
		EXPECT_EQ(a.objects, b.objects) << "tree placement must be identical for seed " << seed;
	}
}

TEST(EnterableForestsTest, NoTwoTreesAreCardinallyAdjacent) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const int w = world.grid.width;
		const int h = world.grid.height;

		// Build a quick lookup: which tiles have a tree footprint?
		std::vector<uint8_t> treeTile(static_cast<size_t>(w) * h, 0);
		for (const WorldObject& obj : world.objects) {
			if (!IsTree(obj)) {
				continue;
			}
			for (const glm::ivec2& t : obj.footprint) {
				if (world.grid.InBounds(t.x, t.y)) {
					treeTile[static_cast<size_t>(t.y) * w + t.x] = 1;
				}
			}
		}

		// For every tree tile, assert none of its four cardinal neighbors also has a tree.
		for (const WorldObject& obj : world.objects) {
			if (!IsTree(obj)) {
				continue;
			}
			for (const glm::ivec2& t : obj.footprint) {
				if (!world.grid.InBounds(t.x, t.y)) {
					continue;
				}
				const std::pair<int, int> neighbors[4] = {
					{t.x - 1, t.y}, {t.x + 1, t.y}, {t.x, t.y - 1}, {t.x, t.y + 1}
				};
				for (const auto [nx, ny] : neighbors) {
					if (!world.grid.InBounds(nx, ny)) {
						continue;
					}
					EXPECT_EQ(treeTile[static_cast<size_t>(ny) * w + nx], 0)
						<< "trees at (" << t.x << "," << t.y << ") and (" << nx << "," << ny
						<< ") are cardinally adjacent, seed " << seed;
				}
			}
		}
	}
}

TEST(EnterableForestsTest, TreeHeightsRespectSpeciesRange) {
	const TreeSpeciesCatalog cat = DefaultTreeSpeciesCatalog();
	// The 2-arg Generate uses DefaultTreeSpeciesCatalog internally, so `cat` is the source of truth.
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	for (const WorldObject& obj : world.objects) {
		if (obj.kind.rfind("tree", 0) != 0) {
			continue;
		}
		// Locate the species across all biomes and confirm the instance height is within its range.
		bool matched = false;
		for (const auto& [biome, species] : cat.speciesByBiome) {
			for (const TreeSpecies& sp : species) {
				if (sp.kind == obj.kind) {
					matched = true;
					EXPECT_GE(obj.height, sp.minHeight - 1e-4F) << obj.kind;
					EXPECT_LE(obj.height, sp.maxHeight + 1e-4F) << obj.kind;
				}
			}
		}
		EXPECT_TRUE(matched) << "tree kind " << obj.kind << " must come from the catalog";
	}
}

// ── Density range, spatial variation, clearings, deep > forest ───────────────────

TEST(EnterableForestsTest, TreeDensityFallsInExpectedRange) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const ForestStats s = MeasureForest(world);
		if (s.woodland < kMinWoodlandForStats) {
			GTEST_SKIP() << "seed " << seed << " has too little woodland (" << s.woodland << ") for a density sample";
		}
		const double fraction = static_cast<double>(s.treed) / s.woodland;
		EXPECT_GE(fraction, 0.05) << "forest too sparse, seed " << seed;
		EXPECT_LE(fraction, 0.40) << "forest too dense (no room to walk), seed " << seed;
	}
}

TEST(EnterableForestsTest, EveryTypicalForestHasAClearing) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const ForestStats s = MeasureForest(world);
		if (s.woodland < kMinWoodlandForStats) {
			GTEST_SKIP() << "seed " << seed << " has too little woodland for a clearing test";
		}
		EXPECT_GE(LargestClearing(world), 12) << "no sizeable clearing, seed " << seed;
	}
}

TEST(EnterableForestsTest, DeepForestIsDenserThanForest) {
	// Deep forest (Biome::Hills) should be noticeably denser than regular forest. Aggregate across
	// seeds so the comparison is robust to any single sparse seed.
	long forestTiles = 0;
	long forestTreed = 0;
	long deepTiles = 0;
	long deepTreed = 0;
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const ForestStats s = MeasureForest(world);
		forestTiles += s.forest;
		forestTreed += s.forestTreed;
		deepTiles += s.deep;
		deepTreed += s.deepTreed;
	}
	ASSERT_GT(forestTiles, 0);
	ASSERT_GT(deepTiles, 0);
	const double forestDensity = static_cast<double>(forestTreed) / forestTiles;
	const double deepDensity = static_cast<double>(deepTreed) / deepTiles;
	EXPECT_GT(deepDensity, forestDensity * 1.1) << "deep forest should be meaningfully denser than forest";
}

// ── Roads/trails clear a passage through the trees ───────────────────────────────

TEST(EnterableForestsTest, NoTreeSitsOnARoadTile) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		if (world.roads.segments.empty()) {
			GTEST_SKIP() << "seed " << seed << " produced no roads";
		}
		const TileSet ts = WoodlandTileSet();
		const Tilemap tm = BuildTilemapFromWorldData(world, ts);
		const auto pathId = ts.ResolveTileId("path");
		ASSERT_TRUE(pathId.has_value());

		for (const WorldObject& obj : world.objects) {
			if (!IsTree(obj)) {
				continue;
			}
			for (const glm::ivec2& t : obj.footprint) {
				EXPECT_NE(GetTileIdAt(tm, t.x, t.y), *pathId)
					<< "tree on road tile ("
					<< t.x
					<< ","
					<< t.y
					<< "), seed "
					<< seed;
			}
		}
	}
}

// ── Diagnostic (always passes): prints measured densities for calibration ────────

TEST(EnterableForestsTest, DiagnosticDensityReport) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const ForestStats s = MeasureForest(world);
		const double frac = s.woodland ? static_cast<double>(s.treed) / s.woodland : 0.0;
		const double fFrac = s.forest ? static_cast<double>(s.forestTreed) / s.forest : 0.0;
		const double dFrac = s.deep ? static_cast<double>(s.deepTreed) / s.deep : 0.0;
		std::cout
			<< "[forest] seed="
			<< seed
			<< " woodland="
			<< s.woodland
			<< " treed="
			<< s.treed
			<< " frac="
			<< frac
			<< " forest("
			<< s.forest
			<< ")="
			<< fFrac
			<< " deep("
			<< s.deep
			<< ")="
			<< dFrac
			<< " clearing="
			<< LargestClearing(world)
			<< "\n";
	}
	SUCCEED();
}
