#include "bridge_builder.hpp"

#include <algorithm>
#include <cstddef>
#include <deque>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "world_data.hpp"

namespace game::farm {
namespace {

// A tile is deep water (Ocean or Lake) — the only terrain a bridge ever spans. Rivers are fordable
// land for routing and are never bridged here; mountains are dry barriers, not water.
bool IsDeepWater(const Biome b) { return b == Biome::Ocean || b == Biome::Lake; }

// A tile is passable land for landmass connectivity when its biome is not blocked by the road cost
// model (negative cost = Ocean / Lake / Mountain). Two towns sharing a landmass therefore have a
// genuine land path and are never bridged. Town footprints are intentionally ignored: the player can
// walk through a settlement, so a town never splits a landmass.
bool IsPassableLand(const Grid& grid, const WorldGenParams& params, const int x, const int y) {
	return params.roadBiomeCost[static_cast<std::size_t>(grid.BiomeAt(x, y))] >= 0.0f;
}

constexpr int dx4[4] = {1, -1, 0, 0};
constexpr int dy4[4] = {0, 0, 1, -1};

// 4-connected flood fill of passable land into landmass components. label[idx] is the 1-based
// landmass id (0 = water / mountain barrier). This connectivity matches the router's reachability:
// a diagonal step the router allows always has a shared orthogonal land neighbour, so an orthogonal
// fill produces the same partition.
std::vector<int> LabelLandmasses(const Grid& grid, const WorldGenParams& params) {
	const int w = grid.width;
	const int h = grid.height;
	std::vector<int> label(static_cast<std::size_t>(w) * h, 0);
	int next = 0;
	std::deque<int> queue;
	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			const int start = grid.Index(x, y);
			if (label[static_cast<std::size_t>(start)] != 0 || !IsPassableLand(grid, params, x, y)) {
				continue;
			}
			++next;
			label[static_cast<std::size_t>(start)] = next;
			queue.push_back(start);
			while (!queue.empty()) {
				const int cur = queue.front();
				queue.pop_front();
				const int cx = cur % w;
				const int cy = cur / w;
				for (int d = 0; d < 4; ++d) {
					const int nx = cx + dx4[d];
					const int ny = cy + dy4[d];
					if (!grid.InBounds(nx, ny)) {
						continue;
					}
					const int ni = grid.Index(nx, ny);
					if (label[static_cast<std::size_t>(ni)] != 0 || !IsPassableLand(grid, params, nx, ny)) {
						continue;
					}
					label[static_cast<std::size_t>(ni)] = next;
					queue.push_back(ni);
				}
			}
		}
	}
	return label;
}

// Landmass id the town sits on. Towns rest on land, so this is normally the tile under the town;
// the spiral fallback keeps it robust if a town centre lands on a barrier tile.
int TownLandmass(const Grid& grid, const std::vector<int>& label, const glm::ivec2 loc) {
	if (grid.InBounds(loc.x, loc.y) && label[static_cast<std::size_t>(grid.Index(loc.x, loc.y))] != 0) {
		return label[static_cast<std::size_t>(grid.Index(loc.x, loc.y))];
	}
	for (int r = 1; r < std::max(grid.width, grid.height); ++r) {
		for (int dy = -r; dy <= r; ++dy) {
			for (int dx = -r; dx <= r; ++dx) {
				if (std::max(std::abs(dx), std::abs(dy)) != r) {
					continue;
				}
				const int nx = loc.x + dx;
				const int ny = loc.y + dy;
				if (grid.InBounds(nx, ny) && label[static_cast<std::size_t>(grid.Index(nx, ny))] != 0) {
					return label[static_cast<std::size_t>(grid.Index(nx, ny))];
				}
			}
		}
	}
	return 0;
}

// Whether tile (x,y) is deep water adjacent (4-connected) to a land tile of the given landmass.
bool WaterTouchesLandmass(const Grid& grid, const std::vector<int>& label, const int x, const int y, const int landmass) {
	for (int d = 0; d < 4; ++d) {
		const int nx = x + dx4[d];
		const int ny = y + dy4[d];
		if (grid.InBounds(nx, ny) && label[static_cast<std::size_t>(grid.Index(nx, ny))] == landmass) {
			return true;
		}
	}
	return false;
}

