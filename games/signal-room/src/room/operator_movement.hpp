#pragma once

#include <flecs.h>

namespace signal_room {

// Register the OperatorMovement system as a child of the Room's root entity (so it exists only while
// the Room is loaded). It reads each Operator's OperatorActions (filled by OperatorInput, registered
// first): it eases the Operator's velocity toward its wish at the MovementTuning walk speed (scaled by
// the carried Item's carry speed, and by the dash multiplier while dashing), turns it toward its input, and moves
// every Operator at once through MoveCircles so it slides along the Layout's solid cells.
void RegisterOperatorMovement(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
