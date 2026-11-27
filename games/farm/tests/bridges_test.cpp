#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "farm/bridge_builder.hpp"
#include "farm/farm_tilemap_builder.hpp"
#include "farm/world_data.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;

namespace {

// ── Fixture builders ─────────────────────────────────────────────────────────────
// Small hand-authored deterministic worlds. BuildBridges is a pure function, so these need no RNG,
// no files and no ECS — just a grid, towns, and the wanted connection pairs. Mirrors the style of
// road_router_test.

Grid MakePlainsGrid(const int w, const int h) {
	Grid g;
	g.width = w;
	g.height = h;
	g.elevation.assign(static_cast<size_t>(w) * h, 0.5f);
	g.biome.assign(static_cast<size_t>(w) * h, Biome::Plains);
	return g;
}

void FillBiome(Grid& g, const int x0, const int y0, const int x1, const int y1, const Biome b) {
	for (int y = y0; y <= y1; ++y) {
		for (int x = x0; x <= x1; ++x) {
			if (g.InBounds(x, y)) {
				g.biome[g.Index(x, y)] = b;
			}
		}
	}
}

TownSite MakeTown(const int id, const glm::ivec2 loc, const int size = 50) {
	TownSite t;
	t.id = id;
	t.name = "Town" + std::to_string(id);
	t.location = loc;
	t.size = size;
	t.status = TownStatus::Living;
	return t;
}

std::set<std::pair<int, int>> Connections(const std::vector<std::pair<int, int>>& pairs) {
	std::set<std::pair<int, int>> out;
	for (const auto& [a, b] : pairs) {
		out.insert({std::min(a, b), std::max(a, b)});
	}
	return out;
}

// A grid split into two Plains landmasses by a vertical ocean strait occupying columns
// [straitX, straitX + width). Left town sits at the left edge, right town at the right edge.
Grid MakeStraitGrid(const int w, const int h, const int straitX, const int straitWidth) {
	Grid g = MakePlainsGrid(w, h);
	FillBiome(g, straitX, 0, straitX + straitWidth - 1, h - 1, Biome::Ocean);
	return g;
}

bool AllTilesDeepWater(const Grid& g, const std::vector<glm::ivec2>& tiles) {
	return std::ranges::all_of(tiles, [&](const glm::ivec2 t) {
		const Biome b = g.BiomeAt(t.x, t.y);
		return b == Biome::Ocean || b == Biome::Lake;
	});
}

} // namespace

// AC: when the router finds no land path for a wanted connection, a bridge is placed across the
// narrow water crossing between the two landmasses.
TEST(BridgesTest, FallbackBridgeAcrossNarrowStrait) {
	Grid grid = MakeStraitGrid(40, 20, 18, 3); // 3-wide ocean strait
	const std::vector<TownSite> towns{MakeTown(1, {5, 10}), MakeTown(2, {34, 10})};

	const std::vector<Bridge> bridges =
		BuildBridges(grid, towns, WorldGenParams{}, Connections({{1, 2}}), {});

	ASSERT_EQ(bridges.size(), 1u);
	EXPECT_EQ(bridges[0].Span(), 3) << "deck must cross exactly the 3 water tiles of the strait";
	EXPECT_EQ(bridges[0].reason, BridgeReason::RoutingFallback);
	EXPECT_EQ(bridges[0].townA, 1);
	EXPECT_EQ(bridges[0].townB, 2);
	EXPECT_TRUE(AllTilesDeepWater(grid, bridges[0].deck)) << "every deck tile must sit on water";
}

// AC: the bridge span never exceeds maxBridgeSpan; a longer crossing is left unbuilt and the place
// stays isolated.
TEST(BridgesTest, OverCapCrossingIsLeftUnbuilt) {
	Grid grid = MakeStraitGrid(40, 20, 10, 12); // 12-wide strait
	const std::vector<TownSite> towns{MakeTown(1, {4, 10}), MakeTown(2, {34, 10})};

	WorldGenParams params;
	params.maxBridgeSpan = 8;
	const std::vector<Bridge> bridges = BuildBridges(grid, towns, params, Connections({{1, 2}}), {});

	EXPECT_TRUE(bridges.empty()) << "a 12-tile crossing exceeds the 8-tile cap and must be left unbuilt";
}

