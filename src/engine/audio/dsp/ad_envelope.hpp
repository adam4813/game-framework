#pragma once

#include <algorithm>
#include <cmath>

namespace engine::audio {

// One-shot attack/decay envelope: a linear rise to 1 over the attack, then an exponential fall that
// reaches -60 dB at the end of the decay, after which it goes idle at exactly 0. Retriggering
// mid-flight restarts the attack from the current level, so it never clicks. Configure() does the
// math (call it off the audio thread or once at init); Trigger() and Next() are real-time safe.
class AdEnvelope {
public:
	void Configure(const float attackSeconds, const float decaySeconds, const float sampleRate) noexcept {
		attackSamples_ = std::max(1, static_cast<int>(attackSeconds * sampleRate));
		decaySamples_ = std::max(1, static_cast<int>(decaySeconds * sampleRate));
		decayFactor_ = std::pow(kSilence, 1.0F / static_cast<float>(decaySamples_));
	}

	void Trigger() noexcept {
		stage_ = Stage::Attack;
		remaining_ = attackSamples_;
		attackStep_ = (1.0F - level_) / static_cast<float>(attackSamples_);
	}

	[[nodiscard]] bool IsActive() const noexcept { return stage_ != Stage::Idle; }

	// Advances one sample and returns the new level in [0, 1].
	float Next() noexcept {
		switch (stage_) {
		case Stage::Idle: return 0.0F;
		case Stage::Attack:
			level_ += attackStep_;
			if (--remaining_ <= 0) {
				level_ = 1.0F;
				stage_ = Stage::Decay;
				remaining_ = decaySamples_;
			}
			return level_;
		case Stage::Decay:
			level_ *= decayFactor_;
			if (--remaining_ <= 0) {
				level_ = 0.0F;
				stage_ = Stage::Idle;
			}
			return level_;
		}
		return 0.0F;
	}

private:
	enum class Stage { Idle, Attack, Decay };

	static constexpr float kSilence = 0.001F; // -60 dB: where the decay lands

	Stage stage_{Stage::Idle};
	float level_{0.0F};
	float attackStep_{1.0F};
	float decayFactor_{0.0F};
	int attackSamples_{1};
	int decaySamples_{1};
	int remaining_{0}; // samples left in the current stage; stages count down so float drift can't stretch them
};

} // namespace engine::audio
