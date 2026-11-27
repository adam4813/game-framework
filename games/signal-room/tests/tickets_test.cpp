#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "room/item_components.hpp"
#include "room/ticket_components.hpp"
#include "room/ticket_simulation.hpp"
#include "room_harness.hpp"

using signal_room::Item;
using signal_room::LastCall;
using signal_room::LostIntercept;
using signal_room::ShiftEnded;
using signal_room::ShiftPhase;
using signal_room::ShiftWindow;
using signal_room::Ticket;
using signal_room::TicketArrived;
using signal_room::TicketCleared;
using signal_room::TicketOnAir;
using signal_room::TicketState;
using signal_room::test::RoomHarness;
using testing::HasSubstr;

namespace {

constexpr std::string_view kShift1 = "shifts/shift_01.shift.json";
constexpr uint64_t kSeed = 42;

std::string Fixture(const std::string_view name) {
	return std::string{SIGNAL_ROOM_FIXTURE_DIR} + "/" + std::string{name} + ".shift.json";
}

// How many times each Ticket / Shift event was raised. Declare it before the harness: the observers
// capture it and live as long as the world.
struct EventCounts {
	int arrived{0};
	int onAir{0};
	int cleared{0};
	int lost{0};
	int lastCall{0};
	int ended{0};
};

void Watch(RoomHarness& room, EventCounts& counts) {
	auto& world = room.World();
	world.observer<Ticket>().event<TicketArrived>().each([&counts](Ticket&) { ++counts.arrived; });
	world.observer<Ticket>().event<TicketOnAir>().each([&counts](Ticket&) { ++counts.onAir; });
	world.observer<Ticket>().event<TicketCleared>().each([&counts](Ticket&) { ++counts.cleared; });
	world.observer<Ticket>().event<LostIntercept>().each([&counts](Ticket&) { ++counts.lost; });
	world.observer<ShiftWindow>().event<LastCall>().each([&counts](ShiftWindow&) { ++counts.lastCall; });
	world.observer<ShiftWindow>().event<ShiftEnded>().each([&counts](ShiftWindow&) { ++counts.ended; });
}

void Load(RoomHarness& room, const std::string_view fixture) {
	const auto loaded = room.LoadShiftWithSeed(Fixture(fixture), kSeed);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
}

TEST(Tickets, Shift1StartsAFourMinuteWindow) {
	RoomHarness room;
	const auto loaded = room.LoadShift(kShift1);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();

	EXPECT_EQ(room.Window().phase, ShiftPhase::Running);
	EXPECT_FLOAT_EQ(room.Window().windowLeft, 240.0F);
	room.StepSeconds(10);
	EXPECT_NEAR(room.Window().windowLeft, 230.0F, 0.1F);
}

TEST(Tickets, AThirtyPercentOverfillNormalTicketIsStandByThenOnAirThenLost) {
	EventCounts counts;
	RoomHarness room;
	Watch(room, counts);
	Load(room, "pinned_overfill");

	room.StepSeconds(26.5F);
	ASSERT_EQ(room.OpenTickets().size(), 1U);
	EXPECT_EQ(room.OpenTickets().front().state, TicketState::StandBy);
	EXPECT_EQ(counts.onAir, 0);

	room.StepSeconds(1.0F);
	ASSERT_EQ(room.OpenTickets().size(), 1U);
	const auto onAir = room.OpenTickets().front();
	EXPECT_EQ(onAir.state, TicketState::OnAir);
	EXPECT_NEAR(onAir.onAirTotal, 90.0F, 0.1F);
	EXPECT_EQ(counts.onAir, 1);

	room.StepSeconds(88.0F);
	EXPECT_EQ(room.OpenTickets().size(), 1U);
	EXPECT_EQ(counts.lost, 0);

	room.StepSeconds(2.5F);
	EXPECT_TRUE(room.OpenTickets().empty());
	EXPECT_EQ(counts.lost, 1);
	EXPECT_EQ(counts.cleared, 0);
}

TEST(Tickets, ATicketGoingOnAirWithFiftySecondsOfWindowLeftGetsAFiftySecondTimer) {
	RoomHarness room;
	Load(room, "pinned_late");

	room.StepSeconds(191.0F);
	const auto tickets = room.OpenTickets();
	ASSERT_EQ(tickets.size(), 1U);
	EXPECT_EQ(tickets.front().state, TicketState::OnAir);
	EXPECT_NEAR(tickets.front().onAirTotal, 50.0F, 0.2F);
}

TEST(Tickets, APinnedEntryArrivesAtItsTime) {
	EventCounts counts;
	RoomHarness room;
	Watch(room, counts);
	Load(room, "pinned_late");

	room.StepSeconds(11.5F);
	EXPECT_EQ(counts.arrived, 0);
	room.StepSeconds(1.0F);
	EXPECT_EQ(counts.arrived, 1);
	ASSERT_EQ(room.OpenTickets().size(), 1U);
	EXPECT_EQ(room.OpenTickets().front().frequency, "7.0");
}

TEST(Tickets, WithTheRackFullADueTicketWaitsForASlot) {
	RoomHarness room;
	Load(room, "dense");

	room.StepSeconds(40.0F);
	auto tickets = room.OpenTickets();
	ASSERT_EQ(tickets.size(), 5U);
	EXPECT_EQ(tickets.back().order, 4);

	signal_room::RaiseTransmission(room.World(), "7.0");
	EXPECT_EQ(room.OpenTickets().size(), 4U);
	room.Step(3);

	tickets = room.OpenTickets();
	ASSERT_EQ(tickets.size(), 5U);
	EXPECT_EQ(tickets.back().order, 5);
}

TEST(Tickets, AnEmptyRackHalvesTheWaitToTheNextTicket) {
	RoomHarness room;
	Load(room, "slow");

	// The first wait is 20 s, drained twice as fast while the rack is empty.
	room.StepSeconds(9.0F);
	EXPECT_TRUE(room.OpenTickets().empty());
	room.StepSeconds(2.0F);
	EXPECT_EQ(room.OpenTickets().size(), 1U);

	// With a Ticket on the rack the next 20 s wait runs at full length: due at about 30 s.
	room.StepSeconds(18.0F);
	EXPECT_EQ(room.OpenTickets().size(), 1U);
	room.StepSeconds(3.0F);
	EXPECT_EQ(room.OpenTickets().size(), 2U);
}

// (arrival frame, tier, frequency, callsign) of every Ticket a 120 s Shift brings.
std::vector<std::tuple<int, std::string, std::string, std::string>> Arrivals(const uint64_t seed) {
	RoomHarness room;
	const auto loaded = room.LoadShiftWithSeed(Fixture("random_schedule"), seed);
	EXPECT_TRUE(loaded.has_value());
	std::vector<std::tuple<int, std::string, std::string, std::string>> arrivals;
	int seen = 0;
	for (int frame = 0; frame < 7200; ++frame) {
		room.Step();
		for (const auto& ticket : room.OpenTickets()) {
			if (ticket.order >= seen) {
				arrivals.emplace_back(frame, ticket.tier, ticket.frequency, ticket.callsign);
				seen = ticket.order + 1;
			}
		}
	}
	return arrivals;
}

TEST(Tickets, ThePinnedSeedRepeatsTheScheduleExactlyAndADifferentSeedDiffers) {
	const auto first = Arrivals(kSeed);
	EXPECT_GE(first.size(), 8U);
	EXPECT_EQ(first, Arrivals(kSeed));
	EXPECT_NE(first, Arrivals(kSeed + 1));
}

TEST(Tickets, NothingGoesOnAirInLastCallAndTheHardStopLeavesOpenTicketsUnscored) {
	EventCounts counts;
	RoomHarness room;
	Watch(room, counts);
	Load(room, "short_window");
	ASSERT_EQ(room.World().count<Item>(), 1);

	// The Ticket's gauge runs out 45 s in, with 15 s left: inside Last Call, so it stays Stand-by.
	room.StepSeconds(50.0F);
	ASSERT_EQ(room.OpenTickets().size(), 1U);
	EXPECT_EQ(room.OpenTickets().front().state, TicketState::StandBy);
	EXPECT_EQ(counts.lastCall, 1);
	EXPECT_EQ(counts.onAir, 0);

	room.StepSeconds(11.0F);
	EXPECT_EQ(room.Window().phase, ShiftPhase::Ended);
	EXPECT_FLOAT_EQ(room.Window().windowLeft, 0.0F);
	EXPECT_EQ(counts.ended, 1);
	EXPECT_TRUE(room.OpenTickets().empty());
	EXPECT_EQ(counts.cleared, 0);
	EXPECT_EQ(counts.lost, 0);
	EXPECT_EQ(room.World().count<Item>(), 0);
}

TEST(Tickets, ATransmissionClearsTheOnAirTicketOfThatFrequencyWithLessTimeLeft) {
	EventCounts counts;
	RoomHarness room;
	Watch(room, counts);
	Load(room, "two_on_air");

	room.StepSeconds(8.0F);
	ASSERT_EQ(room.OpenTickets().size(), 2U);

	// A frequency nothing is On Air on changes nothing.
	signal_room::RaiseTransmission(room.World(), "7.5");
	EXPECT_EQ(room.OpenTickets().size(), 2U);
	EXPECT_EQ(counts.cleared, 0);

	// The first Ticket arrived 5 s earlier, so it has less time left.
	signal_room::RaiseTransmission(room.World(), "7.0");
	const auto left = room.OpenTickets();
	ASSERT_EQ(left.size(), 1U);
	EXPECT_EQ(left.front().order, 1);
	EXPECT_EQ(counts.cleared, 1);
}

TEST(Tickets, FreezeWindowStopsTheClockAndEveryGauge) {
	RoomHarness room;
	Load(room, "pinned_overfill");
	room.StepSeconds(1.0F);

	room.World().get_mut<ShiftWindow>().frozen = true;
	const auto windowLeft = room.Window().windowLeft;
	const auto standbyLeft = room.OpenTickets().front().standbyLeft;
	room.StepSeconds(5.0F);
	EXPECT_FLOAT_EQ(room.Window().windowLeft, windowLeft);
	EXPECT_FLOAT_EQ(room.OpenTickets().front().standbyLeft, standbyLeft);

	room.World().get_mut<ShiftWindow>().frozen = false;
	room.StepSeconds(1.0F);
	EXPECT_LT(room.Window().windowLeft, windowLeft);
	EXPECT_LT(room.OpenTickets().front().standbyLeft, standbyLeft);
}

TEST(Tickets, AShiftNamingAnUnknownTierFailsToLoadNamingIt) {
	RoomHarness room;
	const auto loaded = room.LoadShift(Fixture("unknown_tier"));
	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), HasSubstr("bogus"));
	EXPECT_THAT(loaded.error(), HasSubstr("unknown_tier.shift.json"));
}

TEST(Tickets, AShiftNamingAnUnknownFrequencyFailsToLoadNamingIt) {
	RoomHarness room;
	const auto loaded = room.LoadShift(Fixture("unknown_frequency"));
	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), HasSubstr("9.9"));
}

TEST(Tickets, AFrequencyOnNoInServiceRadioFailsToLoad) {
	RoomHarness room;
	const auto loaded = room.LoadShift(Fixture("dark_frequency"));
	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), HasSubstr("8.0"));
	EXPECT_THAT(loaded.error(), HasSubstr("no in-service Radio"));
}

TEST(Tickets, AMalformedPinnedEntryFailsToLoadNamingIt) {
	RoomHarness room;
	const auto loaded = room.LoadShift(Fixture("bad_pinned"));
	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), HasSubstr("pinned[0]"));
}

} // namespace
