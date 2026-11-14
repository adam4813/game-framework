#include "creature_catalog.hpp"

#include <array>
#include <utility>

#include <nlohmann/json.hpp>

namespace game::farm {

bool TryParseBiome(const std::string_view name, Biome& out) {
	static constexpr std::array<std::pair<std::string_view, Biome>, 8> kNames{{
		{"Ocean", Biome::Ocean},
		{"Lake", Biome::Lake},
		{"Beach", Biome::Beach},
		{"Plains", Biome::Plains},
		{"Forest", Biome::Forest},
		{"Hills", Biome::Hills},
		{"Mountain", Biome::Mountain},
		{"River", Biome::River},
	}};
	for (const auto& [n, b] : kNames) {
		if (n == name) {
			out = b;
			return true;
		}
	}
	return false;
}

CreatureCatalog DefaultCreatureCatalog() {
	CreatureCatalog catalog;
	// Only Plains (grass) and Beach (sand) are walkable natural terrain, so default affinities use
	// those — forest/hills/mountain/water tiles are non-walkable, and placement skips them. Livestock
	// cluster on grassland around living towns; wildlife amble through meadows and along shorelines.
	catalog.defs = {
		{.kind = "cow", .sprite = "cow", .biomes = {Biome::Plains}, .nearTown = true, .wilderness = false, .wanderRadius = 3, .height = 1.5f, .weight = 4},
		{.kind = "sheep", .sprite = "sheep", .biomes = {Biome::Plains}, .nearTown = true, .wilderness = false, .wanderRadius = 3, .height = 1.2f, .weight = 4},
		{.kind = "chicken", .sprite = "chicken", .biomes = {Biome::Plains}, .nearTown = true, .wilderness = false, .wanderRadius = 2, .height = 0.8f, .weight = 3},
		{.kind = "deer", .sprite = "deer", .biomes = {Biome::Plains}, .nearTown = false, .wilderness = true, .wanderRadius = 5, .height = 1.6f, .weight = 3},
		{.kind = "rabbit", .sprite = "rabbit", .biomes = {Biome::Plains, Biome::Beach}, .nearTown = false, .wilderness = true, .wanderRadius = 4, .height = 0.7f, .weight = 4},
		{.kind = "fox", .sprite = "fox", .biomes = {Biome::Plains}, .nearTown = false, .wilderness = true, .wanderRadius = 5, .height = 1.0f, .weight = 2},
	};
	return catalog;
}

CreatureCatalog ParseCreatureCatalog(const nlohmann::json& doc) {
	CreatureCatalog catalog;

	if (!doc.contains("creatures") || !doc["creatures"].is_array()) {
		return catalog;
	}

	for (const auto& entry : doc["creatures"]) {
		CreatureDef def{
			.kind = entry.value("kind", ""),
			.sprite = entry.value("sprite", ""),
			.biomes = {},
			.nearTown = entry.value("nearTown", false),
			.wilderness = entry.value("wilderness", false),
			.wanderRadius = entry.value("wanderRadius", 3),
			.height = entry.value("height", 0.6f),
			.weight = entry.value("weight", 1),
		};

		if (entry.contains("biomes") && entry["biomes"].is_array()) {
			for (const auto& b : entry["biomes"]) {
				if (Biome biome; b.is_string() && TryParseBiome(b.get<std::string>(), biome)) {
					def.biomes.push_back(biome);
				}
			}
		}

		if (!def.kind.empty()) {
			catalog.defs.push_back(std::move(def));
		}
	}

	return catalog;
}

} // namespace game::farm
