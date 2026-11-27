#include "farm_world_gen.hpp"

#include <algorithm>
#include <string>
#include <utility>

#include <nlohmann/json.hpp>

#include "engine/engine.hpp"
#include "tree_species_catalog.hpp"
#include "world_gen.hpp"

using namespace engine;

namespace game::farm {

void RegisterFarmWorldComponents(flecs::world& world) { world.component<CurrentWorld>(); }

WorldGenParams CanonicalFarmParams() {
	// A 1000x1000 world with noise/spacing scaled for the larger canvas. WorldGenParams{} defaults
	// to 160x160 (kept for unit tests); the play world overrides those here so both the preview and
	// the in-game farm generate at the same scale.
	WorldGenParams params;
	params.width = 1000;
	params.height = 1000;
	params.noiseScale = 0.004F;    // scale with world size so landmasses have the same proportions
	params.riverCount = 6;         // more rivers over the larger area
	params.minTownSpacing = 80;    // proportional to world width (was 14 for 160x160)
	params.coastalRadius = 10;     // still a tile-distance check, slightly larger for the bigger world
	params.explorationRange = 200; // ruins must be within 200 tiles of the starting village
	params.pondMaxTiles = 300;     // scale up water-body thresholds
	params.lakeMaxTiles = 3000;
	params.treeDensityNoiseScale = 0.02F; // scale forest core/clearing size with the larger world

	// Road hubs + crossroads outposts — scaled for the 1000x1000 play world.
	params.hubCount = 8;
	params.hubMinSpacing = 140;           // proportional to the larger map (was 24 for 160x160)
	params.hubDiscountRadius = 70;        // the attractor neighbourhood scales with the world
	params.townAttractorRadius = 100;     // large towns pull routes from far across the map
	params.outpostConvergenceRadius = 55; // count roads meeting within a wide junction neighbourhood
	params.outpostMinTownDistance = 120;  // a wilderness crossroads must be well clear of existing towns
	return params;
}

BuildingCatalog LoadFarmBuildingCatalog(const flecs::world& world) {
	BuildingCatalog catalog = DefaultBuildingCatalog();
	if (const auto* pref = world.try_get<platform::PlatformRef>(); pref && pref->ptr) {
		const std::string path = pref->ptr->AssetDirectory() + "/data/buildings.json";
		if (const auto doc = core::LoadJsonFile(path, "BuildingCatalog")) {
			if (BuildingCatalog loaded = ParseBuildingCatalog(*doc); !loaded.defs.empty()) {
				catalog = std::move(loaded);
			}
		}
	}
	return catalog;
}

CreatureCatalog LoadFarmCreatureCatalog(const flecs::world& world) {
	CreatureCatalog catalog = DefaultCreatureCatalog();
	if (const auto* pref = world.try_get<platform::PlatformRef>(); pref && pref->ptr) {
		const std::string path = pref->ptr->AssetDirectory() + "/data/creatures.json";
		if (const auto doc = core::LoadJsonFile(path, "CreatureCatalog")) {
			if (CreatureCatalog loaded = ParseCreatureCatalog(*doc); !loaded.defs.empty()) {
				catalog = std::move(loaded);
			}
		}
	}
	return catalog;
}

TownStyleTable LoadFarmTownStyles(const flecs::world& world) {
	if (const auto* pref = world.try_get<platform::PlatformRef>(); pref && pref->ptr) {
		const std::string path = pref->ptr->AssetDirectory() + "/data/town_styles.json";
		if (const auto doc = core::LoadJsonFile(path, "TownStyleTable")) {
			return ParseTownStyleTable(*doc);
		}
	}
	return DefaultTownStyleTable();
}

WorldData GenerateFarmWorld(flecs::world& world, const int seed, const WorldGenParams& params) {
	const BuildingCatalog buildings = LoadFarmBuildingCatalog(world);
	const TreeSpeciesCatalog trees = LoadTreeSpeciesCatalog(world);
	const CreatureCatalog creatures = LoadFarmCreatureCatalog(world);
	return WorldGen::Generate(params, seed, buildings, trees, creatures);
}

nlohmann::json WorldGenParamsToJson(const WorldGenParams& p) {
	using nlohmann::json;
	json j;
	j["width"] = p.width;
	j["height"] = p.height;
	j["fractal"] = json{
		{"octaves", p.fractal.octaves},
		{"persistence", p.fractal.persistence},
		{"lacunarity", p.fractal.lacunarity},
	};
	j["noiseScale"] = p.noiseScale;
	j["seaLevel"] = p.seaLevel;
	j["beachLevel"] = p.beachLevel;
	j["plainsLevel"] = p.plainsLevel;
	j["forestLevel"] = p.forestLevel;
	j["hillsLevel"] = p.hillsLevel;
	j["edgeFalloff"] = p.edgeFalloff;
	j["pondMaxTiles"] = p.pondMaxTiles;
	j["lakeMaxTiles"] = p.lakeMaxTiles;
	j["riverCount"] = p.riverCount;
	j["townCount"] = p.townCount;
	j["minTownSpacing"] = p.minTownSpacing;
	j["coastalRadius"] = p.coastalRadius;
	j["historyEras"] = p.historyEras;
	j["explorationRange"] = p.explorationRange;
	j["buildingsPerCapita"] = p.buildingsPerCapita;
	j["minBuildingsPerTown"] = p.minBuildingsPerTown;
	j["maxBuildingsPerTown"] = p.maxBuildingsPerTown;
	j["townPlotGap"] = p.townPlotGap;
	j["townPlazaRadius"] = p.townPlazaRadius;
	j["forestTreeDensity"] = p.forestTreeDensity;
	j["deepForestTreeDensity"] = p.deepForestTreeDensity;
	j["forestClearingThreshold"] = p.forestClearingThreshold;
	j["deepForestClearingThreshold"] = p.deepForestClearingThreshold;
	j["forestCoreThreshold"] = p.forestCoreThreshold;
	j["treeDensityNoiseScale"] = p.treeDensityNoiseScale;
	json roadCost = json::array();
	for (const float c : p.roadBiomeCost) roadCost.push_back(c);
	j["roadBiomeCost"] = std::move(roadCost);
	j["roadExistingDiscount"] = p.roadExistingDiscount;
	j["roadTownAvoidCost"] = p.roadTownAvoidCost;
	j["roadRedundancyDetour"] = p.roadRedundancyDetour;
	j["hubCount"] = p.hubCount;
	j["hubMinSpacing"] = p.hubMinSpacing;
	j["hubRouteDiscount"] = p.hubRouteDiscount;
	j["hubDiscountRadius"] = p.hubDiscountRadius;
	j["townAttractorDiscount"] = p.townAttractorDiscount;
	j["townAttractorRadius"] = p.townAttractorRadius;
	j["townAttractorConnectivity"] = p.townAttractorConnectivity;
	j["hubMarkerRadius"] = p.hubMarkerRadius;
	j["outpostConvergenceRadius"] = p.outpostConvergenceRadius;
	j["outpostMinConvergence"] = p.outpostMinConvergence;
	j["outpostMinTownDistance"] = p.outpostMinTownDistance;
	j["maxCrossroadsOutposts"] = p.maxCrossroadsOutposts;
	j["outpostPopulation"] = p.outpostPopulation;
	j["maxBridgeSpan"] = p.maxBridgeSpan;
	j["creatureTownRadius"] = p.creatureTownRadius;
	j["minLivestockPerTown"] = p.minLivestockPerTown;
	j["maxLivestockPerTown"] = p.maxLivestockPerTown;
	j["creatureAreaSize"] = p.creatureAreaSize;
	j["minWildlifePerArea"] = p.minWildlifePerArea;
	j["maxWildlifePerArea"] = p.maxWildlifePerArea;
	return j;
}

WorldGenParams WorldGenParamsFromJson(const nlohmann::json& j) {
	WorldGenParams p; // defaults; overridden by any present key
	if (!j.is_object()) return p;
	p.width = j.value("width", p.width);
	p.height = j.value("height", p.height);
	if (const auto it = j.find("fractal"); it != j.end() && it->is_object()) {
		p.fractal.octaves = it->value("octaves", p.fractal.octaves);
		p.fractal.persistence = it->value("persistence", p.fractal.persistence);
		p.fractal.lacunarity = it->value("lacunarity", p.fractal.lacunarity);
	}
	p.noiseScale = j.value("noiseScale", p.noiseScale);
	p.seaLevel = j.value("seaLevel", p.seaLevel);
	p.beachLevel = j.value("beachLevel", p.beachLevel);
	p.plainsLevel = j.value("plainsLevel", p.plainsLevel);
	p.forestLevel = j.value("forestLevel", p.forestLevel);
	p.hillsLevel = j.value("hillsLevel", p.hillsLevel);
	p.edgeFalloff = j.value("edgeFalloff", p.edgeFalloff);
	p.pondMaxTiles = j.value("pondMaxTiles", p.pondMaxTiles);
	p.lakeMaxTiles = j.value("lakeMaxTiles", p.lakeMaxTiles);
	p.riverCount = j.value("riverCount", p.riverCount);
	p.townCount = j.value("townCount", p.townCount);
	p.minTownSpacing = j.value("minTownSpacing", p.minTownSpacing);
	p.coastalRadius = j.value("coastalRadius", p.coastalRadius);
	p.historyEras = j.value("historyEras", p.historyEras);
	p.explorationRange = j.value("explorationRange", p.explorationRange);
	p.buildingsPerCapita = j.value("buildingsPerCapita", p.buildingsPerCapita);
	p.minBuildingsPerTown = j.value("minBuildingsPerTown", p.minBuildingsPerTown);
	p.maxBuildingsPerTown = j.value("maxBuildingsPerTown", p.maxBuildingsPerTown);
	p.townPlotGap = j.value("townPlotGap", p.townPlotGap);
	p.townPlazaRadius = j.value("townPlazaRadius", p.townPlazaRadius);
	p.forestTreeDensity = j.value("forestTreeDensity", p.forestTreeDensity);
	p.deepForestTreeDensity = j.value("deepForestTreeDensity", p.deepForestTreeDensity);
	p.forestClearingThreshold = j.value("forestClearingThreshold", p.forestClearingThreshold);
	p.deepForestClearingThreshold = j.value("deepForestClearingThreshold", p.deepForestClearingThreshold);
	p.forestCoreThreshold = j.value("forestCoreThreshold", p.forestCoreThreshold);
	p.treeDensityNoiseScale = j.value("treeDensityNoiseScale", p.treeDensityNoiseScale);
	if (const auto it = j.find("roadBiomeCost"); it != j.end() && it->is_array()) {
		const size_t n = std::min(it->size(), p.roadBiomeCost.size());
		for (size_t i = 0; i < n; ++i) p.roadBiomeCost[i] = it->at(i).get<float>();
	}
	p.roadExistingDiscount = j.value("roadExistingDiscount", p.roadExistingDiscount);
	p.roadTownAvoidCost = j.value("roadTownAvoidCost", p.roadTownAvoidCost);
	p.roadRedundancyDetour = j.value("roadRedundancyDetour", p.roadRedundancyDetour);
	p.hubCount = j.value("hubCount", p.hubCount);
	p.hubMinSpacing = j.value("hubMinSpacing", p.hubMinSpacing);
	p.hubRouteDiscount = j.value("hubRouteDiscount", p.hubRouteDiscount);
	p.hubDiscountRadius = j.value("hubDiscountRadius", p.hubDiscountRadius);
	p.townAttractorDiscount = j.value("townAttractorDiscount", p.townAttractorDiscount);
	p.townAttractorRadius = j.value("townAttractorRadius", p.townAttractorRadius);
	p.townAttractorConnectivity = j.value("townAttractorConnectivity", p.townAttractorConnectivity);
	p.hubMarkerRadius = j.value("hubMarkerRadius", p.hubMarkerRadius);
	p.outpostConvergenceRadius = j.value("outpostConvergenceRadius", p.outpostConvergenceRadius);
	p.outpostMinConvergence = j.value("outpostMinConvergence", p.outpostMinConvergence);
	p.outpostMinTownDistance = j.value("outpostMinTownDistance", p.outpostMinTownDistance);
	p.maxCrossroadsOutposts = j.value("maxCrossroadsOutposts", p.maxCrossroadsOutposts);
	p.outpostPopulation = j.value("outpostPopulation", p.outpostPopulation);
	p.maxBridgeSpan = j.value("maxBridgeSpan", p.maxBridgeSpan);
	p.creatureTownRadius = j.value("creatureTownRadius", p.creatureTownRadius);
	p.minLivestockPerTown = j.value("minLivestockPerTown", p.minLivestockPerTown);
	p.maxLivestockPerTown = j.value("maxLivestockPerTown", p.maxLivestockPerTown);
	p.creatureAreaSize = j.value("creatureAreaSize", p.creatureAreaSize);
	p.minWildlifePerArea = j.value("minWildlifePerArea", p.minWildlifePerArea);
	p.maxWildlifePerArea = j.value("maxWildlifePerArea", p.maxWildlifePerArea);
	return p;
}

} // namespace game::farm
