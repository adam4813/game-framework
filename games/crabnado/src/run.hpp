#pragma once

#include <cstdint>

#include <flecs.h>

// The run: one play of the arena from Play to death. Its state is one reflected singleton, so the Remote
// API can read and set it.
namespace crabnado {

enum class RunPhase { Playing, Molting, Lost };

// Singleton: the current run. `menu_paused` is the Esc pause menu, separate from `phase` so pausing mid-run
// resumes the same phase. `clock` counts up in seconds while the run is playing; `rock_kills` counts the kills a rock slam helped. `shells` counts
// toward the next molt, which needs `shells_needed`; `molt_level` starts at 1. `seed` is the RngState
// seed the run started from, so a run can be replayed.
struct RunState {
	RunPhase phase{RunPhase::Playing};
	float clock{0.0F};
	int kills{0};
	int rock_kills{0};
	int molt_level{1};
	int shells{0};
	int shells_needed{0};
	bool menu_paused{false};
	uint64_t seed{0};
};

// Shells needed for the molt after `moltLevel` (the prototype's 6 + level * 5, less 5 so the first is 6).
[[nodiscard]] int ShellsNeeded(int moltLevel);

// Registers and reflects RunState and the RunClock system. Call from the module constructor.
void RegisterRun(const flecs::world& world);

// Starts a fresh run: resets RunState and reseeds RngState with a seed drawn from it.
void StartRun(const flecs::world& world);

// Opens or closes the pause menu (pausing the scene). Does nothing unless the run is playing.
void SetMenuPaused(const flecs::world& world, bool paused);

// Ends the run as lost and pauses the scene. Does nothing unless the run is playing.
void LoseRun(const flecs::world& world);

} // namespace crabnado
