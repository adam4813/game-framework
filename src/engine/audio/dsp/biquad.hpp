#pragma once

namespace engine::audio {

// Second-order IIR filter with RBJ "Audio EQ Cookbook" coefficients, run in transposed direct
// form II. Build coefficients off the audio thread (they call trig functions) and assign them;
// Process() is real-time safe. Swapping coefficients keeps the filter state, so a slow sweep
// stays smooth.
class Biquad {
public:
	// Normalised coefficients (a0 divided out).
	struct Coeffs {
		float b0{1.0F};
		float b1{0.0F};
		float b2{0.0F};
		float a1{0.0F};
		float a2{0.0F};
	};

	static Coeffs LowPass(float cutoffHz, float q, float sampleRate);
	static Coeffs HighPass(float cutoffHz, float q, float sampleRate);
	// Constant 0 dB peak gain at `centerHz`.
	static Coeffs BandPass(float centerHz, float q, float sampleRate);

	Biquad() = default;
	explicit Biquad(const Coeffs& coeffs) noexcept : c_{coeffs} {}

	void SetCoeffs(const Coeffs& coeffs) noexcept { c_ = coeffs; }

	float Process(const float in) noexcept {
		const float out = c_.b0 * in + z1_;
		z1_ = c_.b1 * in - c_.a1 * out + z2_;
		z2_ = c_.b2 * in - c_.a2 * out;
		return out;
	}

	void Reset() noexcept { z1_ = z2_ = 0.0F; }

private:
	Coeffs c_{};
	float z1_{0.0F};
	float z2_{0.0F};
};

} // namespace engine::audio
