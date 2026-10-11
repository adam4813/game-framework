#pragma once

#include "../noise_generator.hpp"

#include <array>

namespace engine::noise::builtin {

/// Improved Perlin gradient noise (Ken Perlin, 2002).
/// Uses a seeded permutation table and the improved quintic fade function.
/// Implements INoiseGenerator as the "perlin" algorithm registered by BuiltinNoiseModule.
class PerlinGenerator final : public INoiseGenerator {
public:
	explicit PerlinGenerator(int seed = 0);

	void SetSeed(int seed) override;

	/// 2D Perlin noise — delegates to Sample3D with z = 0.
	[[nodiscard]] float Sample2D(glm::vec2 pos) const override;

	/// 3D Perlin gradient noise. Returns approximately [-1, 1].
	[[nodiscard]] float Sample3D(glm::vec3 pos) const override;

private:
	std::array<int, 512> perm_{};

	void BuildPermutationTable(int seed);
};

} // namespace engine::noise::builtin
