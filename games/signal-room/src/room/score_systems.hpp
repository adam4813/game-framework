#pragma once

#include <flecs.h>

#include "shift.hpp"

namespace signal_room {

// Reset the Score for a Shift just loaded and record how it awards stars. Needs the TicketRules singleton.
void StartScore(const flecs::world& world, const Shift& shift);

// Register the scoring observers under the Room's root: Clear and Lost Intercept, par on On Air, and the
// stars at the hard stop.
void RegisterScoreSystems(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
