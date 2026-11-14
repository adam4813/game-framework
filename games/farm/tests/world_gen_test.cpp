#include <gtest/gtest.h>

#include <algorithm>
#include <array>
#include <set>
#include <string>
#include <unordered_map>
#include <vector>

#include "farm/starting_village.hpp"
#include "farm/road_hubs.hpp"
#include "farm/world_data.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;

namespace {

// Seeds exercised by the invariant tests — generation guarantees must hold for every seed.
constexpr std::array<int, 6> kSeeds{1, 7, 42, 1337, 90210, 2024};

int CountLivingTowns(const WorldData& world) {
	return static_cast<int>(std::ranges::count_if(world.towns, [](const TownSite& t) {
		return t.status == TownStatus::Living;
	}));
}

int CountRuinedTowns(const WorldData& world) {
	return static_cast<int>(std::ranges::count_if(world.towns, [](const TownSite& t) {
		return t.status == TownStatus::Ruined;
	}));
}

// Returns true if `townId` has at least one road (used to confirm the start village is connected).
bool TownHasRoad(const WorldData& world, const int townId) {
	return std::ranges::any_of(world.roads.segments, [townId](const RoadSegment& s) {
		return s.townA == townId || s.townB == townId;
	});
}

int CountWaterBodiesOfType(const WorldData& world, const WaterBodyType type) {
	return static_cast<int>(std::ranges::count_if(world.waterBodies, [type](const WaterBody& b) {
		return b.type == type;
	}));
}

// ── Ticket 07: road hubs + crossroads outposts ──────────────────────────────────────

// Params tuned to a mid-scale, sparse world where a wilderness crossroads naturally forms. Kept
// local to the outpost tests so the default-param invariants above are untouched.
WorldGenParams OutpostParams() {
	WorldGenParams p;
	p.width = 300;
	p.height = 300;
	p.noiseScale = 0.013f;
	p.riverCount = 6;
	p.minTownSpacing = 30;
	p.coastalRadius = 8;
	p.explorationRange = 90;
	p.pondMaxTiles = 120;
	p.lakeMaxTiles = 1200;
	p.hubCount = 6;
	p.hubMinSpacing = 55;
	p.hubDiscountRadius = 26;
	p.townAttractorRadius = 40;
	p.outpostConvergenceRadius = 22;
	p.outpostMinTownDistance = 45;
	return p;
}

bool IsCrossroadsOutpostFounding(const HistoryEvent& ev) {
	return ev.type == HistoryEventType::TownFounded && ev.description.rfind("crossroads outpost", 0) == 0;
}

} // namespace

// AC: generated hubs respect minimum spacing (and the hub-count cap) in real generated worlds.
TEST(WorldGenTest, generated_hubs_respect_minimum_spacing) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const std::vector<RoadHub> hubs = PlaceRoadHubs(world.grid, world.towns, world.params);
		EXPECT_LE(static_cast<int>(hubs.size()), world.params.hubCount) << "seed " << seed;
		const long minSq = static_cast<long>(world.params.hubMinSpacing) * world.params.hubMinSpacing;
		for (size_t i = 0; i < hubs.size(); ++i) {
			for (size_t j = i + 1; j < hubs.size(); ++j) {
				const glm::ivec2 d = hubs[i].location - hubs[j].location;
				EXPECT_GE(static_cast<long>(d.x) * d.x + static_cast<long>(d.y) * d.y, minSq)
					<< "hubs too close, seed " << seed;
			}
		}
	}
}

// AC: a hub marker world-object is placed where at least one road passes through a hub, in a typical
// world. Seed 42 is the canonical "typical" world used throughout these tests.
TEST(WorldGenTest, typical_world_places_a_hub_marker_object) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const auto markers = std::ranges::count_if(world.objects, [](const WorldObject& o) {
		return o.kind == std::string{kHubMarkerKind};
	});
	EXPECT_GE(markers, 1) << "a typical world should mark at least one road hub";
	for (const WorldObject& o : world.objects) {
		if (o.kind == std::string{kHubMarkerKind}) {
			EXPECT_FALSE(o.blocksMovement) << "a hub signpost must not block the crossroads";
			EXPECT_EQ(o.footprint.size(), 1u);
		}
	}
}

