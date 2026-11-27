#include "world_gen.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <queue>
#include <random>
#include <set>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "bridge_builder.hpp"
#include "building_catalog.hpp"
#include "creature_catalog.hpp"
#include "creature_placement.hpp"
#include "road_hubs.hpp"
#include "road_router.hpp"
#include "seeded_random.hpp"
#include "town_layout.hpp"
#include "tree_species_catalog.hpp"
#include "world_data.hpp"

namespace game::farm {
namespace {

// ── Tuning (module-local, centralized — no magic numbers scattered through the logic) ──
// History spans deep time (~-10000) up to year 0, which is when the player's game begins. Water
// bodies and rivers form in "geological" prehistory (before any town), but they flow through the
// same dated-event timeline as roads/collapses, so a future version can insert river diversion or
// lake-creation events at ANY year — exactly like road connections.
constexpr int kWaterFormedYear = -12000;  // geological: oceans/lakes settle
constexpr int kRiversFormedYear = -11000; // geological: rivers carve to the sea
constexpr int kBaseFoundingYear = -10000; // earliest town founding era
constexpr int kFoundingSpread = 3000;     // early ruins collapse within this window of deep history
constexpr int kGameStartYear = 0;         // the simulation runs up to here (player's start)
constexpr int kInitialPopMin = 25;
constexpr int kInitialPopMax = 120;
constexpr int kMinTownPlacements = 8; // floor on placed town-site candidates
constexpr int kMinLivingTowns = 2;    // the start village plus at least one road neighbour

// Organic history: a chronological sequence of weighted-random events (town founded, road built,
// road lost, town collapsed, or nothing) advancing from deep history to year 0. This interleaves
// foundings with connections and collapses rather than running them in separate phases.
constexpr int kMinHistoryEntries = 45;
constexpr int kMaxHistoryEntries = 95;
constexpr int kWeightFound = 34;     // bias toward founding while candidates remain
constexpr int kWeightConnect = 38;   // build a road between two unconnected living towns
constexpr int kWeightDisconnect = 4; // a spur road falls into disrepair
constexpr int kWeightCollapse = 4;   // a poorly-connected living town collapses into a ruin
constexpr int kWeightNothing = 16;   // a quiet interval (dropped from the timeline)

// A prosperity-gated bridge investment: a thriving, well-connected town funds a crossing to reach an
// isolated place. It only competes when the gate is met (otherwise weight 0, leaving the existing
// RNG stream untouched), and it draws no RNG of its own, so determinism is preserved.
constexpr int kWeightBridge = 10;       // likelihood of a deliberate bridge once prosperity allows one
constexpr int kBridgeProsperityPop = 90; // min population for a town to be "prosperous" enough to fund a bridge
constexpr int kBridgeProsperityDegree = 2; // min road connectivity for that prosperity
constexpr float kBridgeDeckHeight = 0.4f;  // visual height of a bridge-deck world-object in tile units

const std::array<std::string_view, 16> kNamePrefixes{
	"Mill",
	"Cold",
	"Ash",
	"Oak",
	"Red",
	"Stone",
	"Fair",
	"Green",
	"North",
	"Black",
	"White",
	"Bright",
	"Wolf",
	"Raven",
	"Elder",
	"Dun"
};
const std::array<std::string_view, 16> kNameSuffixes{
	"haven",
	"ford",
	"wick",
	"bury",
	"dale",
	"field",
	"ton",
	"mere",
	"reach",
	"gate",
	"hollow",
	"stead",
	"water",
	"brook",
	"crest",
	"march"
};
const std::array<std::string_view, 14> kWaterNames{
	"Coldwater",
	"Mirror",
	"Silver",
	"Deepmere",
	"Blackwater",
	"Clearwater",
	"Still",
	"Moonwater",
	"Frostmere",
	"Greywater",
	"Emberlake",
	"Northwind",
	"Pale",
	"Hollow"
};

// Deterministic RNG wrapper — all stochastic choices draw from one seeded engine in a fixed order.
struct Rng {
	std::mt19937 engine;
	explicit Rng(const int seed) : engine(static_cast<std::uint32_t>(seed)) {}

