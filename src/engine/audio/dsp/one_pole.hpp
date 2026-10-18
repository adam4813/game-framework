#pragma once

#include <algorithm>
#include <cmath>

namespace engine::audio {

// One-pole low-pass smoother for control values: glides toward a target so a parameter written at
// 60 Hz by the game doesn't produce zipper noise. Configure() does the math; Next() is real-time safe.
class OnePole {
public:
	// `glideSeconds` is the time to cover 99% of a step.
	void Configure(const float glideSeconds, const float sampleRate) noexcept {
		const float samples = std::max(1.0F, glideSeconds * sampleRate);
		coeff_ = 1.0F - std::pow(0.01F, 1.0F / samples);
	}

	// Jumps straight to `value`, e.g. so a voice starts at its target instead of gliding up from 0.
	void Reset(const float value) noexcept { value_ = value; }

	float Next(const float target) noexcept {
		value_ += coeff_ * (target - value_);
		return value_;
	}

	[[nodiscard]] float Value() const noexcept { return value_; }

private:
	float value_{0.0F};
	float coeff_{1.0F};
};

} // namespace engine::audio
