#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json_fwd.hpp>

#include "world_data.hpp"

// Data-driven catalog of wandering-creature kinds (ticket 10), loaded from games/farm/assets/data/creatures.json.
// Pure value types — no ECS/Platform — so the generation layer can consume a catalog and stay
// unit-testable without files. Adding a new creature kind is a JSON entry with zero C++ changes
// (mirrors the CropRegistry / ObjectKindRegistry / BuildingCatalog pattern).
//
// A creature kind declares WHERE it spawns (biome affinity + near-town vs wilderness proximity),
// HOW far it ambles (wanderRadius), and HOW it looks (sprite key + upright-quad height). Placement
// only ever puts a creature on a walkable tile whose biome is in `biomes`, so a kind confined to a
// non-walkable biome simply never spawns (conservative, never on water/walls).
namespace game::farm {

// One creature-kind definition. `biomes` is the affinity set (which terrain biomes the creature may
// stand on). `nearTown` creatures are livestock scattered around living towns; `wilderness`
// creatures are wildlife scattered across the open land. A kind may be both. `wanderRadius` is how
// many tiles it ambles from its spawn tile at runtime; `height` is its upright-quad height in world
// units; `weight` is the relative likelihood of picking this kind among the eligible ones.
struct CreatureDef {
	std::string kind;           // catalog key, e.g. "cow"
	std::string sprite;         // sprite-atlas frame key used to render the upright quad
	std::vector<Biome> biomes;  // biome affinity: the walkable biomes this kind may spawn on
	bool nearTown{false};       // livestock: scatter around living towns
	bool wilderness{false};     // wildlife: scatter across the open wilderness
	int wanderRadius{3};        // tiles the creature ambles from its spawn tile
	float height{0.6f};         // upright-quad visual height in world units
	int weight{1};              // relative selection weight among eligible kinds

	[[nodiscard]] bool LikesBiome(Biome b) const {
		for (const Biome affinity : biomes) {
			if (affinity == b) {
				return true;
			}
		}
		return false;
	}

	friend bool operator==(const CreatureDef&, const CreatureDef&) = default;
};

// An ordered set of creature-kind definitions. Iteration order is deterministic (it drives the
// weighted-random kind selection during placement), so load order must be stable.
struct CreatureCatalog {
	std::vector<CreatureDef> defs;

	[[nodiscard]] const CreatureDef* Find(std::string_view kind) const {
		for (const CreatureDef& d : defs) {
			if (d.kind == kind) {
				return &d;
			}
		}
		return nullptr;
	}

	friend bool operator==(const CreatureCatalog&, const CreatureCatalog&) = default;
};

// Map a biome name (matching the Biome enum, e.g. "Plains", "Beach") to its enum value, or nullopt
// when the name is unknown. Used by the JSON parser to resolve a kind's affinity list.
[[nodiscard]] bool TryParseBiome(std::string_view name, Biome& out);

// The built-in default catalog (livestock near towns + wildlife in the open). Used by the
// three-argument WorldGen::Generate and by unit tests so generation produces creatures without any
// file I/O. The runtime game loads games/farm/assets/data/creatures.json instead (see ParseCreatureCatalog),
// which can add or override kinds.
[[nodiscard]] CreatureCatalog DefaultCreatureCatalog();

// Pure parse of a creatures JSON document (the "creatures" array) into a catalog. File-free so it is
// unit-testable without Platform. Missing fields fall back to defaults; a missing or non-array
// "creatures" yields an empty catalog. Unknown biome names in an affinity list are skipped.
[[nodiscard]] CreatureCatalog ParseCreatureCatalog(const nlohmann::json& doc);

} // namespace game::farm
