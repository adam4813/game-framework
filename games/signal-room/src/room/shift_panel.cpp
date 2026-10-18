#include "shift_panel.hpp"

#include <charconv>
#include <cmath>
#include <format>
#include <string>

#include "engine/engine.hpp"
#include "ticket_components.hpp"
#include "ticket_rules.hpp"
#include "ticket_simulation.hpp"

using namespace engine;

namespace signal_room {

namespace {

// A callsign for a panel spawn, from the world RngState so it can't shift the seeded schedule.
std::string DrawCallsign(const flecs::world& world) {
	const auto& callsigns = world.get<TicketRules>().callsigns;
	const int draw = world.get_mut<ecs::RngState>().NextRange(0, static_cast<int>(callsigns.size()));
	return callsigns[static_cast<size_t>(draw)];
}

std::string ClockText(const float seconds) {
	const int whole = static_cast<int>(std::ceil(std::max(seconds, 0.0F)));
	return std::format("{}:{:02}", whole / 60, whole % 60);
}

} // namespace

void DrawShiftSection(const flecs::world& world, const flecs::entity roomRoot, ShiftPanelState& choice) {
	auto& ui = world.get<platform::PlatformRef>().ptr->GetDebugUi();
	if (!ui.CollapsingHeader("Shift") || !world.has<ShiftWindow>() || !world.has<TicketRules>()) {
		return;
	}
	auto& clock = world.get_mut<ShiftWindow>();
	const auto& rules = world.get<TicketRules>();
	const auto& frequencies = world.get<FrequencyTable>();
	if (choice.tier.empty()) {
		choice.tier = rules.tiers.Ids().front();
	}
	if (choice.frequency.empty() && !frequencies.entries.empty()) {
		choice.frequency = frequencies.entries.front().label;
	}

	ui.Text(
		std::format(
			"Phase: {}   Window left: {} ({:.1f} s)   Tickets: {}",
			clock.phase == ShiftPhase::Running ? "Running" : "Ended",
			ClockText(clock.windowLeft),
			clock.windowLeft,
			world.count<Ticket>()
		)
	);
	ui.Checkbox("Freeze Window", clock.frozen);
	ui.Tooltip("Stops the clock, the schedule and every Ticket gauge");

	ui.Separator();
	ui.Text(std::format("Spawn: {} on {}", choice.tier, choice.frequency));
	for (const auto& id : rules.tiers.Ids()) {
		if (ui.Selectable(std::format("tier {}", id), id == choice.tier)) {
			choice.tier = id;
		}
	}
	for (const auto& frequency : frequencies.entries) {
		if (ui.Selectable(std::format("frequency {}", frequency.label), frequency.label == choice.frequency)) {
			choice.frequency = frequency.label;
		}
	}
	ui.SliderFloat("Overfill (fraction of On Air time)", choice.overfill, 0.0F, 0.5F);
	if (ui.Button("Spawn with overfill")) {
		SpawnTicket(world, roomRoot, choice.tier, choice.frequency, choice.overfill, false, false, DrawCallsign(world));
	}
	ui.SameLine();
	if (ui.Button("Spawn On Air")) {
		SpawnTicket(world, roomRoot, choice.tier, choice.frequency, 0.0F, true, false, DrawCallsign(world));
	}

	ui.Separator();
	auto& pin = world.get_mut<SeedPin>();
	ui.Text(std::format("Seed: {} ({})", clock.seed, pin.pinned ? "pinned" : "fresh each attempt"));
	ui.InputText("Seed value", choice.seedText);
	if (ui.Button("Pin seed")) {
		uint64_t value = 0;
		const auto* end = choice.seedText.data() + choice.seedText.size();
		if (std::from_chars(choice.seedText.data(), end, value).ec == std::errc{}) {
			pin = {.pinned = true, .value = value};
		}
	}
	ui.SameLine();
	if (ui.Button("Use current seed")) {
		pin = {.pinned = true, .value = clock.seed};
		choice.seedText = std::to_string(clock.seed);
	}
	ui.SameLine();
	if (ui.Button("Unpin seed")) {
		pin.pinned = false;
	}
	ui.Tooltip("A pinned seed applies from the next attempt: press Reload");
}

} // namespace signal_room
