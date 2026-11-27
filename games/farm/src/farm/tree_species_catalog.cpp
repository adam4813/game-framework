#include "tree_species_catalog.hpp"

#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "engine/engine.hpp"

namespace game::farm {

namespace {

// Parse one biome's species array. Entries missing a "kind" are skipped; a missing maxHeight falls
// back to minHeight so a single-value height is valid.
std::vector<TreeSpecies> ParseSpeciesArray(const nlohmann::json& arr) {
	std::vector<TreeSpecies> species;
	if (!arr.is_array()) {
		return species;
	}
	for (const auto& entry : arr) {
		const float minHeight = entry.value("minHeight", 1.0f);
		TreeSpecies def{
			.kind = entry.value("kind", ""),
			.sprite = entry.value("sprite", ""),
			.minHeight = minHeight,
			.maxHeight = entry.value("maxHeight", minHeight),
		};
		if (def.maxHeight < def.minHeight) {
			def.maxHeight = def.minHeight;
		}
		if (!def.kind.empty()) {
			species.push_back(std::move(def));
		}
	}
	return species;
}

} // namespace

TreeSpeciesCatalog ParseTreeSpeciesCatalog(const nlohmann::json& doc) {
	TreeSpeciesCatalog catalog;

	if (!doc.contains("treeSpecies") || !doc["treeSpecies"].is_object()) {
		return catalog;
	}

	for (const auto& [biomeKey, arr] : doc["treeSpecies"].items()) {
		std::vector<TreeSpecies> species = ParseSpeciesArray(arr);
		if (!species.empty()) {
			catalog.speciesByBiome[biomeKey] = std::move(species);
		}
	}

	return catalog;
}

TreeSpeciesCatalog DefaultTreeSpeciesCatalog() {
	TreeSpeciesCatalog catalog;
	catalog.speciesByBiome["forest"] = {
		TreeSpecies{.kind = "tree_oak", .sprite = "tree_oak", .minHeight = 1.5f, .maxHeight = 2.0f},
		TreeSpecies{.kind = "tree_birch", .sprite = "tree_birch", .minHeight = 1.5f, .maxHeight = 2.0f},
	};
	catalog.speciesByBiome["deep_forest"] = {
		TreeSpecies{.kind = "tree_pine", .sprite = "tree_pine_dark", .minHeight = 2.5f, .maxHeight = 3.0f},
		TreeSpecies{.kind = "tree_fir", .sprite = "tree_fir_dark", .minHeight = 3.5f, .maxHeight = 4.0f},
	};
	return catalog;
}

TreeSpeciesCatalog LoadTreeSpeciesCatalog(flecs::world& world) {
	TreeSpeciesCatalog catalog;

	const auto* ref = world.try_get<engine::platform::PlatformRef>();
	const std::string path = (ref && ref->ptr) ? (ref->ptr->AssetDirectory() + "/data/trees.json") : "";

	if (!path.empty()) {
		if (const auto j = engine::core::LoadJsonFile(path, "TreeSpeciesCatalog")) {
			catalog = ParseTreeSpeciesCatalog(*j);
		}
	}

	if (catalog.Empty()) {
		engine::ecs::Log(world).info(
			"[TreeSpeciesCatalog] trees.json missing or empty; using built-in default catalog"
		);
		catalog = DefaultTreeSpeciesCatalog();
	}
	else {
		engine::ecs::Log(world).info(
			"[TreeSpeciesCatalog] Loaded tree species for {} biome(s)",
			catalog.speciesByBiome.size()
		);
	}

	// Set the singleton for later (deferred-safe: applied when the scene-load defer scope merges), but
	// return the local BY VALUE so the caller never holds a reference into deferred singleton storage.
	world.set<TreeSpeciesCatalog>(catalog);
	return catalog;
}

} // namespace game::farm
