#include <algorithm>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "engine/engine.hpp"
#include "room/hud.hpp"
#include "room/ticket_components.hpp"
#include "room/ticket_simulation.hpp"
#include "room_harness.hpp"

using signal_room::HudCard;
using signal_room::HudGauge;
using signal_room::ShiftWindow;
using signal_room::Ticket;
using signal_room::test::RoomHarness;

namespace {

constexpr uint64_t kSeed = 7;

// A Room with no schedule, so only the Tickets a test puts on the rack exist.
void Load(RoomHarness& room, const bool frozen = true) {
	const auto loaded = room.LoadShiftWithSeed(std::string{SIGNAL_ROOM_FIXTURE_DIR} + "/score_empty.shift.json", kSeed);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	room.World().get_mut<ShiftWindow>().frozen = frozen;
}

void Spawn(RoomHarness& room, const std::string& tier, const std::string& frequency, const float overfill, const bool onAir) {
	auto& world = room.World();
	signal_room::SpawnTicket(world, world.lookup("RoomRoot"), tier, frequency, overfill, onAir, false, "BADGER");
	room.Step();
}

std::vector<flecs::entity> Cards(const RoomHarness& room) {
	std::vector<flecs::entity> cards;
	room.World().query<HudCard>().each([&](const flecs::entity card, const HudCard&) { cards.push_back(card); });
	return cards;
}

std::vector<std::string> LabelsOf(const flecs::entity card) {
	std::vector<std::string> texts;
	card.children([&](const flecs::entity child) {
		if (child.has<engine::ui::Label>()) {
			texts.push_back(child.get<engine::ui::Label>().text);
		}
	});
	return texts;
}

HudGauge GaugeOf(const RoomHarness& room) {
	HudGauge gauge;
	room.World().query<const HudGauge>().each([&](const HudGauge& found) { gauge = found; });
	return gauge;
}

// Set the one Ticket's On Air timer to `fractionLeft` of its total and let the HUD catch up.
void DrainTo(RoomHarness& room, const float fractionLeft) {
	room.World().query<Ticket>().each([&](Ticket& ticket) { ticket.onAirLeft = ticket.onAirTotal * fractionLeft; });
	room.Step();
}

std::string TextOf(const RoomHarness& room, const flecs::entity_t tag) {
	std::string text;
	room.World().query_builder<const engine::ui::Label>().with(tag).build().each([&](const engine::ui::Label& label) {
		text = label.text;
	});
	return text;
}

TEST(Hud, AnArrivingTicketAddsACardShowingItsCallsignTierAndFrequency) {
	RoomHarness room;
	Load(room);
	Spawn(room, "high", "7.0", 0.0F, true);

	const auto cards = Cards(room);
	ASSERT_EQ(cards.size(), 1U);
	auto labels = LabelsOf(cards[0]);
	std::ranges::sort(labels);
	EXPECT_EQ(labels, (std::vector<std::string>{"7.0", "BADGER", "HIGH PRIORITY"}));
}

TEST(Hud, AStandByTicketReportsItsOverfillSegmentAndGoesGreenAmberRedOnceOnAir) {
	RoomHarness room;
	Load(room);
	Spawn(room, "normal", "7.0", 0.3F, false);
	EXPECT_NEAR(GaugeOf(room).standby, 0.3F, 0.01F);

	room.World().query<Ticket>().each([](Ticket& ticket) {
		ticket.state = signal_room::TicketState::OnAir;
		ticket.standbyLeft = 0.0F;
		ticket.onAirTotal = 90.0F;
	});
	DrainTo(room, 0.9F);
	EXPECT_EQ(GaugeOf(room).band, "green");
	EXPECT_EQ(GaugeOf(room).standby, 0.0F);
	DrainTo(room, 0.4F);
	EXPECT_EQ(GaugeOf(room).band, "amber");
	DrainTo(room, 0.1F);
	EXPECT_EQ(GaugeOf(room).band, "red");
}

TEST(Hud, AClearedCardFlashesThenGoesAndTheRackNeverShowsMoreThanFive) {
	RoomHarness room;
	Load(room);
	for (int i = 0; i < 5; ++i) {
		Spawn(room, "normal", "7.0", 0.0F, true);
	}
	EXPECT_EQ(Cards(room).size(), 5U);

	signal_room::RaiseTransmission(room.World(), "7.0");
	room.Step();
	EXPECT_EQ(room.OpenTickets().size(), 4U);
	EXPECT_EQ(Cards(room).size(), 5U); // the Cleared card is still flashing

	Spawn(room, "normal", "7.0", 0.0F, true); // a sixth would push the rack past its cap
	EXPECT_EQ(Cards(room).size(), 5U);

	room.StepSeconds(1.0F);
	EXPECT_EQ(Cards(room).size(), 5U);
	signal_room::RaiseTransmission(room.World(), "7.0");
	room.StepSeconds(1.0F);
	EXPECT_EQ(Cards(room).size(), 4U);
}

TEST(Hud, TheClockReadsTheWindowLeftAsMinutesAndSeconds) {
	RoomHarness room;
	Load(room, false);
	room.Step();
	EXPECT_EQ(TextOf(room, room.World().entity<signal_room::HudClock>()), "4:00");
	room.StepSeconds(30.0F);
	EXPECT_EQ(TextOf(room, room.World().entity<signal_room::HudClock>()), "3:30");
}

TEST(Hud, TheScoreLabelShowsTheNewScoreAfterAClear) {
	RoomHarness room;
	Load(room);
	Spawn(room, "normal", "7.0", 0.0F, true);
	room.StepSeconds(1.0F);
	signal_room::RaiseTransmission(room.World(), "7.0");
	room.Step();
	EXPECT_EQ(TextOf(room, room.World().entity<signal_room::HudScore>()), "20");
	EXPECT_EQ(TextOf(room, room.World().entity<signal_room::HudChain>()), "×" "1.25");
}

} // namespace
