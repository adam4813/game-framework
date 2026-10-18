#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>

#include <flecs.h>

#include "engine/ecs/singletons.hpp"

namespace signal_room {

// Where a Ticket is on its way to being Cleared or Lost. A Cleared or Lost Ticket leaves the rack at
// once (its entity is destroyed), so only these two are ever seen.
enum class TicketState { StandBy, OnAir };

// A Ticket on the rack. While on Stand-by `standbyLeft` drains to 0 (the gauge reads above 100%); then
// the Ticket goes On Air with `onAirTotal` = min(tier time, Window left) and `onAirLeft` drains to 0,
// when it is Lost. `order` counts arrivals from 0 and breaks ties between equally urgent Tickets.
struct Ticket {
	std::string callsign;
	std::string tier;
	std::string frequency;
	int order{0};
	TicketState state{TicketState::StandBy};
	float standbyLeft{0.0F}; // s
	float onAirTotal{0.0F};  // s
	float onAirLeft{0.0F};   // s
	bool unlosable{false};   // pinned Tickets that never run out (the timer rests at 0)
};

// Shift flow phase. Later tickets add Briefing, Orientation and Results around these.
enum class ShiftPhase { Running, Ended };

// World singleton: the Window and the schedule's clock. They tick only while Running and not `frozen`.
struct ShiftWindow {
	ShiftPhase phase{ShiftPhase::Running};
	float window{240.0F};     // s, the Shift's full Window
	float windowLeft{240.0F}; // s
	uint64_t seed{0};         // the attempt's seed
	float nextArrival{0.0F};  // s until the generator's next Ticket
	int nextOrder{0};         // order of the next arriving Ticket
	int nextPinned{0};        // index of the next pinned entry due
	bool lastCall{false};     // Last Call has begun
	bool frozen{false};       // debug: stops the clock, the schedule and every gauge
};

// World singleton: the seed the Room panel (or a test) pins for the next attempts. Survives Reloads.
struct SeedPin {
	bool pinned{false};
	uint64_t value{1};
};

// World singleton: the schedule's own random stream, reseeded at each attempt start. It is a separate
// stream from the world RngState so that unrelated draws (dust puffs) cannot change which Tickets arrive
// for a given seed.
struct ScheduleRng {
	engine::ecs::RngState rng;
};

// World singleton: when each frequency last had a Ticket arrive, in Window seconds (min_repeat_gap).
struct ScheduleState {
	std::unordered_map<std::string, float> lastArrival;
};

// Events, raised on the Ticket entity (or on the ShiftWindow for the Shift-wide ones). Audio and the HUD
// observe them.
struct TicketArrived {};
struct TicketOnAir {};
struct TicketCleared {};
struct LostIntercept {};
struct LastCall {};
struct ShiftEnded {};
// Raised on the ShiftWindow by whatever transmits (the Telex); carries the frequency label and the
// transmitting Station (0 when a test raises it directly).
struct Transmission {
	std::string frequency;
	flecs::entity_t station{0};
};

} // namespace signal_room
