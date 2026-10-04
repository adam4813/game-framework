#pragma once

#include <array>

#include "../noise_generator.hpp"

namespace engine::noise::builtin {

/// 2D/3D Value noise.
/// Assigns a pseudo-random scalar to each integer grid corner, then tri/bilinearly
/// interpolates using the same quintic fade as Improved Perlin. Simpler and cheaper
/// than gradient noise; produces a blockier look that can be desirable for stylised
/// terrain or cloud textures.
class ValueGenerator final : public INoiseGenerator {
public:
	explicit ValueGenerator(int seed = 0);

	void SetSeed(int seed) override;

	/// 2D Value noise. Returns approximately [-1, 1].
	[[nodiscard]] float Sample2D(glm::vec2 pos) const override;

	/// 3D Value noise. Returns approximately [-1, 1].
	[[nodiscard]] float Sample3D(glm::vec3 pos) const override;

private:
	std::array<int, 512> perm_{};
	std::array<float, 512> values_{}; // random scalars in [-1, 1] indexed by permuted hash

	void BuildTables(int seed);
};

} // namespace engine::noise::builtin
