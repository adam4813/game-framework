#pragma once

#include <flecs.h>

namespace signal_room {

// Stand `item` on `counter`'s top (the GreyboxCatalogue's Counter model Top()). Reads the Counter's
// Transform, never its WorldTransform, so it works during a Room load once BuildLayoutGreybox has set
// that Transform outside deferral.
void PlaceOnCounter(flecs::entity item, flecs::entity counter);

// Stand `item` on `station`: just in front of its south edge, at table height. Reads the Station's
// Transform, like PlaceOnCounter.
void PlaceOnStation(flecs::entity item, flecs::entity station);

// Register the CarriedItemPlacement system as a child of the Room's root entity. Each frame it shows
// every carried Item in front of its Operator (call after the systems that move Operators and change
// what they carry): tilted toward the camera when its Item has tiltToCamera, otherwise turned with
// the Operator.
void RegisterCarriedItemPlacement(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
