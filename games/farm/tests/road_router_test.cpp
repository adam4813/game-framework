#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "farm/road_router.hpp"
#include "farm/world_data.hpp"

using namespace game::farm;

namespace {

// ── Fixture builders ─────────────────────────────────────────────────────────────
// Small, hand-authored deterministic worlds. The router is a pure function, so these need no RNG,
// no files and no ECS — just a grid, towns, their layouts, and the wanted connection pairs.

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

// A minimal walkable plaza layout: a 3×3 floor around the centre and a blocking landmark just north.
TownLayout MakePlazaLayout(const int id, const glm::ivec2 centre) {
	TownLayout l;
	l.townId = id;
	l.ruined = false;
	l.landmarkKind = "well";
	l.landmark = {centre.x, centre.y - 1};
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			l.floorTiles.push_back({centre.x + dx, centre.y + dy});
		}
	}
	return l;
}

// A plaza layout plus a 3×3 building centred on the town, so it has real wall footprint tiles that a
// passing road must route around.
TownLayout MakeBuildingLayout(const int id, const glm::ivec2 centre) {
	TownLayout l = MakePlazaLayout(id, centre);
	TownBuilding b;
	b.kind = "house";
	b.door = {centre.x, centre.y + 1};
	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const glm::ivec2 t{centre.x + dx, centre.y + dy};
			const bool edge = (dx == -1 || dx == 1 || dy == -1 || dy == 1);
			if (edge && t != b.door) {
				b.footprint.push_back(t);
			}
			l.floorTiles.push_back(t); // the whole plot is paved walkable ground, walls sit on top
		}
	}
	l.buildings.push_back(std::move(b));
	return l;
}

std::set<std::pair<int, int>> Connections(const std::vector<std::pair<int, int>>& pairs) {
	std::set<std::pair<int, int>> out;
	for (const auto& [a, b] : pairs) {
		out.insert({std::min(a, b), std::max(a, b)});
	}
	return out;
}

bool HasSegment(const RoadGraph& g, const int a, const int b) {
	const int lo = std::min(a, b);
	const int hi = std::max(a, b);
	return std::ranges::any_of(g.segments, [lo, hi](const RoadSegment& s) {
		return std::min(s.townA, s.townB) == lo && std::max(s.townA, s.townB) == hi;
	});
}

const RoadSegment* FindSegment(const RoadGraph& g, const int a, const int b) {
	const int lo = std::min(a, b);
	const int hi = std::max(a, b);
	for (const RoadSegment& s : g.segments) {
		if (std::min(s.townA, s.townB) == lo && std::max(s.townA, s.townB) == hi) {
			return &s;
		}
	}
	return nullptr;
}

std::set<std::pair<int, int>> PathTileSet(const RoadSegment& s) {
	std::set<std::pair<int, int>> out;
	for (const glm::ivec2 p : s.path) {
		out.insert({p.x, p.y});
	}
	return out;
}

} // namespace

// AC: generated road paths contain no mountain or deep-water tiles, and the path curves (A*, not a
// straight Bresenham line) to route through a gap in a barrier.
TEST(RoadRouterTest, PathAvoidsMountainAndCurvesThroughGap) {
	Grid grid = MakePlainsGrid(40, 40);
	// A vertical mountain wall at x=20 from the top down to y=30, leaving a gap at y=31..39.
	FillBiome(grid, 20, 0, 20, 30, Biome::Mountain);

	const std::vector<TownSite> towns{MakeTown(1, {5, 20}), MakeTown(2, {35, 20})};
	const std::vector<TownLayout> layouts{MakePlazaLayout(1, {5, 20}), MakePlazaLayout(2, {35, 20})};

	const RoadGraph g = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, Connections({{1, 2}}));

	ASSERT_TRUE(HasSegment(g, 1, 2));
	const RoadSegment* seg = FindSegment(g, 1, 2);
	ASSERT_NE(seg, nullptr);
	ASSERT_FALSE(seg->path.empty());

	int maxY = 0;
	for (const glm::ivec2 p : seg->path) {
		const Biome b = grid.BiomeAt(p.x, p.y);
		EXPECT_NE(b, Biome::Mountain);
		EXPECT_NE(b, Biome::Ocean);
		EXPECT_NE(b, Biome::Lake);
		maxY = std::max(maxY, p.y);
	}
	// A straight route would hug y≈20; the detour must dip through the gap below y=30.
	EXPECT_GE(maxY, 31) << "path did not curve around the mountain barrier";
}

