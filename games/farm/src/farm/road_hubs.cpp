#include "road_hubs.hpp"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "world_data.hpp"

namespace game::farm {
namespace {

// ── Funnel-score tuning (module-local; the spec-mandated knobs live in WorldGenParams) ──
// A tile counts as "between" two towns within this detour fraction of their direct line.
constexpr float kCorridorSlack = 0.30f;
// Baseline geographic weight so a central crossroads still scores without a hard pinch.
constexpr float kGeoBase = 0.35f;
// Boost for a tile squeezed between barriers on opposing sides (a pass / isthmus).
constexpr float kGeoPinchWeight = 1.25f;
// Boost for a river tile (a ford); confluences add more via their river neighbours.
constexpr float kRiverFordBonus = 0.60f;
// Tiles at/below this funnel score are not hub candidates.
constexpr float kScoreEpsilon = 1e-6f;

// ── Cost-field tuning ──
// The discounted cost floor: a tile never costs less than this fraction (soft, never free).
constexpr float kDiscountFloor = 0.10f;
// Absolute population a town's size is normalised against for its attractor strength.
constexpr float kReferencePopulation = 120.0f;

// ── Hub marker ──
// Visual height of the signpost marker object.
constexpr float kHubMarkerHeight = 2.0f;

[[nodiscard]] bool IsBlocked(const Grid& grid, const WorldGenParams& params, const int x, const int y) {
	return params.roadBiomeCost[static_cast<std::size_t>(grid.BiomeAt(x, y))] < 0.0f;
}

[[nodiscard]] float Distance(const glm::ivec2 a, const glm::ivec2 b) {
	const glm::vec2 d = glm::vec2(a - b);
	return std::sqrt(d.x * d.x + d.y * d.y);
}

[[nodiscard]] long DistanceSquared(const glm::ivec2 a, const glm::ivec2 b) {
	const long dx = a.x - b.x;
	const long dy = a.y - b.y;
	return dx * dx + dy * dy;
}

// Proximity in [0, 1] of the nearest barrier (impassable biome) when walking out from (x, y) in
// direction (dx, dy) up to `radius` steps: 1 when a barrier is immediately adjacent, fading to 0 at
// the probe edge, and 0 when no barrier is found. Walking off the map counts as a barrier (the sea).
[[nodiscard]] float BarrierProximity(
	const Grid& grid,
	const WorldGenParams& params,
	const int x,
	const int y,
	const int dx,
	const int dy,
	const int radius
) {
	for (int s = 1; s <= radius; ++s) {
		const int nx = x + dx * s;
		const int ny = y + dy * s;
		if (!grid.InBounds(nx, ny) || IsBlocked(grid, params, nx, ny)) {
			return static_cast<float>(radius - s + 1) / static_cast<float>(radius);
		}
	}
	return 0.0f;
}

// How much a tile reads as a geographic funnel: a pinch between barriers on opposing sides (a
// mountain pass or an isthmus between two waters) plus a bonus for river tiles (fords / confluences).
[[nodiscard]] float
GeoFunnel(const Grid& grid, const WorldGenParams& params, const int x, const int y, const int radius) {
	static constexpr int kAxes[4][2] = {{1, 0}, {0, 1}, {1, 1}, {1, -1}};
	float pinch = 0.0f;
	for (const auto& ax : kAxes) {
		const float a = BarrierProximity(grid, params, x, y, ax[0], ax[1], radius);
		const float b = BarrierProximity(grid, params, x, y, -ax[0], -ax[1], radius);
		pinch = std::max(pinch, a * b); // both ends must be squeezed for a true pass / isthmus
	}

	float geo = kGeoBase + kGeoPinchWeight * pinch;

	if (grid.BiomeAt(x, y) == Biome::River) {
		int riverNeighbours = 0;
		for (int ny = y - 1; ny <= y + 1; ++ny) {
			for (int nx = x - 1; nx <= x + 1; ++nx) {
				if ((nx != x || ny != y) && grid.InBounds(nx, ny) && grid.BiomeAt(nx, ny) == Biome::River) {
					++riverNeighbours;
				}
			}
		}
		geo += kRiverFordBonus
			   * (1.0f + 0.25f * static_cast<float>(riverNeighbours)); // confluences (>2 arms) score higher
	}
	return geo;
}

} // namespace

std::vector<RoadHub> PlaceRoadHubs(const Grid& grid, const std::vector<TownSite>& towns, const WorldGenParams& params) {
	std::vector<RoadHub> hubs;
	if (grid.width <= 0 || grid.height <= 0 || params.hubCount <= 0) {
		return hubs;
	}

	std::vector<glm::ivec2> anchors;
	for (const TownSite& t : towns) {
		if (t.status == TownStatus::Living) {
			anchors.push_back(t.location);
		}
	}
	if (anchors.size() < 2) {
		return hubs; // "central to multiple towns" needs at least two living towns
	}

	// Precompute the living-town pairs with direct distances, corridor thresholds,
	// and an integer AABB bounding the corridor ellipse for fast per-tile rejection.
	struct Pair {
		glm::ivec2 a;
		glm::ivec2 b;
		float direct;
		float slack;       // direct × kCorridorSlack
		float threshold;   // direct × (1 + kCorridorSlack)
		long long thresholdSq;  // threshold² (long long for sqrt-free ellipse test)
		int x0, x1, y0, y1; // AABB of the corridor ellipse, clamped to grid
	};
	std::vector<Pair> pairs;
	pairs.reserve(anchors.size() * (anchors.size() - 1) / 2);
	for (std::size_t i = 0; i < anchors.size(); ++i) {
		for (std::size_t j = i + 1; j < anchors.size(); ++j) {
			Pair p;
			p.a = anchors[i];
			p.b = anchors[j];
			p.direct = std::max(1.0f, Distance(p.a, p.b));
			p.slack = p.direct * kCorridorSlack;
			p.threshold = p.direct + p.slack;
			p.thresholdSq = static_cast<long long>(p.threshold * p.threshold) + 1LL;
			// Conservative AABB: square of side threshold centred on the pair midpoint.
			// Every point inside the corridor ellipse is guaranteed to lie within this box.
			const int cx = (p.a.x + p.b.x) / 2;
			const int cy = (p.a.y + p.b.y) / 2;
			const int half = static_cast<int>(p.threshold * 0.5f) + 1;
			p.x0 = std::max(0, cx - half);
			p.x1 = std::min(grid.width - 1, cx + half);
			p.y0 = std::max(0, cy - half);
			p.y1 = std::min(grid.height - 1, cy + half);
			pairs.push_back(p);
		}
	}

	const int w = grid.width;
	const int h = grid.height;

	struct Candidate {
		float score;
		int x;
		int y;
	};
	struct ScoredCandidate {
		float centrality;
		int x;
		int y;
	};
	std::vector<ScoredCandidate> central; // tiles with positive centrality (pre-GeoFunnel)
	std::vector<Candidate> candidates;

	{
		engine::profiling::Zone _sub{"WorldGen/PlaceRoadHubs/CentralityScan"};
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				if (IsBlocked(grid, params, x, y)) {
					continue;
				}

				// Centrality: sum over town pairs whose direct corridor passes near this tile.
				// Sqrt-free ellipse test: di+dj ≤ T ⟺ 4·di²·dj² ≤ (T²−di²−dj²)²
				float centrality = 0.0f;
				for (const Pair& p : pairs) {
					if (x < p.x0 || x > p.x1 || y < p.y0 || y > p.y1) {
						continue;
					}
					const long long dxi = x - p.a.x;
					const long long dyi = y - p.a.y;
					const long long di_sq = dxi * dxi + dyi * dyi;
					const long long dxj = x - p.b.x;
					const long long dyj = y - p.b.y;
					const long long dj_sq = dxj * dxj + dyj * dyj;
					const long long S = di_sq + dj_sq;
					const long long Tdiff = p.thresholdSq - S;
					if (Tdiff < 0LL) {
						continue;
					}
					if (4LL * di_sq * dj_sq > Tdiff * Tdiff) {
						continue;
					}
					const float di = std::sqrt(static_cast<float>(di_sq));
					const float dj = std::sqrt(static_cast<float>(dj_sq));
					const float detour = di + dj - p.direct;
					if (detour > p.slack) {
						continue;
					}
					const float corridor = 1.0f - detour / p.slack;
					const float mid = 1.0f - std::fabs(di - dj) / p.direct;
					centrality += corridor * std::max(0.0f, mid);
				}
				if (centrality > 0.0f) {
					central.push_back({centrality, x, y});
				}
			}
		}
	}

	{
		engine::profiling::Zone _sub{"WorldGen/PlaceRoadHubs/GeoFunnelPass"};
		// Probe radius for geographic feature detection (pass/isthmus/ford).
		// Local terrain features are 1-3 tiles wide; probeR=5 captures any realistic funnel
		// with half the lookups of probeR=10.
		constexpr int kGeoProbeR = 5;
		for (const ScoredCandidate& sc : central) {
			const float score = sc.centrality * GeoFunnel(grid, params, sc.x, sc.y, kGeoProbeR);
			if (score > kScoreEpsilon) {
				candidates.push_back({score, sc.x, sc.y});
			}
		}
	}

	// Strongest first; deterministic tiebreak by (y, x) so equal scores resolve identically.
	// partial_sort top K×100 — the spacing filter needs at most a few hundred sorted candidates;
	// sorting all 600K+ entries (O(n log n)) would dominate. O(n log K) is much cheaper.
	{
		engine::profiling::Zone _sub{"WorldGen/PlaceRoadHubs/SortAndSelect"};
		const auto cmp = [](const Candidate& a, const Candidate& b) {
			if (a.score != b.score) {
				return a.score > b.score;
			}
			if (a.y != b.y) {
				return a.y < b.y;
			}
			return a.x < b.x;
		};
		// hubCount*100 gives the spacing filter ample room to skip rejected candidates while
		// remaining far less than the full list size.
		const auto topK = std::min(
			candidates.size(),
			static_cast<std::size_t>(std::max(1, params.hubCount)) * 100
		);
		std::partial_sort(
			candidates.begin(),
			candidates.begin() + static_cast<std::ptrdiff_t>(topK),
			candidates.end(),
			cmp
		);

		const long minSpacingSq = static_cast<long>(params.hubMinSpacing) * params.hubMinSpacing;
		for (std::size_t i = 0; i < topK; ++i) {
			if (static_cast<int>(hubs.size()) >= params.hubCount) {
				break;
			}
			const Candidate& c = candidates[i];
			const glm::ivec2 loc{c.x, c.y};
			const bool spaced = std::ranges::all_of(hubs, [&](const RoadHub& existing) {
				return DistanceSquared(existing.location, loc) >= minSpacingSq;
			});
			if (spaced) {
				hubs.push_back({.location = loc, .funnelScore = c.score});
			}
		}
	}
	return hubs;
}

