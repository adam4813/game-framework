#include "town_style.hpp"

#include <algorithm>
#include <random>

#include <nlohmann/json.hpp>

#include "seeded_random.hpp"

namespace game::farm {

TownTier TierForPopulation(const int population) {
	if (population >= kCityPop) {
		return TownTier::City;
	}
	if (population >= kTownPop) {
		return TownTier::Town;
	}
	if (population >= kVillagePop) {
		return TownTier::Village;
	}
	return TownTier::Hamlet;
}

std::string_view BiomeName(const Biome biome) {
	switch (biome) {
	case Biome::Ocean: return "ocean";
	case Biome::Lake: return "lake";
	case Biome::Beach: return "beach";
	case Biome::Plains: return "plains";
	case Biome::Forest: return "forest";
	case Biome::Hills: return "hills";
	case Biome::Mountain: return "mountain";
	case Biome::River: return "river";
	case Biome::Count_: break;
	}
	return "plains";
}

const TownStyle* TownStyleTable::FindStyle(const std::string_view name) const {
	for (const auto& [key, style] : styles) {
		if (key == name) {
			return &style;
		}
	}
	return nullptr;
}

std::string_view TownStyleTable::StyleForBiome(const std::string_view biome) const {
	for (const auto& [key, styleName] : biomeStyle) {
		if (key == biome) {
			return styleName;
		}
	}
	return "default";
}

namespace {

// Build a per-tier weighted list, padding/truncating to TownTier::Count_ entries so indexing by tier
// is always safe.
std::vector<std::vector<FloorMaterialWeight>> MakeTiers(std::vector<std::vector<FloorMaterialWeight>> tiers) {
	tiers.resize(static_cast<std::size_t>(TownTier::Count_));
	return tiers;
}

} // namespace

TownStyleTable DefaultTownStyleTable() {
	TownStyleTable table;
	TownStyle def;
	def.tiers = MakeTiers({
		{{.material = "dirt", .weight = 1}},                                                    // Hamlet
		{{.material = "dirt", .weight = 70}, {.material = "cobblestone", .weight = 30}},         // Village
		{{.material = "cobblestone", .weight = 70}, {.material = "dirt", .weight = 15}, {.material = "brick", .weight = 15}}, // Town
		{{.material = "brick", .weight = 60}, {.material = "cobblestone", .weight = 35}, {.material = "dirt", .weight = 5}},  // City
	});
	table.styles.emplace_back("default", std::move(def));
	table.fallbackMaterial = "dirt";
	return table;
}

namespace {

std::vector<FloorMaterialWeight> ParseWeightList(const nlohmann::json& arr) {
	std::vector<FloorMaterialWeight> out;
	if (!arr.is_array()) {
		return out;
	}
	for (const auto& entry : arr) {
		FloorMaterialWeight w{
			.material = entry.value("material", std::string{}),
			.weight = entry.value("weight", 1),
		};
		if (!w.material.empty()) {
			out.push_back(std::move(w));
		}
	}
	return out;
}

} // namespace

TownStyleTable ParseTownStyleTable(const nlohmann::json& doc) {
	if (!doc.is_object()) {
		return DefaultTownStyleTable();
	}

	TownStyleTable table;
	table.fallbackMaterial = doc.value("fallbackMaterial", std::string{"dirt"});

	if (const auto it = doc.find("styles"); it != doc.end() && it->is_object()) {
		for (const auto& [name, styleJson] : it->items()) {
			TownStyle style;
			style.tiers.resize(static_cast<std::size_t>(TownTier::Count_));
			if (const auto tiersIt = styleJson.find("tiers"); tiersIt != styleJson.end() && tiersIt->is_object()) {
				const std::pair<const char*, TownTier> names[] = {
					{"hamlet", TownTier::Hamlet},
					{"village", TownTier::Village},
					{"town", TownTier::Town},
					{"city", TownTier::City},
				};
				for (const auto& [key, tier] : names) {
					if (const auto tierIt = tiersIt->find(key); tierIt != tiersIt->end()) {
						style.tiers[static_cast<std::size_t>(tier)] = ParseWeightList(*tierIt);
					}
				}
			}
			table.styles.emplace_back(name, std::move(style));
		}
	}

	if (const auto it = doc.find("biomeStyle"); it != doc.end() && it->is_object()) {
		for (const auto& [biome, styleName] : it->items()) {
			if (styleName.is_string()) {
				table.biomeStyle.emplace_back(biome, styleName.get<std::string>());
			}
		}
	}

	if (table.FindStyle("default") == nullptr) {
		// Guarantee a usable fallback palette even if the document omitted "default".
		const TownStyleTable builtin = DefaultTownStyleTable();
		table.styles.emplace_back("default", *builtin.FindStyle("default"));
	}
	return table;
}

std::string
PickFloorMaterial(const TownStyleTable& table, const Biome biome, const TownTier tier, const std::uint32_t seed) {
	const std::string_view styleName = table.StyleForBiome(BiomeName(biome));
	const TownStyle* style = table.FindStyle(styleName);
	if (style == nullptr) {
		style = table.FindStyle("default");
	}

	if (style != nullptr) {
		const auto idx = static_cast<std::size_t>(tier);
		if (idx < style->tiers.size()) {
			const std::vector<FloorMaterialWeight>& list = style->tiers[idx];
			int total = 0;
			for (const FloorMaterialWeight& w : list) {
				total += std::max(1, w.weight);
			}
			if (total > 0) {
				std::mt19937 rng(seed);
				int roll = UniformInt(rng, 0, total - 1);
				for (const FloorMaterialWeight& w : list) {
					roll -= std::max(1, w.weight);
					if (roll < 0) {
						return w.material;
					}
				}
			}
		}
	}
	return table.fallbackMaterial;
}

} // namespace game::farm