// AC: mountain/deep water is blocked — a connection separated only by blocked terrain yields no
// segment (a bridge is ticket 08's job).
TEST(RoadRouterTest, BlockedTerrainWithNoLandPathYieldsNoSegment) {
	Grid grid = MakePlainsGrid(40, 40);
	FillBiome(grid, 20, 0, 20, 39, Biome::Mountain); // full wall, no gap
	// Also a deep-water moat variant on another column to be safe.
	FillBiome(grid, 21, 0, 21, 39, Biome::Lake);

	const std::vector<TownSite> towns{MakeTown(1, {5, 20}), MakeTown(2, {35, 20})};
	const std::vector<TownLayout> layouts{MakePlazaLayout(1, {5, 20}), MakePlazaLayout(2, {35, 20})};

	const RoadGraph g = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, Connections({{1, 2}}));

	EXPECT_FALSE(HasSegment(g, 1, 2)) << "a road crossed impassable terrain";
	EXPECT_TRUE(g.segments.empty());
}

// AC: no road path passes through a town's building footprint.
TEST(RoadRouterTest, PathNeverCrossesBuildingFootprint) {
	Grid grid = MakePlainsGrid(40, 40);
	const std::vector<TownSite> towns{MakeTown(1, {5, 20}), MakeTown(2, {35, 20}), MakeTown(3, {20, 20})};
	const std::vector<TownLayout> layouts{
		MakePlazaLayout(1, {5, 20}),
		MakePlazaLayout(2, {35, 20}),
		MakeBuildingLayout(3, {20, 20}), // sits squarely on the straight A–B line
	};

	const RoadGraph g = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, Connections({{1, 2}}));

	ASSERT_TRUE(HasSegment(g, 1, 2));
	const RoadSegment* seg = FindSegment(g, 1, 2);
	ASSERT_NE(seg, nullptr);

	std::set<std::pair<int, int>> walls;
	for (const TownLayout& l : layouts) {
		for (const TownBuilding& b : l.buildings) {
			for (const glm::ivec2 t : b.footprint) {
				walls.insert({t.x, t.y});
			}
		}
	}
	ASSERT_FALSE(walls.empty());
	for (const glm::ivec2 p : seg->path) {
		EXPECT_FALSE(walls.contains({p.x, p.y})) << "road crossed a building footprint at " << p.x << "," << p.y;
	}
}

// AC: for A, B, C collinear with B between A and C, an A–C road does not bisect B.
TEST(RoadRouterTest, CollinearRoadDoesNotBisectMiddleTown) {
	Grid grid = MakePlainsGrid(60, 40);
	const std::vector<TownSite> towns{MakeTown(1, {5, 20}), MakeTown(2, {30, 20}), MakeTown(3, {55, 20})};
	const std::vector<TownLayout> layouts{
		MakeBuildingLayout(1, {5, 20}),
		MakeBuildingLayout(2, {30, 20}), // the middle town A–C would otherwise plough through
		MakeBuildingLayout(3, {55, 20}),
	};

	const RoadGraph g = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, Connections({{1, 3}}));

	ASSERT_TRUE(HasSegment(g, 1, 3));
	const RoadSegment* seg = FindSegment(g, 1, 3);
	ASSERT_NE(seg, nullptr);

	// Collect every footprint tile of the middle town B: walls, landmark, plaza floor and centre.
	std::set<std::pair<int, int>> bFootprint;
	const TownLayout& b = layouts[1];
	bFootprint.insert({b.landmark.x, b.landmark.y});
	for (const glm::ivec2 t : b.floorTiles) {
		bFootprint.insert({t.x, t.y});
	}
	for (const TownBuilding& bld : b.buildings) {
		for (const glm::ivec2 t : bld.footprint) {
			bFootprint.insert({t.x, t.y});
		}
	}
	bFootprint.insert({towns[1].location.x, towns[1].location.y});

	for (const glm::ivec2 p : seg->path) {
		EXPECT_FALSE(bFootprint.contains({p.x, p.y})) << "A–C road bisected town B at " << p.x << "," << p.y;
	}
}

