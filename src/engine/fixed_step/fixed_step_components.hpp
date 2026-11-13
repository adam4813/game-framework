#pragma once

#include <algorithm>
#include <cstdint>

namespace engine::fixed_step {

// Singleton: how fast the simulation Ticks and how far one Frame may catch up. Set it before the first Frame; a change
// takes effect on the next Frame.
struct FixedStepConfig {
	// Ticks per second. The fixed dt is 1 / tick_rate.
	int tick_rate{60};
	// The most Ticks one Frame runs. Time beyond them is dropped (and logged at debug level), so a long stall
	// doesn't make the next Frame run ever more Ticks to catch up.
	int max_catch_up_ticks{4};

	// The fixed dt, in seconds, every FixedStep system gets as `it.delta_time()`. A tick_rate below 1 counts as 1.
	[[nodiscard]] float TickDeltaTime() const { return 1.0F / static_cast<float>(std::max(tick_rate, 1)); }
};

// Singleton: where the fixed step stands. Written by RunFrame and RunTick only; read it, never write it.
struct FixedStepState {
	// The Ticks that have run. During a Tick it is that Tick's index (the first Tick is 0), so a FixedStep system can
	// stamp what it does against it. A paused Frame runs none and leaves it alone.
	uint64_t tick{0};
	// Frame time not yet spent on a Tick, in seconds: always less than one fixed dt after a Frame.
	double accumulator{0.0};
	// The fraction of a Tick left in the accumulator after the last Frame's Ticks, in [0, 1): how far the present is
	// between the last Tick and the next, for render interpolation. Nothing in the Framework reads it yet.
	float alpha{0.0F};
};

} // namespace engine::fixed_step
