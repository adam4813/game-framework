#include <gtest/gtest.h>

#include <algorithm>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "farm/road_hubs.hpp"
#include "farm/road_router.hpp"
#include "farm/world_data.hpp"

using namespace game::farm;

namespace {

// ── Small, hand-authored deterministic fixtures (pure functions: no RNG, files or ECS) ──

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

TownSite MakeTown(const int id, const glm::ivec2 loc, const int size = 50, const TownStatus st = TownStatus::Living) {
	TownSite t;
	t.id = id;
	t.name = "Town" + std::to_string(id);
	t.location = loc;
	t.size = size;
	t.status = st;
	return t;
}

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

std::set<std::pair<int, int>> Connections(const std::vector<std::pair<int, int>>& pairs) {
	std::set<std::pair<int, int>> out;
	for (const auto& [a, b] : pairs) {
		out.insert({std::min(a, b), std::max(a, b)});
	}
	return out;
}

long DistSq(const glm::ivec2 a, const glm::ivec2 b) {
	const long dx = a.x - b.x;
	const long dy = a.y - b.y;
	return dx * dx + dy * dy;
}

// A RoadGraph whose single segment follows a straight run of tiles (so convergence probes hit it).
RoadSegment StraightSegment(const int a, const int b, const glm::ivec2 from, const glm::ivec2 to) {
	RoadSegment s;
	s.townA = std::min(a, b);
	s.townB = std::max(a, b);
	const int steps = std::max(std::abs(to.x - from.x), std::abs(to.y - from.y));
	for (int i = 0; i <= steps; ++i) {
		const float t = steps == 0 ? 0.0f : static_cast<float>(i) / static_cast<float>(steps);
		s.path.push_back(
			{from.x + static_cast<int>((to.x - from.x) * t), from.y + static_cast<int>((to.y - from.y) * t)}
		);
	}
	return s;
}

} // namespace

// ── AC: generated hubs respect minimum spacing (and the hub-count cap) ──────────────

TEST(RoadHubsTest, HubsRespectMinimumSpacingAndCount) {
	Grid grid = MakePlainsGrid(80, 80);
	// Four towns forming an X so several town-to-town corridors cross the interior.
	const std::vector<TownSite> towns{
		MakeTown(1, {10, 10}),
		MakeTown(2, {70, 10}),
		MakeTown(3, {10, 70}),
		MakeTown(4, {70, 70}),
	};

	WorldGenParams params; // hubCount = 3, hubMinSpacing = 24
	const std::vector<RoadHub> hubs = PlaceRoadHubs(grid, towns, params);

	ASSERT_FALSE(hubs.empty()) << "a world central to multiple towns should yield at least one hub";
	EXPECT_LE(static_cast<int>(hubs.size()), params.hubCount);
	const long minSq = static_cast<long>(params.hubMinSpacing) * params.hubMinSpacing;
	for (size_t i = 0; i < hubs.size(); ++i) {
		for (size_t j = i + 1; j < hubs.size(); ++j) {
			EXPECT_GE(DistSq(hubs[i].location, hubs[j].location), minSq)
				<< "hubs "
				<< i
				<< " and "
				<< j
				<< " are closer than hubMinSpacing";
		}
	}
}

TEST(RoadHubsTest, HubPlacementIsDeterministic) {
	Grid grid = MakePlainsGrid(80, 80);
	FillBiome(grid, 38, 0, 42, 30, Biome::Mountain); // a pass between the lower towns
	const std::vector<TownSite> towns{
		MakeTown(1, {10, 15}),
		MakeTown(2, {70, 15}),
		MakeTown(3, {40, 70}),
	};
	const std::vector<RoadHub> a = PlaceRoadHubs(grid, towns, WorldGenParams{});
	const std::vector<RoadHub> b = PlaceRoadHubs(grid, towns, WorldGenParams{});
	EXPECT_EQ(a, b);
}

TEST(RoadHubsTest, HubsNeverSitOnImpassableTerrain) {
	Grid grid = MakePlainsGrid(80, 80);
	FillBiome(grid, 30, 30, 50, 50, Biome::Mountain); // a central massif the hubs must avoid
	FillBiome(grid, 0, 0, 5, 79, Biome::Ocean);
	const std::vector<TownSite> towns{
		MakeTown(1, {12, 12}),
		MakeTown(2, {70, 12}),
		MakeTown(3, {12, 70}),
		MakeTown(4, {70, 70}),
	};
	WorldGenParams params;
	for (const RoadHub& hub : PlaceRoadHubs(grid, towns, params)) {
		EXPECT_GE(params.roadBiomeCost[static_cast<size_t>(grid.BiomeAt(hub.location.x, hub.location.y))], 0.0f)
			<< "hub placed on blocked terrain at "
			<< hub.location.x
			<< ","
			<< hub.location.y;
	}
}

