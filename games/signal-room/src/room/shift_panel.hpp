#pragma once

#include <cstdint>
#include <string>

#include <flecs.h>

namespace signal_room {

// What the Shift section's spawn and seed controls have picked; the Room panel owns one.
struct ShiftPanelState {
	std::string tier; // empty until the first draw picks the first tier
	std::string frequency;
	float overfill{0.0F};
	std::string seedText{"1"};
};

// The Room panel's Shift section (call between BeginPanel and EndPanel): phase and Window left, Freeze
// Window, spawning a Ticket, and the attempt's seed with pin / unpin. Its buttons only write components
// (the clock, the SeedPin, new Ticket entities), so the Flecs Remote API can drive the same things.
// The seed applies from the next attempt (Reload).
void DrawShiftSection(const flecs::world& world, flecs::entity roomRoot, ShiftPanelState& choice);

} // namespace signal_room
