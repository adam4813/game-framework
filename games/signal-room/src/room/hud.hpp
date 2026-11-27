#pragma once

#include <string>

#include <flecs.h>

namespace signal_room {

// A Ticket card on the rack. It outlives its Ticket by the exit flash: a Cleared or Lost Ticket is
// destroyed at once, so the card keeps what it needs. `order` is the Ticket's arrival order.
struct HudCard {
	flecs::entity_t ticket{0};
	int order{0};
	bool exiting{false};
	bool cleared{false}; // why it is leaving: flashes green when Cleared, red when Lost
	float exitLeft{0.0F};
	float age{0.0F}; // s on the rack, phases the red-band flash
	flecs::entity callsign;
	flecs::entity tier;
	flecs::entity swatch;
	flecs::entity frequency;
	flecs::entity gauge;
	flecs::entity standbySegment;
};

// On a card's gauge entity: the On Air fraction (0-1), the Stand-by overfill past 100% (as a fraction of the
// tier's On Air time) and the band id ("green", "amber", "red").
struct HudGauge {
	float onAir{0.0F};
	float standby{0.0F};
	std::string band;
};

// Tags on the labels that are not cards.
struct HudClock {};
struct HudClockTag {};
struct HudScore {};
struct HudChain {};

// World singleton: seconds left of the scale pop on the score and chain labels.
struct HudPulse {
	float score{0.0F};
	float chain{0.0F};
};

// Register the in-play HUD under the Room's root: the clock, score and chain labels, the Ticket cards
// that follow TicketArrived / TicketCleared / LostIntercept, and the per-frame update. Needs the
// HudLayout, Score and TicketRules singletons.
void RegisterHud(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
