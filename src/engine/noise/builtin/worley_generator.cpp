#include "worley_generator.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numeric>
#include <random>

#include <glm/glm.hpp>

namespace engine::noise::builtin {

WorkleyGenerator::WorkleyGenerator(const WorkleyMode mode, const int seed) : mode_(mode) {
	BuildPermutationTable(seed);
}

void WorkleyGenerator::SetSeed(const int seed) { BuildPermutationTable(seed); }

void WorkleyGenerator::BuildPermutationTable(const int seed) {
	std::array<int, 256> p{};
	std::iota(p.begin(), p.end(), 0);
	std::mt19937 rng(static_cast<uint32_t>(seed));
	std::ranges::shuffle(p, rng);
	for (int i = 0; i < 256; ++i) {
		perm_[i] = p[i];
		perm_[i + 256] = p[i];
	}
}

glm::vec2 WorkleyGenerator::FeaturePoint2D(const int cx, const int cy) const {
	// Two independent hash lookups so X and Y offsets are not derived from the same byte.
	const int hx = perm_[(cx & 255) + perm_[cy & 255]];
	const int hy = perm_[((cx + 127) & 255) + perm_[(cy + 83) & 255]];
	return glm::vec2{cx + static_cast<float>(hx) / 255.0f, cy + static_cast<float>(hy) / 255.0f};
}

glm::vec3 WorkleyGenerator::FeaturePoint3D(const int cx, const int cy, const int cz) const {
	const int hx = perm_[(cx & 255) + perm_[cy & 255]];
	const int hy = perm_[((cx + 127) & 255) + perm_[(cy + 83) & 255]];
	const int hz = perm_[((cx + 211) & 255) + perm_[(cy + 163) & 255] + perm_[cz & 255]];
	return glm::vec3{
		cx + static_cast<float>(hx) / 255.0f,
		cy + static_cast<float>(hy) / 255.0f,
		cz + static_cast<float>(hz) / 255.0f
	};
}

float WorkleyGenerator::Combine(const float f1, const float f2) const {
	switch (mode_) {
	case WorkleyMode::F1: return f1;
	case WorkleyMode::F2: return f2;
	case WorkleyMode::F2MinusF1: return f2 - f1;
	case WorkleyMode::F1PlusF2: return f1 + f2;
	}
	return f1;
}

glm::vec2 WorkleyGenerator::SampleCellular2D(const glm::vec2 pos) const {
	const int xi = static_cast<int>(std::floor(pos.x));
	const int yi = static_cast<int>(std::floor(pos.y));

	float f1 = std::numeric_limits<float>::max();
	float f2 = std::numeric_limits<float>::max();

	for (int dy = -1; dy <= 1; ++dy) {
		for (int dx = -1; dx <= 1; ++dx) {
			const glm::vec2 fp = FeaturePoint2D(xi + dx, yi + dy);
			const float d = glm::distance(pos, fp);
			if (d < f1) {
				f2 = f1;
				f1 = d;
			}
			else if (d < f2) {
				f2 = d;
			}
		}
	}
	return {f1, f2};
}

glm::vec2 WorkleyGenerator::SampleCellular3D(const glm::vec3 pos) const {
	const int xi = static_cast<int>(std::floor(pos.x));
	const int yi = static_cast<int>(std::floor(pos.y));
	const int zi = static_cast<int>(std::floor(pos.z));

	float f1 = std::numeric_limits<float>::max();
	float f2 = std::numeric_limits<float>::max();

	for (int dz = -1; dz <= 1; ++dz) {
		for (int dy = -1; dy <= 1; ++dy) {
			for (int dx = -1; dx <= 1; ++dx) {
				const glm::vec3 fp = FeaturePoint3D(xi + dx, yi + dy, zi + dz);
				const float d = glm::distance(pos, fp);
				if (d < f1) {
					f2 = f1;
					f1 = d;
				}
				else if (d < f2) {
					f2 = d;
				}
			}
		}
	}
	return {f1, f2};
}

float WorkleyGenerator::Sample2D(const glm::vec2 pos) const {
	const glm::vec2 f = SampleCellular2D(pos);
	// F1 max distance ≈ 1.5 (diagonal of cell + neighbor); map to [-1, 1] approximately.
	return glm::clamp(Combine(f.x, f.y) * 1.5f - 1.0f, -1.0f, 1.0f);
}

float WorkleyGenerator::Sample3D(const glm::vec3 pos) const {
	const glm::vec2 f = SampleCellular3D(pos);
	return glm::clamp(Combine(f.x, f.y) * 1.5f - 1.0f, -1.0f, 1.0f);
}

} // namespace engine::noise::builtin
