#pragma once

#include <flecs.h>
#include <nlohmann/json_fwd.hpp>

#include "building_catalog.hpp"
#include "creature_catalog.hpp"
#include "town_style.hpp"
#include "world_data.hpp"

namespace game::farm {

/// The authoritative, generated world state for the active (or previewed) farm. The World Gen
/// Preview and the in-game farm are two lenses on this one singleton: the preview renders it as a
/// biome map, FarmWorldScene builds its tilemap + entities from it. See games/farm/docs/adr/0001.
struct CurrentWorld {
	WorldData world;
};

/// Pre-register farm world-gen component types (notably CurrentWorld) in a non-system context so
/// Flecs never lazily registers them while the world is readonly (physics sets stage_count > 1).
void RegisterFarmWorldComponents(flecs::world& world);

/// The canonical parameters for the full-size play world (1000x1000, catalogs scaled to match).
/// Both the preview and FarmWorldScene start from these so what the player previews is what plays.
[[nodiscard]] WorldGenParams CanonicalFarmParams();

/// Load the farm building catalog from assets (games/farm/assets/data/buildings.json), falling back to the
/// built-in default when the file is missing or empty.
[[nodiscard]] BuildingCatalog LoadFarmBuildingCatalog(const flecs::world& world);

/// Load the farm creature catalog from assets (games/farm/assets/data/creatures.json), falling back to the
/// built-in default when the file is missing or empty.
[[nodiscard]] CreatureCatalog LoadFarmCreatureCatalog(const flecs::world& world);

/// Load the town-style table from assets (games/farm/assets/data/town_styles.json), which maps a town's biome
/// and size tier to its ground material. Falls back to the built-in default when the file is
/// missing or malformed.
[[nodiscard]] TownStyleTable LoadFarmTownStyles(const flecs::world& world);

/// Generate a farm world deterministically from `seed` + `params`, loading the building, tree, and
/// creature catalogs at the scene boundary so WorldGen::Generate stays a pure value->value call.
[[nodiscard]] WorldData GenerateFarmWorld(flecs::world& world, int seed, const WorldGenParams& params);

/// Serialize every WorldGenParams knob to JSON (faithful, replayable record of the inputs).
[[nodiscard]] nlohmann::json WorldGenParamsToJson(const WorldGenParams& params);

/// Inverse of WorldGenParamsToJson. Missing keys fall back to WorldGenParams defaults.
[[nodiscard]] WorldGenParams WorldGenParamsFromJson(const nlohmann::json& j);

} // namespace game::farm
