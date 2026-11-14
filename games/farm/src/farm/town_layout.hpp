#pragma once

#include <cstdint>
#include <vector>

#include "building_catalog.hpp"
#include "world_data.hpp"

// Town-layout generator (ticket 04): turns a TownSite (a point + population + living/ruined status)
// into a concrete village — a central landmark, population-scaled building footprints along short
// streets, decorative field plots near farm buildings, and the walkable floor/door terrain tiles.
// Pure + file-free (the BuildingCatalog is passed in), so it is deterministic and unit-testable in
// isolation. A town's layout is seeded from (worldSeed, townId) rather than the pipeline RNG, so it
// is independent of RNG ordering elsewhere in the pipeline.
namespace game::farm {

// The result of laying out one town: the semantic layout (consumed by the tilemap builder and by
// ticket 09 for NPC placement) plus the WorldObjects to append to WorldData::objects (the landmark
// first, then each building's blocking walls/roof, in deterministic order).
struct TownLayoutResult {
	TownLayout layout;
	std::vector<WorldObject> objects;
};

// Number of buildings a living town of the given population should have: monotonic non-decreasing in
// population, clamped to [minBuildingsPerTown, maxBuildingsPerTown].
[[nodiscard]] int TownBuildingCount(const WorldGenParams& params, int population);

// Derive a stable per-town RNG seed from the world seed and the town id so a town's layout is
// reproducible and independent of the pipeline RNG stream.
[[nodiscard]] std::uint32_t DeriveTownSeed(int worldSeed, int townId);

// Build the full village layout for one town. Living towns use intact building/well variants; ruined
// towns use low rubble variants and ruins flooring. Buildings whose footprint would leave the grid
// are skipped so nothing clips off the map.
[[nodiscard]] TownLayoutResult BuildTownLayout(
	const TownSite& town,
	const Grid& grid,
	const WorldGenParams& params,
	const BuildingCatalog& catalog,
	std::uint32_t townSeed
);

} // namespace game::farm
