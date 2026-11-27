#include "road_router.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <functional>
#include <limits>
#include <queue>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "world_data.hpp"

namespace game::farm {
namespace {

constexpr float kDiagonalCost = 1.41421356f; // sqrt(2): a diagonal step covers more ground than an orthogonal one

const TownSite& TownById(const std::vector<TownSite>& towns, const int id) {
	return towns[static_cast<std::size_t>(id - 1)];
}

float DirectDistance(const glm::ivec2 a, const glm::ivec2 b) {
	const glm::vec2 d = glm::vec2(a - b);
	return std::sqrt(d.x * d.x + d.y * d.y);
}

// ── Static cost field ────────────────────────────────────────────────────────────────────────
// What the biome layer and the town footprints make of each tile, independent of which connection
// is being routed. Rebuilt once per BuildRoadNetwork call.
struct CostField {
	int width{0};
	int height{0};
	std::vector<char> wallBlocked;  // building walls + landmarks: impassable for every road
	std::vector<int> floorOwner;    // town id whose walkable floor/field a tile belongs to, else 0
	std::vector<float> biomeCost;   // per-tile biome weight; < 0 => blocked (mountain / deep water)

	[[nodiscard]] bool InBounds(const int x, const int y) const { return x >= 0 && y >= 0 && x < width && y < height; }
	[[nodiscard]] int Index(const int x, const int y) const { return y * width + x; }
};

CostField BuildCostField(const Grid& grid, const std::vector<TownLayout>& layouts, const WorldGenParams& params) {
	CostField field;
	field.width = grid.width;
	field.height = grid.height;
	const std::size_t n = static_cast<std::size_t>(grid.width) * grid.height;
	field.wallBlocked.assign(n, 0);
	field.floorOwner.assign(n, 0);
	field.biomeCost.assign(n, 0.0f);

	for (int y = 0; y < grid.height; ++y) {
		for (int x = 0; x < grid.width; ++x) {
			field.biomeCost[field.Index(x, y)] = params.roadBiomeCost[static_cast<std::size_t>(grid.BiomeAt(x, y))];
		}
	}

	// Floors first (so they claim ownership), then walls/landmarks (which override as hard blocks).
	for (const TownLayout& layout : layouts) {
		const auto claimFloor = [&](const glm::ivec2 t) {
			if (field.InBounds(t.x, t.y) && field.floorOwner[field.Index(t.x, t.y)] == 0) {
				field.floorOwner[field.Index(t.x, t.y)] = layout.townId;
			}
		};
		for (const glm::ivec2 t : layout.floorTiles) {
			claimFloor(t);
		}
		for (const glm::ivec2 t : layout.fieldTiles) {
			claimFloor(t);
		}
	}
	for (const TownLayout& layout : layouts) {
		const auto block = [&](const glm::ivec2 t) {
			if (field.InBounds(t.x, t.y)) {
				field.wallBlocked[field.Index(t.x, t.y)] = 1;
			}
		};
		block(layout.landmark);
		for (const TownBuilding& bld : layout.buildings) {
			for (const glm::ivec2 t : bld.footprint) {
				block(t);
			}
		}
	}
	return field;
}

// A tile is impassable for a route whose endpoint towns are endA/endB when it is a building wall, a
// blocked biome (mountain / deep water), or the walkable footprint of a THIRD town (so a road never
// cuts through a settlement that is not one of its own endpoints).
bool TileBlocked(const CostField& f, const int idx, const int endA, const int endB) {
	if (f.wallBlocked[idx] != 0) {
		return true;
	}
	if (f.biomeCost[idx] < 0.0f) {
		return true;
	}
	const int owner = f.floorOwner[idx];
	return owner != 0 && owner != endA && owner != endB;
}

// Cost of entering a tile: its biome weight, discounted where a road already runs (so new routes
// merge onto trunks) and where an optional attractor field pulls (hubs / town attractors / bridges),
// plus a town-footprint avoidance surcharge on the endpoint towns' own floor so the road hugs the
// edge instead of ploughing through the interior.
float TileEnterCost(
	const CostField& f,
	const std::vector<char>& laid,
	const std::vector<float>& costDiscount,
	const int idx,
	const int endA,
	const int endB,
	const WorldGenParams& params
) {
	float cost = f.biomeCost[idx];
	if (!costDiscount.empty()) {
		cost *= costDiscount[static_cast<std::size_t>(idx)]; // soft attractor pull (ticket 07 hubs/towns, ticket 08 bridges)
	}
	if (laid[idx] != 0) {
		cost *= params.roadExistingDiscount;
	}
	const int owner = f.floorOwner[idx];
	if (owner == endA || owner == endB) {
		cost += params.roadTownAvoidCost;
	}
	return cost;
}

glm::ivec2 NearestOpenTile(const CostField& f, const glm::ivec2 from) {
	const auto open = [&](const glm::ivec2 t) {
		return f.InBounds(t.x, t.y) && f.wallBlocked[f.Index(t.x, t.y)] == 0 && f.biomeCost[f.Index(t.x, t.y)] >= 0.0f;
	};
	if (open(from)) {
		return from;
	}
	for (int r = 1; r < std::max(f.width, f.height); ++r) {
		for (int dy = -r; dy <= r; ++dy) {
			for (int dx = -r; dx <= r; ++dx) {
				if (std::max(std::abs(dx), std::abs(dy)) != r) {
					continue;
				}
				const glm::ivec2 t{from.x + dx, from.y + dy};
				if (open(t)) {
					return t;
				}
			}
		}
	}
	return from;
}

// Pick a town's walkable approach tile closest to `toward` — an edge / door tile on the side facing
// the neighbour — so the road meets the town at its perimeter, not its centre.
glm::ivec2 PickEndpoint(const CostField& f, const TownLayout* layout, const TownSite& town, const glm::ivec2 toward) {
	bool have = false;
	glm::ivec2 best{0, 0};
	long bestDist = 0;
	const auto consider = [&](const glm::ivec2 t) {
		if (!f.InBounds(t.x, t.y)) {
			return;
		}
		const int idx = f.Index(t.x, t.y);
		if (f.wallBlocked[idx] != 0 || f.biomeCost[idx] < 0.0f) {
			return;
		}
		const long dx = t.x - toward.x;
		const long dy = t.y - toward.y;
		const long d = dx * dx + dy * dy;
		const bool better =
			!have || d < bestDist || (d == bestDist && (t.y < best.y || (t.y == best.y && t.x < best.x)));
		if (better) {
			have = true;
			bestDist = d;
			best = t;
		}
	};
	if (layout != nullptr) {
		for (const glm::ivec2 t : layout->floorTiles) {
			consider(t);
		}
	}
	if (have) {
		return best;
	}
	return NearestOpenTile(f, town.location);
}

struct RouteResult {
	bool found{false};
	std::vector<glm::ivec2> path;
	float cost{0.0f};
};

// Least-cost 8-connected A* over the cost field. Deterministic: a fixed neighbour order, strict
// relaxation and an index tiebreak in the open set mean identical inputs reconstruct an identical
// path. Diagonal steps may not cut between two blocked tiles, so a road never slips through a wall
// corner.
RouteResult RouteAStar(
	const CostField& f,
	const std::vector<char>& laid,
	const std::vector<float>& costDiscount,
	const float minDiscount,
	const WorldGenParams& params,
	const glm::ivec2 start,
	const glm::ivec2 goal,
	const int endA,
	const int endB
) {
	RouteResult res;
	const int w = f.width;
	const int n = w * f.height;
	const int startIdx = f.Index(start.x, start.y);
	const int goalIdx = f.Index(goal.x, goal.y);

	// Cheapest achievable per-step cost, for an admissible octile heuristic. The attractor field can
	// only lower costs, so fold its minimum factor in to keep the heuristic from overestimating.
	float minStep = std::numeric_limits<float>::max();
	for (const float c : params.roadBiomeCost) {
		if (c > 0.0f) {
			minStep = std::min(minStep, c);
		}
	}
	if (minStep == std::numeric_limits<float>::max()) {
		minStep = 1.0f;
	}
	minStep *= std::min(1.0f, params.roadExistingDiscount);
	minStep *= std::min(1.0f, minDiscount);

	const auto octile = [&](const int idx) {
		const int dx = std::abs((idx % w) - goal.x);
		const int dy = std::abs((idx / w) - goal.y);
		const int lo = std::min(dx, dy);
		const int hi = std::max(dx, dy);
		return minStep * (static_cast<float>(hi - lo) + kDiagonalCost * static_cast<float>(lo));
	};

	std::vector<float> g(static_cast<std::size_t>(n), std::numeric_limits<float>::infinity());
	std::vector<int> came(static_cast<std::size_t>(n), -1);
	std::vector<char> closed(static_cast<std::size_t>(n), 0);

	struct Node {
		float f;
		int idx;
	};
	const auto cmp = [](const Node& a, const Node& b) {
		if (a.f != b.f) {
			return a.f > b.f; // min-heap on f
		}
		return a.idx > b.idx; // deterministic tiebreak
	};
	std::priority_queue<Node, std::vector<Node>, decltype(cmp)> open(cmp);

	g[static_cast<std::size_t>(startIdx)] = 0.0f;
	open.push({octile(startIdx), startIdx});

	static constexpr int dirs[8][2] = {{-1, -1}, {0, -1}, {1, -1}, {-1, 0}, {1, 0}, {-1, 1}, {0, 1}, {1, 1}};

	bool reached = false;
	while (!open.empty()) {
		const Node cur = open.top();
		open.pop();
		if (closed[static_cast<std::size_t>(cur.idx)] != 0) {
			continue;
		}
		closed[static_cast<std::size_t>(cur.idx)] = 1;
		if (cur.idx == goalIdx) {
			reached = true;
			break;
		}
		const int cx = cur.idx % w;
		const int cy = cur.idx / w;
		for (const auto& d : dirs) {
			const int nx = cx + d[0];
			const int ny = cy + d[1];
			if (!f.InBounds(nx, ny)) {
				continue;
			}
			const int nIdx = f.Index(nx, ny);
			if (closed[static_cast<std::size_t>(nIdx)] != 0 || TileBlocked(f, nIdx, endA, endB)) {
				continue;
			}
			const bool diagonal = d[0] != 0 && d[1] != 0;
			if (diagonal
				&& (TileBlocked(f, f.Index(cx + d[0], cy), endA, endB)
					|| TileBlocked(f, f.Index(cx, cy + d[1]), endA, endB))) {
				continue; // no corner-cutting through a wall
			}
			float step = TileEnterCost(f, laid, costDiscount, nIdx, endA, endB, params);
			if (diagonal) {
				step *= kDiagonalCost;
			}
			const float tentative = g[static_cast<std::size_t>(cur.idx)] + step;
			if (tentative < g[static_cast<std::size_t>(nIdx)]) {
				g[static_cast<std::size_t>(nIdx)] = tentative;
				came[static_cast<std::size_t>(nIdx)] = cur.idx;
				open.push({tentative + octile(nIdx), nIdx});
			}
		}
	}

	if (!reached) {
		return res;
	}
	res.found = true;
	res.cost = g[static_cast<std::size_t>(goalIdx)];
	for (int at = goalIdx; at != -1; at = came[static_cast<std::size_t>(at)]) {
		res.path.push_back({at % w, at / w});
		if (at == startIdx) {
			break;
		}
	}
	std::ranges::reverse(res.path);
	return res;
}

// Shortest realised-network tile distance between two towns (Dijkstra over laid segments), used by
// the redundancy filter. Returns +inf when they are not yet connected.
float NetworkDistance(
	const std::unordered_map<int, std::vector<std::pair<int, float>>>& adj,
	const int from,
	const int to
) {
	std::unordered_map<int, float> dist;
	using QN = std::pair<float, int>;
	std::priority_queue<QN, std::vector<QN>, std::greater<QN>> pq;
	dist[from] = 0.0f;
	pq.push({0.0f, from});
	while (!pq.empty()) {
		const auto [d, u] = pq.top();
		pq.pop();
		if (u == to) {
			return d;
		}
		if (d > dist[u]) {
			continue;
		}
		const auto it = adj.find(u);
		if (it == adj.end()) {
			continue;
		}
		for (const auto& [v, len] : it->second) {
			const float nd = d + len;
			const auto vi = dist.find(v);
			if (vi == dist.end() || nd < vi->second) {
				dist[v] = nd;
				pq.push({nd, v});
			}
		}
	}
	const auto it = dist.find(to);
	return it == dist.end() ? std::numeric_limits<float>::infinity() : it->second;
}

} // namespace

RoadGraph BuildRoadNetwork(
	const Grid& grid,
	const std::vector<TownSite>& towns,
	const std::vector<TownLayout>& townLayouts,
	const WorldGenParams& params,
	const std::set<std::pair<int, int>>& connections,
	const std::vector<float>& costDiscount
) {
	RoadGraph graph;
	if (connections.empty() || grid.width <= 0 || grid.height <= 0) {
		return graph;
	}

	const CostField field = [&] {
		engine::profiling::Zone _sub{"WorldGen/RoadCostField"};
		return BuildCostField(grid, townLayouts, params);
	}();

	// An optional attractor field only lowers costs; its minimum factor keeps the A* heuristic
	// admissible. A field of the wrong size is ignored (treated as "no discount").
	const bool haveDiscount =
		costDiscount.size() == static_cast<std::size_t>(grid.width) * static_cast<std::size_t>(grid.height);
	const std::vector<float> emptyDiscount;
	const std::vector<float>& discount = haveDiscount ? costDiscount : emptyDiscount;
	float minDiscount = 1.0f;
	if (haveDiscount) {
		for (const float d : discount) {
			minDiscount = std::min(minDiscount, d);
		}
	}

	std::unordered_map<int, const TownLayout*> layoutById;
	for (const TownLayout& l : townLayouts) {
		layoutById[l.townId] = &l;
	}

	// Realisation order: shorter, local roads first so later trunks merge onto them (existing-road
	// discount) and so a genuinely redundant express is recognised against an already-laid chain.
	// Ties resolve by larger combined population then by id pair — fully deterministic.
	struct Wanted {
		int a{0};
		int b{0};
		float direct{0.0f};
		int sizeSum{0};
	};
	std::vector<Wanted> wanted;
	wanted.reserve(connections.size());
	for (const auto& [a, b] : connections) {
		const TownSite& ta = TownById(towns, a);
		const TownSite& tb = TownById(towns, b);
		wanted.push_back({a, b, DirectDistance(ta.location, tb.location), ta.size + tb.size});
	}
	std::ranges::sort(wanted, [](const Wanted& x, const Wanted& y) {
		if (x.direct != y.direct) {
			return x.direct < y.direct;
		}
		if (x.sizeSum != y.sizeSum) {
			return x.sizeSum > y.sizeSum;
		}
		if (x.a != y.a) {
			return x.a < y.a;
		}
		return x.b < y.b;
	});

	std::vector<char> laid(static_cast<std::size_t>(grid.width) * grid.height, 0);
	std::unordered_map<int, std::vector<std::pair<int, float>>> adj;

	for (const Wanted& wc : wanted) {
		// Redundancy filter: skip when the realised network already links the two towns within the
		// detour factor of their direct distance.
		if (NetworkDistance(adj, wc.a, wc.b) <= params.roadRedundancyDetour * wc.direct) {
			continue;
		}

		const TownSite& ta = TownById(towns, wc.a);
		const TownSite& tb = TownById(towns, wc.b);
		const auto la = layoutById.find(wc.a);
		const auto lb = layoutById.find(wc.b);
		const glm::ivec2 start = PickEndpoint(field, la == layoutById.end() ? nullptr : la->second, ta, tb.location);
		const glm::ivec2 goal = PickEndpoint(field, lb == layoutById.end() ? nullptr : lb->second, tb, ta.location);

		const RouteResult route = [&] {
			engine::profiling::Zone _sub{"WorldGen/RouteAStar"};
			return RouteAStar(field, laid, discount, minDiscount, params, start, goal, wc.a, wc.b);
		}();
		if (!route.found) {
			continue; // no passable land path — a bridge is ticket 08's job
		}

		for (const glm::ivec2 p : route.path) {
			laid[static_cast<std::size_t>(field.Index(p.x, p.y))] = 1;
		}

		RoadSegment seg;
		seg.townA = std::min(wc.a, wc.b);
		seg.townB = std::max(wc.a, wc.b);
		seg.weight = route.cost;
		seg.path = route.path;
		graph.segments.push_back(std::move(seg));

		const float len = static_cast<float>(route.path.size());
		adj[wc.a].push_back({wc.b, len});
		adj[wc.b].push_back({wc.a, len});
	}

	// Canonical, deterministic ordering regardless of realisation order.
	std::ranges::sort(graph.segments, [](const RoadSegment& x, const RoadSegment& y) {
		if (x.townA != y.townA) {
			return x.townA < y.townA;
		}
		return x.townB < y.townB;
	});
	return graph;
}

} // namespace game::farm
