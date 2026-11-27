#include "town_layout.hpp"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <deque>
#include <map>
#include <random>
#include <set>
#include <utility>

#include "seeded_random.hpp"
#include "town_style.hpp"

namespace game::farm {
namespace {

constexpr const char* kWellKind = "well";     // living-town central landmark
constexpr const char* kRubbleKind = "rubble"; // ruined-town buildings + landmark
constexpr float kWellHeight = 2.0f;
constexpr float kRubbleHeightScale = 0.4f; // rubble is a low, broken remnant
constexpr int kRuinedBuildingMin = 2;      // a ruin is still a scatter of several rubble piles
constexpr int kRuinedBuildingMax = 4;
constexpr int kMaxBuildings = 96; // hard safety bound so a pathological catalog can't loop forever
constexpr int kPlotJitter = 1;    // ± tiles a plot centre is nudged so the village is not a rigid grid

// Local deterministic RNG for one town's layout, seeded independently of the pipeline RNG.
struct LayoutRng {
	std::mt19937 engine;
	explicit LayoutRng(const std::uint32_t s) : engine(s) {}
	int Int(const int lo, const int hi) { return UniformInt(engine, lo, hi); }
};

// A building is buildable only on walkable land. Water and mountains are skipped so a spread-out
// village never drops a house into a lake or onto a peak; the town simply places what fits.
bool IsBuildable(const Grid& grid, const int x, const int y) {
	if (!grid.InBounds(x, y)) {
		return false;
	}
	switch (grid.BiomeAt(x, y)) {
	case Biome::Ocean:
	case Biome::Lake:
	case Biome::River:
	case Biome::Mountain: return false;
	default: return true;
	}
}

// Weighted-random pick among the residential building kinds. Deterministic in `rng`.
const BuildingDef* PickResidential(const std::vector<const BuildingDef*>& residential, LayoutRng& rng) {
	if (residential.empty()) {
		return nullptr;
	}
	int total = 0;
	for (const BuildingDef* d : residential) {
		total += std::max(1, d->weight);
	}
	int roll = rng.Int(0, total - 1);
	for (const BuildingDef* d : residential) {
		roll -= std::max(1, d->weight);
		if (roll < 0) {
			return d;
		}
	}
	return residential.back();
}

// Plan the ordered list of building defs a living town should contain: population-scaled housing
// first (residential kinds drawn until everyone is housed), then the tier-gated communal buildings.
std::vector<const BuildingDef*>
PlanBuildings(const BuildingCatalog& catalog, const TownTier tier, const int population, LayoutRng& rng) {
	std::vector<const BuildingDef*> residential;
	std::vector<const BuildingDef*> communal;
	for (const BuildingDef& d : catalog.defs) {
		(d.IsResidential() ? residential : communal).push_back(&d);
	}

	std::vector<const BuildingDef*> plan;

	// Housing: keep drawing weighted residential kinds until the population is sheltered. Each home
	// covers `occupancy` residents, so the count rises with population without a fixed cap.
	int remaining = std::max(1, population);
	while (remaining > 0 && static_cast<int>(plan.size()) < kMaxBuildings) {
		const BuildingDef* home = PickResidential(residential, rng);
		if (home == nullptr) {
			break; // no residential kinds in this catalog
		}
		plan.push_back(home);
		remaining -= std::max(1, home->occupancy);
	}

	// Communal buildings: gated by tier, count 1 unless `perCapita` scales it up to `maxCount`.
	for (const BuildingDef* d : communal) {
		if (static_cast<int>(tier) < d->minTier) {
			continue;
		}
		int count = 1;
		if (d->perCapita > 0.0f) {
			count = static_cast<int>(std::lround(static_cast<double>(population) * d->perCapita));
		}
		count = std::clamp(count, 1, std::max(1, d->maxCount));
		for (int i = 0; i < count && static_cast<int>(plan.size()) < kMaxBuildings; ++i) {
			plan.push_back(d);
		}
	}

	return plan;
}

// Deterministic spiral of grid-cell offsets around (0,0), excluding the centre, ordered by Chebyshev
// ring then row-major within the ring. Buildings fill these plots outward from the plaza.
std::vector<glm::ivec2> PlotOffsets(const int needed) {
	std::vector<glm::ivec2> offsets;
	for (int r = 1; static_cast<int>(offsets.size()) < needed && r <= 64; ++r) {
		for (int dy = -r; dy <= r; ++dy) {
			for (int dx = -r; dx <= r; ++dx) {
				if (std::max(std::abs(dx), std::abs(dy)) == r) {
					offsets.push_back({dx, dy});
				}
			}
		}
	}
	return offsets;
}

using ClaimSet = std::set<std::pair<int, int>>;

bool Claimed(const ClaimSet& set, const glm::ivec2 t) { return set.contains({t.x, t.y}); }
void Claim(ClaimSet& set, const glm::ivec2 t) { set.insert({t.x, t.y}); }

// Lay out one living town: plaza, well, spread housing + communal buildings, a cardinal street spine
// with door branches, and the walkable floor/field tiles.
TownLayoutResult BuildLivingTown(
	const TownSite& town,
	const Grid& grid,
	const WorldGenParams& params,
	const BuildingCatalog& catalog,
	LayoutRng& rng
) {
	TownLayoutResult result;
	TownLayout& layout = result.layout;
	layout.townId = town.id;
	layout.ruined = false;

	const glm::ivec2 centre = town.location;
	ClaimSet claimed;      // building/landmark tiles already occupied, so plots never overlap
	ClaimSet doorReserved; // south entrance tiles kept clear so no later stamp blocks an entrance

	const auto addFloor = [&](const glm::ivec2 t) {
		if (IsBuildable(grid, t.x, t.y)) {
			layout.floorTiles.push_back(t);
		}
	};

	// ── Plaza + central well ──
	const int plaza = std::max(0, params.townPlazaRadius);
	for (int dy = -plaza; dy <= plaza; ++dy) {
		for (int dx = -plaza; dx <= plaza; ++dx) {
			addFloor({centre.x + dx, centre.y + dy});
		}
	}

	const glm::ivec2 landmarkTile{centre.x, centre.y - 1};
	layout.landmarkKind = kWellKind;
	layout.landmark = landmarkTile;
	if (grid.InBounds(landmarkTile.x, landmarkTile.y)) {
		WorldObject landmark;
		landmark.footprint.push_back(landmarkTile);
		landmark.kind = kWellKind;
		landmark.height = kWellHeight;
		landmark.blocksMovement = true;
		result.objects.push_back(std::move(landmark));
		Claim(claimed, landmarkTile);
	}

	if (catalog.defs.empty()) {
		return result;
	}

	// ── Plan + place buildings on a jittered spiral of plots ──
	const TownTier tier = TierForPopulation(town.size);
	const std::vector<const BuildingDef*> plan = PlanBuildings(catalog, tier, town.size, rng);

	const int maxDim = catalog.MaxFootprintDim();
	const int stride = maxDim + std::max(1, params.townPlotGap) + 1;
	const std::vector<glm::ivec2> offsets = PlotOffsets(static_cast<int>(plan.size()) * 6 + 24);

	int maxRing = 0;           // furthest placed ring, used to size the street spine
	std::size_t offsetIdx = 0; // next spiral plot to try

	for (const BuildingDef* def : plan) {
		const int bw = std::max(2, def->width);
		const int bh = std::max(2, def->length);

		bool placed = false;
		while (offsetIdx < offsets.size() && !placed) {
			const glm::ivec2 off = offsets[offsetIdx++];
			const int jx = rng.Int(-kPlotJitter, kPlotJitter);
			const int jy = rng.Int(-kPlotJitter, kPlotJitter);
			const glm::ivec2 plotCentre{centre.x + off.x * stride + jx, centre.y + off.y * stride + jy};
			const int ox = plotCentre.x - bw / 2;
			const int oy = plotCentre.y - bh / 2;

			// The plot fits only if every tile (and a 1-tile border) is buildable, unclaimed, and not a
			// reserved entrance of an already-placed building.
			bool fits = true;
			for (int y = oy - 1; y <= oy + bh && fits; ++y) {
				for (int x = ox - 1; x <= ox + bw && fits; ++x) {
					if (!IsBuildable(grid, x, y) || Claimed(claimed, {x, y}) || Claimed(doorReserved, {x, y})) {
						fits = false;
					}
				}
			}
			if (!fits) {
				continue;
			}

			// Solid building "stamp": the whole footprint is one blocking object — buildings are not
			// enterable (no walkable interior/door gap). The entrance is the tile just south of the
			// footprint (the player-facing front), where villagers stand and the path begins.
			std::vector<glm::ivec2> footprint;
			footprint.reserve(static_cast<size_t>(bw) * static_cast<size_t>(bh));
			for (int y = oy; y < oy + bh; ++y) {
				for (int x = ox; x < ox + bw; ++x) {
					footprint.push_back({x, y});
					Claim(claimed, {x, y});
				}
			}
			const glm::ivec2 door{ox + bw / 2, oy + bh}; // one tile south of the south edge
			Claim(doorReserved, door);

			WorldObject stampObj;
			stampObj.footprint = footprint;
			stampObj.kind = def->kind;
			stampObj.height = def->wallHeight;
			stampObj.blocksMovement = true;
			result.objects.push_back(std::move(stampObj));

			TownBuilding building;
			building.kind = def->kind;
			building.villagerArchetype = def->villagerArchetype;
			building.footprint = std::move(footprint);
			building.door = door;
			building.ruined = false;
			building.isHome = def->IsResidential();
			layout.buildings.push_back(std::move(building));

			// Pave the plot — a generous yard around the building (margin 2) so neighbouring plots and
			// the streets knit into one continuous village lot rather than islands of ground.
			for (int y = oy - 2; y <= oy + bh + 1; ++y) {
				for (int x = ox - 2; x <= ox + bw + 1; ++x) {
					addFloor({x, y});
				}
			}

			// Decorative field plots beside farm buildings (visual only; walkable tilled soil).
			if (def->farm) {
				const glm::ivec2 away = (std::abs(plotCentre.x - centre.x) >= std::abs(plotCentre.y - centre.y))
					? glm::ivec2{plotCentre.x >= centre.x ? 1 : -1, 0}
					: glm::ivec2{0, plotCentre.y >= centre.y ? 1 : -1};
				const glm::ivec2 perp{away.y, away.x};
				const glm::ivec2 base{ox + bw / 2 + away.x * (bw / 2 + 1), oy + bh / 2 + away.y * (bh / 2 + 1)};
				for (int a = 0; a < 2; ++a) {
					for (int b = -1; b <= 1; ++b) {
						const glm::ivec2 f{base.x + away.x * a + perp.x * b, base.y + away.y * a + perp.y * b};
						if (IsBuildable(grid, f.x, f.y) && !Claimed(claimed, f)) {
							layout.fieldTiles.push_back(f);
						}
					}
				}
			}

			maxRing = std::max(maxRing, std::max(std::abs(off.x), std::abs(off.y)));
			placed = true;
		}
	}

	// ── Street network: a 2-wide cardinal spine from the plaza, then 1-wide orthogonal branches from
	// every door to the nearest spine tile. 4-connected throughout (no diagonal-only links). ──
	ClaimSet pathSet;
	const int reach = std::max(2, maxRing * stride + maxDim);
	const std::pair<glm::ivec2, glm::ivec2> arms[] = {
		{{1, 0}, {0, 1}}, {{-1, 0}, {0, 1}}, {{0, 1}, {1, 0}}, {{0, -1}, {1, 0}}};
	for (const auto& [dir, perp] : arms) {
		for (int d = 1; d <= reach; ++d) {
			for (int w = 0; w < 2; ++w) { // 2-wide
				const glm::ivec2 t{centre.x + dir.x * d + perp.x * w, centre.y + dir.y * d + perp.y * w};
				if (IsBuildable(grid, t.x, t.y) && !Claimed(claimed, t)) {
					Claim(pathSet, t);
				}
			}
		}
	}

	// Connect each building's south entrance to the road network. A breadth-first search over open
	// village ground (never through a solid building) finds the shortest route from the entrance to an
	// existing street, so a path winds around the stamps. A building with no clear route within the
	// town neighbourhood stays an unconnected stub — outlying houses need not reach the roads.
	const int bfsRadius = reach + stride + 2;
	for (const TownBuilding& b : layout.buildings) {
		const glm::ivec2 start = b.door;
		if (!IsBuildable(grid, start.x, start.y) || Claimed(claimed, start)) {
			continue;
		}

		ClaimSet visited;
		std::map<std::pair<int, int>, std::pair<int, int>> parent;
		std::deque<glm::ivec2> frontier;
		frontier.push_back(start);
		Claim(visited, start);
		parent[{start.x, start.y}] = {start.x, start.y};

		bool connected = false;
		glm::ivec2 hit = start;
		while (!frontier.empty()) {
			const glm::ivec2 cur = frontier.front();
			frontier.pop_front();
			if (cur != start && Claimed(pathSet, cur)) {
				hit = cur; // reached an existing street
				connected = true;
				break;
			}
			constexpr glm::ivec2 kStep[4] = {{0, 1}, {1, 0}, {-1, 0}, {0, -1}};
			for (const glm::ivec2 s : kStep) {
				const glm::ivec2 n{cur.x + s.x, cur.y + s.y};
				if (std::max(std::abs(n.x - centre.x), std::abs(n.y - centre.y)) > bfsRadius) {
					continue; // keep the search within the town neighbourhood
				}
				if (!IsBuildable(grid, n.x, n.y) || Claimed(claimed, n) || Claimed(visited, n)) {
					continue; // never route through a building or revisit a tile
				}
				Claim(visited, n);
				parent[{n.x, n.y}] = {cur.x, cur.y};
				frontier.push_back(n);
			}
		}

		if (connected) {
			// Lay the found route (entrance → street) onto the path network.
			for (glm::ivec2 p = hit;; ) {
				Claim(pathSet, p);
				const auto it = parent.find({p.x, p.y});
				if (it == parent.end() || (it->second.first == p.x && it->second.second == p.y)) {
					break;
				}
				p = {it->second.first, it->second.second};
			}
		}
		else {
			Claim(pathSet, start); // a short front stub for an unconnected outlier
		}
	}

	// Pave ground beneath every street tile so the road network sits on village ground and ties the
	// spread-out buildings together (the yards from margin-2 plots cover the area between them).
	for (const auto& [x, y] : pathSet) {
		addFloor({x, y});
	}

	for (const auto& [x, y] : pathSet) {
		layout.pathTiles.push_back({x, y});
	}

	return result;
}

// Lay out a ruined town: a rubble landmark and a small scatter of rubble piles over ruins flooring.
TownLayoutResult BuildRuinedTown(const TownSite& town, const Grid& grid, const WorldGenParams& params, LayoutRng& rng) {
	TownLayoutResult result;
	TownLayout& layout = result.layout;
	layout.townId = town.id;
	layout.ruined = true;

	const glm::ivec2 centre = town.location;
	const auto addFloor = [&](const glm::ivec2 t) {
		if (grid.InBounds(t.x, t.y)) {
			layout.floorTiles.push_back(t);
		}
	};

	const int plaza = std::max(0, params.townPlazaRadius);
	for (int dy = -plaza; dy <= plaza; ++dy) {
		for (int dx = -plaza; dx <= plaza; ++dx) {
			addFloor({centre.x + dx, centre.y + dy});
		}
	}

	const glm::ivec2 landmarkTile{centre.x, centre.y - 1};
	layout.landmarkKind = kRubbleKind;
	layout.landmark = landmarkTile;
	if (grid.InBounds(landmarkTile.x, landmarkTile.y)) {
		WorldObject landmark;
		landmark.footprint.push_back(landmarkTile);
		landmark.kind = kRubbleKind;
		landmark.height = kWellHeight * kRubbleHeightScale;
		landmark.blocksMovement = true;
		result.objects.push_back(std::move(landmark));
	}

	const int count = rng.Int(kRuinedBuildingMin, kRuinedBuildingMax);
	const std::vector<glm::ivec2> offsets = PlotOffsets(count * 4 + 8);
	const int stride = 3 + std::max(1, params.townPlotGap);
	int placed = 0;
	for (const glm::ivec2 off : offsets) {
		if (placed >= count) {
			break;
		}
		const glm::ivec2 plotCentre{centre.x + off.x * stride, centre.y + off.y * stride};
		if (!grid.InBounds(plotCentre.x, plotCentre.y)) {
			continue;
		}

		WorldObject rubble;
		rubble.footprint.push_back(plotCentre);
		rubble.kind = kRubbleKind;
		rubble.height = kWellHeight * kRubbleHeightScale;
		rubble.blocksMovement = true;
		result.objects.push_back(std::move(rubble));

		TownBuilding building;
		building.kind = kRubbleKind;
		building.footprint.push_back(plotCentre);
		building.door = {plotCentre.x, plotCentre.y + 1};
		building.ruined = true;
		layout.buildings.push_back(std::move(building));

		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				addFloor({plotCentre.x + dx, plotCentre.y + dy});
			}
		}
		++placed;
	}

	return result;
}

} // namespace

int TownBuildingCount(const WorldGenParams& params, const int population) {
	const int raw = static_cast<int>(std::lround(static_cast<double>(population) * params.buildingsPerCapita));
	return std::clamp(raw, params.minBuildingsPerTown, params.maxBuildingsPerTown);
}

std::uint32_t DeriveTownSeed(const int worldSeed, const int townId) {
	std::uint32_t h = static_cast<std::uint32_t>(worldSeed) * 0x9E3779B9u;
	h ^= static_cast<std::uint32_t>(townId) + 0x85EBCA6Bu + (h << 6) + (h >> 2);
	h ^= h >> 16;
	h *= 0x7FEB352Du;
	h ^= h >> 15;
	return h;
}

TownLayoutResult BuildTownLayout(
	const TownSite& town,
	const Grid& grid,
	const WorldGenParams& params,
	const BuildingCatalog& catalog,
	const std::uint32_t townSeed
) {
	LayoutRng rng(townSeed);
	if (town.status == TownStatus::Ruined) {
		return BuildRuinedTown(town, grid, params, rng);
	}
	return BuildLivingTown(town, grid, params, catalog, rng);
}

} // namespace game::farm