// AC: the narrowest crossing is chosen — a bridge spans the pinch point, not a wider section.
TEST(BridgesTest, BridgesTheNarrowestCrossing) {
	// A wide 10-tile sea (columns 15..24) separates the two landmasses, EXCEPT at rows 14-15 where
	// both shores bulge toward each other leaving only a clean 2-tile water neck (columns 20..21).
	Grid grid = MakePlainsGrid(40, 30);
	FillBiome(grid, 15, 0, 24, 29, Biome::Ocean); // the wide sea
	FillBiome(grid, 15, 14, 19, 15, Biome::Plains); // left shore bulges out to column 19
	FillBiome(grid, 22, 14, 24, 15, Biome::Plains); // right shore bulges out to column 22
	// Columns 20..21 at rows 14..15 remain Ocean — the 2-tile pinch point.

	const std::vector<TownSite> towns{MakeTown(1, {5, 14}), MakeTown(2, {34, 14})};
	const std::vector<Bridge> bridges =
		BuildBridges(grid, towns, WorldGenParams{}, Connections({{1, 2}}), {});

	ASSERT_EQ(bridges.size(), 1u);
	EXPECT_LE(bridges[0].Span(), 2) << "the bridge must take the 2-tile neck, not the 10-tile sea";
	EXPECT_TRUE(AllTilesDeepWater(grid, bridges[0].deck));
}

// AC: no bridge is placed where a land path already exists (a single landmass).
TEST(BridgesTest, NoBridgeWhenLandPathExists) {
	Grid grid = MakePlainsGrid(40, 20); // one contiguous landmass
	const std::vector<TownSite> towns{MakeTown(1, {5, 10}), MakeTown(2, {34, 10})};

	const std::vector<Bridge> bridges =
		BuildBridges(grid, towns, WorldGenParams{}, Connections({{1, 2}}), {});

	EXPECT_TRUE(bridges.empty()) << "two towns on the same landmass must never be bridged";
}

// AC: a connection separated by a MOUNTAIN (not water) is not bridged — bridges span water only.
TEST(BridgesTest, MountainBarrierIsNotBridged) {
	Grid grid = MakePlainsGrid(40, 20);
	FillBiome(grid, 18, 0, 20, 19, Biome::Mountain); // a dry wall, no water
	const std::vector<TownSite> towns{MakeTown(1, {5, 10}), MakeTown(2, {34, 10})};

	const std::vector<Bridge> bridges =
		BuildBridges(grid, towns, WorldGenParams{}, Connections({{1, 2}}), {});

	EXPECT_TRUE(bridges.empty()) << "a mountain barrier has no water crossing to bridge";
}

// AC: a bridge the history simulation funded is tagged as a historical investment, carrying the
// event's year.
TEST(BridgesTest, HistoricalInvestmentReasonFromTimeline) {
	Grid grid = MakeStraitGrid(40, 20, 18, 3);
	const std::vector<TownSite> towns{MakeTown(1, {5, 10}), MakeTown(2, {34, 10})};
	const std::vector<HistoryEvent> history{
		{.year = -42, .type = HistoryEventType::BridgeBuilt, .a = 1, .b = 2, .description = "funded a bridge"}
	};

	const std::vector<Bridge> bridges =
		BuildBridges(grid, towns, WorldGenParams{}, Connections({{1, 2}}), history);

	ASSERT_EQ(bridges.size(), 1u);
	EXPECT_EQ(bridges[0].reason, BridgeReason::HistoricalInvestment);
	EXPECT_EQ(bridges[0].year, -42);
}

// AC: determinism — identical inputs produce identical bridges.
TEST(BridgesTest, IsDeterministic) {
	Grid grid = MakeStraitGrid(50, 30, 22, 4);
	const std::vector<TownSite> towns{MakeTown(1, {6, 15}), MakeTown(2, {44, 15})};
	const auto conns = Connections({{1, 2}});

	const std::vector<Bridge> a = BuildBridges(grid, towns, WorldGenParams{}, conns, {});
	const std::vector<Bridge> b = BuildBridges(grid, towns, WorldGenParams{}, conns, {});
	EXPECT_EQ(a, b);
}

