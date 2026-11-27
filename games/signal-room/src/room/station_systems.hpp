#pragma once

#include <flecs.h>

namespace signal_room {

// Register StationHolding as a child of the Room's root entity: it ends an Operator's Hold when it lets
// go of `interact`, gives any move input or a dash, or its Station's Job is no longer Running, and
// otherwise counts how long it has Held. Register it before RegisterOperatorTargeting, which freezes
// the Target of an Operator that is Holding.
void RegisterStationHolding(const flecs::world& world, flecs::entity roomRoot);

// Register, as children of the Room's root entity (after RegisterOperatorTargeting, beside
// RegisterItemInteraction):
// - StationInteraction: resolves an `interact` press at a Station (see PressStation);
// - StationJobProgress: advances every Running Job, cranked or held when an Operator Holds it.
void RegisterStationSystems(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
