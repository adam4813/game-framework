#pragma once

#include <span>

#include "engine/engine.hpp"

namespace game {

// Sandbox demo of the procedural audio stream: a gliding sine "whistle", band-passed hiss, and a
// resonant relay click — one of each kind of voice the DSP kit is meant for. Controls are written
// from the game thread; Render() runs on the audio thread.
class SynthDemoVoice final : public engine::audio::SynthSource {
public:
	SynthDemoVoice();

	engine::audio::SynthParam whistleHz{440.0F};
	engine::audio::SynthParam whistleLevel{0.2F};
	engine::audio::SynthParam hissLevel{0.0F};
	engine::audio::SynthTrigger click;

	void Render(std::span<float> out) noexcept override;

private:
	engine::audio::SineOsc whistle_;
	engine::audio::OnePole whistleHzSmooth_;
	engine::audio::OnePole whistleLevelSmooth_;

	engine::audio::WhiteNoise hissNoise_{1};
	engine::audio::Biquad hissFilter_;
	engine::audio::OnePole hissLevelSmooth_;

	engine::audio::WhiteNoise clickNoise_{2};
	engine::audio::Biquad clickFilter_;
	engine::audio::AdEnvelope clickEnvelope_;
};

} // namespace game