// AC: a bridge deck placed on water makes those tiles walkable so the player and NPCs can cross.
// Seam: the tilemap builder, which converts WorldData bridges into the walkability layer.
TEST(BridgesTest, BridgeDeckMakesWaterTilesWalkable) {
	using namespace engine::tilemap;

	// A tiny world: all water except one deck span bridged across it.
	WorldData world;
	world.grid = MakeStraitGrid(10, 5, 4, 2); // columns 4,5 are ocean
	world.params.width = 10;
	world.params.height = 5;
	Bridge bridge;
	bridge.townA = 1;
	bridge.townB = 2;
	bridge.deck = {{4, 2}, {5, 2}};
	bridge.reason = BridgeReason::RoutingFallback;
	world.bridges.push_back(bridge);
	world.objects.push_back(
		{.footprint = bridge.deck, .kind = "bridge", .height = 0.2f, .blocksMovement = false}
	);

	TileSet ts;
	ts.RegisterTile({.id = 0, .name = "grass", .walkable = true});
	ts.RegisterTile({.id = 2, .name = "path", .walkable = true});
	ts.RegisterTile({.id = 3, .name = "water", .walkable = false});
	// Water must be non-walkable for the test to be meaningful.
	ASSERT_FALSE(ts.GetTile(*ts.ResolveTileId("water"))->walkable);

	const Tilemap tm = BuildTilemapFromWorldData(world, ts);

	// The deck tiles are now walkable; a neighbouring un-decked water tile is still blocked.
	EXPECT_TRUE(IsTileWalkable(tm, ts, 4, 2)) << "deck tile must be walkable";
	EXPECT_TRUE(IsTileWalkable(tm, ts, 5, 2)) << "deck tile must be walkable";
	EXPECT_FALSE(IsTileWalkable(tm, ts, 4, 0)) << "un-decked water stays non-walkable";
}

// ── Full-pipeline integration (WorldGen::Generate) ───────────────────────────────

namespace {

bool HistoryHasBridge(const WorldData& w) {
	return std::ranges::any_of(w.history, [](const HistoryEvent& e) {
		return e.type == HistoryEventType::BridgeBuilt;
	});
}

bool TimelineHasBridge(const WorldData& w) {
	return std::ranges::any_of(w.timeline.steps, [](const WorldSnapshot& s) {
		return s.kind == SnapshotKind::BridgeBuilt;
	});
}

} // namespace

// AC: every bridge the full pipeline builds is a genuine water crossing within the span cap — the
// generation never emits an over-cap or non-water bridge for any seed.
TEST(BridgesTest, GeneratedBridgesAreWaterCrossingsWithinCap) {
	for (const int seed : {1, 7, 42, 1337, 90210, 2024}) {
		const WorldData w = WorldGen::Generate(WorldGenParams{}, seed);
		for (const Bridge& b : w.bridges) {
			EXPECT_GE(b.Span(), 1) << "seed " << seed;
			EXPECT_LE(b.Span(), w.params.maxBridgeSpan) << "seed " << seed;
			EXPECT_TRUE(AllTilesDeepWater(w.grid, b.deck)) << "seed " << seed;
		}
	}
}

// AC: a bridge the prosperity-gated history event funds is recorded in the generation timeline with
// a year and description. The event fires on some seeds; whenever it does, the timeline carries it.
TEST(BridgesTest, HistoryEventBridgeAppearsInTimeline) {
	int firing = 0;
	for (int seed = 1; seed <= 32; ++seed) {
		const WorldData w = WorldGen::Generate(WorldGenParams{}, seed);
		if (!HistoryHasBridge(w)) {
			continue;
		}
		++firing;
		EXPECT_TRUE(TimelineHasBridge(w)) << "BridgeBuilt history event missing from timeline, seed " << seed;
		const bool described = std::ranges::any_of(w.history, [](const HistoryEvent& e) {
			return e.type == HistoryEventType::BridgeBuilt && !e.description.empty();
		});
		EXPECT_TRUE(described) << "bridge history event lacks a description, seed " << seed;
	}
	EXPECT_GT(firing, 0) << "the prosperity-gated bridge event never fired across seeds 1..32";
}

// Determinism at the full-pipeline level: the whole bridge set is reproducible for a seed.
TEST(BridgesTest, GeneratedBridgesAreDeterministic) {
	const WorldData a = WorldGen::Generate(WorldGenParams{}, 1337);
	const WorldData b = WorldGen::Generate(WorldGenParams{}, 1337);
	EXPECT_EQ(a.bridges, b.bridges);
}