	int Int(const int lo, const int hi) { return UniformInt(engine, lo, hi); }
	float Float(const float lo, const float hi) { return UniformFloat(engine, lo, hi); }
};

int SquaredDistance(const glm::ivec2 a, const glm::ivec2 b) {
	const glm::ivec2 d = a - b;
	return d.x * d.x + d.y * d.y;
}

bool IsWaterBiome(const Biome b) { return b == Biome::Ocean || b == Biome::Lake; }
bool IsLandForTown(const Biome b) { return b == Biome::Plains || b == Biome::Forest || b == Biome::Hills; }

// ── Stage 1: elevation ──────────────────────────────────────────────────────────────────────────
// Fractal Perlin heightmap stretched to [0,1], then a radial falloff pushes the map edges underwater
// so the border becomes ocean (an island world).
Grid BuildElevation(const WorldGenParams& params, const int seed, Rng& rng) {
	Grid grid;
	grid.width = params.width;
	grid.height = params.height;

	engine::noise::NoiseSampler sampler(std::make_unique<engine::noise::builtin::PerlinGenerator>(seed));
	const glm::vec2 offset{rng.Float(0.0f, 1000.0f), rng.Float(0.0f, 1000.0f)};
	grid.elevation =
		engine::noise::GenerateHeightMap(sampler, grid.width, grid.height, offset, params.noiseScale, params.fractal);

	const float halfW = static_cast<float>(grid.width - 1) * 0.5f;
	const float halfH = static_cast<float>(grid.height - 1) * 0.5f;
	const float invDiag = 1.0f / std::sqrt(halfW * halfW + halfH * halfH);
	for (int y = 0; y < grid.height; ++y) {
		for (int x = 0; x < grid.width; ++x) {
			const float dx = (static_cast<float>(x) - halfW);
			const float dy = (static_cast<float>(y) - halfH);
			const float d = std::sqrt(dx * dx + dy * dy) * invDiag; // 0 centre … 1 corner
			float& e = grid.elevation[grid.Index(x, y)];
			e = std::clamp(e - params.edgeFalloff * d * d, 0.0f, 1.0f);
		}
	}

	grid.biome.assign(static_cast<size_t>(grid.width) * grid.height, Biome::Plains);
	return grid;
}

// Carve a circular depression below sea level — guarantees interior water (lakes/ponds) and gives
// rivers a destination even when the noise alone produced none.
void CarveLake(Grid& grid, const glm::ivec2 centre, const int radius, const float floorElevation) {
	for (int y = centre.y - radius; y <= centre.y + radius; ++y) {
		for (int x = centre.x - radius; x <= centre.x + radius; ++x) {
			if (!grid.InBounds(x, y)) {
				continue;
			}
			if (SquaredDistance({x, y}, centre) <= radius * radius) {
				grid.elevation[grid.Index(x, y)] = std::min(grid.elevation[grid.Index(x, y)], floorElevation);
			}
		}
	}
}

// Place two guaranteed interior lakes at low interior tiles, well apart from each other and the edge.
void GuaranteeInteriorWater(Grid& grid, const WorldGenParams& params) {
	const int margin = std::max(8, params.width / 8);
	glm::ivec2 bestA{-1, -1};
	glm::ivec2 bestB{-1, -1};
	float lowA = std::numeric_limits<float>::max();
	float lowB = std::numeric_limits<float>::max();
	const int minSep = params.width / 3;
	for (int y = margin; y < grid.height - margin; ++y) {
		for (int x = margin; x < grid.width - margin; ++x) {
			const float e = grid.ElevationAt(x, y);
			if (e < lowA) {
				lowA = e;
				bestA = {x, y};
			}
		}
	}
	for (int y = margin; y < grid.height - margin; ++y) {
		for (int x = margin; x < grid.width - margin; ++x) {
			if (bestA.x >= 0 && SquaredDistance({x, y}, bestA) < minSep * minSep) {
				continue;
			}
			const float e = grid.ElevationAt(x, y);
			if (e < lowB) {
				lowB = e;
				bestB = {x, y};
			}
		}
	}

	const float floor = params.seaLevel - 0.08f;
	if (bestA.x >= 0) {
		CarveLake(grid, bestA, std::max(3, params.width / 40), floor);
	}
	if (bestB.x >= 0) {
		CarveLake(grid, bestB, std::max(3, params.width / 48), floor);
	}
}

// ── Stage 2: biomes + water bodies ──────────────────────────────────────────────────────────────
void AssignBiomes(Grid& grid, const WorldGenParams& params) {
	for (int i = 0; i < static_cast<int>(grid.elevation.size()); ++i) {
		const float e = grid.elevation[i];
		Biome b{};
		if (e <= params.seaLevel) {
			b = Biome::Ocean; // refined to Ocean/Lake during flood fill
		}
		else if (e <= params.beachLevel) {
			b = Biome::Beach;
		}
		else if (e <= params.plainsLevel) {
			b = Biome::Plains;
		}
		else if (e <= params.forestLevel) {
			b = Biome::Forest;
		}
		else if (e <= params.hillsLevel) {
			b = Biome::Hills;
		}
		else {
			b = Biome::Mountain;
		}
		grid.biome[i] = b;
	}
}

// Flood-fill connected water tiles into WaterBody records. Border-touching components are Oceans;
// interior components are Lakes or Ponds by size. Tiles are re-tagged Ocean/Lake accordingly, and
// the per-tile component label map is returned alongside the bodies.
struct WaterResult {
	std::vector<WaterBody> bodies;
	std::vector<int> label;
};

WaterResult ClassifyWater(Grid& grid, const WorldGenParams& params) {
	WaterResult result;
	std::vector<int> label(grid.elevation.size(), 0);
	const std::array<glm::ivec2, 4> neighbours{glm::ivec2{1, 0}, {-1, 0}, {0, 1}, {0, -1}};

	int nextId = 1;
	for (int sy = 0; sy < grid.height; ++sy) {
		for (int sx = 0; sx < grid.width; ++sx) {
			const int start = grid.Index(sx, sy);
			if (label[start] != 0 || grid.elevation[start] > params.seaLevel) {
				continue;
			}
			const int id = nextId++;
			std::queue<glm::ivec2> frontier;
			frontier.push({sx, sy});
			label[start] = id;
			glm::ivec2 sum{0, 0};
			int count = 0;
			bool touchesBorder = false;
			while (!frontier.empty()) {
				const glm::ivec2 cur = frontier.front();
				frontier.pop();
				sum += cur;
				++count;
				if (cur.x == 0 || cur.y == 0 || cur.x == grid.width - 1 || cur.y == grid.height - 1) {
					touchesBorder = true;
				}
				for (const glm::ivec2 off : neighbours) {
					const glm::ivec2 n = cur + off;
					if (!grid.InBounds(n.x, n.y)) {
						continue;
					}
					const int ni = grid.Index(n.x, n.y);
					if (label[ni] == 0 && grid.elevation[ni] <= params.seaLevel) {
						label[ni] = id;
						frontier.push(n);
					}
				}
			}
			WaterBody body;
			body.id = id;
			body.tileCount = count;
			body.centroid = {sum.x / std::max(1, count), sum.y / std::max(1, count)};
			body.type = touchesBorder                  ? WaterBodyType::Ocean
						: count <= params.pondMaxTiles ? WaterBodyType::Pond
													   : WaterBodyType::Lake;
			result.bodies.push_back(body);
		}
	}

	for (int i = 0; i < static_cast<int>(label.size()); ++i) {
		if (label[i] == 0) {
			continue;
		}
		const WaterBody& body = result.bodies[label[i] - 1];
		grid.biome[i] = body.type == WaterBodyType::Ocean ? Biome::Ocean : Biome::Lake;
	}
	result.label = std::move(label);
	return result;
}

// Multi-source BFS distance (in tiles) from every water tile. Used to pick highland springs and to
// steer the river fallback toward the nearest water.
std::vector<int> DistanceToWater(const Grid& grid, const std::vector<int>& waterLabel) {
	std::vector<int> dist(grid.elevation.size(), std::numeric_limits<int>::max());
	std::queue<glm::ivec2> frontier;
	for (int y = 0; y < grid.height; ++y) {
		for (int x = 0; x < grid.width; ++x) {
			if (waterLabel[grid.Index(x, y)] != 0) {
				dist[grid.Index(x, y)] = 0;
				frontier.push({x, y});
			}
		}
	}
	const std::array<glm::ivec2, 4> neighbours{glm::ivec2{1, 0}, {-1, 0}, {0, 1}, {0, -1}};
	while (!frontier.empty()) {
		const glm::ivec2 cur = frontier.front();
		frontier.pop();
		const int cd = dist[grid.Index(cur.x, cur.y)];
		for (const glm::ivec2 off : neighbours) {
			const glm::ivec2 n = cur + off;
			if (!grid.InBounds(n.x, n.y)) {
				continue;
			}
			const int ni = grid.Index(n.x, n.y);
			if (dist[ni] > cd + 1) {
				dist[ni] = cd + 1;
				frontier.push(n);
			}
		}
	}
	return dist;
}

// Name notable bodies of water (oceans, lakes, and sizeable ponds) so they can be labelled on the
// map and referenced in the report. Tiny specks stay unnamed.
void NameWaterBodies(std::vector<WaterBody>& bodies, Rng& rng) {
	constexpr int kNotableWaterTiles = 8;
	std::set<std::string> used;
	const auto pickBase = [&]() {
		const int start = rng.Int(0, static_cast<int>(kWaterNames.size()) - 1);
		for (size_t i = 0; i < kWaterNames.size(); ++i) {
			std::string candidate{kWaterNames[(static_cast<size_t>(start) + i) % kWaterNames.size()]};
			if (used.insert(candidate).second) {
				return candidate;
			}
		}
		return std::string{kWaterNames[static_cast<size_t>(start)]}; // pool exhausted: allow a repeat
	};
	for (WaterBody& body : bodies) {
		if (body.tileCount < kNotableWaterTiles) {
			continue;
		}
		const std::string base = pickBase();
		switch (body.type) {
		case WaterBodyType::Ocean: body.name = "The " + base + " Sea"; break;
		case WaterBodyType::Lake: body.name = "Lake " + base; break;
		case WaterBodyType::Pond: body.name = base + " Pond"; break;
		}
	}
}

// ── Stage 3: rivers ──────────────────────────────────────────────────────────────────────────────
// Trace a river from a highland spring, following steepest descent, falling back to "step toward the
// nearest water" whenever it reaches a local minimum, until it meets a water body.
River TraceRiver(
	Grid& grid,
	const std::vector<int>& waterLabel,
	const std::vector<int>& distToWater,
	const glm::ivec2 spring,
	const int riverId
) {
	River river;
	river.id = riverId;
	const std::array<glm::ivec2, 8>
		neighbours{glm::ivec2{1, 0}, {-1, 0}, {0, 1}, {0, -1}, glm::ivec2{1, 1}, {1, -1}, {-1, 1}, {-1, -1}};
	std::vector<char> visited(grid.elevation.size(), 0);
	glm::ivec2 cur = spring;
	const int maxSteps = grid.width * grid.height;
	for (int step = 0; step < maxSteps; ++step) {
		river.path.push_back(cur);
		visited[grid.Index(cur.x, cur.y)] = 1;
		const int label = waterLabel[grid.Index(cur.x, cur.y)];
		if (label != 0) {
			river.toBodyId = label;
			break;
		}

		glm::ivec2 best{-1, -1};
		float bestElev = grid.ElevationAt(cur.x, cur.y);
		for (const glm::ivec2 off : neighbours) {
			const glm::ivec2 n = cur + off;
			if (!grid.InBounds(n.x, n.y) || visited[grid.Index(n.x, n.y)]) {
				continue;
			}
			if (grid.ElevationAt(n.x, n.y) < bestElev) {
				bestElev = grid.ElevationAt(n.x, n.y);
				best = n;
			}
		}
		if (best.x < 0) {
			// Local minimum: step toward the nearest water using the distance field.
			int bestDist = distToWater[grid.Index(cur.x, cur.y)];
			for (const glm::ivec2 off : neighbours) {
				const glm::ivec2 n = cur + off;
				if (!grid.InBounds(n.x, n.y) || visited[grid.Index(n.x, n.y)]) {
					continue;
				}
				if (distToWater[grid.Index(n.x, n.y)] < bestDist) {
					bestDist = distToWater[grid.Index(n.x, n.y)];
					best = n;
				}
			}
		}
		if (best.x < 0) {
			break; // fully boxed in (should not happen on an island with border ocean)
		}
		cur = best;
	}

	// Carve the river into the biome layer (skip tiles that are already water).
	for (const glm::ivec2 p : river.path) {
		if (!IsWaterBiome(grid.BiomeAt(p.x, p.y))) {
			grid.biome[grid.Index(p.x, p.y)] = Biome::River;
		}
	}
	return river;
}

std::vector<River> TraceRivers(
	Grid& grid,
	const WorldGenParams& params,
	const std::vector<int>& waterLabel,
	const std::vector<int>& distToWater,
	Rng& rng
) {
	// Spring candidates: interior highland tiles far from water, sorted by (far-from-water, high).
	struct Candidate {
		glm::ivec2 pos;
		int dist;
		float elev;
	};
	std::vector<Candidate> candidates;
	for (int y = 2; y < grid.height - 2; ++y) {
		for (int x = 2; x < grid.width - 2; ++x) {
			const int d = distToWater[grid.Index(x, y)];
			if (d == std::numeric_limits<int>::max() || d < 4) {
				continue;
			}
			candidates.push_back({.pos = {x, y}, .dist = d, .elev = grid.ElevationAt(x, y)});
		}
	}
	std::ranges::sort(candidates, [](const Candidate& a, const Candidate& b) {
		if (a.dist != b.dist) {
			return a.dist > b.dist;
		}
		if (a.elev != b.elev) {
			return a.elev > b.elev;
		}
		return a.pos.x != b.pos.x ? a.pos.x < b.pos.x : a.pos.y < b.pos.y;
	});

	// Pick spaced springs so rivers form distinct networks rather than overlapping.
	std::vector<glm::ivec2> springs;
	const int minSpringSep = std::max(6, params.width / 12);
	for (const Candidate& c : candidates) {
		const bool farEnough = std::ranges::all_of(springs, [&](const glm::ivec2 s) {
			return SquaredDistance(s, c.pos) >= minSpringSep * minSpringSep;
		});
		if (farEnough) {
			springs.push_back(c.pos);
		}
		if (static_cast<int>(springs.size()) >= params.riverCount) {
			break;
		}
	}
	// Guarantee at least two springs even on low-relief maps.
	for (size_t i = 0; springs.size() < 2 && i < candidates.size(); ++i) {
		if (std::ranges::find(springs, candidates[i].pos) == springs.end()) {
			springs.push_back(candidates[i].pos);
		}
	}
	(void) rng;

	std::vector<River> rivers;
	int riverId = 1;
	for (const glm::ivec2 spring : springs) {
		rivers.push_back(TraceRiver(grid, waterLabel, distToWater, spring, riverId++));
	}
	return rivers;
}

// ── Stage 4: towns, history, roads ────────────────────────────────────────────────────────────────
bool OceanWithin(const Grid& grid, const glm::ivec2 p, const int radius) {
	for (int y = p.y - radius; y <= p.y + radius; ++y) {
		for (int x = p.x - radius; x <= p.x + radius; ++x) {
			if (grid.InBounds(x, y) && grid.BiomeAt(x, y) == Biome::Ocean) {
				return true;
			}
		}
	}
	return false;
}

std::string MakeTownName(Rng& rng) {
	const auto prefix = kNamePrefixes[static_cast<size_t>(rng.Int(0, static_cast<int>(kNamePrefixes.size()) - 1))];
	const auto suffix = kNameSuffixes[static_cast<size_t>(rng.Int(0, static_cast<int>(kNameSuffixes.size()) - 1))];
	return std::string{prefix} + std::string{suffix};
}

// A placed-but-not-yet-founded settlement site. The organic simulation founds these over time.
struct TownCandidate {
	glm::ivec2 pos{0, 0};
	std::string name;
	int size{0};
};

// Reject-sample inland, non-coastal land tiles spaced at least minTownSpacing apart. Spacing relaxes
// if needed so at least kMinTownPlacements candidate sites are always produced.
std::vector<TownCandidate> PlaceTownCandidates(const Grid& grid, const WorldGenParams& params, Rng& rng) {
	std::vector<TownCandidate> sites;
	int spacing = params.minTownSpacing;
	const int target = std::max(params.townCount, kMinTownPlacements);
	for (int relax = 0; relax < 4 && static_cast<int>(sites.size()) < target; ++relax) {
		const int attempts = grid.width * grid.height;
		for (int a = 0; a < attempts && static_cast<int>(sites.size()) < target; ++a) {
			const glm::ivec2 p{rng.Int(2, grid.width - 3), rng.Int(2, grid.height - 3)};
			if (!IsLandForTown(grid.BiomeAt(p.x, p.y))) {
				continue;
			}
			if (OceanWithin(grid, p, params.coastalRadius)) {
				continue;
			}
			const bool farEnough = std::ranges::all_of(sites, [&](const TownCandidate& t) {
				return SquaredDistance(t.pos, p) >= spacing * spacing;
			});
			if (!farEnough) {
				continue;
			}
			sites.push_back({.pos = p, .name = MakeTownName(rng), .size = rng.Int(kInitialPopMin, kInitialPopMax)});
		}
		spacing = std::max(4, spacing - 4);
	}
	return sites;
}

int CountLiving(const std::vector<TownSite>& towns) {
	return static_cast<int>(std::ranges::count_if(towns, [](const TownSite& t) {
		return t.status == TownStatus::Living;
	}));
}

glm::ivec2 GridCentre(const Grid& grid) { return {grid.width / 2, grid.height / 2}; }

std::pair<int, int> NormalizedRoad(const int a, const int b) { return {std::min(a, b), std::max(a, b)}; }

// Active road set: (townA, townB) id pairs that currently exist. Roads are added and removed over the
// course of the simulation, so this is the single source of truth for "what is connected right now".
using RoadSet = std::set<std::pair<int, int>>;

bool AreConnected(const RoadSet& roads, const int a, const int b) { return roads.contains(NormalizedRoad(a, b)); }

// Number of roads incident to a town — its connectivity. Well-connected towns are more resilient.
int TownDegree(const RoadSet& roads, const int id) {
	int degree = 0;
	for (const auto& [a, b] : roads) {
		if (a == id || b == id) {
			++degree;
		}
	}
	return degree;
}

std::vector<int> LivingIds(const std::vector<TownSite>& towns) {
	std::vector<int> ids;
	for (const TownSite& t : towns) {
		if (t.status == TownStatus::Living) {
			ids.push_back(t.id);
		}
	}
	return ids;
}

const TownSite& TownById(const std::vector<TownSite>& towns, const int id) {
	return towns[static_cast<size_t>(id - 1)];
}

// Remove every road incident to a town (used when it collapses) and record each loss.
void SeverRoads(
	RoadSet& roads,
	const std::vector<TownSite>& towns,
	const int townId,
	const int year,
	std::vector<HistoryEvent>& history
) {
	for (auto it = roads.begin(); it != roads.end();) {
		if (it->first == townId || it->second == townId) {
			const int other = it->first == townId ? it->second : it->first;
			history.push_back(
				{.year = year,
				 .type = HistoryEventType::RoadRemoved,
				 .a = townId,
				 .b = other,
				 .description = "road to " + TownById(towns, other).name + " abandoned"}
			);
			it = roads.erase(it);
		}
		else {
			++it;
		}
	}
}

// Found the next candidate: assign the next contiguous id and append it as a living town.
void FoundTown(
	std::vector<TownCandidate>& pool,
	size_t& poolIdx,
	std::vector<TownSite>& towns,
	const int year,
	std::vector<HistoryEvent>& history
) {
	const TownCandidate& c = pool[poolIdx++];
	TownSite town;
	town.id = static_cast<int>(towns.size()) + 1;
	town.name = c.name;
	town.location = c.pos;
	town.size = c.size;
	town.status = TownStatus::Living;
	town.foundingYear = year;
	towns.push_back(town);
	history.push_back({.year = year, .type = HistoryEventType::TownFounded, .a = town.id, .b = 0, .description = town.name + " founded"});
}

// Build a road between the closest pair of living towns that are not already connected.
bool ConnectClosestPair(
	RoadSet& roads,
	const std::vector<TownSite>& towns,
	const int year,
	std::vector<HistoryEvent>& history
) {
	const std::vector<int> living = LivingIds(towns);
	int bestA = 0;
	int bestB = 0;
	int bestDist = std::numeric_limits<int>::max();
	for (size_t i = 0; i < living.size(); ++i) {
		for (size_t j = i + 1; j < living.size(); ++j) {
			if (AreConnected(roads, living[i], living[j])) {
				continue;
			}
			const int d = SquaredDistance(TownById(towns, living[i]).location, TownById(towns, living[j]).location);
			if (d < bestDist) {
				bestDist = d;
				bestA = living[i];
				bestB = living[j];
			}
		}
	}
	if (bestA == 0) {
		return false;
	}
	roads.insert(NormalizedRoad(bestA, bestB));
	history.push_back(
		{.year = year,
		 .type = HistoryEventType::RoadBuilt,
		 .a = bestA,
		 .b = bestB,
		 .description = TownById(towns, bestA).name + " connected to " + TownById(towns, bestB).name}
	);
	return true;
}

// Sever the lowest-value road: the one touching the least-connected town. Well-connected trade
// links between two healthy cities endure.
void DisconnectSpur(
	RoadSet& roads,
	const std::vector<TownSite>& towns,
	const int year,
	std::vector<HistoryEvent>& history
) {
	if (roads.empty()) {
		return;
	}
	auto worst = roads.begin();
	int worstScore = std::numeric_limits<int>::max();
	for (auto it = roads.begin(); it != roads.end(); ++it) {
		const int score = std::min(TownDegree(roads, it->first), TownDegree(roads, it->second));
		if (score < worstScore) {
			worstScore = score;
			worst = it;
		}
	}
	const auto [a, b] = *worst;
	roads.erase(worst);
	history.push_back(
		{.year = year,
		 .type = HistoryEventType::RoadRemoved,
		 .a = a,
		 .b = b,
		 .description =
			 "road between " + TownById(towns, a).name + " and " + TownById(towns, b).name + " fell into disrepair"}
	);
}

// Collapse the least-connected living town (ties → smallest). Connected cities are resilient: an
// isolated outpost crumbles outright, but if every living town is connected collapse is rare.
void CollapseWeakest(
	RoadSet& roads,
	std::vector<TownSite>& towns,
	const int year,
	Rng& rng,
	std::vector<HistoryEvent>& history
) {
	const std::vector<int> living = LivingIds(towns);
	int victim = 0;
	int worstDegree = std::numeric_limits<int>::max();
	int smallest = std::numeric_limits<int>::max();
	for (const int id : living) {
		const int degree = TownDegree(roads, id);
		const int size = TownById(towns, id).size;
		if (degree < worstDegree || (degree == worstDegree && size < smallest)) {
			worstDegree = degree;
			smallest = size;
			victim = id;
		}
	}
	if (victim == 0) {
		return;
	}
	const bool isolated = worstDegree == 0;
	if (!isolated && rng.Int(0, 3) != 0) {
		return; // connected city endures
	}
	TownSite& t = towns[static_cast<size_t>(victim - 1)];
	t.status = TownStatus::Ruined;
	t.collapseYear = year;
	t.size = 0;
	history.push_back({.year = year, .type = HistoryEventType::TownCollapsed, .a = victim, .b = 0, .description = t.name + " collapsed into ruin"});
	SeverRoads(roads, towns, victim, year, history);
}

// A prosperity-gated bridge investment: pick a thriving, well-connected living town (the funder, the
// largest qualifying; ties → smallest id) and an isolated living town it would reach (degree 0,
// nearest; ties → smallest id), or {0, 0} when no such pair exists this tick. Deterministic — draws
// no RNG — so the simulation's reproducibility is preserved.
std::pair<int, int> FindBridgeInvestment(const std::vector<TownSite>& towns, const RoadSet& roads) {
	int funder = 0;
	int funderSize = -1;
	for (const int id : LivingIds(towns)) {
		const TownSite& t = TownById(towns, id);
		if (t.size < kBridgeProsperityPop || TownDegree(roads, id) < kBridgeProsperityDegree) {
			continue;
		}
		if (t.size > funderSize) {
			funderSize = t.size;
			funder = id;
		}
	}
	if (funder == 0) {
		return {0, 0};
	}
	int target = 0;
	int bestDist = std::numeric_limits<int>::max();
	for (const int id : LivingIds(towns)) {
		if (id == funder || TownDegree(roads, id) != 0) {
			continue;
		}
		const int d = SquaredDistance(TownById(towns, funder).location, TownById(towns, id).location);
		if (d < bestDist) {
			bestDist = d;
			target = id;
		}
	}
	return target == 0 ? std::pair<int, int>{0, 0} : std::pair<int, int>{funder, target};
}

// Record a deliberate bridge investment: add the wanted connection so the spatial pass can realise it
// (a bridge over water, or an ordinary road if the target turns out land-reachable) and push a dated
// BridgeBuilt event into the timeline/history.
void InvestInBridge(
	RoadSet& roads,
	const std::vector<TownSite>& towns,
	const int funder,
	const int target,
	const int year,
	std::vector<HistoryEvent>& history
) {
	roads.insert(NormalizedRoad(funder, target));
	history.push_back(
		{.year = year,
		 .type = HistoryEventType::BridgeBuilt,
		 .a = funder,
		 .b = target,
		 .description = TownById(towns, funder).name + " funded a bridge to reach " + TownById(towns, target).name}
	);
}

// ── Pluggable history events (Strategy + Registry) ──────────────────────────────────────────────
// Mutable state threaded through every event's weight/apply callbacks for the current tick.
struct SimContext {
	const WorldGenParams& params;
	Rng& rng;
	int year;
	std::vector<TownCandidate>& pool;
	size_t& poolIdx;
	std::vector<TownSite>& towns;
	RoadSet& roads;
	std::vector<HistoryEvent>& history;
};

// One kind of history event. `weight` returns this event's relative likelihood given the current
// state (0 = not applicable this tick); `apply` performs it (mutating towns/roads and pushing
// HistoryEvents). New event types (e.g. a natural disaster, a migration, a river diversion) are
// added purely by registering another HistoryEventKind — no changes to the simulation loop.
struct HistoryEventKind {
	std::string name;
	std::function<int(const SimContext&)> weight;
	std::function<void(SimContext&)> apply;
};

// The default event registry. Append to this (or expose a RegisterHistoryEvent hook) to add events.
std::vector<HistoryEventKind> DefaultHistoryEvents() {
	std::vector<HistoryEventKind> kinds;
	kinds.push_back(
		{.name = "found",
		 .weight = [](const SimContext& c) { return c.poolIdx < c.pool.size() ? kWeightFound : 0; },
		 .apply = [](SimContext& c) { FoundTown(c.pool, c.poolIdx, c.towns, c.year, c.history); }}
	);
	kinds.push_back(
		{.name = "connect",
		 .weight = [](const SimContext& c) { return CountLiving(c.towns) >= 2 ? kWeightConnect : 0; },
		 .apply = [](SimContext& c) { ConnectClosestPair(c.roads, c.towns, c.year, c.history); }}
	);
	kinds.push_back(
		{.name = "disconnect",
		 .weight = [](const SimContext& c) { return c.roads.empty() ? 0 : kWeightDisconnect; },
		 .apply = [](SimContext& c) { DisconnectSpur(c.roads, c.towns, c.year, c.history); }}
	);
	kinds.push_back(
		{.name = "collapse",
		 .weight = [](const SimContext& c) { return CountLiving(c.towns) > kMinLivingTowns ? kWeightCollapse : 0; },
		 .apply = [](SimContext& c) { CollapseWeakest(c.roads, c.towns, c.year, c.rng, c.history); }}
	);
	kinds.push_back(
		{.name = "nothing",
		 .weight = [](const SimContext&) { return kWeightNothing; },
		 .apply = [](SimContext&) { /* a quiet interval — dropped from the timeline */ }}
	);
	kinds.push_back(
		{.name = "bridge",
		 .weight = [](const SimContext& c) { return FindBridgeInvestment(c.towns, c.roads).first != 0 ? kWeightBridge : 0; },
		 .apply =
			 [](SimContext& c) {
				 const auto [funder, target] = FindBridgeInvestment(c.towns, c.roads);
				 if (funder != 0) {
					 InvestInBridge(c.roads, c.towns, funder, target, c.year, c.history);
				 }
			 }}
	);
	return kinds;
}

// The organic, event-driven history: a single chronological pass from deep history up to the
// player's start year (0). Each tick advances the year and fires one weighted-random event from the
// pluggable registry.
void SimulateHistory(
	std::vector<TownCandidate>& pool,
	std::vector<TownSite>& towns,
	const WorldGenParams& params,
	Rng& rng,
	std::vector<HistoryEvent>& history,
	RoadSet& roads
) {
	const std::vector<HistoryEventKind> events = DefaultHistoryEvents();
	const int entries = rng.Int(kMinHistoryEntries, kMaxHistoryEntries);
	const int span = kGameStartYear - kBaseFoundingYear;
	const int avgStep = std::max(1, span / entries);
	size_t poolIdx = 0;
	int year = kBaseFoundingYear;

	for (int e = 0; e < entries; ++e) {
		year = std::min(kGameStartYear, year + rng.Int(avgStep / 2 + 1, avgStep * 2 + 1));
		SimContext ctx{
			.params = params,
			.rng = rng,
			.year = year,
			.pool = pool,
			.poolIdx = poolIdx,
			.towns = towns,
			.roads = roads,
			.history = history
		};

		std::vector<int> weights;
		weights.reserve(events.size());
		int total = 0;
		for (const HistoryEventKind& kind : events) {
			const int w = std::max(0, kind.weight(ctx));
			weights.push_back(w);
			total += w;
		}
		if (total <= 0) {
			continue;
		}
		int roll = rng.Int(0, total - 1);
		for (size_t i = 0; i < events.size(); ++i) {
			if (roll < weights[i]) {
				events[i].apply(ctx);
				break;
			}
			roll -= weights[i];
		}
	}

	// Found any remaining candidates late so every placed site becomes a real town (more living towns
	// survive to the present; stragglers simply join as young settlements).
	while (poolIdx < pool.size()) {
		year = std::min(kGameStartYear, year + 1);
		FoundTown(pool, poolIdx, towns, year, history);
	}
}

int FindCentralLivingTown(const std::vector<TownSite>& towns, const glm::ivec2 centre) {
	int best = 0;
	int bestDist = std::numeric_limits<int>::max();
	for (const TownSite& t : towns) {
		if (t.status != TownStatus::Living) {
			continue;
		}
		const int d = SquaredDistance(t.location, centre);
		if (d < bestDist) {
			bestDist = d;
			best = t.id;
		}
	}
	return best;
}

// Add a ruined settlement on inland land within explorationRange of the anchor (spiral search).
TownSite MakeRuinNear(const Grid& grid, const WorldGenParams& params, const glm::ivec2 anchor, const int id, Rng& rng) {
	for (int radius = 6; radius <= params.explorationRange; radius += 2) {
		for (int step = 0; step < 24; ++step) {
			// A uniform random direction from a point in the unit disc, not cos/sin of an angle: the math libraries
			// round those differently, and the cast to int would turn that into a different tile per platform.
			glm::vec2 dir{0.0f};
			float lengthSq = 0.0f;
			do {
				dir = {rng.Float(-1.0f, 1.0f), rng.Float(-1.0f, 1.0f)};
				lengthSq = dir.x * dir.x + dir.y * dir.y;
			} while (lengthSq > 1.0f || lengthSq < 0.01f);
			const float length = std::sqrt(lengthSq);
			const glm::ivec2 p{
				anchor.x + static_cast<int>(dir.x / length * static_cast<float>(radius)),
				anchor.y + static_cast<int>(dir.y / length * static_cast<float>(radius))
			};
			if (grid.InBounds(p.x, p.y) && IsLandForTown(grid.BiomeAt(p.x, p.y))) {
				TownSite ruin;
				ruin.id = id;
				ruin.name = MakeTownName(rng);
				ruin.location = p;
				ruin.size = 0;
				ruin.status = TownStatus::Ruined;
				ruin.foundingYear = kBaseFoundingYear;
				ruin.collapseYear = kBaseFoundingYear + kFoundingSpread;
				return ruin;
			}
		}
	}
	// Fallback: sit the ruin on the anchor tile (always valid land).
	TownSite ruin;
	ruin.id = id;
	ruin.name = MakeTownName(rng);
	ruin.location = anchor;
	ruin.status = TownStatus::Ruined;
	ruin.foundingYear = kBaseFoundingYear;
	ruin.collapseYear = kBaseFoundingYear + kFoundingSpread;
	return ruin;
}

// Guarantee the invariants SelectStartingVillage depends on: >= kMinLivingTowns living towns, and a
// ruin within explorationRange of the central living town (the natural start-village candidate).
void GuaranteeStartViability(
	std::vector<TownSite>& towns,
	const Grid& grid,
	const WorldGenParams& params,
	Rng& rng,
	std::vector<HistoryEvent>& history,
	RoadSet& roads
) {
	const glm::ivec2 centre = GridCentre(grid);

	// Ensure at least one living town (revive the largest ruin if the sim wiped everyone out).
	if (CountLiving(towns) == 0 && !towns.empty()) {
		auto largest = std::ranges::max_element(towns, {}, &TownSite::size);
		largest->status = TownStatus::Living;
		largest->collapseYear = 0;
	}

	// Keep enough living towns for a connected road network (revive ruins farthest from the centre).
	while (CountLiving(towns) < kMinLivingTowns + 1) {
		TownSite* revive = nullptr;
		int farthest = -1;
		for (TownSite& t : towns) {
			if (t.status != TownStatus::Ruined) {
				continue;
			}
			const int d = SquaredDistance(t.location, centre);
			if (d > farthest) {
				farthest = d;
				revive = &t;
			}
		}
		if (revive == nullptr) {
			break; // no ruins left to revive
		}
		revive->status = TownStatus::Living;
		revive->collapseYear = 0;
		revive->size = std::max(revive->size, kInitialPopMin);
	}

	const int candidateId = FindCentralLivingTown(towns, centre);
	if (candidateId == 0) {
		return;
	}
	const glm::ivec2 candidatePos = towns[static_cast<size_t>(candidateId - 1)].location;
	const int rangeSq = params.explorationRange * params.explorationRange;

	const bool ruinInRange = std::ranges::any_of(towns, [&](const TownSite& t) {
		return t.status == TownStatus::Ruined && SquaredDistance(t.location, candidatePos) <= rangeSq;
	});
	if (ruinInRange) {
		return;
	}

	// Convert the smallest living town within range (other than the candidate) into a ruin…
	TownSite* victim = nullptr;
	for (TownSite& t : towns) {
		if (t.id == candidateId || t.status != TownStatus::Living) {
			continue;
		}
		if (SquaredDistance(t.location, candidatePos) > rangeSq) {
			continue;
		}
		if (victim == nullptr || t.size < victim->size) {
			victim = &t;
		}
	}
	if (victim != nullptr && CountLiving(towns) > kMinLivingTowns) {
		victim->status = TownStatus::Ruined;
		victim->collapseYear = kGameStartYear - 250; // shortly before the player's era
		victim->size = 0;
		const int victimId = victim->id;
		history.push_back(
			{.year = victim->collapseYear, .type = HistoryEventType::TownCollapsed, .a = victimId, .b = 0, .description = victim->name + " collapsed into ruin"}
		);
		SeverRoads(roads, towns, victimId, kGameStartYear - 250, history);
		return;
	}

	// …otherwise drop a fresh ruined settlement near the candidate.
	const int newId = static_cast<int>(towns.size()) + 1;
	TownSite ruin = MakeRuinNear(grid, params, candidatePos, newId, rng);
	history.push_back(
		{.year = ruin.collapseYear, .type = HistoryEventType::TownCollapsed, .a = ruin.id, .b = 0, .description = ruin.name + " collapsed into ruin"}
	);
	towns.push_back(ruin);
}

// Disconnected clusters of towns are a natural, desirable outcome, so we do NOT force the whole
// network to be connected. We only guarantee the player's start village has at least one road (so
// SelectStartingVillage can pick it): if the central living town is isolated, link it to its nearest
// living neighbour at the start year.
void EnsureStartVillageConnected(
	const std::vector<TownSite>& towns,
	RoadSet& roads,
	std::vector<HistoryEvent>& history,
	const glm::ivec2 centre
) {
	const std::vector<int> living = LivingIds(towns);
	if (living.size() < 2) {
		return;
	}
	const int candidate = FindCentralLivingTown(towns, centre);
	if (candidate == 0 || TownDegree(roads, candidate) > 0) {
		return;
	}
	int nearest = 0;
	int bestDist = std::numeric_limits<int>::max();
	for (const int id : living) {
		if (id == candidate) {
			continue;
		}
		const int d = SquaredDistance(TownById(towns, candidate).location, TownById(towns, id).location);
		if (d < bestDist) {
			bestDist = d;
			nearest = id;
		}
	}
	if (nearest != 0) {
		roads.insert(NormalizedRoad(candidate, nearest));
		history.push_back(
			{.year = kGameStartYear,
			 .type = HistoryEventType::RoadBuilt,
			 .a = candidate,
			 .b = nearest,
			 .description = TownById(towns, candidate).name + " connected to " + TownById(towns, nearest).name}
		);
	}
}

// ── Timeline ──────────────────────────────────────────────────────────────────────────────────────
// A single dated timeline step before it is linearized. Landmass is the undated base (always first);
// every other step — water, rivers, foundings, collapses, roads — carries a year and flows through
// one chronological sort. Water/river formation is therefore not special-cased to "step 2/3": a
// future river-diversion or lake-creation event simply pushes another dated step at any year.
struct DatedStep {
	int year{0};
	int priority{0}; // tiebreak within the same year
	WorldSnapshot snapshot;
};

GenerationTimeline BuildTimeline(const WorldData& world) {
	std::vector<DatedStep> dated;

	WorldSnapshot water;
	water.kind = SnapshotKind::WaterBodies;
	water.label = "Water bodies settle (" + std::to_string(world.waterBodies.size()) + ")";
	for (const WaterBody& b : world.waterBodies) {
		water.addedWaterBodyIds.push_back(b.id);
	}
	dated.push_back({.year = kWaterFormedYear, .priority = 0, .snapshot = std::move(water)});

	WorldSnapshot rivers;
	rivers.kind = SnapshotKind::Rivers;
	rivers.label = "Rivers carve to the sea (" + std::to_string(world.rivers.size()) + ")";
	for (const River& r : world.rivers) {
		rivers.addedRiverIds.push_back(r.id);
	}
	dated.push_back({.year = kRiversFormedYear, .priority = 0, .snapshot = std::move(rivers)});

	// Collapses + roads from the history log.
	for (const HistoryEvent& ev : world.history) {
		WorldSnapshot step;
		step.label = "Year " + std::to_string(ev.year) + ": " + ev.description;
		switch (ev.type) {
		case HistoryEventType::TownCollapsed:
			step.kind = SnapshotKind::TownCollapsed;
			step.addedTownIds.push_back(ev.a);
			dated.push_back({.year = ev.year, .priority = 2, .snapshot = std::move(step)});
			break;
		case HistoryEventType::RoadBuilt:
			step.kind = SnapshotKind::RoadBuilt;
			step.addedRoads.emplace_back(ev.a, ev.b);
			dated.push_back({.year = ev.year, .priority = 3, .snapshot = std::move(step)});
			break;
		case HistoryEventType::BridgeBuilt:
			step.kind = SnapshotKind::BridgeBuilt;
			step.addedRoads.emplace_back(ev.a, ev.b);
			dated.push_back({.year = ev.year, .priority = 3, .snapshot = std::move(step)});
			break;
		case HistoryEventType::RoadRemoved:
			step.kind = SnapshotKind::RoadRemoved;
			step.removedRoads.emplace_back(ev.a, ev.b);
			dated.push_back({.year = ev.year, .priority = 4, .snapshot = std::move(step)});
			break;
		case HistoryEventType::TownFounded:
			step.kind = SnapshotKind::TownFounded;
			step.addedTownIds.push_back(ev.a);
			dated.push_back({.year = ev.year, .priority = 1, .snapshot = std::move(step)});
			break;
		}
	}

	std::ranges::stable_sort(dated, [](const DatedStep& a, const DatedStep& b) {
		if (a.year != b.year) {
			return a.year < b.year;
		}
		return a.priority < b.priority;
	});

	GenerationTimeline timeline;
	WorldSnapshot landmass;
	landmass.kind = SnapshotKind::Landmass;
	landmass.label = "Landmass takes shape";
	timeline.steps.push_back(std::move(landmass));
	for (DatedStep& step : dated) {
		timeline.steps.push_back(std::move(step.snapshot));
	}
	return timeline;
}

// ── Stage: forest trees (ticket 05) ───────────────────────────────────────────────────────────
// Make woodland enterable. The forest/deep-forest terrain tile is walkable ground (tileset); this
// stage scatters impassable tree world-objects across it from a SECONDARY noise density field —
// decorrelated from elevation — so forests vary spatially: high field values form dense cores, the
// mid band thins toward sparse edges, and sub-threshold regions stay as contiguous un-treed
// clearings. Deep forest (Biome::Hills) is denser and draws taller/darker species. Road passages
// (matching the tilemap builder's 3×3 painted trail) and town footprints are kept clear so travel
// through the woods stays possible. Species + per-instance height come from the data-driven
// TreeSpeciesCatalog, so adding a species needs no C++ change. Appended LAST in the pipeline so the
// existing RNG stream is unchanged; every choice draws from the single pipeline Rng in a fixed order.

// Decorrelates the tree density field's Perlin permutation from the elevation field.
constexpr std::uint32_t kTreeNoiseSeedSalt = 0x9e3779b9u;

// Density floor at the clearing edge: a woodland tile just above the clearing threshold still places
// trees at this fraction of the peak density, so forest edges thin out toward clearings rather than
// snapping from empty to full. The core threshold ramps this up to the full peak.
constexpr float kEdgeDensityFloor = 0.5f;

// Catalog biome key for a woodland tile, or empty for non-woodland biomes.
std::string_view WoodlandBiomeKey(const Biome b) {
	if (b == Biome::Forest) {
		return "forest";
	}
	if (b == Biome::Hills) {
		return "deep_forest";
	}
	return {};
}

std::vector<WorldObject> ScatterForestTrees(
	const Grid& grid,
	const WorldGenParams& params,
	const std::vector<TownSite>& towns,
	const std::vector<TownLayout>& townLayouts,
	const RoadGraph& roads,
	const TreeSpeciesCatalog& catalog,
	const int seed,
	Rng& rng
) {
	std::vector<WorldObject> trees;
	const int w = grid.width;
	const int h = grid.height;
	if (w <= 0 || h <= 0) {
		return trees;
	}

	// Tiles kept clear of trees during scatter: road passages (3×3) and a coarse town-radius box. This
	// gating is kept identical to the original so the (seed, params)-deterministic density field is
	// unchanged. The exact town layout (which can spread past the box) is handled by a post-filter
	// below that removes any tree sitting on real town ground/street/wall tiles.
	std::vector<std::uint8_t> clear(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
	std::vector<std::uint8_t> townTile(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
	{
		engine::profiling::Zone _sub{"WorldGen/ForestClearanceMap"};
		const auto markRect = [&](const int x0, const int y0, const int x1, const int y1) {
			for (int y = std::max(0, y0); y <= std::min(h - 1, y1); ++y) {
				for (int x = std::max(0, x0); x <= std::min(w - 1, x1); ++x) {
					clear[static_cast<size_t>(y) * w + x] = 1;
				}
			}
		};
		for (const RoadSegment& seg : roads.segments) {
			for (const glm::ivec2& pt : seg.path) {
				markRect(pt.x - 1, pt.y - 1, pt.x + 1, pt.y + 1);
			}
		}
		for (const TownSite& town : towns) {
			const int radius = std::clamp(std::max(1, town.size / 4), 1, 30);
			markRect(town.location.x - radius, town.location.y - radius, town.location.x + radius, town.location.y + radius);
		}
		// Precise town footprint (floor plots, streets, fields, building walls) for the post-filter.
		const auto markTile = [&](const glm::ivec2 t) {
			if (t.x >= 0 && t.y >= 0 && t.x < w && t.y < h) {
				townTile[static_cast<size_t>(t.y) * w + t.x] = 1;
			}
		};
		for (const TownLayout& layout : townLayouts) {
			for (const glm::ivec2 t : layout.floorTiles) markTile(t);
			for (const glm::ivec2 t : layout.pathTiles) markTile(t);
			for (const glm::ivec2 t : layout.fieldTiles) markTile(t);
			for (const TownBuilding& b : layout.buildings) {
				for (const glm::ivec2 t : b.footprint) markTile(t);
			}
		}
	}

	// Secondary density field in [0,1], contrast-stretched by GenerateHeightMap. The offset draws
	// from the pipeline Rng (fixed order) so the field is seed-reproducible.
	engine::noise::NoiseSampler sampler(
		std::make_unique<engine::noise::builtin::PerlinGenerator>(
			static_cast<int>(static_cast<std::uint32_t>(seed) ^ kTreeNoiseSeedSalt)
		)
	);
	const glm::vec2 offset{rng.Float(0.0f, 1000.0f), rng.Float(0.0f, 1000.0f)};
	std::vector<float> density;
	{
		engine::profiling::Zone _sub{"WorldGen/ForestNoiseField"};
		// 3 octaves: density modulation only needs large-to-medium scale variation; fine detail
		// at octave 4-6 adds cost without meaningfully changing where clearings occur.
		const engine::noise::FractalNoiseParams treeFractal{
			.octaves = 3,
			.persistence = params.fractal.persistence,
			.lacunarity = params.fractal.lacunarity,
		};
		density =
			engine::noise::GenerateHeightMap(sampler, w, h, offset, params.treeDensityNoiseScale, treeFractal);
	}

	// Tracks tiles that have already received a tree, used for the 4-connected adjacency check.
	std::vector<std::uint8_t> placed(static_cast<size_t>(w) * static_cast<size_t>(h), 0);

	{
		engine::profiling::Zone _sub{"WorldGen/ForestScatterLoop"};
		for (int y = 0; y < h; ++y) {
			for (int x = 0; x < w; ++x) {
				const Biome biome = grid.BiomeAt(x, y);
				const std::string_view key = WoodlandBiomeKey(biome);
				if (key.empty()) {
					continue;
				}
				const size_t idx = static_cast<size_t>(y) * w + x;
				if (clear[idx] != 0) {
					continue; // road passage / town footprint
				}
				const std::vector<TreeSpecies>* species = catalog.SpeciesForBiome(key);
				if (species == nullptr) {
					continue; // no species defined for this woodland — leave it open
				}
				// Deep forest uses a lower clearing threshold + higher peak density, so it is reliably
				// denser than regular forest (fewer, smaller clearings) regardless of the local field.
				const bool deep = biome == Biome::Hills;
				const float clearThreshold = deep ? params.deepForestClearingThreshold : params.forestClearingThreshold;
				const float d = density[idx];
				if (d < clearThreshold) {
					continue; // clearing: contiguous un-treed ground (no Rng drawn)
				}
				// Ramp from sparse edges (clearing threshold) to the peak density at the core threshold.
				const float span = std::max(1e-4f, params.forestCoreThreshold - clearThreshold);
				const float t = std::clamp((d - clearThreshold) / span, 0.0f, 1.0f);
				const float peak = deep ? params.deepForestTreeDensity : params.forestTreeDensity;
				const float probability = peak * (kEdgeDensityFloor + (1.0f - kEdgeDensityFloor) * t);
				if (rng.Float(0.0f, 1.0f) >= probability) {
					continue; // open woodland-floor tile between the trees
				}

				// 4-connected adjacency constraint: never place a tree immediately N/S/E/W of an
				// existing tree. Diagonal neighbors are permitted. The probability RNG is consumed
				// above regardless so the stream shifts only for tiles that pass the probability gate
				// but fail adjacency — the output is still fully deterministic from (seed, params).
				const bool adjacentOccupied =
					(x > 0     && placed[static_cast<size_t>(y) * w + (x - 1)] != 0) ||
					(x < w - 1 && placed[static_cast<size_t>(y) * w + (x + 1)] != 0) ||
					(y > 0     && placed[static_cast<size_t>(y - 1) * w + x] != 0) ||
					(y < h - 1 && placed[static_cast<size_t>(y + 1) * w + x] != 0);
				if (adjacentOccupied) {
					continue;
				}

				const TreeSpecies& chosen =
					(*species)[static_cast<size_t>(rng.Int(0, static_cast<int>(species->size()) - 1))];
				WorldObject tree;
				tree.footprint.push_back(glm::ivec2{x, y});
				tree.kind = chosen.kind;
				tree.height = rng.Float(chosen.minHeight, chosen.maxHeight);
				tree.blocksMovement = true;
				placed[idx] = 1;
				trees.push_back(std::move(tree));
			}
		}
	}

	// Post-filter: drop any tree that landed on a precise town tile (ground/street/field/wall). Done
	// after scatter so it never perturbs the deterministic rng stream — a spread-out village simply
	// clears the trees it actually occupies, carving a clean settlement out of the woodland.
	std::erase_if(trees, [&](const WorldObject& t) {
		const glm::ivec2 p = t.footprint.front();
		return townTile[static_cast<size_t>(p.y) * w + p.x] != 0;
	});

	return trees;
}

// ── Stage: town-layout world-objects ──────────────────────────────────────────────────────────
// Replaces the old solid grey town square with a generated village layout per town (see
// town_layout.cpp): a central landmark, population-scaled buildings along short streets, decorative
// field plots near farm buildings, and walkable floor/door terrain. Each town is laid out from a
// seed derived from (worldSeed, townId), so it is deterministic and independent of the pipeline RNG
// stream — appended after every existing stage so no existing golden behaviour shifts.
void PlaceTownLayouts(WorldData& world, const BuildingCatalog& catalog) {
	for (const TownSite& town : world.towns) {
		TownLayoutResult result =
			BuildTownLayout(town, world.grid, world.params, catalog, DeriveTownSeed(world.seed, town.id));
		for (WorldObject& obj : result.objects) {
			world.objects.push_back(std::move(obj));
		}
		world.townLayouts.push_back(std::move(result.layout));
	}
}

// ── Stage: crossroads-outpost post-pass (ticket 07) ───────────────────────────────────────────
// After routing, a hub that accumulated very high road convergence yet has no living town nearby is
// promoted into a young living "crossroads outpost" — a settlement that grew where the roads meet.
// Each promoted outpost becomes a first-class living TownSite (preserving the 1-based contiguous id
// invariant), gains a generated village layout like any other town, and records a TownFounded history
// event so it flows into the timeline. Ticket 09 populates it automatically because it iterates living
// towns. Deterministic: hub selection is pure and the outpost name/layout derive from (seed, townId),
// independent of the pipeline RNG stream.
void PromoteCrossroadsOutposts(WorldData& world, const std::vector<RoadHub>& hubs, const BuildingCatalog& catalog) {
	const std::vector<int> promote = SelectCrossroadsOutposts(hubs, world.roads, world.towns, world.params);
	for (const int hubIndex : promote) {
		const RoadHub& hub = hubs[static_cast<size_t>(hubIndex)];

		TownSite outpost;
		outpost.id = static_cast<int>(world.towns.size()) + 1;
		Rng nameRng(static_cast<int>(DeriveTownSeed(world.seed, outpost.id)));
		outpost.name = MakeTownName(nameRng);
		outpost.location = hub.location;
		outpost.size = std::max(1, world.params.outpostPopulation);
		outpost.status = TownStatus::Living;
		outpost.foundingYear = kGameStartYear; // young: it grew only recently, where the roads already met
		world.towns.push_back(outpost);

		world.history.push_back(
			{.year = kGameStartYear,
			 .type = HistoryEventType::TownFounded,
			 .a = outpost.id,
			 .b = 0,
			 .description = "crossroads outpost " + outpost.name + " grew where the roads meet"}
		);

		TownLayoutResult result =
			BuildTownLayout(outpost, world.grid, world.params, catalog, DeriveTownSeed(world.seed, outpost.id));
		for (WorldObject& obj : result.objects) {
			world.objects.push_back(std::move(obj));
		}
		world.townLayouts.push_back(std::move(result.layout));
	}
}

} // namespace

const TownSite* WorldData::FindTown(const int id) const {
	for (const TownSite& t : towns) {
		if (t.id == id) {
			return &t;
		}
	}
	return nullptr;
}

WorldData WorldGen::Generate(const WorldGenParams& params, const int seed) {
	return Generate(params, seed, DefaultBuildingCatalog(), DefaultTreeSpeciesCatalog());
}

WorldData WorldGen::Generate(const WorldGenParams& params, const int seed, const BuildingCatalog& buildings) {
	return Generate(params, seed, buildings, DefaultTreeSpeciesCatalog());
}

WorldData WorldGen::Generate(
	const WorldGenParams& params,
	const int seed,
	const BuildingCatalog& buildings,
	const TreeSpeciesCatalog& treeCatalog
) {
	return Generate(params, seed, buildings, treeCatalog, DefaultCreatureCatalog());
}

WorldData WorldGen::Generate(
	const WorldGenParams& params,
	const int seed,
	const BuildingCatalog& buildings,
	const TreeSpeciesCatalog& treeCatalog,
	const CreatureCatalog& creatureCatalog
) {
	Rng rng(seed);

	WorldData world;
	world.params = params;
	world.seed = seed;

	{
		engine::profiling::Zone _zone{"WorldGen/Elevation"};
		{
			engine::profiling::Zone _sub{"WorldGen/ElevationNoise"};
			world.grid = BuildElevation(params, seed, rng);
		}
		{
			engine::profiling::Zone _sub{"WorldGen/InteriorWaterGuarantee"};
			GuaranteeInteriorWater(world.grid, params);
		}
		{
			engine::profiling::Zone _sub{"WorldGen/BiomeAssignment"};
			AssignBiomes(world.grid, params);
		}
	}

	WaterResult water;
	{
		engine::profiling::Zone _zone{"WorldGen/WaterClassification"};
		water = ClassifyWater(world.grid, params);
		world.waterBodies = std::move(water.bodies);
		NameWaterBodies(world.waterBodies, rng);
	}

	{
		engine::profiling::Zone _zone{"WorldGen/RiverTracing"};
		std::vector<int> distToWater;
		{
			engine::profiling::Zone _sub{"WorldGen/DistanceToWater"};
			distToWater = DistanceToWater(world.grid, water.label);
		}
		{
			engine::profiling::Zone _sub{"WorldGen/RiverTrace"};
			world.rivers = TraceRivers(world.grid, params, water.label, distToWater, rng);
		}
	}

	// Organic, event-driven history: found towns, build/sever roads, and collapse towns over time.
	RoadSet roads;
	{
		engine::profiling::Zone _zone{"WorldGen/HistorySimulation"};
		std::vector<TownCandidate> pool = PlaceTownCandidates(world.grid, params, rng);
		SimulateHistory(pool, world.towns, params, rng, world.history, roads);
		GuaranteeStartViability(world.towns, world.grid, params, rng, world.history, roads);
		// Disconnected town clusters are fine; only ensure the start village itself has a road.
		EnsureStartVillageConnected(world.towns, roads, world.history, GridCentre(world.grid));
	}

	{
		engine::profiling::Zone _zone{"WorldGen/TownLayoutGeneration"};
		{
			engine::profiling::Zone _sub{"WorldGen/TownLayouts"};
			// Town-layout world-object layer (ticket 04): a generated village per town. Deterministic from
			// (seed, townId), so it does not depend on the pipeline RNG stream above. Laid out BEFORE the
			// road router so the router can route around each town's footprint (ticket 06).
			PlaceTownLayouts(world, buildings);
		}

		// Road hubs + town attractors (ticket 07): place sparse hub nodes at geographic funnel points
		// central to multiple towns, then build the per-tile SOFT cost-discount field the router reads so
		// nearby roads tend to converge through hubs and toward large, well-connected towns ("all roads
		// lead to Rome"). Pure + deterministic; independent of the pipeline RNG stream.
		std::vector<RoadHub> hubs;
		std::vector<float> routeDiscount;
		{
			engine::profiling::Zone _sub{"WorldGen/RoadHubPlacement"};
			hubs = PlaceRoadHubs(world.grid, world.towns, world.params);
			routeDiscount = BuildRouteCostField(world.grid, world.towns, hubs, roads, world.params);
		}

		{
			engine::profiling::Zone _sub{"WorldGen/RoadRouting"};
			// Spatial road router (ticket 06): realise the history-decided connection set into terrain-
			// following least-cost paths that avoid mountains/deep water, skirt town footprints, reuse
			// existing trunks, and drop redundant links. The ticket-07 attractor field bends routes toward
			// hubs/towns without ever forcing a waypoint. Replaces the old straight-line Bresenham roads.
			world.roads = BuildRoadNetwork(world.grid, world.towns, world.townLayouts, world.params, roads, routeDiscount);
		}

		{
			engine::profiling::Zone _sub{"WorldGen/HubMarkersAndOutposts"};
			// Hub meeting-place markers (ticket 07): a non-blocking signpost where a realised road actually
			// passes a hub, so the convergence reads as a crossroads.
			for (WorldObject& marker : BuildHubMarkers(hubs, world.roads, world.params)) {
				world.objects.push_back(std::move(marker));
			}
			// Crossroads-outpost post-pass (ticket 07): promote high-convergence wilderness hubs into young
			// living outpost towns (in world.towns AND the timeline). Runs after routing so convergence is
			// measured on the realised network.
			PromoteCrossroadsOutposts(world, hubs, buildings);
		}

		{
			engine::profiling::Zone _sub{"WorldGen/Bridges"};
			// Bridges (ticket 08): the router blocks deep water, so a wanted connection whose towns sit on
			// different landmasses got no road. Those water-separated links trigger a bridge across the
			// narrowest crossing (within maxBridgeSpan); a prosperity-gated BridgeBuilt history event marks
			// the deliberate ones. Each bridge is a walkable deck (a non-blocking world-object) over water.
			world.bridges = BuildBridges(world.grid, world.towns, world.params, roads, world.history);
			for (const Bridge& bridge : world.bridges) {
				WorldObject deck;
				deck.footprint = bridge.deck;
				deck.kind = "bridge";
				deck.height = kBridgeDeckHeight;
				deck.blocksMovement = false; // the deck MAKES water walkable; the tilemap builder paves it
				world.objects.push_back(std::move(deck));
			}
		}
	}

	{
		engine::profiling::Zone _zone{"WorldGen/ForestTreeScatter"};
		// Forest-tree world-object layer (ticket 02 / 05): blocking trees over walkable woodland.
		// Appended AFTER the road router so trees avoid the realised road passages + town footprints, and
		// drawn last from the pipeline Rng so the existing stages' stream is unchanged.
		std::vector<WorldObject> forestTrees =
			ScatterForestTrees(world.grid, params, world.towns, world.townLayouts, world.roads, treeCatalog, seed, rng);
		for (WorldObject& tree : forestTrees) {
			world.objects.push_back(std::move(tree));
		}
	}

	{
		engine::profiling::Zone _zone{"WorldGen/CreaturePlacement"};
		// Ambient-creature layer (ticket 10): sparse livestock near towns + wildlife in the open.
		// Deterministic from (seed, creatureCatalog); seeded from the world seed independently of the
		// pipeline RNG stream. Runs after town layouts + forest trees so blocking tiles are all known.
		PlaceCreatures(world, creatureCatalog);
	}

	world.timeline = BuildTimeline(world);
	return world;
}

} // namespace game::farm
