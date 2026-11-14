#pragma once

#include <glm/glm.hpp>

#include "villager_components.hpp"

namespace game::farm {

/// Returns the waypoint the villager should occupy at the given in-game `hour`.
///
/// Rules:
/// - Entries are assumed to be sorted ascending by `startHour`.
/// - The active entry is the last one whose `startHour` <= `hour` (so the villager
///   stays at a waypoint until the next scheduled change).
/// - If `hour` is before the first entry of the day (e.g. 03:00 with first entry at
///   06:00) the villager is still at the *last* entry from the previous day cycle.
/// - If `schedule` is empty, returns {0, 0}.
[[nodiscard]] glm::ivec2 SelectWaypoint(const VillagerSchedule& schedule, int hour);

} // namespace game::farm
