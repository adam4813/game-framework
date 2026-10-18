#pragma once

#include <span>

namespace engine::audio {

// Sample rate of the procedural audio stream (mono f32). Matches most desktop devices and browser
// AudioContexts, so the backend usually skips resampling.
inline constexpr float kSynthSampleRate = 48000.0F;

// Procedural audio generator, rendered by the platform on the audio thread (see
// Platform::StartSynth). Render() fills `out` (mono, kSynthSampleRate) and may be asked for any
// frame count, including odd ones. It runs under the backend's audio lock: it must never lock,
// allocate, log, or touch raylib or Flecs. Read game input through SynthParam / SynthTrigger.
class SynthSource {
public:
	virtual ~SynthSource() = default;
	virtual void Render(std::span<float> out) noexcept = 0;
};

} // namespace engine::audio
