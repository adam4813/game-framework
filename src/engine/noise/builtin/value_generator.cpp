#include "value_generator.hpp"

#include <algorithm>
#include <numeric>
#include <random>

#include <glm/glm.hpp>

namespace engine::noise::builtin {

namespace {

// Quintic fade (same as Perlin improved) for smooth corner interpolation.
glm::vec3 Fade(const glm::vec3 t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

} // namespace

ValueGenerator::ValueGenerator(const int seed) { BuildTables(seed); }

void ValueGenerator::SetSeed(const int seed) { BuildTables(seed); }

void ValueGenerator::BuildTables(const int seed) {
	std::array<int, 256> p{};
	std::iota(p.begin(), p.end(), 0);
	std::mt19937 rng(static_cast<uint32_t>(seed));
	std::ranges::shuffle(p, rng);

	// Build a second shuffle for the value lookup (independent of the coordinate shuffle).
	std::uniform_real_distribution<float> dist(-1.0f, 1.0f);
	std::array<float, 256> vals{};
	for (auto& v : vals) v = dist(rng);

	for (int i = 0; i < 256; ++i) {
		perm_[i] = p[i];
		perm_[i + 256] = p[i];
		values_[i] = vals[p[i]]; // value at permuted index i
		values_[i + 256] = vals[p[i]];
	}
}

float ValueGenerator::Sample2D(const glm::vec2 pos) const { return Sample3D({pos, 0.0f}); }

float ValueGenerator::Sample3D(const glm::vec3 pos) const {
	const glm::ivec3 c = glm::ivec3(glm::floor(pos)) & 255;
	const glm::vec3 f = glm::fract(pos);
	const glm::vec3 fade = Fade(f);

	// Corner values via Perlin-style chained permutation: each lookup feeds the next,
	// keeping all indices within [0, 511] (perm_ size).
	const auto corner = [&](const int dx, const int dy, const int dz) {
		const int hz = perm_[(c.z + dz) & 255];
		const int hy = perm_[((c.y + dy) & 255) + hz];
		const int hx = perm_[((c.x + dx) & 255) + hy];
		return values_[hx];
	};

	// Trilinear interpolation with smooth fade curves.
	return glm::mix(
		glm::mix(
			glm::mix(corner(0, 0, 0), corner(1, 0, 0), fade.x),
			glm::mix(corner(0, 1, 0), corner(1, 1, 0), fade.x),
			fade.y
		),
		glm::mix(
			glm::mix(corner(0, 0, 1), corner(1, 0, 1), fade.x),
			glm::mix(corner(0, 1, 1), corner(1, 1, 1), fade.x),
			fade.y
		),
		fade.z
	);
}

} // namespace engine::noise::builtin
