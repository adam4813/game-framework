#pragma once

#include <string>

namespace signal_room {

// An Item on the bunker floor, spawned from a Shift's `counter_items`. Its fields are copied from
// its ItemKind in the Item catalogue when it spawns: `carrySpeed` scales its carrier's walk and dash,
// and `tiltToCamera` tilts it toward the camera while carried (cards).
//
// An Item always has exactly one holder: the Operator that (Carries, item) or the Counter that
// (Holds, item). There is no floor state. Its Transform follows the holder (see item_placement.hpp).
struct Item {
	std::string id;
	float carrySpeed{1.0F};
	bool tiltToCamera{false};
};

// Relationship from an Operator to the Item it carries, as (Carries, item). Exclusive: one at a time.
struct Carries {};

// Relationship from a Counter to the Item set down on it, as (Holds, item). Exclusive: one at a time.
struct Holds {};

} // namespace signal_room
