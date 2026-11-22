#include "ticket_simulation.hpp"

#include <algorithm>
#include <iterator>
#include <type_traits>
#include <utility>
#include <vector>


#include "engine/engine.hpp"
#include "item_components.hpp"
#include "ticket_components.hpp"
#include "ticket_rules.hpp"

using namespace engine;

namespace signal_room {

namespace {

using Weights = std::vector<std::pair<std::string, float>>;

// The Window, the gauges and the schedule tick only while the Shift is Running and not frozen.
bool Ticking(const ShiftWindow& clock) { return clock.phase == ShiftPhase::Running && !clock.frozen; }

// Draw an id from `candidates` (not empty) in proportion to its weight.
std::string PickWeighted(ecs::RngState& rng, const Weights& candidates) {
	float total = 0.0F;
	for (const auto& candidate : candidates) {
		total += candidate.second;
	}
	float roll = rng.NextFloat() * total;
	for (const auto& candidate : candidates) {
		roll -= candidate.second;
		if (roll < 0.0F) {
			return candidate.first;
		}
	}
	return candidates.back().first;
}

// The next frequency to arrive: by weight among those not used within min_repeat_gap. The gap is ignored
// when only one frequency is live, and when no frequency qualifies every one is a candidate.
std::string
PickFrequency(ecs::RngState& rng, const Schedule& schedule, const ScheduleState& state, const float elapsed) {
	if (schedule.minRepeatGap > 0.0F && schedule.frequencyWeights.size() > 1) {
		Weights allowed;
		std::ranges::copy_if(schedule.frequencyWeights, std::back_inserter(allowed), [&](const auto& candidate) {
			const auto last = state.lastArrival.find(candidate.first);
			return last == state.lastArrival.end() || elapsed - last->second >= schedule.minRepeatGap;
		});
		if (!allowed.empty()) {
			return PickWeighted(rng, allowed);
		}
	}
	return PickWeighted(rng, schedule.frequencyWeights);
}

// Move a Ticket from Stand-by to On Air, its timer capped to the Window left.
void GoOnAir(Ticket& ticket, const TicketRules& rules, const ShiftWindow& clock) {
	ticket.state = TicketState::OnAir;
	ticket.standbyLeft = 0.0F;
	ticket.onAirTotal = std::min(rules.tiers.Get(ticket.tier)->onAirTime, clock.windowLeft);
	ticket.onAirLeft = ticket.onAirTotal;
}

// The seconds to the next generated arrival.
float DrawInterval(ecs::RngState& rng, const Schedule& schedule) {
	return rng.NextFloatRange(schedule.intervalMin, schedule.intervalMax);
}

// Flecs event on one entity, carrying nothing.
template<typename Event, typename Component>
void Raise(const flecs::world& world, const flecs::entity target) {
	world.event<Event>().template id<Component>().entity(target).emit();
}

void RaiseOnClock(const flecs::world& world, const auto& event) {
	world.event<std::remove_cvref_t<decltype(event)>>()
		.template id<ShiftWindow>()
		.entity(world.entity<ShiftWindow>())
		.emit();
}

// The hard stop: open Tickets go unscored and Items in flight are destroyed.
void EndShift(const flecs::world& world, ShiftWindow& clock) {
	clock.windowLeft = 0.0F;
	SetShiftPhase(world, ShiftPhase::Ended);
	world.delete_with<Ticket>();
	world.delete_with<Item>();
	RaiseOnClock(world, ShiftEnded{});
}

void RegisterShiftWindow(const flecs::world& world, const flecs::entity roomRoot) {
	world.system("ShiftWindowTick")
		.kind(flecs::OnUpdate)
		.run([](flecs::iter& it) {
			while (it.next()) {
				const auto world = it.world();
				auto& clock = world.get_mut<ShiftWindow>();
				if (!Ticking(clock)) {
					continue;
				}
				clock.windowLeft -= it.delta_time();
				if (clock.windowLeft <= 0.0F) {
					EndShift(world, clock);
					continue;
				}
				if (!clock.lastCall && clock.windowLeft <= world.get<TicketRules>().lastCall) {
					clock.lastCall = true;
					RaiseOnClock(world, LastCall{});
				}
			}
		})
		.add<ecs::Pausable>()
		.child_of(roomRoot);
}

void RegisterTicketGauges(const flecs::world& world, const flecs::entity roomRoot) {
	world.system<Ticket>("TicketGauges")
		.kind(flecs::OnUpdate)
		.each([](flecs::iter& it, const size_t i, Ticket& ticket) {
			const auto world = it.world();
			const auto& clock = world.get<ShiftWindow>();
			if (!Ticking(clock)) {
				return;
			}
			const float dt = it.delta_time();
			if (ticket.state == TicketState::StandBy) {
				// In Last Call the gauge drains to its On Air line and stops there.
				ticket.standbyLeft = std::max(0.0F, ticket.standbyLeft - dt);
				if (ticket.standbyLeft > 0.0F || clock.lastCall) {
					return;
				}
				GoOnAir(ticket, world.get<TicketRules>(), clock);
				Raise<TicketOnAir, Ticket>(world, it.entity(i));
				return;
			}
			ticket.onAirLeft -= dt;
			if (ticket.onAirLeft > 0.0F) {
				return;
			}
			if (ticket.unlosable) {
				ticket.onAirLeft = 0.0F;
				return;
			}
			Raise<LostIntercept, Ticket>(world, it.entity(i));
			it.entity(i).destruct();
		})
		.add<ecs::Pausable>()
		.child_of(roomRoot);
}

void RegisterCollectionSchedule(const flecs::world& world, const flecs::entity roomRoot) {
	world.system("CollectionSchedule")
		.kind(flecs::OnUpdate)
		.run([roomRoot](flecs::iter& it) {
			while (it.next()) {
				const auto world = it.world();
				auto& clock = world.get_mut<ShiftWindow>();
				// Last Call stops the schedule.
				if (!Ticking(clock) || clock.lastCall) {
					continue;
				}
				const auto& rules = world.get<TicketRules>();
				const auto& schedule = world.get<Schedule>();
				auto& state = world.get_mut<ScheduleState>();
				auto& rng = world.get_mut<ScheduleRng>().rng;
				const float elapsed = clock.window - clock.windowLeft;
				// Spawned Tickets only land at the next sync point, so count them as we go.
				int rack = world.count<Ticket>();
				const auto arrive = [&](const std::string& tier,
										const std::string& frequency,
										const float overfill,
										const bool unlosable) {
					SpawnTicket(world, roomRoot, tier, frequency, overfill, false, unlosable);
					state.lastArrival[frequency] = elapsed;
					++rack;
				};

				// A full rack makes a due Ticket wait for a slot.
				while (rack < rules.rackCap
					   && clock.nextPinned < static_cast<int>(schedule.pinned.size())
					   && schedule.pinned[static_cast<size_t>(clock.nextPinned)].at <= elapsed) {
					const auto& pinned = schedule.pinned[static_cast<size_t>(clock.nextPinned++)];
					arrive(pinned.tier, pinned.frequency, pinned.overfill, pinned.unlosable);
				}

				if (schedule.tierWeights.empty()) {
					continue;
				}
				// An empty rack drains the wait at 1 / the factor: twice as fast by default.
				clock.nextArrival -= it.delta_time() * (rack == 0 ? 1.0F / rules.emptyRackWaitFactor : 1.0F);
				if (clock.nextArrival > 0.0F || rack >= rules.rackCap) {
					clock.nextArrival = std::max(clock.nextArrival, 0.0F);
					continue;
				}
				const auto tierId = PickWeighted(rng, schedule.tierWeights);
				const auto frequency = PickFrequency(rng, schedule, state, elapsed);
				const auto& tier = *rules.tiers.Get(tierId);
				arrive(tierId, frequency, rng.NextFloatRange(tier.overfillMin, tier.overfillMax), false);
				clock.nextArrival = DrawInterval(rng, schedule);
			}
		})
		.add<ecs::Pausable>()
		.child_of(roomRoot);
}

// A Ticket's arrival (and its straight-to-On-Air start, from the debug panel) is announced once its data has
// landed: the spawn's set is a command when it comes from a system, so it can't be announced from the spawn.
void RegisterArrivalObserver(const flecs::world& world, const flecs::entity roomRoot) {
	world.observer<const Ticket>("AnnounceTicketArrival")
		.event(flecs::OnSet)
		.each([](flecs::iter& it, const size_t i, const Ticket& ticket) {
			Raise<TicketArrived, Ticket>(it.world(), it.entity(i));
			if (ticket.state == TicketState::OnAir) {
				Raise<TicketOnAir, Ticket>(it.world(), it.entity(i));
			}
		})
		.child_of(roomRoot);
}

// Clear the On Air Ticket of the transmitted frequency with the least time left (the earliest arrival on
// a tie). No match: nothing happens.
void RegisterTransmissionObserver(const flecs::world& world, const flecs::entity roomRoot) {
	world.observer<ShiftWindow>("ClearOnTransmission")
		.event<Transmission>()
		.each([tickets = world.query<Ticket>()](flecs::iter& it, size_t, ShiftWindow&) {
			const auto& frequency = it.param<Transmission>()->frequency;
			flecs::entity best;
			const Ticket* bestTicket = nullptr;
			tickets.each([&](const flecs::entity entity, const Ticket& ticket) {
				if (ticket.state != TicketState::OnAir || ticket.frequency != frequency) {
					return;
				}
				const bool better = !bestTicket
									|| ticket.onAirLeft < bestTicket->onAirLeft
									|| (ticket.onAirLeft == bestTicket->onAirLeft && ticket.order < bestTicket->order);
				if (better) {
					best = entity;
					bestTicket = &ticket;
				}
			});
			if (best) {
				Raise<TicketCleared, Ticket>(it.world(), best);
				best.destruct();
			}
		})
		.child_of(roomRoot);
}

} // namespace

void SetShiftPhase(const flecs::world& world, const ShiftPhase phase) {
	world.get_mut<ShiftWindow>().phase = phase;
	// Briefing and Ended hold the Room's simulation (ADR 0004); Render, UI and the panel keep running. A fresh
	// attempt's Briefing is held by the Shift flow after one frame instead: the engine's transform propagation is
	// Pausable, so a Room paused from its first frame would never be drawn behind the card.
	if (phase == ShiftPhase::Running) {
		world.remove<scene::Paused>();
	} else if (phase == ShiftPhase::Ended) {
		world.add<scene::Paused>();
	}
	RaiseOnClock(world, ShiftPhaseChanged{});
}

void EndShiftNow(const flecs::world& world) {
	if (auto& clock = world.get_mut<ShiftWindow>(); clock.phase != ShiftPhase::Ended) {
		EndShift(world, clock);
	}
}

void StartShiftWindow(const flecs::world& world, const Shift& shift) {
	const auto& pin = world.get<SeedPin>();
	const uint64_t seed = pin.pinned ? pin.value : world.get_mut<ecs::RngState>().Next();

	ScheduleRng scheduleRng{.rng = {.seed = seed}};
	ShiftWindow clock{.window = shift.window, .windowLeft = shift.window, .seed = seed};
	if (!shift.schedule.tierWeights.empty()) {
		clock.nextArrival = DrawInterval(scheduleRng.rng, shift.schedule);
	}
	world.set<ShiftWindow>(clock);
	world.set<ScheduleRng>(scheduleRng);
	world.set<ScheduleState>({});
	world.set<Schedule>(shift.schedule);
	SetShiftPhase(world, ShiftPhase::Briefing);
	engine::ecs::Log(world).info("[Tickets] Window started: Window {} s, seed {}", shift.window, seed);
}

void RegisterTicketSimulation(const flecs::world& world, const flecs::entity roomRoot) {
	RegisterShiftWindow(world, roomRoot);
	RegisterTicketGauges(world, roomRoot);
	RegisterCollectionSchedule(world, roomRoot);
	RegisterArrivalObserver(world, roomRoot);
	RegisterTransmissionObserver(world, roomRoot);
}

void SpawnTicket(
	const flecs::world& world,
	const flecs::entity roomRoot,
	const std::string& tier,
	const std::string& frequency,
	const float overfill,
	const bool onAir,
	const bool unlosable,
	const std::string& callsign
) {
	const auto& rules = world.get<TicketRules>();
	auto& clock = world.get_mut<ShiftWindow>();
	Ticket ticket{
		.callsign = callsign,
		.tier = tier,
		.frequency = frequency,
		.order = clock.nextOrder++,
		.standbyLeft = overfill * rules.tiers.Get(tier)->onAirTime,
		.unlosable = unlosable,
	};
	if (ticket.callsign.empty()) {
		const auto draw = world.get_mut<ScheduleRng>().rng.NextRange(0, static_cast<int>(rules.callsigns.size()));
		ticket.callsign = rules.callsigns[static_cast<size_t>(draw)];
	}
	if (onAir) {
		GoOnAir(ticket, rules, clock);
	}
	world.entity().child_of(roomRoot).set<Ticket>(std::move(ticket));
}

void RaiseTransmission(const flecs::world& world, const std::string& frequency, const flecs::entity_t station) {
	world.event<Transmission>()
		.id<ShiftWindow>()
		.entity(world.entity<ShiftWindow>())
		.ctx(Transmission{.frequency = frequency, .station = station})
		.emit();
}

} // namespace signal_room
