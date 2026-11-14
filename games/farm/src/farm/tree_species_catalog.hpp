#pragma once

#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <flecs.h>
#include <nlohmann/json_fwd.hpp>

namespace game::farm {

// One tree species in the data-driven catalog. `kind` is the open-taxonomy identity that becomes a
// WorldObject::kind when a tree is placed; `sprite` is the atlas frame the renderer resolves. A
// placed tree draws its concrete height deterministically from [minHeight, maxHeight]. Adding a new
// species is a JSON edit only — no C++ change. Mirrors the CropRegistry / ObjectKindRegistry shape.
struct TreeSpecies {
	std::string kind;      // registry key / WorldObject kind, e.g. "tree_oak"
	std::string sprite;    // sprite-atlas frame reference used to render this species
	float minHeight{1.0f}; // lower bound of the per-instance visual height in world units
	float maxHeight{1.0f}; // upper bound of the per-instance visual height in world units

	friend bool operator==(const TreeSpecies&, const TreeSpecies&) = default;
};

// Catalog of tree species keyed by biome key ("forest", "deep_forest" — matching the woodland
// terrain tile names). The generation layer consumes this to decide which species grows in which
// woodland; a designer extends it by adding JSON entries. The per-biome species order is preserved
// (JSON array order) so species selection stays deterministic for a given seed.
struct TreeSpeciesCatalog {
	std::unordered_map<std::string, std::vector<TreeSpecies>> speciesByBiome;

	[[nodiscard]] bool Empty() const { return speciesByBiome.empty(); }

	// Species list for a biome key, or nullptr when the biome has no entry.
	[[nodiscard]] const std::vector<TreeSpecies>* SpeciesForBiome(std::string_view biomeKey) const {
		const auto it = speciesByBiome.find(std::string{biomeKey});
		return (it != speciesByBiome.end() && !it->second.empty()) ? &it->second : nullptr;
	}

	friend bool operator==(const TreeSpeciesCatalog&, const TreeSpeciesCatalog&) = default;
};

// Pure parse of a tree-catalog JSON document (the "treeSpecies" object: biome key -> species array)
// into a catalog. File-free so it is unit-testable without Platform. Missing fields fall back to
// defaults; a missing or non-object "treeSpecies" yields an empty catalog.
[[nodiscard]] TreeSpeciesCatalog ParseTreeSpeciesCatalog(const nlohmann::json& doc);

// Built-in fallback catalog used when no JSON is supplied (pure unit tests) or the file is missing.
// Keeps generation deterministic and woodland populated without a running Platform.
[[nodiscard]] TreeSpeciesCatalog DefaultTreeSpeciesCatalog();

// Load games/farm/assets/data/trees.json via Platform, set the TreeSpeciesCatalog singleton (for later render
// lookups), and return the catalog BY VALUE. Returning a copy — not a reference to the singleton —
// is deliberate: the scene calls this inside a DEFERRED observer context (scene activation), where
// the singleton `set` is queued and a `get` reference would be stale; the returned value is always a
// valid, stable copy safe to hand to the pure generation pass. Falls back to
// DefaultTreeSpeciesCatalog when the file is missing or empty so downstream generation always has
// species to place.
[[nodiscard]] TreeSpeciesCatalog LoadTreeSpeciesCatalog(flecs::world& world);

} // namespace game::farm
