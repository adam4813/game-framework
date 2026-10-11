#pragma once

#include "../noise_generator.hpp"

#include <array>

namespace engine::noise::builtin {

/// 2D/3D Simplex noise (Ken Perlin, 2001).
/// Smoother and more isotropic than classic Perlin noise; avoids the directional
/// artifacts of the grid-aligned gradient approach.
class SimplexGenerator final : public INoiseGenerator {
public:
	explicit SimplexGenerator(int seed = 0);

	void SetSeed(int seed) override;

	/// 2D Simplex noise. Returns approximately [-1, 1].
	[[nodiscard]] float Sample2D(glm::vec2 pos) const override;

	/// 3D Simplex noise. Returns approximately [-1, 1].
	[[nodiscard]] float Sample3D(glm::vec3 pos) const override;

private:
	std::array<int, 512> perm_{};
	std::array<int, 512> perm_mod12_{};

	void BuildPermutationTable(int seed);
};

} // namespace engine::noise::builtin
