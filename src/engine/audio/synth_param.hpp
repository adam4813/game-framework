#pragma once

#include <atomic>

namespace engine::audio {

// Game→audio continuous control (a whistle's pitch, a hiss level). Any game thread calls Set();
// the audio callback reads Target() once per render and glides toward it with a OnePole, so a
// 60 Hz update doesn't zipper. Lock-free; last write wins.
class SynthParam {
public:
	explicit SynthParam(const float initial = 0.0F) noexcept : target_{initial} {}

	void Set(const float value) noexcept { target_.store(value, std::memory_order_relaxed); }
	[[nodiscard]] float Target() const noexcept { return target_.load(std::memory_order_relaxed); }

private:
	static_assert(std::atomic<float>::is_always_lock_free, "the audio callback must never take a lock");
	std::atomic<float> target_;
};

} // namespace engine::audio
