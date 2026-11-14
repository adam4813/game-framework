#include "building_catalog.hpp"

#include <utility>

#include <nlohmann/json.hpp>

namespace game::farm {

BuildingCatalog DefaultBuildingCatalog() {
	BuildingCatalog catalog;
	catalog.defs = {
		// ── Residential (occupancy > 0): housing, scaled to population ──
		{.kind = "cottage", .width = 2, .length = 2, .wallHeight = 2.5f, .weight = 5, .occupancy = 2},
		{.kind = "house", .width = 3, .length = 3, .wallHeight = 3.0f, .weight = 6, .occupancy = 4},
		{.kind = "longhouse", .width = 3, .length = 4, .wallHeight = 3.0f, .weight = 2, .occupancy = 6},

		// ── Communal (occupancy == 0): tier-gated services ──
		{.kind = "barn", .width = 4, .length = 4, .wallHeight = 4.0f, .farm = true, .minTier = 0, .maxCount = 2},
		{.kind = "shop", .width = 4, .length = 3, .wallHeight = 3.0f, .villagerArchetype = "shopkeeper", .minTier = 1, .maxCount = 4, .perCapita = 0.02f},
		{.kind = "tavern", .width = 4, .length = 4, .wallHeight = 3.5f, .villagerArchetype = "barkeep", .minTier = 1, .maxCount = 1},
		{.kind = "smithy", .width = 3, .length = 3, .wallHeight = 3.0f, .villagerArchetype = "blacksmith", .minTier = 2, .maxCount = 1},
		{.kind = "hall", .width = 5, .length = 4, .wallHeight = 5.0f, .villagerArchetype = "elder", .minTier = 2, .maxCount = 1},
		{.kind = "chapel", .width = 4, .length = 5, .wallHeight = 4.5f, .villagerArchetype = "priest", .minTier = 2, .maxCount = 1},
		{.kind = "market", .width = 4, .length = 4, .wallHeight = 2.5f, .villagerArchetype = "merchant", .minTier = 3, .maxCount = 3, .perCapita = 0.03f},
		{.kind = "temple", .width = 5, .length = 5, .wallHeight = 6.0f, .villagerArchetype = "priest", .minTier = 3, .maxCount = 1},
		{.kind = "guardhouse", .width = 3, .length = 3, .wallHeight = 3.5f, .villagerArchetype = "guard", .minTier = 3, .maxCount = 1},
	};
	return catalog;
}

namespace {

// Accept `minTier` as either a tier name ("hamlet"/"village"/"town"/"city") or a 0-based integer.
int ParseMinTier(const nlohmann::json& entry) {
	const auto it = entry.find("minTier");
	if (it == entry.end()) {
		return 0;
	}
	if (it->is_number_integer()) {
		return it->get<int>();
	}
	if (it->is_string()) {
		const std::string name = it->get<std::string>();
		if (name == "village") return 1;
		if (name == "town") return 2;
		if (name == "city") return 3;
		return 0; // "hamlet" or unknown
	}
	return 0;
}

} // namespace

BuildingCatalog ParseBuildingCatalog(const nlohmann::json& doc) {
	BuildingCatalog catalog;

	if (!doc.contains("buildings") || !doc["buildings"].is_array()) {
		return catalog;
	}

	for (const auto& entry : doc["buildings"]) {
		BuildingDef def{
			.kind = entry.value("kind", ""),
			.width = entry.value("width", 3),
			.length = entry.value("length", 3),
			.wallHeight = entry.value("wallHeight", 1.5f),
			.weight = entry.value("weight", 1),
			.villagerArchetype = entry.value("villagerArchetype", ""),
			.farm = entry.value("farm", false),
			.occupancy = entry.value("occupancy", 0),
			.minTier = ParseMinTier(entry),
			.maxCount = entry.value("maxCount", 1),
			.perCapita = entry.value("perCapita", 0.0f),
		};

		if (!def.kind.empty()) {
			catalog.defs.push_back(std::move(def));
		}
	}

	return catalog;
}

} // namespace game::farm
