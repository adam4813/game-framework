#pragma once

#include <algorithm>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

// Data-driven catalog of town-building kinds (ticket 04), loaded from games/farm/assets/data/buildings.json.
// Pure value types — no ECS/Platform — so the generation layer can consume a catalog and stay
// unit-testable without files. Adding a new building kind is a JSON entry with zero C++ changes
// (mirrors the CropRegistry / ObjectKindRegistry pattern). Some kinds map to a villager archetype
// so the town layout informs NPC placement in ticket 09.
namespace game::farm {

// One building-kind definition. `width`/`length` are the footprint size in tiles (x = columns,
// y = rows); `wallHeight` is the visual height of the walls/roof world-object. `villagerArchetype`
// ties this kind to an NPC role ("shopkeeper", "elder", …) or is empty for none. `farm` buildings
// get decorative tilled-soil field plots beside them. `weight` is the relative selection likelihood.
//
// A building is either RESIDENTIAL or COMMUNAL, distinguished by `occupancy`:
//   • occupancy > 0  — housing. The generator keeps placing weighted residential kinds until the
//     town's population is housed (each one shelters `occupancy` residents), so housing scales with
//     population without a hard count. Residential buildings are villager homes (`isHome`).
//   • occupancy == 0 — a communal/service building (shop, hall, smithy, …). It appears only once the
//     town reaches `minTier`, up to `maxCount` of them; `perCapita` > 0 makes the count scale with
//     population (e.g. more market stalls in a city), otherwise exactly one is placed.
struct BuildingDef {
	std::string kind;              // catalog key, e.g. "house"
	int width{3};                  // footprint columns (x)
	int length{3};                 // footprint rows (y)
	float wallHeight{1.5f};        // visual wall/roof height in world units
	int weight{1};                 // relative weight when choosing a residential kind
	std::string villagerArchetype; // NPC role bound to this kind (ticket 09); empty = none
	bool farm{false};              // farm buildings get adjacent decorative field plots
	int occupancy{0};              // > 0 => residential home sheltering this many residents
	int minTier{0};                // communal: lowest TownTier (0=Hamlet) it appears at
	int maxCount{1};               // communal: cap on how many per town
	float perCapita{0.0f};         // communal: count scaling with population (0 => fixed 1)

	[[nodiscard]] bool IsResidential() const { return occupancy > 0; }

	friend bool operator==(const BuildingDef&, const BuildingDef&) = default;
};

// An ordered set of building-kind definitions. Iteration order is deterministic (it drives the
// weighted-random building selection during generation), so load order must be stable.
struct BuildingCatalog {
	std::vector<BuildingDef> defs;

	// Largest footprint dimension across all kinds — the generator uses it to size building plots.
	[[nodiscard]] int MaxFootprintDim() const {
		int m = 1;
		for (const BuildingDef& d : defs) {
			m = std::max({m, d.width, d.length});
		}
		return m;
	}

	[[nodiscard]] const BuildingDef* Find(std::string_view kind) const {
		for (const BuildingDef& d : defs) {
			if (d.kind == kind) {
				return &d;
			}
		}
		return nullptr;
	}

	friend bool operator==(const BuildingCatalog&, const BuildingCatalog&) = default;
};

// The built-in default catalog (house/shop/barn/hall). Used by the two-argument WorldGen::Generate
// and by unit tests so generation produces buildings without any file I/O. The runtime game loads
// games/farm/assets/data/buildings.json instead (see ParseBuildingCatalog), which can add/override kinds.
[[nodiscard]] BuildingCatalog DefaultBuildingCatalog();

// Pure parse of a buildings JSON document (the "buildings" array) into a catalog. File-free so it
// is unit-testable without Platform. Missing fields fall back to defaults; a missing or non-array
// "buildings" yields an empty catalog.
[[nodiscard]] BuildingCatalog ParseBuildingCatalog(const nlohmann::json& doc);

} // namespace game::farm
