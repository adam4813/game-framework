#pragma once

#include "noise_generator.hpp"

#include <glm/glm.hpp>

#include <memory>
#include <vector>

namespace engine::noise {

/// FBM parameters — scripting value type.
struct FractalNoiseParams {
	int octaves{4};
	float persistence{0.5f};
	float lacunarity{2.0f};
};

/// Move-only owning sampling handle. Obtained from NoiseRegistry::GetSampler.
/// Each instance has its own isolated INoiseGenerator — seeding does not affect other consumers.
struct NoiseSampler {
	NoiseSampler() = default;
	explicit NoiseSampler(std::unique_ptr<INoiseGenerator> gen) : generator_(std::move(gen)) {}
	NoiseSampler(NoiseSampler&&) = default;
	NoiseSampler& operator=(NoiseSampler&&) = default;
	NoiseSampler(const NoiseSampler&) = delete;
	NoiseSampler& operator=(const NoiseSampler&) = delete;

	[[nodiscard]] bool IsValid() const { return generator_ != nullptr; }
	[[nodiscard]] float Sample2D(glm::vec2 pos) const;
	[[nodiscard]] float Sample3D(glm::vec3 pos) const;
	[[nodiscard]] float FractalNoise2D(glm::vec2 pos, FractalNoiseParams params) const;
	[[nodiscard]] float FractalNoise3D(glm::vec3 pos, FractalNoiseParams params) const;

private:
	std::unique_ptr<INoiseGenerator> generator_;
};

/// Generate a flat row-major width×height heightmap using FractalNoise2D.
/// Each cell at (col, row) is sampled at: offset + vec2(col, row) * scale.
/// Output values are contrast-stretched to [0, 1].
[[nodiscard]] std::vector<float> GenerateHeightMap(
	const NoiseSampler& sampler,
	int width,
	int height,
	glm::vec2 offset,
	float scale,
	FractalNoiseParams params
);

} // namespace engine::noise
