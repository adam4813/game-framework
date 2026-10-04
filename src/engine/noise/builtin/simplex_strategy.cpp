#include "simplex_strategy.hpp"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <random>

#include <glm/glm.hpp>

namespace engine::noise::builtin {

namespace {

// Gradient vectors for 3D simplex (12 edges of a cube, same as improved Perlin).
constexpr int kGrad3[12][3] = {
	{1, 1, 0},
	{-1, 1, 0},
	{1, -1, 0},
	{-1, -1, 0},
	{1, 0, 1},
	{-1, 0, 1},
	{1, 0, -1},
	{-1, 0, -1},
	{0, 1, 1},
	{0, -1, 1},
	{0, 1, -1},
	{0, -1, -1}
};

// Dot product with a 3D gradient vector indexed by hash % 12.
float Dot3(const int* g, const float x, const float y, const float z) { return g[0] * x + g[1] * y + g[2] * z; }

// 2D gradient directions (8 axes).
constexpr int kGrad2[8][2] = {{1, 1}, {-1, 1}, {1, -1}, {-1, -1}, {1, 0}, {-1, 0}, {0, 1}, {0, -1}};

float Dot2(const int* g, const float x, const float y) { return g[0] * x + g[1] * y; }

} // namespace

SimplexGenerator::SimplexGenerator(const int seed) { BuildPermutationTable(seed); }

void SimplexGenerator::SetSeed(const int seed) { BuildPermutationTable(seed); }

void SimplexGenerator::BuildPermutationTable(const int seed) {
	std::array<int, 256> p{};
	std::iota(p.begin(), p.end(), 0);
	std::mt19937 rng(static_cast<uint32_t>(seed));
	std::ranges::shuffle(p, rng);
	for (int i = 0; i < 256; ++i) {
		perm_[i] = p[i];
		perm_[i + 256] = p[i];
		perm_mod12_[i] = p[i] % 12;
		perm_mod12_[i + 256] = p[i] % 12;
	}
}

float SimplexGenerator::Sample2D(const glm::vec2 pos) const {
	constexpr float kF2 = 0.5f * (1.732050808f - 1.0f); // (sqrt(3) - 1) / 2
	constexpr float kG2 = (3.0f - 1.732050808f) / 6.0f; // (3 - sqrt(3)) / 6

	// Skew input to simplex grid.
	const float s = (pos.x + pos.y) * kF2;
	const int i = static_cast<int>(std::floor(pos.x + s));
	const int j = static_cast<int>(std::floor(pos.y + s));

	// Unskew back to (x, y) space for the first corner.
	const float t = static_cast<float>(i + j) * kG2;
	const float x0 = pos.x - (static_cast<float>(i) - t);
	const float y0 = pos.y - (static_cast<float>(j) - t);

	// Which simplex triangle are we in?
	const int i1 = (x0 > y0) ? 1 : 0;
	const int j1 = (x0 > y0) ? 0 : 1;

	// Other two corners.
	const float x1 = x0 - static_cast<float>(i1) + kG2;
	const float y1 = y0 - static_cast<float>(j1) + kG2;
	const float x2 = x0 - 1.0f + 2.0f * kG2;
	const float y2 = y0 - 1.0f + 2.0f * kG2;

	// Hash the corner coordinates.
	const int ii = i & 255;
	const int jj = j & 255;
	const int gi0 = perm_[ii + perm_[jj]] % 8;
	const int gi1 = perm_[ii + i1 + perm_[jj + j1]] % 8;
	const int gi2 = perm_[ii + 1 + perm_[jj + 1]] % 8;

	// Contribution from each corner (falloff t^4).
	float n0 = 0.0f;
	if (const float t0 = 0.5f - x0 * x0 - y0 * y0; t0 > 0) {
		n0 = t0 * t0 * t0 * t0 * Dot2(kGrad2[gi0], x0, y0);
	}
	float n1 = 0.0f;
	if (const float t1 = 0.5f - x1 * x1 - y1 * y1; t1 > 0) {
		n1 = t1 * t1 * t1 * t1 * Dot2(kGrad2[gi1], x1, y1);
	}
	float n2 = 0.0f;
	if (const float t2 = 0.5f - x2 * x2 - y2 * y2; t2 > 0) {
		n2 = t2 * t2 * t2 * t2 * Dot2(kGrad2[gi2], x2, y2);
	}

	// Scale to [-1, 1].
	return 70.0f * (n0 + n1 + n2);
}

float SimplexGenerator::Sample3D(const glm::vec3 pos) const {
	constexpr float kF3 = 1.0f / 3.0f;
	constexpr float kG3 = 1.0f / 6.0f;

	const float s = (pos.x + pos.y + pos.z) * kF3;
	const int i = static_cast<int>(std::floor(pos.x + s));
	const int j = static_cast<int>(std::floor(pos.y + s));
	const int k = static_cast<int>(std::floor(pos.z + s));

	const float t = static_cast<float>(i + j + k) * kG3;
	const float x0 = pos.x - (static_cast<float>(i) - t);
	const float y0 = pos.y - (static_cast<float>(j) - t);
	const float z0 = pos.z - (static_cast<float>(k) - t);

	// Determine which simplex tetrahedron we're in.
	int i1, j1, k1, i2, j2, k2;
	if (x0 >= y0) {
		if (y0 >= z0) {
			i1 = 1;
			j1 = 0;
			k1 = 0;
			i2 = 1;
			j2 = 1;
			k2 = 0;
		}
		else if (x0 >= z0) {
			i1 = 1;
			j1 = 0;
			k1 = 0;
			i2 = 1;
			j2 = 0;
			k2 = 1;
		}
		else {
			i1 = 0;
			j1 = 0;
			k1 = 1;
			i2 = 1;
			j2 = 0;
			k2 = 1;
		}
	}
	else {
		if (y0 < z0) {
			i1 = 0;
			j1 = 0;
			k1 = 1;
			i2 = 0;
			j2 = 1;
			k2 = 1;
		}
		else if (x0 < z0) {
			i1 = 0;
			j1 = 1;
			k1 = 0;
			i2 = 0;
			j2 = 1;
			k2 = 1;
		}
		else {
			i1 = 0;
			j1 = 1;
			k1 = 0;
			i2 = 1;
			j2 = 1;
			k2 = 0;
		}
	}

	const float x1 = x0 - static_cast<float>(i1) + kG3;
	const float y1 = y0 - static_cast<float>(j1) + kG3;
	const float z1 = z0 - static_cast<float>(k1) + kG3;
	const float x2 = x0 - static_cast<float>(i2) + 2.0f * kG3;
	const float y2 = y0 - static_cast<float>(j2) + 2.0f * kG3;
	const float z2 = z0 - static_cast<float>(k2) + 2.0f * kG3;
	const float x3 = x0 - 1.0f + 3.0f * kG3;
	const float y3 = y0 - 1.0f + 3.0f * kG3;
	const float z3 = z0 - 1.0f + 3.0f * kG3;

	const int ii = i & 255;
	const int jj = j & 255;
	const int kk = k & 255;
	const int gi0 = perm_mod12_[ii + perm_[jj + perm_[kk]]];
	const int gi1 = perm_mod12_[ii + i1 + perm_[jj + j1 + perm_[kk + k1]]];
	const int gi2 = perm_mod12_[ii + i2 + perm_[jj + j2 + perm_[kk + k2]]];
	const int gi3 = perm_mod12_[ii + 1 + perm_[jj + 1 + perm_[kk + 1]]];

	float n0 = 0.0f;
	if (const float t0 = 0.6f - x0 * x0 - y0 * y0 - z0 * z0; t0 > 0) {
		n0 = t0 * t0 * t0 * t0 * Dot3(kGrad3[gi0], x0, y0, z0);
	}
	float n1 = 0.0f;
	if (const float t1 = 0.6f - x1 * x1 - y1 * y1 - z1 * z1; t1 > 0) {
		n1 = t1 * t1 * t1 * t1 * Dot3(kGrad3[gi1], x1, y1, z1);
	}
	float n2 = 0.0f;
	if (const float t2 = 0.6f - x2 * x2 - y2 * y2 - z2 * z2; t2 > 0) {
		n2 = t2 * t2 * t2 * t2 * Dot3(kGrad3[gi2], x2, y2, z2);
	}
	float n3 = 0.0f;
	if (const float t3 = 0.6f - x3 * x3 - y3 * y3 - z3 * z3; t3 > 0) {
		n3 = t3 * t3 * t3 * t3 * Dot3(kGrad3[gi3], x3, y3, z3);
	}

	return 32.0f * (n0 + n1 + n2 + n3);
}

} // namespace engine::noise::builtin