// AC: a connection redundant within the detour factor is absent from the road graph.
TEST(RoadRouterTest, RedundantConnectionIsFiltered) {
	Grid grid = MakePlainsGrid(60, 40);
	const std::vector<TownSite> towns{MakeTown(1, {5, 20}), MakeTown(2, {30, 20}), MakeTown(3, {55, 20})};
	const std::vector<TownLayout> layouts{
		MakePlazaLayout(1, {5, 20}),
		MakePlazaLayout(2, {30, 20}),
		MakePlazaLayout(3, {55, 20}),
	};

	// Wanted: the A–B–C chain plus a direct A–C express. A–C is ~2× the chain hops but the chain
	// links A and C well within the detour factor, so the express must be dropped.
	const RoadGraph g = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, Connections({{1, 2}, {2, 3}, {1, 3}}));

	EXPECT_TRUE(HasSegment(g, 1, 2));
	EXPECT_TRUE(HasSegment(g, 2, 3));
	EXPECT_FALSE(HasSegment(g, 1, 3)) << "redundant A–C express should be filtered";
}

// AC: road tiles already laid cost significantly less, so new routes share trunks rather than
// running as parallels.
TEST(RoadRouterTest, NewRouteReusesExistingTrunk) {
	Grid grid = MakePlainsGrid(40, 70);
	const std::vector<TownSite> towns{MakeTown(1, {20, 8}), MakeTown(2, {28, 8}), MakeTown(3, {20, 60})};
	const std::vector<TownLayout> layouts{
		MakePlazaLayout(1, {20, 8}),
		MakePlazaLayout(2, {28, 8}),
		MakePlazaLayout(3, {20, 60}),
	};

	// Both A and B want a road to the far town D; B's route should merge onto A's trunk.
	const RoadGraph g = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, Connections({{1, 3}, {2, 3}}));

	const RoadSegment* ad = FindSegment(g, 1, 3);
	const RoadSegment* bd = FindSegment(g, 2, 3);
	ASSERT_NE(ad, nullptr);
	ASSERT_NE(bd, nullptr);

	const std::set<std::pair<int, int>> adTiles = PathTileSet(*ad);
	int shared = 0;
	for (const glm::ivec2 p : bd->path) {
		if (adTiles.contains({p.x, p.y})) {
			++shared;
		}
	}
	EXPECT_GE(shared, 20) << "second route did not reuse the existing trunk (shared " << shared << " tiles)";
}

// AC: two calls with the same inputs produce identical road paths (determinism).
TEST(RoadRouterTest, IsDeterministic) {
	Grid grid = MakePlainsGrid(50, 50);
	FillBiome(grid, 25, 0, 25, 20, Biome::Mountain);
	FillBiome(grid, 10, 30, 30, 32, Biome::Forest);
	const std::vector<TownSite> towns{MakeTown(1, {5, 5}), MakeTown(2, {45, 45}), MakeTown(3, {45, 5})};
	const std::vector<TownLayout> layouts{
		MakePlazaLayout(1, {5, 5}),
		MakePlazaLayout(2, {45, 45}),
		MakePlazaLayout(3, {45, 5}),
	};
	const auto conns = Connections({{1, 2}, {2, 3}, {1, 3}});

	const RoadGraph a = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, conns);
	const RoadGraph b = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, conns);
	EXPECT_TRUE(a == b);
}
