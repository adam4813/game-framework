#include <gtest/gtest.h>

#include <array>
#include <string>

#include "farm/building_catalog.hpp"
#include "farm/creature_catalog.hpp"
#include "farm/generation_log.hpp"
#include "farm/tree_species_catalog.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;

namespace {

// The exact 1000×1000 parameter overrides the farm scene applies (farm_world_scene.cpp). Unit tests
// otherwise run at the 160×160 default, so a crash that only manifests at play scale slips past them
// — this guards the runtime AC (farm scene loads without crash) at the pure-generation seam.
WorldGenParams PlayScaleParams() {
	WorldGenParams p;
	p.width = 1000;
	p.height = 1000;
	p.noiseScale = 0.004F;
	p.riverCount = 6;
	p.minTownSpacing = 80;
	p.coastalRadius = 10;
	p.explorationRange = 200;
	p.pondMaxTiles = 300;
	p.lakeMaxTiles = 3000;
	p.treeDensityNoiseScale = 0.02F;
	p.hubCount = 8;
	p.hubMinSpacing = 140;
	p.hubDiscountRadius = 70;
	p.townAttractorRadius = 100;
	p.outpostConvergenceRadius = 55;
	p.outpostMinTownDistance = 120;
	return p;
}

} // namespace

TEST(PlayScaleGenerationTest, generates_without_crash_across_seeds) {
	const WorldGenParams params = PlayScaleParams();
	for (const int seed : {1, 42, 1337, 540846061, 99999, 2468013, 7, 2024}) {
		const WorldData world = WorldGen::Generate(
			params, seed, DefaultBuildingCatalog(), DefaultTreeSpeciesCatalog(), DefaultCreatureCatalog()
		);
		EXPECT_EQ(world.grid.width, 1000) << "seed " << seed;
		EXPECT_EQ(world.grid.height, 1000) << "seed " << seed;
		EXPECT_FALSE(world.towns.empty()) << "seed " << seed;
		// The generation log must serialize at play scale too (hundreds of thousands of objects).
		const std::string log = FormatGenerationLog(world);
		EXPECT_FALSE(log.empty()) << "seed " << seed;
	}
}
