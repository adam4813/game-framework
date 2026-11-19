#pragma once

#include <cstdint>

#include <flecs.h>

// The run: one play of the arena from Play to death. Its state is one reflected singleton, so the Remote
// API can read and set it.
namespace crabnado {

// Countdown holds the scene paused before play starts (3-2-1) and after a molt (a short "Ready..."), then hands over to
// Playing, or to Boss once the boss is due. It comes first so `phase >= Boss` still means the boss or the run's end.
enum class RunPhase { Countdown, Playing, Molting, Boss, Won, Lost };

// Whether the run is live: the clock counts, enemies spawn and the player can be hurt.
[[nodiscard]] constexpr bool IsRunning(const RunPhase phase) {
	return phase == RunPhase::Playing || phase == RunPhase::Boss;
}

// Singleton: the current run. `menu_paused` is the Esc pause menu, separate from `phase` so pausing mid-run
// resumes the same phase. `slams` counts the boss slams so far (kBossSlamsToWin wins the run), `slam_cooldown` is the
// seconds before the next can count and `banner_timer` how long the boss banner shows. `clock` counts up in seconds while the run is playing; `rock_kills` counts the kills a rock slam helped. `shells` counts
// toward the next molt, which needs `shells_needed`; `molt_level` starts at 1. `seed` is the RngState
// seed the run started from, so a run can be replayed. `countdown` is the seconds left of the start countdown; it
// runs on below 0, to -kGoTime, while "Go!" shows.
// `act` and `stage` (both 0-based; `stage == stages.size()` is the Boss Stage) say where the campaign director has the
// run. The Remote API jumps to a Stage by writing both; the director then enters it with `stage_clock` 0. Writing
// `stage_clock` alone just moves the Stage's gate. `stage_clock` and `act_clock` count up next to `clock`, which stays
// the campaign clock; all of them stand still unless the run is live (molts and countdowns pause them).
// `stage_kills_start` is `kills` when the Stage began, so a quota counts `kills - stage_kills_start`. `stage_banner` is
// the seconds left of the Stage-change banner, `challenge_cleared` of "Challenge cleared!". When the last normal Stage
// gates, `stage` moves to the Boss Stage with `phase` still Playing while `storm_up` counts down from kStormUpTime;
// at 0 the phase becomes Boss. `resume_phase` is the phase a molt returns to when its countdown ends. `rerolls` is the
// molt rerolls in hand.
struct RunState {
	RunPhase phase{RunPhase::Playing};
	float clock{0.0F};
	int kills{0};
	int rock_kills{0};
	int molt_level{1};
	int shells{0};
	int shells_needed{0};
	bool menu_paused{false};
	int slams{0};
	float slam_cooldown{0.0F};
	float banner_timer{0.0F};
	uint64_t seed{0};
	float countdown{0.0F};
	int act{0};
	int stage{0};
	float stage_clock{0.0F};
	float act_clock{0.0F};
	int stage_kills_start{0};
	float stage_banner{0.0F};
	float challenge_cleared{0.0F};
	float storm_up{0.0F};
	RunPhase resume_phase{RunPhase::Playing};
	int rerolls{0};
};

// Shells needed for the molt after `moltLevel` (geometric: kShellsFirstMolt * kShellsGrowth^(level - 1)).
[[nodiscard]] int ShellsNeeded(int moltLevel);

// Registers and reflects RunState and the RunClock, RunCountdown and PauseToggle systems. Call from the module constructor.
void RegisterRun(const flecs::world& world);

// Starts a fresh run: resets RunState, reseeds RngState with a seed drawn from it, and starts the countdown (its
// system pauses the scene on its first tick, once the arena is the active scene).
void StartRun(const flecs::world& world);

// Opens or closes the pause menu (pausing the scene). Does nothing unless the run is live.
void SetMenuPaused(const flecs::world& world, bool paused);

// Ends the run as won (the third boss slam) and pauses the scene. Does nothing unless the boss phase is running.
void WinRun(const flecs::world& world);

// Ends the run as lost and pauses the scene. Does nothing unless the run is live.
void LoseRun(const flecs::world& world);

} // namespace crabnado
