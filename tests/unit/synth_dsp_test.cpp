#include <algorithm>
#include <cmath>

#include <gtest/gtest.h>

#include "engine/audio/dsp/ad_envelope.hpp"
#include "engine/audio/dsp/biquad.hpp"
#include "engine/audio/dsp/one_pole.hpp"
#include "engine/audio/dsp/sine_osc.hpp"
#include "engine/audio/dsp/white_noise.hpp"

using namespace engine::audio;

namespace {
constexpr float kRate = 48000.0F;

// Settled output amplitude of `filter` driven by DC (every sample 1) or by Nyquist (+1, -1, ...).
float SettledGain(Biquad filter, const bool nyquist) {
	float out = 0.0F;
	for (int i = 0; i < 4800; ++i) {
		const float in = nyquist && (i % 2 == 1) ? -1.0F : 1.0F;
		out = filter.Process(in);
	}
	return std::abs(out);
}
} // namespace

TEST(SineOscTest, one_kilohertz_peaks_a_quarter_period_in_and_returns_to_zero_at_half) {
	SineOsc osc;
	// 1 kHz at 48 kHz is 48 samples per period: sample 12 is the peak, sample 24 the zero crossing.
	float samples[25];
	for (float& s : samples) {
		s = osc.Next(1000.0F, kRate);
	}
	EXPECT_NEAR(samples[0], 0.0F, 1e-6F);
	EXPECT_NEAR(samples[12], 1.0F, 1e-5F);
	EXPECT_NEAR(samples[24], 0.0F, 1e-5F);
}

TEST(WhiteNoiseTest, stays_in_unit_range_and_is_not_silent_even_with_a_zero_seed) {
	// A raw xorshift state of 0 is a fixed point; seed 0 must still produce noise.
	WhiteNoise noise{0};
	float peak = 0.0F;
	for (int i = 0; i < 10000; ++i) {
		const float s = noise.Next();
		ASSERT_GE(s, -1.0F);
		ASSERT_LT(s, 1.0F);
		peak = std::max(peak, std::abs(s));
	}
	EXPECT_GT(peak, 0.9F);
}

TEST(BiquadTest, low_pass_keeps_dc_and_kills_nyquist_and_high_pass_does_the_opposite) {
	const Biquad lowPass{Biquad::LowPass(1000.0F, 0.707F, kRate)};
	EXPECT_NEAR(SettledGain(lowPass, false), 1.0F, 1e-3F);
	EXPECT_NEAR(SettledGain(lowPass, true), 0.0F, 1e-3F);

	const Biquad highPass{Biquad::HighPass(1000.0F, 0.707F, kRate)};
	EXPECT_NEAR(SettledGain(highPass, false), 0.0F, 1e-3F);
	EXPECT_NEAR(SettledGain(highPass, true), 1.0F, 1e-3F);
}

TEST(AdEnvelopeTest, is_silent_until_triggered_then_peaks_after_attack_and_decays_60db_after_decay) {
	AdEnvelope env;
	env.Configure(0.001F, 0.1F, kRate); // 48 samples up, 4800 samples down to -60 dB
	EXPECT_EQ(env.Next(), 0.0F);
	EXPECT_FALSE(env.IsActive());

	env.Trigger();
	float level = 0.0F;
	for (int i = 0; i < 48; ++i) {
		level = env.Next();
	}
	EXPECT_NEAR(level, 1.0F, 1e-3F);

	for (int i = 0; i < 4800; ++i) {
		level = env.Next();
	}
	EXPECT_LE(level, 0.001F);
	EXPECT_FALSE(env.IsActive());
}

TEST(OnePoleTest, glides_to_99_percent_of_a_step_in_the_configured_time_not_instantly) {
	OnePole smoother;
	smoother.Configure(0.01F, kRate); // 480 samples
	smoother.Reset(200.0F);

	float value = 0.0F;
	for (int i = 0; i < 240; ++i) {
		value = smoother.Next(300.0F);
	}
	EXPECT_GT(value, 250.0F);
	EXPECT_LT(value, 295.0F);

	for (int i = 0; i < 240; ++i) {
		value = smoother.Next(300.0F);
	}
	EXPECT_NEAR(value, 299.0F, 0.1F);
}