std::vector<float> BuildRouteCostField(
	const Grid& grid,
	const std::vector<TownSite>& towns,
	const std::vector<RoadHub>& hubs,
	const std::set<std::pair<int, int>>& connections,
	const WorldGenParams& params
) {
	const int w = grid.width;
	const int h = grid.height;
	std::vector<float> field(static_cast<std::size_t>(std::max(0, w)) * std::max(0, h), 1.0f);
	if (w <= 0 || h <= 0) {
		return field;
	}

	// A radial soft discount: cheapest (minFactor) at the centre, easing back to 1.0 at the edge, then
	// multiplied into the field so overlapping attractors compound. minFactor stays above the floor.
	const auto applyDiscount = [&](const glm::ivec2 centre, const int radius, const float minFactorIn) {
		if (radius <= 0) {
			return;
		}
		const float minFactor = std::clamp(minFactorIn, kDiscountFloor, 1.0f);
		const float r = static_cast<float>(radius);
		for (int dy = -radius; dy <= radius; ++dy) {
			for (int dx = -radius; dx <= radius; ++dx) {
				const int x = centre.x + dx;
				const int y = centre.y + dy;
				if (!grid.InBounds(x, y)) {
					continue;
				}
				const float d = std::sqrt(static_cast<float>(dx * dx + dy * dy));
				if (d > r) {
					continue;
				}
				const float factor = minFactor + (1.0f - minFactor) * (d / r);
				field[static_cast<std::size_t>(grid.Index(x, y))] *= factor;
			}
		}
	};

	{
		engine::profiling::Zone _sub{"WorldGen/BuildRouteCostField/HubDiscounts"};
		for (const RoadHub& hub : hubs) {
			applyDiscount(hub.location, params.hubDiscountRadius, params.hubRouteDiscount);
		}
	}

	// Road-connectivity (degree) per town from the active connection set.
	{
		engine::profiling::Zone _sub{"WorldGen/BuildRouteCostField/TownAttractors"};
		std::unordered_map<int, int> degree;
		for (const auto& [a, b] : connections) {
			++degree[a];
			++degree[b];
		}

		for (const TownSite& town : towns) {
			if (town.status != TownStatus::Living) {
				continue;
			}
			const float sizeNorm = std::clamp(static_cast<float>(town.size) / kReferencePopulation, 0.0f, 1.0f);
			const int deg = degree.contains(town.id) ? degree.at(town.id) : 0;
			// Strength grows with BOTH size and connectivity so a large, well-connected town pulls harder
			// than a small isolated one ("all roads lead to Rome"). The minFactor clamp keeps it soft.
			const float strength = sizeNorm + params.townAttractorConnectivity * static_cast<float>(deg);
			const float minFactor = 1.0f - params.townAttractorDiscount * strength;
			applyDiscount(town.location, params.townAttractorRadius, minFactor);
		}
	}

	for (float& f : field) {
		f = std::max(f, kDiscountFloor);
	}
	return field;
}

