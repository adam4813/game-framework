#include "perlin_strategy.hpp"

#include <algorithm>
#include <numeric>
#include <random>

#include <glm/glm.hpp>

namespace engine::noise::builtin {

namespace {

// Improved quintic fade for each axis: smooths transitions between grid cells.
glm::vec3 Fade(const glm::vec3 t) { return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f); }

// Maps a hash value to one of 12 gradient directions (edges of a unit cube).
// Ken Perlin's improved noise gradient function (2002).
float Grad(const int hash, const glm::vec3 pos) {
	const int h = hash & 15;
	const float u = h < 8 ? pos.x : pos.y;
	const float v = h < 4 ? pos.y : (h == 12 || h == 14 ? pos.x : pos.z);
	return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
}

} // namespace

PerlinGenerator::PerlinGenerator(const int seed) { BuildPermutationTable(seed); }

void PerlinGenerator::SetSeed(const int seed) { BuildPermutationTable(seed); }

void PerlinGenerator::BuildPermutationTable(const int seed) {
	std::array<int, 256> p{};
	std::iota(p.begin(), p.end(), 0);
	std::mt19937 rng(static_cast<uint32_t>(seed));
	std::ranges::shuffle(p, rng);
	for (int i = 0; i < 256; ++i) {
		perm_[i] = p[i];
		perm_[i + 256] = p[i];
	}
}

float PerlinGenerator::Sample2D(const glm::vec2 pos) const { return Sample3D({pos, 0.0f}); }

float PerlinGenerator::Sample3D(const glm::vec3 pos) const {
	// Integer unit cube corner and fractional offset.
	const glm::ivec3 c = glm::ivec3(glm::floor(pos)) & 255;
	const glm::vec3 f = glm::fract(pos);
	const glm::vec3 fade = Fade(f);

	// The "far" corner offsets (f - 1) for the 4 corners on the +x/+y/+z sides.
	const glm::vec3 d = f - 1.0f;

	// Hash coordinates of the 8 cube corners.
	const int a = perm_[c.x] + c.y;
	const int aa = perm_[a] + c.z;
	const int ab = perm_[a + 1] + c.z;
	const int b = perm_[c.x + 1] + c.y;
	const int ba = perm_[b] + c.z;
	const int bb = perm_[b + 1] + c.z;

	// Trilinear interpolation of gradients at each corner.
	return glm::mix(
		glm::mix(
			glm::mix(Grad(perm_[aa], f), Grad(perm_[ba], {d.x, f.y, f.z}), fade.x),
			glm::mix(Grad(perm_[ab], {f.x, d.y, f.z}), Grad(perm_[bb], {d.x, d.y, f.z}), fade.x),
			fade.y
		),
		glm::mix(
			glm::mix(Grad(perm_[aa + 1], {f.x, f.y, d.z}), Grad(perm_[ba + 1], {d.x, f.y, d.z}), fade.x),
			glm::mix(Grad(perm_[ab + 1], {f.x, d.y, d.z}), Grad(perm_[bb + 1], {d.x, d.y, d.z}), fade.x),
			fade.y
		),
		fade.z
	);
}

} // namespace engine::noise::builtin
