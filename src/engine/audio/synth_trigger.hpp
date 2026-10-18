#pragma once

#include <atomic>
#include <cstdint>

namespace engine::audio {

// Game→audio one-shot (a relay click, a stamp thud). Any game thread calls Fire(); the audio
// callback calls Consume() once per render to learn whether to start the voice. Fires between two
// renders coalesce into one, so a backlog built up while web audio is suspended doesn't replay as
// a burst on unlock. Lock-free; carries no per-event data (use a ring buffer if that's needed).
class SynthTrigger {
public:
	void Fire() noexcept { fired_.fetch_add(1, std::memory_order_relaxed); }

	// Audio thread only. True if Fire() was called since the last Consume().
	bool Consume() noexcept {
		const std::uint32_t fired = fired_.load(std::memory_order_relaxed);
		const bool any = fired != seen_;
		seen_ = fired;
		return any;
	}

private:
	std::atomic<std::uint32_t> fired_{0};
	std::uint32_t seen_{0}; // owned by the audio thread
};

} // namespace engine::audio
