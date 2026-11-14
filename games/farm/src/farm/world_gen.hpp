#pragma once

#include "building_catalog.hpp"
#include "creature_catalog.hpp"
#include "tree_species_catalog.hpp"
#include "world_data.hpp"

// Deterministic, pure-C++ world-generation pipeline. No ECS or Raylib dependency: the same
// WorldGenParams + seed always produces an identical WorldData, so bugs are reproducible and the
// pipeline is unit-testable in isolation.
//
// Pipeline stages (each records a cumulative GenerationTimeline snapshot):
//   1. Landmass      — fractal Perlin elevation with a radial edge falloff (ocean at the borders).
//   2. Water bodies  — flood-filled connected water; border-touching = Ocean, interior = Lake/Pond.
//   3. Rivers        — highland springs traced downhill until they meet a water body (the edges).
//   4. History       — town placement + population cycles: foundings, collapses (ruins), roads.
//   5. Town layouts  — per-town village world-objects (landmark, buildings, doors, fields) from a
//                      data-driven BuildingCatalog, replacing the old grey town square.
//   6. Forest trees  — blocking tree world-objects scattered over walkable woodland by a secondary
//                      noise density field (dense cores, sparse edges, clearings), species keyed by
//                      biome from a data-driven TreeSpeciesCatalog.
//   7. Creatures     — sparse ambient creature spawns (livestock near towns, wildlife in the open)
//                      from a data-driven CreatureCatalog, placed on walkable tiles.
namespace game::farm {

class WorldGen {
public:
	// Generate a fully populated world using the built-in default building + tree catalogs.
	// Deterministic in (params, seed).
	[[nodiscard]] static WorldData Generate(const WorldGenParams& params, int seed);

	// Generate with a specific building catalog (ticket 04); forest trees use the default tree
	// catalog. Deterministic in (params, seed, buildings).
	[[nodiscard]] static WorldData Generate(const WorldGenParams& params, int seed, const BuildingCatalog& buildings);

	// Generate with explicit building + tree catalogs (tickets 04 + 05). The runtime game loads both
	// from assets/data/*.json at the scene boundary and passes them in, keeping generation pure and
	// file-free. Deterministic in (params, seed, buildings, trees).
	[[nodiscard]] static WorldData
	Generate(const WorldGenParams& params, int seed, const BuildingCatalog& buildings, const TreeSpeciesCatalog& trees);

	// Generate with explicit building + tree + creature catalogs (tickets 04 + 05 + 10). All three
	// are loaded from JSON at the scene boundary and passed in so generation stays pure/file-free.
	// Deterministic in (params, seed, buildings, trees, creatures).
	[[nodiscard]] static WorldData
	Generate(const WorldGenParams& params, int seed, const BuildingCatalog& buildings, const TreeSpeciesCatalog& trees, const CreatureCatalog& creatures);
};

} // namespace game::farm
