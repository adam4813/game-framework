#pragma once

#include <flecs.h>

namespace signal_room {

// Register the OperatorInput system as a child of the Room's root entity (so it exists only while the
// Room is loaded). Each frame it fills every Operator's OperatorActions from the keyboard Bindings:
// the normalised move wish, and dash and interact presses. Call before RegisterOperatorMovement so it
// runs first in OnUpdate.
void RegisterOperatorInput(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