// AC: a post-pass promotes a high-convergence wilderness hub into a LIVING crossroads-outpost town
// that appears in WorldData.towns AND in the generation timeline.
TEST(WorldGenTest, promotes_a_living_crossroads_outpost_in_towns_and_timeline) {
	const WorldData world = WorldGen::Generate(OutpostParams(), 2);

	// Find the outpost(s) from the recorded history.
	std::vector<int> outpostIds;
	for (const HistoryEvent& ev : world.history) {
		if (IsCrossroadsOutpostFounding(ev)) {
			outpostIds.push_back(ev.a);
		}
	}
	ASSERT_FALSE(outpostIds.empty()) << "seed 2 should promote at least one crossroads outpost";
	EXPECT_LE(static_cast<int>(outpostIds.size()), world.params.maxCrossroadsOutposts) << "outpost count is bounded";

	for (const int id : outpostIds) {
		// Present in the town list as a Living town.
		const TownSite* town = world.FindTown(id);
		ASSERT_NE(town, nullptr) << "outpost id " << id << " missing from towns";
		EXPECT_EQ(town->status, TownStatus::Living) << "a crossroads outpost is a living town";

		// Present in the timeline as a founding step.
		const bool inTimeline = std::ranges::any_of(world.timeline.steps, [id](const WorldSnapshot& s) {
			return s.kind == SnapshotKind::TownFounded
				   && std::ranges::find(s.addedTownIds, id) != s.addedTownIds.end();
		});
		EXPECT_TRUE(inTimeline) << "outpost id " << id << " missing from the timeline";
	}

	// Every town still has exactly one layout (outposts get a generated village like any town).
	EXPECT_EQ(world.townLayouts.size(), world.towns.size());
}

// AC: same seed ⇒ identical hubs + outpost promotions (determinism of the whole ticket-07 pass).
TEST(WorldGenTest, hub_and_outpost_generation_is_deterministic) {
	const WorldGenParams p = OutpostParams();
	const WorldData a = WorldGen::Generate(p, 2);
	const WorldData b = WorldGen::Generate(p, 2);
	EXPECT_TRUE(a == b) << "hubs, markers and outpost promotions must be identical for the same seed";
}

TEST(WorldGenTest, generate_returns_populated_world) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	EXPECT_EQ(world.grid.width, world.params.width);
	EXPECT_EQ(world.grid.height, world.params.height);
	EXPECT_EQ(world.grid.elevation.size(), static_cast<size_t>(world.params.width * world.params.height));
	EXPECT_EQ(world.grid.biome.size(), world.grid.elevation.size());
	EXPECT_FALSE(world.towns.empty());
	EXPECT_FALSE(world.waterBodies.empty());
}

TEST(WorldGenTest, is_deterministic_for_same_seed) {
	const WorldData a = WorldGen::Generate(WorldGenParams{}, 12345);
	const WorldData b = WorldGen::Generate(WorldGenParams{}, 12345);
	EXPECT_TRUE(a == b);
}

TEST(WorldGenTest, different_seeds_produce_different_worlds) {
	const WorldData a = WorldGen::Generate(WorldGenParams{}, 1);
	const WorldData b = WorldGen::Generate(WorldGenParams{}, 2);
	EXPECT_FALSE(a.grid.elevation == b.grid.elevation);
}

TEST(WorldGenTest, elevation_is_natural_not_flat) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const auto [minIt, maxIt] = std::ranges::minmax_element(world.grid.elevation);
	// A natural heightmap spans a wide elevation range rather than a single flat value.
	EXPECT_GT(*maxIt - *minIt, 0.5f);
}

TEST(WorldGenTest, has_multiple_distinct_water_bodies) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		EXPECT_GE(world.waterBodies.size(), 2u) << "seed " << seed;
		EXPECT_GE(CountWaterBodiesOfType(world, WaterBodyType::Ocean), 1) << "ocean missing, seed " << seed;
		const int interior =
			CountWaterBodiesOfType(world, WaterBodyType::Lake) + CountWaterBodiesOfType(world, WaterBodyType::Pond);
		EXPECT_GE(interior, 1) << "interior water missing, seed " << seed;
	}
}

TEST(WorldGenTest, has_at_least_two_river_networks) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		EXPECT_GE(world.rivers.size(), 2u) << "seed " << seed;
		for (const River& river : world.rivers) {
			EXPECT_FALSE(river.path.empty()) << "empty river path, seed " << seed;
		}
	}
}

