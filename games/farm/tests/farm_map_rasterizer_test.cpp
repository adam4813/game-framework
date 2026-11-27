/// Unit tests for the pure rasterizer functions in farm_map_rasterizer.hpp.
/// Testable seam: RasterizeWorldMap, RasterizeLocalArea, BiomeToColor, Compute*PinFraction.
/// No ECS world, no GPU, no file I/O.
#include <gtest/gtest.h>

#include <array>
#include <set>
#include <tuple>

#include "farm/farm_map_rasterizer.hpp"
#include "farm/world_gen.hpp"

using namespace game::farm;

// ── BiomeToColor ─────────────────────────────────────────────────────────────────

TEST(FarmMapRasterizerTest, BiomeToColorAllBiomesDistinct) {
	constexpr std::array kBiomes = {
		Biome::Ocean,
		Biome::Lake,
		Biome::Beach,
		Biome::Plains,
		Biome::Forest,
		Biome::Hills,
		Biome::Mountain,
		Biome::River,
	};
	std::set<std::tuple<uint8_t, uint8_t, uint8_t>> seen;
	for (const Biome b : kBiomes)
		seen.insert({BiomeToColor(b).r, BiomeToColor(b).g, BiomeToColor(b).b});
	EXPECT_EQ(seen.size(), kBiomes.size()) << "Every biome must map to a distinct colour";
}

TEST(FarmMapRasterizerTest, BiomeToColorAllFullyOpaque) {
	constexpr std::array kBiomes = {
		Biome::Ocean, Biome::Lake, Biome::Beach, Biome::Plains,
		Biome::Forest, Biome::Hills, Biome::Mountain, Biome::River,
	};
	for (const Biome b : kBiomes)
		EXPECT_EQ(BiomeToColor(b).a, 255u) << "Colour for biome must have alpha=255";
}

// ── RasterizeWorldMap ─────────────────────────────────────────────────────────────

TEST(FarmMapRasterizerTest, RasterizeOutputDimensions) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const auto pixels = RasterizeWorldMap(world, 100, 80);
	EXPECT_EQ(pixels.size(), 100u * 80u * 4u);
}

TEST(FarmMapRasterizerTest, RasterizeAllPixelsFullyOpaque) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const auto pixels = RasterizeWorldMap(world, 64, 64);
	for (std::size_t i = 3; i < pixels.size(); i += 4)
		ASSERT_EQ(pixels[i], 255u) << "Alpha must be 255 at pixel " << (i / 4);
}

TEST(FarmMapRasterizerTest, AtLeastFiveDistinctColoursInFullWorld) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const auto pixels = RasterizeWorldMap(world, world.grid.width, world.grid.height);
	std::set<std::tuple<uint8_t, uint8_t, uint8_t>> colors;
	for (std::size_t i = 0; i < pixels.size(); i += 4)
		colors.insert({pixels[i], pixels[i + 1], pixels[i + 2]});
	EXPECT_GE(colors.size(), 5u) << "Expected >=5 distinct biome colours in a full world map";
}

// Verify across multiple seeds to rule out degenerate worlds.
TEST(FarmMapRasterizerTest, AtLeastFiveDistinctColoursMultipleSeeds) {
	for (const int seed : {1, 7, 13, 42, 99}) {
		const WorldData world = WorldGen::Generate(WorldGenParams{}, seed);
		const auto pixels = RasterizeWorldMap(world, world.grid.width, world.grid.height);
		std::set<std::tuple<uint8_t, uint8_t, uint8_t>> colors;
		for (std::size_t i = 0; i < pixels.size(); i += 4)
			colors.insert({pixels[i], pixels[i + 1], pixels[i + 2]});
		EXPECT_GE(colors.size(), 5u) << "Seed " << seed << " produced fewer than 5 distinct colours";
	}
}

// ── RasterizeLocalArea ────────────────────────────────────────────────────────────

TEST(FarmMapRasterizerTest, LocalAreaOutputDimensions) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const auto pixels = RasterizeLocalArea(world, 80, 80, 30, 200, 200);
	EXPECT_EQ(pixels.size(), 200u * 200u * 4u);
}

TEST(FarmMapRasterizerTest, LocalAreaAllPixelsFullyOpaque) {
	const WorldData world = WorldGen::Generate(WorldGenParams{}, 42);
	const auto pixels = RasterizeLocalArea(world, 80, 80, 30, 100, 100);
	for (std::size_t i = 3; i < pixels.size(); i += 4)
		ASSERT_EQ(pixels[i], 255u) << "Alpha must be 255 at pixel " << (i / 4);
}

// ── ComputePinFraction ────────────────────────────────────────────────────────────

TEST(FarmMapRasterizerTest, ComputePinFractionAtOrigin) {
	// (0,0) in a 100×100 grid — after the axis flip, small coords appear near the bottom-right
	// of the texture: fraction (0.995, 0.995)
	const auto frac = ComputePinFraction({0, 0}, 100, 100);
	EXPECT_NEAR(frac.x, 0.995F, 0.001F);
	EXPECT_NEAR(frac.y, 0.995F, 0.001F);
}

TEST(FarmMapRasterizerTest, ComputePinFractionAtGridCenter) {
	// (49,49) in a 100×100 grid → fraction near (0.505, 0.505) after flip
	const auto frac = ComputePinFraction({49, 49}, 100, 100);
	EXPECT_NEAR(frac.x, 0.505F, 0.01F);
	EXPECT_NEAR(frac.y, 0.505F, 0.01F);
}

// ── ComputeLocalPinFraction ───────────────────────────────────────────────────────

TEST(FarmMapRasterizerTest, ComputeLocalPinFractionCenterIsMidpoint) {
	// The center coordinate itself must map to (0.5, 0.5)
	const auto frac = ComputeLocalPinFraction({50, 50}, {50, 50}, 20);
	EXPECT_NEAR(frac.x, 0.5F, 0.01F);
	EXPECT_NEAR(frac.y, 0.5F, 0.01F);
}

TEST(FarmMapRasterizerTest, ComputeLocalPinFractionTopLeftEdge) {
	// After the axis flip, the point at (center - radius) in world coords maps to screen (1,1).
	// Conversely, (center + radius) maps to screen (0,0).
	const auto fracLow = ComputeLocalPinFraction({30, 30}, {50, 50}, 20); // world bottom-right → screen (1,1)
	EXPECT_NEAR(fracLow.x, 1.0F, 0.01F);
	EXPECT_NEAR(fracLow.y, 1.0F, 0.01F);
	const auto fracHigh = ComputeLocalPinFraction({70, 70}, {50, 50}, 20); // world top-left → screen (0,0)
	EXPECT_NEAR(fracHigh.x, 0.0F, 0.01F);
	EXPECT_NEAR(fracHigh.y, 0.0F, 0.01F);
}

TEST(FarmMapRasterizerTest, ComputeLocalPinFractionOutsideWindow) {
	// A point well outside the window has frac outside [0,1]. After axis flip, the far-low-coord
	// point maps to frac > 1.
	const auto frac = ComputeLocalPinFraction({0, 0}, {80, 80}, 20);
	EXPECT_GT(frac.x, 1.0F);
	EXPECT_GT(frac.y, 1.0F);
}
