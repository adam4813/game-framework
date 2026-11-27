#pragma once

#include <string>

namespace signal_room {

// An Item on the bunker floor, spawned from a Shift's `counter_items`. Its fields are copied from
// its ItemKind in the Item catalogue when it spawns: `carrySpeed` scales its carrier's walk and dash,
// and `tiltToCamera` tilts it toward the camera while carried (cards).
//
// An Item always has exactly one holder: the Operator that (Carries, item) or the Counter that
// (Holds, item). There is no floor state. Its Transform follows the holder (see item_placement.hpp).
//
// `frequency` is the label of the Radio frequency the Item was copied from ("" for tools): it shows as
// the stripe on cards and tapes, and a Transform's outputs inherit it (see station_catalogue.hpp).
struct Item {
	std::string id;
	std::string frequency;
	float carrySpeed{1.0F};
	bool tiltToCamera{false};
};

// Relationship from an Operator to the Item it carries, as (Carries, item). Exclusive: one at a time.
struct Carries {};

// Relationship from a Counter or Station to an Item resting on it, as (Holds, item). A Counter holds at
// most one (its interaction checks); a Station holds a Job's placed inputs, then its waiting outputs.
struct Holds {};

} // namespace signal_room
