#pragma once

#include <string>
#include <vector>

#include <flecs.h>

namespace signal_room {

// World singleton: what the Briefing card says about the loaded Shift. Set by the Room load.
struct ShiftBriefing {
	std::string name;
	std::vector<std::string> notes;
};

// World singleton: the Briefing card's and the results screen's Modals (both under the Room's root) and the
// seconds the Shift has been Ended. Written by RegisterShiftFlow, so it holds no handles across a Reload.
struct FlowUi {
	flecs::entity briefing;
	flecs::entity results;
	float endedFor{0.0F};
};

// On each text line of the Briefing card and the results screen. `key` is "route" on the Briefing card and
// "score", "par", "stars", "cleared", "lost" or "chain" on the results screen.
struct FlowLine {
	std::string key;
};

// The route the Shift's in-service Stations make: the display names of the kind that takes no input (the Radio),
// then each kind that takes what the last one makes, up to the one that transmits, joined by `arrow`. Derived
// from the Stations' Transforms, so a Modifier that adds a Station shows up. Needs the StationCatalogue and the
// Room's Stations.
[[nodiscard]] std::string DeriveRoute(const flecs::world& world, const std::string& arrow);

// Register the Shift flow under the Room's root: the Briefing card and the results screen (Modals shown in
// the Briefing and Ended phases) and the `interact` press that dismisses the first and presses Retry on the
// second. Needs the HudLayout, ShiftBriefing, Score and TicketRules singletons.
void RegisterShiftFlow(const flecs::world& world, flecs::entity roomRoot);

} // namespace signal_room
