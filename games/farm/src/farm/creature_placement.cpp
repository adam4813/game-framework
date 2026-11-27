#include "creature_placement.hpp"

#include <algorithm>
#include <array>
#include <cstdint>
#include <random>
#include <unordered_set>
#include <vector>

#include "seeded_random.hpp"

namespace game::farm {
namespace {

// Local deterministic RNG for one placement region, seeded independently of the pipeline RNG.
struct PlacementRng {
	std::mt19937 engine;
	explicit PlacementRng(const std::uint32_t s) : engine(s) {}
	int Int(const int lo, const int hi) { return UniformInt(engine, lo, hi); }
};

// Derive a stable per-region RNG seed from the world seed, a region id (town id or cell index) and a
// stage salt (livestock vs wildlife), so a region's creatures are reproducible and independent of
// the pipeline RNG stream.
std::uint32_t DeriveSeed(const int worldSeed, const int regionId, const std::uint32_t stageSalt) {
	std::uint32_t h = static_cast<std::uint32_t>(worldSeed) * 0x9E3779B9u;
	h ^= static_cast<std::uint32_t>(regionId) + 0x85EBCA6Bu + (h << 6) + (h >> 2);
	h ^= stageSalt + 0x165667B1u + (h << 5) + (h >> 3);
	h ^= h >> 16;
	h *= 0x7FEB352Du;
	h ^= h >> 15;
	return h;
}

std::uint64_t TileKey(const int x, const int y) {
	return (static_cast<std::uint64_t>(static_cast<std::uint32_t>(y)) << 32) | static_cast<std::uint32_t>(x);
}

// Weighted-random pick among the eligible kinds. Deterministic in `rng`.
const CreatureDef* PickKind(const std::vector<const CreatureDef*>& eligible, PlacementRng& rng) {
	int total = 0;
	for (const CreatureDef* d : eligible) {
		total += std::max(1, d->weight);
	}
	if (total <= 0) {
		return nullptr;
	}
	int roll = rng.Int(0, total - 1);
	for (const CreatureDef* d : eligible) {
		roll -= std::max(1, d->weight);
		if (roll < 0) {
			return d;
		}
	}
	return eligible.back();
}

constexpr size_t kBiomeCount = static_cast<size_t>(Biome::Count_);
using BiomeBuckets = std::array<std::vector<glm::ivec2>, kBiomeCount>;

} // namespace

bool IsCreatureWalkable(const Biome b) {
	return b == Biome::Plains || b == Biome::Beach;
}

void PlaceCreatures(WorldData& world, const CreatureCatalog& catalog) {
	if (catalog.defs.empty() || world.grid.width <= 0 || world.grid.height <= 0) {
		return;
	}

	const Grid& grid = world.grid;
	const int w = grid.width;
	const int h = grid.height;

	// Tiles covered by a blocking world-object (building walls, landmarks, trees) are off-limits.
	std::unordered_set<std::uint64_t> blocked;
	for (const WorldObject& obj : world.objects) {
		if (!obj.blocksMovement) {
			continue;
		}
		for (const glm::ivec2 t : obj.footprint) {
			blocked.insert(TileKey(t.x, t.y));
		}
	}

	// Tiles already holding a creature (no two creatures share a tile).
	std::unordered_set<std::uint64_t> taken;

	// Split the catalog into livestock (near-town) and wildlife (wilderness) subsets once; a kind
	// with no biome affinity can never find a tile, so it is dropped here.
	std::vector<const CreatureDef*> livestock;
	std::vector<const CreatureDef*> wildlife;
	for (const CreatureDef& d : catalog.defs) {
		if (d.biomes.empty()) {
			continue;
		}
		if (d.nearTown) {
			livestock.push_back(&d);
		}
		if (d.wilderness) {
			wildlife.push_back(&d);
		}
	}

	// Collect free, walkable candidate tiles within an inclusive rect, grouped by biome so a kind's
	// affinity can be matched quickly. Scan order is row-major and deterministic.
	const auto gatherByBiome = [&](const int x0, const int y0, const int x1, const int y1, BiomeBuckets& out) {
		for (auto& v : out) {
			v.clear();
		}
		for (int y = std::max(0, y0); y <= std::min(h - 1, y1); ++y) {
			for (int x = std::max(0, x0); x <= std::min(w - 1, x1); ++x) {
				const Biome b = grid.BiomeAt(x, y);
				if (!IsCreatureWalkable(b)) {
					continue;
				}
				const std::uint64_t key = TileKey(x, y);
				if (blocked.count(key) != 0 || taken.count(key) != 0) {
					continue;
				}
				out[static_cast<size_t>(b)].push_back({x, y});
			}
		}
	};

	// Place up to `count` creatures from `defs` into the gathered candidate tiles. Each creature picks
	// an eligible kind (weighted) whose affinity still has a free tile, then picks a free tile for it.
	const auto placeInRegion = [&](const std::vector<const CreatureDef*>& defs,
								   const int count,
								   const int homeTownId,
								   PlacementRng& rng,
								   BiomeBuckets& byBiome) {
		for (int i = 0; i < count; ++i) {
			std::vector<const CreatureDef*> eligible;
			for (const CreatureDef* d : defs) {
				const bool hasTile = std::ranges::any_of(d->biomes, [&](const Biome b) {
					return !byBiome[static_cast<size_t>(b)].empty();
				});
				if (hasTile) {
					eligible.push_back(d);
				}
			}
			if (eligible.empty()) {
				break;
			}

			const CreatureDef* def = PickKind(eligible, rng);
			if (def == nullptr) {
				break;
			}

			std::vector<glm::ivec2> pool;
			for (const Biome b : def->biomes) {
				const auto& v = byBiome[static_cast<size_t>(b)];
				pool.insert(pool.end(), v.begin(), v.end());
			}
			if (pool.empty()) {
				break;
			}
			const glm::ivec2 tile = pool[static_cast<size_t>(rng.Int(0, static_cast<int>(pool.size()) - 1))];

			taken.insert(TileKey(tile.x, tile.y));
			std::erase(byBiome[static_cast<size_t>(grid.BiomeAt(tile.x, tile.y))], tile);

			world.creatures.push_back({
				.tile = tile,
				.kind = def->kind,
				.wanderRadius = def->wanderRadius,
				.homeTownId = homeTownId,
			});
		}
	};

	BiomeBuckets byBiome;

	// ── Livestock near living towns ─────────────────────────────────────────────────────────────
	if (!livestock.empty()) {
		const int radius = std::max(1, world.params.creatureTownRadius);
		for (const TownSite& town : world.towns) {
			if (town.status != TownStatus::Living) {
				continue;
			}
			PlacementRng rng(DeriveSeed(world.seed, town.id, 0x11u));
			const int count = rng.Int(
				std::max(0, world.params.minLivestockPerTown),
				std::max(0, world.params.maxLivestockPerTown)
			);
			gatherByBiome(
				town.location.x - radius,
				town.location.y - radius,
				town.location.x + radius,
				town.location.y + radius,
				byBiome
			);
			placeInRegion(livestock, count, town.id, rng, byBiome);
		}
	}

	// ── Wildlife across the open wilderness, one count roll per sampling cell ────────────────────
	if (!wildlife.empty()) {
		const int cell = std::max(8, world.params.creatureAreaSize);
		const int cellsX = (w + cell - 1) / cell;
		const int cellsY = (h + cell - 1) / cell;
		for (int cy = 0; cy < cellsY; ++cy) {
			for (int cx = 0; cx < cellsX; ++cx) {
				const int regionId = cy * cellsX + cx;
				PlacementRng rng(DeriveSeed(world.seed, regionId, 0x22u));
				const int count = rng.Int(
					std::max(0, world.params.minWildlifePerArea),
					std::max(0, world.params.maxWildlifePerArea)
				);
				if (count <= 0) {
					continue;
				}
				gatherByBiome(cx * cell, cy * cell, (cx + 1) * cell - 1, (cy + 1) * cell - 1, byBiome);
				placeInRegion(wildlife, count, 0, rng, byBiome);
			}
		}
	}
}

} // namespace game::farm
