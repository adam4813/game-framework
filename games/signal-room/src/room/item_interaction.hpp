#pragma once

#include <flecs.h>

namespace signal_room {

// Register the ItemInteraction system as a child of the Room's root entity (after
// RegisterOperatorTargeting, so it acts on this frame's Target). On an Operator's `interact` press:
// - empty-handed at a Counter holding an Item, it picks the Item up;
// - carrying at an empty Counter, it sets the Item down there with a dust puff (DustPuffTuning);
// - an occupied Counter while carrying, or an empty one while empty-handed, raises a Refusal;
// - a Station is StationInteraction's (station_systems.hpp); no Target does nothing.
// When it picks up or sets down, the Operator turns to face the Counter; it never moves.
void RegisterItemInteraction(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
