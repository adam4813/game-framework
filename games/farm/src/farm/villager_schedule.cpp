#include "villager_schedule.hpp"

namespace game::farm {

glm::ivec2 SelectWaypoint(const VillagerSchedule& schedule, int hour) {
	if (schedule.entries.empty())
		return {0, 0};

	const VillagerScheduleEntry* best = nullptr;
	for (const auto& entry : schedule.entries) {
		if (entry.startHour <= hour)
			best = &entry;
	}

	// Before the first entry of the day → use the last entry (previous day's final position).
	if (!best)
		best = &schedule.entries.back();

	return best->waypoint;
}

} // namespace game::farm
