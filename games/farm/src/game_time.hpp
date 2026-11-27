#pragma once

#include <flecs.h>
#include <string>

namespace game {

enum class Season : int32_t {
	Spring = 0,
	Summer = 1,
	Autumn = 2,
	Winter = 3,
};

enum class DayPhase : int32_t {
	Dawn  = 0,
	Day   = 1,
	Dusk  = 2,
	Night = 3,
};

// World singleton tracking in-game time.
// Config fields (ticksPerHour etc.) are runtime-overridable so tests can use
// smaller thresholds without recompiling; production defaults come from constants.hpp.
struct GameTime {
	int tick        = 0;              // raw frame counter
	int day         = 1;              // 1-based day within the current season
	int hour        = 0;              // 0–23
	int absoluteDay = 0;              // 0-based total elapsed days (used to deduplicate hooks)
	Season   season = Season::Spring;
	DayPhase phase  = DayPhase::Night; // derived from hour; hour 0 is Night

	int ticksPerHour  = 1;   // overridden to kTicksPerHour in RegisterGameTime
	int hoursPerDay   = 24;
	int daysPerSeason = 7;   // overridden to kDaysPerSeason in RegisterGameTime
};

// Event emitted on world.entity<GameTime>() the first tick each day that the
// clock first reaches or passes a registered hook's hour.
struct TimeHookFired {
	const char* name; // points into the hook registry; valid for the observer's call duration
	int         hour;
};

// Register a named time hook. Call after RegisterGameTime(). The TimeAdvanceSystem
// emits a TimeHookFired event on world.entity<GameTime>() once per day, the first
// tick the clock reaches or passes `hour` (0–23).
void AddTimeHook(const flecs::world& world, std::string name, int hour);

// Initialise the GameTime singleton, register Flecs component reflection, and
// register TimeAdvanceSystem (OnUpdate, Pausable). Does not touch game.cpp or
// any farm module; ticket 04 wires this in.
void RegisterGameTime(const flecs::world& world);

} // namespace game