// Narrowest water crossing (fewest deep-water tiles) between landmasses `la` and `lb`, as the deck
// tiles to span. Empty when no all-water path connects the two shores (e.g. separated by mountains,
// or too far). A breadth-first search over deep water from `la`'s shore gives the minimum tile count
// deterministically (sorted sources, fixed neighbour order, first goal reached wins).
std::vector<glm::ivec2> NarrowestCrossing(const Grid& grid, const std::vector<int>& label, const int la, const int lb) {
	const int w = grid.width;
	const int h = grid.height;
	const std::size_t n = static_cast<std::size_t>(w) * h;
	std::vector<int> parent(n, -2); // -2 = unvisited, -1 = source (no parent)
	std::deque<int> queue;

	for (int y = 0; y < h; ++y) {
		for (int x = 0; x < w; ++x) {
			if (!IsDeepWater(grid.BiomeAt(x, y)) || !WaterTouchesLandmass(grid, label, x, y, la)) {
				continue;
			}
			const int idx = grid.Index(x, y);
			parent[static_cast<std::size_t>(idx)] = -1;
			queue.push_back(idx);
		}
	}

	int goal = -1;
	while (!queue.empty() && goal < 0) {
		const int cur = queue.front();
		queue.pop_front();
		const int cx = cur % w;
		const int cy = cur / w;
		if (WaterTouchesLandmass(grid, label, cx, cy, lb)) {
			goal = cur;
			break;
		}
		for (int d = 0; d < 4; ++d) {
			const int nx = cx + dx4[d];
			const int ny = cy + dy4[d];
			if (!grid.InBounds(nx, ny) || !IsDeepWater(grid.BiomeAt(nx, ny))) {
				continue;
			}
			const int ni = grid.Index(nx, ny);
			if (parent[static_cast<std::size_t>(ni)] != -2) {
				continue;
			}
			parent[static_cast<std::size_t>(ni)] = cur;
			queue.push_back(ni);
		}
	}

	std::vector<glm::ivec2> deck;
	if (goal < 0) {
		return deck;
	}
	for (int at = goal; at != -1; at = parent[static_cast<std::size_t>(at)]) {
		deck.push_back({at % w, at / w});
	}
	std::ranges::reverse(deck);
	return deck;
}

std::pair<int, int> Normalized(const int a, const int b) { return {std::min(a, b), std::max(a, b)}; }

// A wanted connection that needs a bridge, resolved to a landmass pair with a chosen reason.
struct Candidate {
	int townA{0};
	int townB{0};
	BridgeReason reason{BridgeReason::RoutingFallback};
	int year{0};
	std::string description;
};

} // namespace

std::vector<Bridge> BuildBridges(
	const Grid& grid,
	const std::vector<TownSite>& towns,
	const WorldGenParams& params,
	const std::set<std::pair<int, int>>& connections,
	const std::vector<HistoryEvent>& history
) {
	std::vector<Bridge> bridges;
	if (connections.empty() || grid.width <= 0 || grid.height <= 0) {
		return bridges;
	}

	const std::vector<int> label = LabelLandmasses(grid, params);

	const auto townLoc = [&](const int id) -> glm::ivec2 {
		return towns[static_cast<std::size_t>(id - 1)].location;
	};
	const auto historicalEvent = [&](const int a, const int b) -> const HistoryEvent* {
		for (const HistoryEvent& ev : history) {
			if (ev.type == HistoryEventType::BridgeBuilt && Normalized(ev.a, ev.b) == Normalized(a, b)) {
				return &ev;
			}
		}
		return nullptr;
	};

	// One bridge per water-separated landmass pair. A historical investment takes precedence over a
	// plain routing fallback so the bridge carries the recorded year/description; otherwise the first
	// (smallest id-pair) connection represents the crossing. `connections` is already sorted.
	std::map<std::pair<int, int>, Candidate> byLandmassPair;
	for (const auto& [a, b] : connections) {
		if (a < 1 || b < 1 || a > static_cast<int>(towns.size()) || b > static_cast<int>(towns.size())) {
			continue;
		}
		const int la = TownLandmass(grid, label, townLoc(a));
		const int lb = TownLandmass(grid, label, townLoc(b));
		if (la <= 0 || lb <= 0 || la == lb) {
			continue; // same landmass => a land path exists => never bridged
		}

		Candidate cand{.townA = std::min(a, b), .townB = std::max(a, b)};
		if (const HistoryEvent* ev = historicalEvent(a, b)) {
			cand.reason = BridgeReason::HistoricalInvestment;
			cand.year = ev->year;
			cand.description = ev->description;
		}

		const std::pair<int, int> key = Normalized(la, lb);
		const auto it = byLandmassPair.find(key);
		if (it == byLandmassPair.end()) {
			byLandmassPair.emplace(key, std::move(cand));
		}
		else if (cand.reason == BridgeReason::HistoricalInvestment
				 && it->second.reason == BridgeReason::RoutingFallback) {
			it->second = std::move(cand); // upgrade a fallback pair to its deliberate historical build
		}
	}

	for (const auto& [key, cand] : byLandmassPair) {
		const std::vector<glm::ivec2> deck = NarrowestCrossing(grid, label, key.first, key.second);
		if (deck.empty() || static_cast<int>(deck.size()) > params.maxBridgeSpan) {
			continue; // no water path, or wider than the cap => left unbuilt, the place stays isolated
		}
		Bridge bridge;
		bridge.townA = cand.townA;
		bridge.townB = cand.townB;
		bridge.deck = deck;
		bridge.reason = cand.reason;
		bridge.year = cand.year;
		bridge.description = cand.description;
		bridges.push_back(std::move(bridge));
	}

	std::ranges::sort(bridges, [](const Bridge& x, const Bridge& y) {
		if (x.townA != y.townA) {
			return x.townA < y.townA;
		}
		return x.townB < y.townB;
	});
	return bridges;
}

} // namespace game::farm
