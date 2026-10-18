#pragma once

#include <flecs.h>

namespace signal_room {

// Register the Operator systems as children of the Room's root entity (so they exist only while the
// Room is loaded), in frame order:
// - OperatorInput fills each Operator's OperatorActions from the keyboard Bindings.
// - OperatorMovement reads OperatorActions: it eases each Operator's velocity toward its wish at the
//   MovementTuning walk speed (scaled by its carry speed, and by the dash multiplier while dashing),
//   turns it toward its input, and moves every Operator at once through MoveCircles so it slides
//   along the Layout's solid cells.
void RegisterOperatorMovement(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