TEST(RoadHubsTest, FewerThanTwoLivingTownsYieldsNoHubs) {
	Grid grid = MakePlainsGrid(60, 60);
	const std::vector<TownSite> towns{MakeTown(1, {30, 30})};
	EXPECT_TRUE(PlaceRoadHubs(grid, towns, WorldGenParams{}).empty());
}

// ── AC: the attractor field is a SOFT, bounded discount ─────────────────────────────

TEST(RoadHubsTest, CostFieldIsSoftAndBounded) {
	Grid grid = MakePlainsGrid(40, 40);
	WorldGenParams params;
	const std::vector<RoadHub> hubs{{.location = {20, 20}, .funnelScore = 1.0f}};
	const std::vector<float> field = BuildRouteCostField(grid, {}, hubs, {}, params);

	ASSERT_EQ(field.size(), static_cast<size_t>(grid.width) * grid.height);
	for (const float f : field) {
		EXPECT_GT(f, 0.0f) << "a discount must never make a tile free (soft, never forced)";
		EXPECT_LE(f, 1.0f) << "the field is a discount only (never a surcharge)";
	}
	// Discounted at the hub, untouched far away.
	EXPECT_LT(field[grid.Index(20, 20)], 1.0f) << "the hub centre must be discounted";
	EXPECT_FLOAT_EQ(field[grid.Index(0, 0)], 1.0f) << "a tile beyond the hub radius is unchanged";
}

// ── AC: towns attract scaled by size AND road-connectivity ──────────────────────────

TEST(RoadHubsTest, LargeWellConnectedTownAttractsMoreThanSmallIsolatedTown) {
	Grid grid = MakePlainsGrid(60, 40);
	WorldGenParams params;
	const std::vector<TownSite> towns{
		MakeTown(1, {12, 20}, 120), // large
		MakeTown(2, {48, 20}, 20),  // small
	};
	// Town 1 is well connected (degree 3); town 2 is isolated (degree 0).
	const auto conns = Connections({{1, 3}, {1, 4}, {1, 5}});
	const std::vector<float> field = BuildRouteCostField(grid, towns, {}, conns, params);

	EXPECT_LT(field[grid.Index(12, 20)], field[grid.Index(48, 20)])
		<< "a large, well-connected town must pull routes harder (deeper discount)";
}

TEST(RoadHubsTest, ConnectivityDeepensTheTownAttractor) {
	Grid grid = MakePlainsGrid(60, 40);
	WorldGenParams params;
	// Same size; only connectivity differs, so the attractor difference is attributable to it alone.
	const std::vector<TownSite> towns{
		MakeTown(1, {12, 20}, 60),
		MakeTown(2, {48, 20}, 60),
	};
	const auto conns = Connections({{1, 3}, {1, 4}}); // town 1 degree 2, town 2 degree 0
	const std::vector<float> field = BuildRouteCostField(grid, towns, {}, conns, params);

	EXPECT_LT(field[grid.Index(12, 20)], field[grid.Index(48, 20)])
		<< "the better-connected town of equal size must have the deeper discount";
}

// ── AC: the router honours the discount field (soft) but still routes validly ───────

TEST(RoadHubsTest, DiscountFieldBendsTheChosenRoute) {
	// A mountain wall at x=20 with two gaps: a near gap (y≈12, cheaper) and a far gap (y≈30).
	Grid grid = MakePlainsGrid(40, 44);
	FillBiome(grid, 20, 0, 20, 10, Biome::Mountain);
	FillBiome(grid, 20, 14, 20, 27, Biome::Mountain);
	FillBiome(grid, 20, 33, 20, 43, Biome::Mountain);
	const std::vector<TownSite> towns{MakeTown(1, {5, 20}), MakeTown(2, {35, 20})};
	const std::vector<TownLayout> layouts{MakePlazaLayout(1, {5, 20}), MakePlazaLayout(2, {35, 20})};
	const auto conns = Connections({{1, 2}});

	// Baseline: no field → the router takes the nearer (upper) gap.
	const RoadGraph base = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, conns);
	ASSERT_EQ(base.segments.size(), 1u);
	int baseMinY = 999;
	for (const glm::ivec2 p : base.segments[0].path) {
		baseMinY = std::min(baseMinY, p.y);
	}
	EXPECT_LE(baseMinY, 14) << "baseline route should use the upper gap";

	// A strong discount centred on the far (lower) gap should pull the route down through it.
	WorldGenParams params;
	params.hubRouteDiscount = 0.15f; // a decisive pull for the test
	params.hubDiscountRadius = 18;
	const std::vector<RoadHub> hubs{{.location = {20, 30}, .funnelScore = 1.0f}};
	std::vector<float> field = BuildRouteCostField(grid, {}, hubs, {}, params);
	const RoadGraph pulled = BuildRoadNetwork(grid, towns, layouts, params, conns, field);
	ASSERT_EQ(pulled.segments.size(), 1u);
	int pulledMaxY = 0;
	for (const glm::ivec2 p : pulled.segments[0].path) {
		pulledMaxY = std::max(pulledMaxY, p.y);
	}
	EXPECT_GE(pulledMaxY, 28) << "the soft discount should bend the route toward the lower gap";
}

