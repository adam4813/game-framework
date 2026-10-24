#pragma once

#include <cstdint>

#include <flecs.h>

// The run: one play of the arena from Play to death. Its state is one reflected singleton, so the Remote
// API can read and set it.
namespace crabnado {

enum class RunPhase { Playing, Lost };

// Singleton: the current run. `clock` counts up in seconds while the run is playing. `seed` is the
// RngState seed the run started from, so a run can be replayed.
struct RunState {
	RunPhase phase{RunPhase::Playing};
	float clock{0.0F};
	int kills{0};
	uint64_t seed{0};
};

// Registers and reflects RunState and the RunClock system. Call from the module constructor.
void RegisterRun(const flecs::world& world);

// Starts a fresh run: resets RunState and reseeds RngState with a seed drawn from it.
void StartRun(const flecs::world& world);

// Ends the run as lost and pauses the scene. Does nothing unless the run is playing.
void LoseRun(const flecs::world& world);

} // namespace crabnado
