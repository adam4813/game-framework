#pragma once

#include <cmath>
#include <numbers>

namespace engine::audio {

// Phase-accumulator sine oscillator. The frequency is passed per sample so a smoothed parameter
// can glide it without clicks; the phase stays continuous across frequency changes. Real-time safe.
class SineOsc {
public:
	// Returns the current sample, then advances the phase by one sample at `frequencyHz`.
	float Next(const float frequencyHz, const float sampleRate) noexcept {
		const float out = std::sin(2.0F * std::numbers::pi_v<float> * phase_);
		phase_ += frequencyHz / sampleRate;
		phase_ -= std::floor(phase_); // wrap to [0, 1) so precision doesn't decay over long runs
		return out;
	}

	void Reset() noexcept { phase_ = 0.0F; }

private:
	float phase_{0.0F};
};

} // namespace engine::audio
