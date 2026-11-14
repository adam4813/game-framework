#pragma once

#include <string>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

namespace game::farm {

// Zero-size tag applied to every NPC villager entity.
struct VillagerTag {};

// Display name used as the dialog speaker label.
struct VillagerName {
	std::string name;
};

// One timed slot in a villager's daily routine.
// `startHour` is the in-game hour (0-23) when the villager moves to this waypoint.
// `waypoint` is an absolute tile position (town location + authored offset, resolved at spawn time).
struct VillagerScheduleEntry {
	int startHour = 0;
	glm::ivec2 waypoint{};
};

// Daily schedule for one villager: ordered list of (startHour, waypoint) pairs.
// SelectWaypoint(schedule, hour) returns the tile the villager should currently occupy.
struct VillagerSchedule {
	std::vector<VillagerScheduleEntry> entries;
};

// Attaches a pre-built dialog tree root to a villager.
// The InteractWith observer on the villager calls StartDialog(world, tree, actor).
struct DialogTrigger {
	flecs::entity tree;
};

// Per-town parent entity for a town's villagers (ticket 09). Every living town that is populated
// gets one TownRoot entity under the scene root; its villagers are children of it, so a town's
// inhabitants are grouped by (and anchored to) their settlement. `location` is the town-centre tile.
struct TownRoot {
	int townId = 0;
	glm::ivec2 location{};
};

} // namespace game::farm