TEST(RoadHubsTest, EmptyDiscountFieldMatchesTheBaselineRouter) {
	Grid grid = MakePlainsGrid(40, 40);
	FillBiome(grid, 20, 0, 20, 30, Biome::Mountain);
	const std::vector<TownSite> towns{MakeTown(1, {5, 20}), MakeTown(2, {35, 20})};
	const std::vector<TownLayout> layouts{MakePlazaLayout(1, {5, 20}), MakePlazaLayout(2, {35, 20})};
	const auto conns = Connections({{1, 2}});

	const RoadGraph base = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, conns);
	const RoadGraph empty = BuildRoadNetwork(grid, towns, layouts, WorldGenParams{}, conns, std::vector<float>{});
	EXPECT_EQ(base, empty) << "an empty discount field must leave routing unchanged (backward compatible)";
}

// ── AC: hub markers + crossroads-outpost selection ──────────────────────────────────

TEST(RoadHubsTest, MarkersOnlyWhereRoadsPassThroughAHub) {
	WorldGenParams params;
	RoadGraph roads;
	roads.segments.push_back(StraightSegment(1, 2, {0, 20}, {40, 20})); // runs along y=20

	const std::vector<RoadHub> hubs{
		{.location = {20, 20}, .funnelScore = 2.0f}, // on the road
		{.location = {20, 5}, .funnelScore = 1.0f},  // far from any road
	};
	const std::vector<WorldObject> markers = BuildHubMarkers(hubs, roads, params);

	ASSERT_EQ(markers.size(), 1u) << "only the hub a road passes through is marked";
	EXPECT_EQ(markers[0].kind, std::string{kHubMarkerKind});
	EXPECT_FALSE(markers[0].blocksMovement) << "a signpost must not block the crossroads";
	ASSERT_EQ(markers[0].footprint.size(), 1u);
	EXPECT_EQ(markers[0].footprint[0], (glm::ivec2{20, 20}));
}

TEST(RoadHubsTest, PromotesHighConvergenceWildernessHubsOnly) {
	WorldGenParams params; // outpostMinConvergence = 3, outpostMinTownDistance = 18, cap = 2
	RoadGraph roads;
	// Three distinct roads converge on (25, 25): a genuine crossroads.
	roads.segments.push_back(StraightSegment(1, 2, {0, 25}, {50, 25}));
	roads.segments.push_back(StraightSegment(3, 4, {25, 0}, {25, 50}));
	roads.segments.push_back(StraightSegment(5, 6, {0, 0}, {50, 50}));
	// One road also brushes (60, 25) but that hub sits next to a living town.
	roads.segments.push_back(StraightSegment(7, 8, {55, 25}, {70, 25}));

	const std::vector<RoadHub> hubs{
		{.location = {25, 25}, .funnelScore = 3.0f}, // crossroads in the wilderness → promote
		{.location = {60, 25}, .funnelScore = 1.0f}, // only 1 road, and a town is adjacent → skip
	};
	const std::vector<TownSite> towns{MakeTown(1, {62, 25})}; // living town next to the second hub

	const std::vector<int> promote = SelectCrossroadsOutposts(hubs, roads, towns, params);
	ASSERT_EQ(promote.size(), 1u);
	EXPECT_EQ(promote[0], 0) << "only the high-convergence wilderness hub is promoted";
}

TEST(RoadHubsTest, PromotionRespectsTheOutpostCap) {
	WorldGenParams params;
	params.maxCrossroadsOutposts = 1;
	RoadGraph roads;
	const auto crossroads = [&](const glm::ivec2 c) {
		roads.segments.push_back(StraightSegment(1, 2, {c.x - 20, c.y}, {c.x + 20, c.y}));
		roads.segments.push_back(StraightSegment(3, 4, {c.x, c.y - 20}, {c.x, c.y + 20}));
		roads.segments.push_back(StraightSegment(5, 6, {c.x - 14, c.y - 14}, {c.x + 14, c.y + 14}));
	};
	crossroads({30, 30});
	crossroads({80, 80});
	const std::vector<RoadHub> hubs{
		{.location = {30, 30}, .funnelScore = 3.0f},
		{.location = {80, 80}, .funnelScore = 2.0f},
	};
	const std::vector<int> promote = SelectCrossroadsOutposts(hubs, roads, {}, params);
	EXPECT_EQ(promote.size(), 1u) << "no more than maxCrossroadsOutposts are promoted";
}
