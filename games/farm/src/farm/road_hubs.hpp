#pragma once

#include <set>
#include <string_view>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "world_data.hpp"

// Road hubs + crossroads-outpost post-pass (ticket 07). Two complementary forces that make the road
// network feel grown from geography and commerce rather than shortest-path:
//
//   * HUBS are a sparse set of geographic funnel points — river fords/confluences, mountain passes,
//     isthmuses — that are central to multiple towns. Each hub, and each living town, contributes a
//     SOFT multiplicative discount to the router's per-tile cost field (a pull, never a forced
//     waypoint), so nearby roads tend to converge. Towns scale their pull by size and
//     road-connectivity, giving the network an "all roads lead to Rome" topology.
//   * The CROSSROADS-OUTPOST post-pass promotes a bounded number of high road-convergence hubs that
//     sit in the wilderness (no living town nearby) into young living outpost towns.
//
// Everything here is PURE C++ (no ECS, no Platform, no file I/O) and deterministic for a given input,
// mirroring road_router.cpp. The cost field returned by BuildRouteCostField is the shared seam the
// least-cost router reads (ticket 08's bridges discount the same field).
namespace game::farm {

// A geographic funnel point where roads tend to converge: a river ford/confluence, a mountain pass,
// or an isthmus that is also central to multiple towns.
struct RoadHub {
	glm::ivec2 location{0, 0};
	float funnelScore{0.0f}; // higher = a stronger funnel more central to the towns

	friend bool operator==(const RoadHub&, const RoadHub&) = default;
};

// Object-kind key for the small marker placed where roads actually meet at a hub (a signpost). Added
// to games/farm/assets/data/object_kinds.json; non-blocking so roads still run through the crossroads.
inline constexpr std::string_view kHubMarkerKind = "signpost";

// Place up to params.hubCount hub nodes at spaced local maxima of a "funnel score" (geographic pinch
// × town centrality). Every returned hub sits on a passable, non-town tile and is at least
// params.hubMinSpacing tiles from every other hub. Deterministic in (grid, towns, params).
[[nodiscard]] std::vector<RoadHub>
PlaceRoadHubs(const Grid& grid, const std::vector<TownSite>& towns, const WorldGenParams& params);

// Build the per-tile multiplicative route-cost discount field the least-cost router multiplies into
// each tile's enter cost. Values lie in (0, 1]: 1.0 leaves a tile unchanged, and a value below 1
// makes the tile cheaper (an attractor). Hubs lower it within params.hubDiscountRadius; living towns
// lower it within params.townAttractorRadius, scaled by size and road-connectivity (degree in
// `connections`) so a large, well-connected town pulls harder than a small isolated one. The field
// is clamped to a strictly positive floor so the discount is always SOFT — never free, never a
// forced waypoint. Deterministic.
[[nodiscard]] std::vector<float> BuildRouteCostField(
	const Grid& grid,
	const std::vector<TownSite>& towns,
	const std::vector<RoadHub>& hubs,
	const std::set<std::pair<int, int>>& connections,
	const WorldGenParams& params
);

// Number of DISTINCT realised road segments that pass within `radius` tiles of `location`.
[[nodiscard]] int HubRoadConvergence(glm::ivec2 location, const RoadGraph& roads, int radius);

// A marker WorldObject (kind == kHubMarkerKind, non-blocking) for each hub that at least one realised
// road route passes through (convergence >= 1). Deterministic, hub order preserved.
[[nodiscard]] std::vector<WorldObject>
BuildHubMarkers(const std::vector<RoadHub>& hubs, const RoadGraph& roads, const WorldGenParams& params);

// Select which hubs to promote into crossroads-outpost towns: a hub qualifies when its road
// convergence is at least params.outpostMinConvergence AND no living town lies within
// params.outpostMinTownDistance tiles. At most params.maxCrossroadsOutposts are returned, strongest
// convergence first (deterministic tiebreak by location). Returns indices into `hubs`. Pure — the
// caller materialises the TownSites so the id/timeline bookkeeping stays in the generation pipeline.
[[nodiscard]] std::vector<int> SelectCrossroadsOutposts(
	const std::vector<RoadHub>& hubs,
	const RoadGraph& roads,
	const std::vector<TownSite>& towns,
	const WorldGenParams& params
);

} // namespace game::farm
