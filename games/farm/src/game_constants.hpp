#pragma once

#include <string_view>

namespace game::constats {

inline constexpr int kInventorySlots = 16;

// Frames per in-game hour. At 60 fps this gives ~14 s/hour and ~5.5 min/day.
constexpr int kTicksPerHour = 60;
// In-game days per season.
constexpr int kDaysPerSeason = 7;

// Hour boundaries for DayPhase derivation (inclusive on both ends).
constexpr int kDawnHourStart = 5;
constexpr int kDawnHourEnd = 7;
constexpr int kDayHourStart = 8;
constexpr int kDayHourEnd = 17;
constexpr int kDuskHourStart = 18;
constexpr int kDuskHourEnd = 20;
// Night covers [0, kDawnHourStart) and (kDuskHourEnd, 23].
} // namespace game::constats
