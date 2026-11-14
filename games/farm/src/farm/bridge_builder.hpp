#pragma once

#include <set>
#include <utility>
#include <vector>

#include "world_data.hpp"

namespace game::farm {

// Realise the bridges a world needs. A bridge is the ONLY way a route crosses water, so it is a
// deliberate, rare structure — never placed where a land path already exists.
//
// The land router (ticket 06) blocks deep water (Ocean/Lake), so a wanted connection whose two
// towns sit on different landmasses yields no road. Those genuinely water-separated connections are
// this function's trigger set. For each, it finds the NARROWEST water crossing (fewest water tiles)
// between the two landmasses and lays a bridge deck across it — but only when the span is within
// `WorldGenParams::maxBridgeSpan`. A wider crossing is left unbuilt so the place stays isolated and
// no implausible ocean-spanning bridge is ever placed.
//
// `connections` is the final wanted road set (history-decided, including any prosperity-gated bridge
// investments). `history` is scanned for `HistoryEventType::BridgeBuilt` events so a bridge the
// history simulation deliberately funded is tagged `BridgeReason::HistoricalInvestment` (with that
// event's year); every other bridge is a `BridgeReason::RoutingFallback`.
//
// Pure C++ — no ECS or Platform dependency — and deterministic: identical inputs produce an
// identical, canonically-ordered bridge list.
[[nodiscard]] std::vector<Bridge> BuildBridges(
	const Grid& grid,
	const std::vector<TownSite>& towns,
	const WorldGenParams& params,
	const std::set<std::pair<int, int>>& connections,
	const std::vector<HistoryEvent>& history
);

} // namespace game::farm
