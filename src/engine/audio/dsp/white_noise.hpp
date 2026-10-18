#pragma once

#include <cstdint>

namespace engine::audio {

// White-noise sample stream from a 32-bit xorshift generator: cheap, deterministic per seed, real-time
// safe. Not to be confused with engine::noise, which samples coherent 2D noise for procgen.
class WhiteNoise {
public:
	explicit WhiteNoise(const std::uint32_t seed = 0x9E3779B9U) noexcept :
			state_{seed != 0 ? seed : 0x9E3779B9U} {} // 0 is xorshift's fixed point and would stay silent

	// Next sample, uniform in [-1, 1).
	float Next() noexcept {
		state_ ^= state_ << 13;
		state_ ^= state_ >> 17;
		state_ ^= state_ << 5;
		// Top 24 bits map exactly onto a float mantissa: [0, 2^24) -> [-1, 1).
		return static_cast<float>(state_ >> 8) * (2.0F / 16777216.0F) - 1.0F;
	}

private:
	std::uint32_t state_;
};

} // namespace engine::audio
