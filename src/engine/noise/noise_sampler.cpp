#include "noise_sampler.hpp"

#include <algorithm>
#include <limits>

namespace engine::noise {

namespace {

template<typename TPos, typename TSampleFn>
float FractalNoise(
	const NoiseSampler& sampler,
	const TPos pos,
	const FractalNoiseParams params,
	TSampleFn&& sample_fn
) {
	if (!sampler.IsValid() || params.octaves <= 0) {
		return 0.0f;
	}

	float value = 0.0f;
	float amplitude = 1.0f;
	float frequency = 1.0f;
	float amplitude_sum = 0.0f;

	for (int octave = 0; octave < params.octaves; ++octave) {
		value += sample_fn(pos * frequency) * amplitude;
		amplitude_sum += amplitude;
		amplitude *= params.persistence;
		frequency *= params.lacunarity;
	}

	return amplitude_sum > 0.0f ? value / amplitude_sum : 0.0f;
}

} // namespace

float NoiseSampler::Sample2D(const glm::vec2 pos) const { return generator_ ? generator_->Sample2D(pos) : 0.0f; }

float NoiseSampler::Sample3D(const glm::vec3 pos) const { return generator_ ? generator_->Sample3D(pos) : 0.0f; }

float NoiseSampler::FractalNoise2D(const glm::vec2 pos, const FractalNoiseParams params) const {
	return FractalNoise(*this, pos, params, [this](const glm::vec2 sample_pos) { return Sample2D(sample_pos); });
}

float NoiseSampler::FractalNoise3D(const glm::vec3 pos, const FractalNoiseParams params) const {
	return FractalNoise(*this, pos, params, [this](const glm::vec3 sample_pos) { return Sample3D(sample_pos); });
}

std::vector<float> GenerateHeightMap(
	const NoiseSampler& sampler,
	const int width,
	const int height,
	const glm::vec2 offset,
	const float scale,
	const FractalNoiseParams params
) {
	if (width <= 0 || height <= 0) {
		return {};
	}

	std::vector<float> map(static_cast<size_t>(width) * static_cast<size_t>(height), 0.0f);
	if (!sampler.IsValid() || params.octaves <= 0) {
		return map;
	}

	float min_value = std::numeric_limits<float>::max();
	float max_value = std::numeric_limits<float>::lowest();

	for (int row = 0; row < height; ++row) {
		for (int col = 0; col < width; ++col) {
			const glm::vec2 pos = offset + glm::vec2(col, row) * scale;
			const float sample = sampler.FractalNoise2D(pos, params);
			map[static_cast<size_t>(row) * static_cast<size_t>(width) + static_cast<size_t>(col)] = sample;
			min_value = std::min(min_value, sample);
			max_value = std::max(max_value, sample);
		}
	}

	if (max_value <= min_value) {
		std::fill(map.begin(), map.end(), 0.5f);
		return map;
	}

	const float range = max_value - min_value;
	for (float& value : map) {
		value = (value - min_value) / range;
	}

	return map;
}

} // namespace engine::noise
