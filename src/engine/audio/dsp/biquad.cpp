#include "biquad.hpp"

#include <cmath>
#include <numbers>

namespace engine::audio {

namespace {

struct Prewarp {
	float cosW;
	float alpha;
};

Prewarp Warp(const float hz, const float q, const float sampleRate) {
	const float w = 2.0F * std::numbers::pi_v<float> * hz / sampleRate;
	return {.cosW = std::cos(w), .alpha = std::sin(w) / (2.0F * q)};
}

Biquad::Coeffs
Normalise(const float b0, const float b1, const float b2, const float a0, const float a1, const float a2) {
	return {.b0 = b0 / a0, .b1 = b1 / a0, .b2 = b2 / a0, .a1 = a1 / a0, .a2 = a2 / a0};
}

} // namespace

Biquad::Coeffs Biquad::LowPass(const float cutoffHz, const float q, const float sampleRate) {
	const auto [cosW, alpha] = Warp(cutoffHz, q, sampleRate);
	const float b = (1.0F - cosW) / 2.0F;
	return Normalise(b, 1.0F - cosW, b, 1.0F + alpha, -2.0F * cosW, 1.0F - alpha);
}

Biquad::Coeffs Biquad::HighPass(const float cutoffHz, const float q, const float sampleRate) {
	const auto [cosW, alpha] = Warp(cutoffHz, q, sampleRate);
	const float b = (1.0F + cosW) / 2.0F;
	return Normalise(b, -(1.0F + cosW), b, 1.0F + alpha, -2.0F * cosW, 1.0F - alpha);
}

Biquad::Coeffs Biquad::BandPass(const float centerHz, const float q, const float sampleRate) {
	const auto [cosW, alpha] = Warp(centerHz, q, sampleRate);
	return Normalise(alpha, 0.0F, -alpha, 1.0F + alpha, -2.0F * cosW, 1.0F - alpha);
}

} // namespace engine::audio
