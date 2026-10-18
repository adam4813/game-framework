#include "synth_demo_voice.hpp"

#include <algorithm>

using namespace engine::audio;

namespace game {

namespace {
constexpr float kGlideSeconds = 0.05F;
constexpr float kHissCenterHz = 3000.0F;
constexpr float kHissQ = 0.7F;
constexpr float kClickCenterHz = 2500.0F;
constexpr float kClickQ = 8.0F; // resonant, for a metallic ring
constexpr float kClickAttackSeconds = 0.0005F;
constexpr float kClickDecaySeconds = 0.03F;
constexpr float kClickGain = 1.5F;
} // namespace

SynthDemoVoice::SynthDemoVoice() {
	whistleHzSmooth_.Configure(kGlideSeconds, kSynthSampleRate);
	whistleHzSmooth_.Reset(whistleHz.Target());
	whistleLevelSmooth_.Configure(kGlideSeconds, kSynthSampleRate);
	hissLevelSmooth_.Configure(kGlideSeconds, kSynthSampleRate);
	hissFilter_.SetCoeffs(Biquad::BandPass(kHissCenterHz, kHissQ, kSynthSampleRate));
	clickFilter_.SetCoeffs(Biquad::BandPass(kClickCenterHz, kClickQ, kSynthSampleRate));
	clickEnvelope_.Configure(kClickAttackSeconds, kClickDecaySeconds, kSynthSampleRate);
}

void SynthDemoVoice::Render(const std::span<float> out) noexcept {
	// Read each control once per render; the smoothers glide between renders.
	const float hzTarget = whistleHz.Target();
	const float whistleTarget = whistleLevel.Target();
	const float hissTarget = hissLevel.Target();
	if (click.Consume()) {
		clickEnvelope_.Trigger();
	}

	for (float& sample : out) {
		const float tone =
			whistle_.Next(whistleHzSmooth_.Next(hzTarget), kSynthSampleRate) * whistleLevelSmooth_.Next(whistleTarget);
		const float hiss = hissFilter_.Process(hissNoise_.Next()) * hissLevelSmooth_.Next(hissTarget);
		const float relay = clickFilter_.Process(clickNoise_.Next()) * clickEnvelope_.Next() * kClickGain;
		sample = std::clamp(tone + hiss + relay, -1.0F, 1.0F);
	}
}

} // namespace game