int HubRoadConvergence(const glm::ivec2 location, const RoadGraph& roads, const int radius) {
	const long radiusSq = static_cast<long>(radius) * radius;
	int converging = 0;
	for (const RoadSegment& seg : roads.segments) {
		const bool passes =
			std::ranges::any_of(seg.path, [&](const glm::ivec2 p) { return DistanceSquared(p, location) <= radiusSq; });
		if (passes) {
			++converging;
		}
	}
	return converging;
}

std::vector<WorldObject>
BuildHubMarkers(const std::vector<RoadHub>& hubs, const RoadGraph& roads, const WorldGenParams& params) {
	std::vector<WorldObject> markers;
	for (const RoadHub& hub : hubs) {
		if (HubRoadConvergence(hub.location, roads, params.hubMarkerRadius) < 1) {
			continue; // no realised road passes through this hub — no meeting-place to mark
		}
		WorldObject marker;
		marker.footprint.push_back(hub.location);
		marker.kind = std::string{kHubMarkerKind};
		marker.height = kHubMarkerHeight;
		marker.blocksMovement = false; // a signpost sits ON the crossroads; roads still run through it
		markers.push_back(std::move(marker));
	}
	return markers;
}

std::vector<int> SelectCrossroadsOutposts(
	const std::vector<RoadHub>& hubs,
	const RoadGraph& roads,
	const std::vector<TownSite>& towns,
	const WorldGenParams& params
) {
	std::vector<int> selected;
	if (params.maxCrossroadsOutposts <= 0) {
		return selected;
	}

	const long townDistSq = static_cast<long>(params.outpostMinTownDistance) * params.outpostMinTownDistance;

	struct Candidate {
		int index;
		int convergence;
		glm::ivec2 loc;
	};
	std::vector<Candidate> candidates;
	for (int i = 0; i < static_cast<int>(hubs.size()); ++i) {
		const RoadHub& hub = hubs[static_cast<std::size_t>(i)];
		const int convergence = HubRoadConvergence(hub.location, roads, params.outpostConvergenceRadius);
		if (convergence < params.outpostMinConvergence) {
			continue; // not a strong enough crossroads
		}
		const bool nearLivingTown = std::ranges::any_of(towns, [&](const TownSite& t) {
			return t.status == TownStatus::Living && DistanceSquared(t.location, hub.location) <= townDistSq;
		});
		if (nearLivingTown) {
			continue; // a real town is already here
		}
		candidates.push_back({i, convergence, hub.location});
	}

	// Strongest crossroads first; deterministic tiebreak by location.
	std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
		if (a.convergence != b.convergence) {
			return a.convergence > b.convergence;
		}
		if (a.loc.y != b.loc.y) {
			return a.loc.y < b.loc.y;
		}
		return a.loc.x < b.loc.x;
	});

	const int cap = std::min(params.maxCrossroadsOutposts, static_cast<int>(candidates.size()));
	selected.reserve(static_cast<std::size_t>(cap));
	for (int i = 0; i < cap; ++i) {
		selected.push_back(candidates[static_cast<std::size_t>(i)].index);
	}
	return selected;
}

} // namespace game::farm
