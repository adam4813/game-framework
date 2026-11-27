#pragma once

#include <set>
#include <utility>
#include <vector>

#include "world_data.hpp"

namespace game::farm {

// Realise a set of wanted (townA, townB) connections into drawable RoadSegments using a least-cost
// (A*) tile router. The cost model lives entirely in WorldGenParams, so it is data-configurable and
// deterministic for a given (grid, towns, townLayouts, params, connections):
//
//   * biome movement costs (WorldGenParams::roadBiomeCost); a negative weight blocks a tile, so
//     mountains and deep water (Ocean/Lake) are impassable — a connection separated only by blocked
//     terrain yields no segment (a bridge is ticket 08's job);
//   * an existing-road discount (roadExistingDiscount) so later routes merge onto trunks already
//     laid instead of running as parallels;
//   * a town-footprint avoidance cost (roadTownAvoidCost): building walls / landmarks are blocked and
//     a town's walkable floor/field tiles cost extra, so roads approach from the edge near a door and
//     never cut through the interior (and never bisect a third town sitting between two others);
//   * a redundancy filter (roadRedundancyDetour): a wanted connection is skipped when the already-
//     realised network links the two towns within the detour factor of their direct tile distance.
//
// `connections` is the active road set from the history simulation (normalised, min-first town-id
// pairs). The router lays shorter, local roads first so later trunks merge onto them and so a
// genuinely redundant express can be recognised and dropped. Pure C++ — no ECS or Platform
// dependency — and deterministic: identical inputs produce an identical RoadGraph.
//
// `costDiscount` is an OPTIONAL per-tile multiplicative discount field (values in (0, 1]; empty =
// no change) that is multiplied into each tile's enter cost before the router minimises it. It is the
// shared seam ticket 07 uses for hub / town attractors (BuildRouteCostField) and ticket 08 for
// bridge crossings — a SOFT pull that bends routes without ever forcing a waypoint.
RoadGraph BuildRoadNetwork(
	const Grid& grid,
	const std::vector<TownSite>& towns,
	const std::vector<TownLayout>& townLayouts,
	const WorldGenParams& params,
	const std::set<std::pair<int, int>>& connections,
	const std::vector<float>& costDiscount = {}
);

} // namespace game::farm
