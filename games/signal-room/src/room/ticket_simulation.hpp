#pragma once

#include <string>

#include <flecs.h>

#include "shift.hpp"
#include "ticket_components.hpp"

namespace signal_room {

// Reset the Shift Window for a Shift just loaded: the full Window, the Briefing phase (Room paused), a
// fresh seed (or the pinned one in SeedPin) driving the schedule's random stream, and the first arrival's
// wait. Needs the TicketRules singleton. Called from the Room load, which suspends deferral.
void StartShiftWindow(const flecs::world& world, const Shift& shift);

// Move the Shift to `phase` and raise ShiftPhaseChanged. Briefing and Ended pause the Room's simulation
// (the world's scene::Paused tag); Running resumes it. Needs the ShiftWindow singleton.
void SetShiftPhase(const flecs::world& world, ShiftPhase phase);

// The hard stop now (the debug panel's End Shift): as when the Window runs out, unless already Ended.
void EndShiftNow(const flecs::world& world);

// Register the Ticket simulation's systems and observer under the Room's root: the Shift Window (Window,
// Last Call, hard stop), the Ticket gauges, the Collection Schedule, and the Transmission observer.
// Tickets are created as children of `roomRoot`, so they go when the Room unloads.
void RegisterTicketSimulation(const flecs::world& world, flecs::entity roomRoot);

// Put a Ticket on the rack: Stand-by with its gauge `overfill` (a fraction of the tier's On Air time)
// above 100%, or straight On Air. `callsign` empty draws one from the pool. TicketArrived (and
// TicketOnAir) are raised once the Ticket exists. Works from systems and the Room panel; the tier must be in the TicketRules.
void SpawnTicket(
	const flecs::world& world,
	flecs::entity roomRoot,
	const std::string& tier,
	const std::string& frequency,
	float overfill,
	bool onAir,
	bool unlosable = false,
	const std::string& callsign = {}
);

// Transmit on a frequency: raises Transmission on the Shift Window, which clears the most urgent On Air
// Ticket of that frequency (none: nothing happens). The Telex raises it; tests and the debug panel too.
void RaiseTransmission(const flecs::world& world, const std::string& frequency, flecs::entity_t station = 0);

} // namespace signal_room
