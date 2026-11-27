#include <gtest/gtest.h>

#include <array>

#include "engine/noise/builtin/perlin_strategy.hpp"
#include "engine/noise/builtin/permutation.hpp"
#include "engine/noise/builtin/simplex_strategy.hpp"
#include "engine/noise/builtin/value_generator.hpp"
#include "engine/noise/builtin/worley_generator.hpp"

// A seed must make the same noise on every platform, so a procedural world is the same on Linux, Windows and the
// web. The values below were recorded once; CI runs this under libstdc++ and the MSVC STL, so a table built through
// anything library-dependent (std::shuffle, a std distribution) fails on one of them. A different permutation moves
// the samples by tenths, so the float tolerance only absorbs last-bit differences between compilers.
using namespace engine::noise::builtin;

namespace {

constexpr int kSeed = 1337;
constexpr float kTolerance = 1e-5f;
constexpr glm::vec2 kPointA{3.7f, 1.2f};
constexpr glm::vec2 kPointB{-12.31f, 40.5f};

} // namespace

TEST(NoiseDeterminismTest, PermutationIsTheSameForASeedOnEveryPlatform) {
	auto rng = SeededNoiseRng(kSeed);
	const std::array<int, 256> p = ShuffledPermutation(rng);
	constexpr std::array<int, 8> kExpected{193, 238, 88, 59, 97, 135, 140, 130};
	for (int i = 0; i < 8; ++i) {
		EXPECT_EQ(p[i], kExpected[i]) << "index " << i;
	}
}

TEST(NoiseDeterminismTest, PermutationHoldsEachValueOnce) {
	auto rng = SeededNoiseRng(kSeed);
	const std::array<int, 256> p = ShuffledPermutation(rng);
	std::array<int, 256> seen{};
	for (const int v : p) {
		ASSERT_GE(v, 0);
		ASSERT_LT(v, 256);
		++seen[v];
	}
	for (int v = 0; v < 256; ++v) {
		EXPECT_EQ(seen[v], 1) << "value " << v;
	}
}

TEST(NoiseDeterminismTest, GeneratorsSampleTheSameForASeedOnEveryPlatform) {
	EXPECT_NEAR(PerlinGenerator(kSeed).Sample2D(kPointA), -0.0477928147f, kTolerance);
	EXPECT_NEAR(PerlinGenerator(kSeed).Sample2D(kPointB), 0.293599665f, kTolerance);
	EXPECT_NEAR(SimplexGenerator(kSeed).Sample2D(kPointA), -0.681346774f, kTolerance);
	EXPECT_NEAR(SimplexGenerator(kSeed).Sample2D(kPointB), -0.687638283f, kTolerance);
	EXPECT_NEAR(ValueGenerator(kSeed).Sample2D(kPointA), -0.142079964f, kTolerance);
	EXPECT_NEAR(ValueGenerator(kSeed).Sample2D(kPointB), -0.327378511f, kTolerance);
	EXPECT_NEAR(WorkleyGenerator(WorkleyMode::F1, kSeed).Sample2D(kPointA), -0.21395731f, kTolerance);
	EXPECT_NEAR(WorkleyGenerator(WorkleyMode::F1, kSeed).Sample2D(kPointB), -0.341654062f, kTolerance);
}