TEST(WorldGenTest, every_world_has_a_ruin) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		EXPECT_GE(CountRuinedTowns(world), 1) << "no ruin, seed " << seed;
		EXPECT_GE(CountLivingTowns(world), 1) << "no living town, seed " << seed;
	}
}

TEST(WorldGenTest, start_village_is_road_connected) {
	// Disconnected town clusters are allowed; only the start village must have a road so the player
	// has somewhere to travel.
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const std::optional<TownSite> village = SelectStartingVillage(world);
		ASSERT_TRUE(village.has_value()) << "seed " << seed;
		EXPECT_TRUE(TownHasRoad(world, village->id)) << "start village isolated, seed " << seed;
	}
}

TEST(WorldGenTest, timeline_includes_core_stages_and_history) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	ASSERT_FALSE(world.timeline.steps.empty());

	const auto hasKind = [&](const SnapshotKind kind) {
		return std::ranges::any_of(world.timeline.steps, [kind](const WorldSnapshot& s) { return s.kind == kind; });
	};
	EXPECT_TRUE(hasKind(SnapshotKind::Landmass));
	EXPECT_TRUE(hasKind(SnapshotKind::WaterBodies));
	EXPECT_TRUE(hasKind(SnapshotKind::Rivers));
	// At least one history step (founding / road / collapse).
	const bool hasHistory = hasKind(SnapshotKind::TownFounded) || hasKind(SnapshotKind::RoadBuilt) ||
							hasKind(SnapshotKind::TownCollapsed);
	EXPECT_TRUE(hasHistory);

	for (const WorldSnapshot& step : world.timeline.steps) {
		EXPECT_FALSE(step.label.empty());
	}
}

TEST(WorldGenTest, history_interleaves_foundings_roads_and_collapses) {
	// The organic simulation should weave roads/collapses in among foundings rather than running all
	// foundings first. For each seed, check whether a road/collapse step appears before the last
	// founding step in the timeline; require it for the clear majority of seeds.
	int interleaved = 0;
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		int lastFounded = -1;
		int firstRoadOrCollapse = -1;
		for (int i = 0; i < static_cast<int>(world.timeline.steps.size()); ++i) {
			const SnapshotKind kind = world.timeline.steps[static_cast<size_t>(i)].kind;
			if (kind == SnapshotKind::TownFounded) {
				lastFounded = i;
			}
			if (firstRoadOrCollapse < 0 &&
				(kind == SnapshotKind::RoadBuilt || kind == SnapshotKind::TownCollapsed ||
				 kind == SnapshotKind::RoadRemoved)) {
				firstRoadOrCollapse = i;
			}
		}
		if (firstRoadOrCollapse >= 0 && lastFounded >= 0 && firstRoadOrCollapse < lastFounded) {
			++interleaved;
		}
	}
	EXPECT_GE(interleaved, static_cast<int>(kSeeds.size()) / 2) << "history is not interleaving over time";
}

TEST(WorldGenTest, select_starting_village_meets_all_criteria) {
	for (const int seed : kSeeds) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const std::optional<TownSite> village = SelectStartingVillage(world);
		ASSERT_TRUE(village.has_value()) << "no starting village, seed " << seed;

		// Living.
		EXPECT_EQ(village->status, TownStatus::Living) << "seed " << seed;

		// Not mountain terrain.
		EXPECT_NE(world.grid.BiomeAt(village->location.x, village->location.y), Biome::Mountain) << "seed " << seed;

		// Has at least one road connection.
		const bool hasRoad = std::ranges::any_of(world.roads.segments, [&](const RoadSegment& s) {
			return s.townA == village->id || s.townB == village->id;
		});
		EXPECT_TRUE(hasRoad) << "no road, seed " << seed;

		// Has at least one ruin within exploration range.
		const bool hasNearRuin = std::ranges::any_of(world.towns, [&](const TownSite& t) {
			if (t.status != TownStatus::Ruined) {
				return false;
			}
			const glm::ivec2 d = t.location - village->location;
			return (d.x * d.x + d.y * d.y) <= world.params.explorationRange * world.params.explorationRange;
		});
		EXPECT_TRUE(hasNearRuin) << "no nearby ruin, seed " << seed;
	}
}
