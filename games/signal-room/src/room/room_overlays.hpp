#pragma once

#include <flecs.h>

namespace signal_room {

// (Re)build the Room panel's debug overlays under `roomRoot` from the RoomOverlays singleton: the
// floor grid lines, a marker over each solid cell, and a bar along each worked side (in-service
// Stations' authored sides, and every open side of a Counter), and a disc under each Operator the size
// of its collision circle. Called on Room load and whenever the panel toggles an overlay or changes
// the radius; safe inside a system (deferred).
void BuildRoomOverlays(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
