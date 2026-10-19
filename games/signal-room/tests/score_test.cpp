#include <string>
#include <vector>

#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "room/score_components.hpp"
#include "room/ticket_components.hpp"
#include "room/ticket_simulation.hpp"
#include "room_harness.hpp"

using signal_room::Score;
using signal_room::ScoreChanged;
using signal_room::ShiftPhase;
using signal_room::ShiftWindow;
using signal_room::Ticket;
using signal_room::test::RoomHarness;
using testing::HasSubstr;

namespace {

constexpr uint64_t kSeed = 7;

std::string Fixture(const std::string_view name) {
	return std::string{SIGNAL_ROOM_FIXTURE_DIR} + "/" + std::string{name} + ".shift.json";
}

// Load a fixture with the clock frozen, so Tickets sit at the gauge fraction the test sets.
void Load(RoomHarness& room, const std::string_view fixture) {
	const auto loaded = room.LoadShiftWithSeed(Fixture(fixture), kSeed);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	room.World().get_mut<ShiftWindow>().frozen = true;
}

// Put an On Air Ticket on the rack with `fractionLeft` of its timer left.
void Spawn(RoomHarness& room, const std::string& tier, const std::string& frequency, const float fractionLeft) {
	auto& world = room.World();
	signal_room::SpawnTicket(world, world.lookup("RoomRoot"), tier, frequency, 0.0F, true);
	room.Step();
	world.query<Ticket>().each([&](Ticket& ticket) {
		if (ticket.state == signal_room::TicketState::OnAir && ticket.onAirLeft == ticket.onAirTotal) {
			ticket.onAirLeft = ticket.onAirTotal * fractionLeft;
		}
	});
}

void Transmit(RoomHarness& room, const std::string& frequency) {
	signal_room::RaiseTransmission(room.World(), frequency);
	room.Step();
}

const Score& Current(const RoomHarness& room) { return room.World().get<Score>(); }

TEST(Scoring, AGreenNormalClearScoresTwentyAndTheNextIsTimesOnePointTwoFive) {
	RoomHarness room;
	Load(room, "score_empty");

	Spawn(room, "normal", "7.0", 0.9F);
	Transmit(room, "7.0");
	EXPECT_EQ(Current(room).score, 20);
	EXPECT_EQ(Current(room).chainStep, 1);

	Spawn(room, "normal", "7.0", 0.9F);
	Transmit(room, "7.0");
	EXPECT_EQ(Current(room).score, 45);
	EXPECT_EQ(Current(room).cleared, 2);
}

TEST(Scoring, ARedBandClearAtChainStepTwoScoresFifteen) {
	RoomHarness room;
	Load(room, "score_empty");
	for (int i = 0; i < 2; ++i) {
		Spawn(room, "normal", "7.0", 0.9F);
		Transmit(room, "7.0");
	}
	ASSERT_EQ(Current(room).chainStep, 2);
	const int before = Current(room).score;

	Spawn(room, "normal", "7.0", 0.1F);
	Transmit(room, "7.0");
	EXPECT_EQ(Current(room).score - before, 15);
}

TEST(Scoring, TheChainCapsAtTimesTwo) {
	RoomHarness room;
	Load(room, "score_empty");
	for (int i = 0; i < 8; ++i) {
		Spawn(room, "normal", "7.0", 0.9F);
		Transmit(room, "7.0");
	}
	EXPECT_EQ(Current(room).chainStep, 4);
	EXPECT_EQ(Current(room).bestChainStep, 4);
	const int before = Current(room).score;

	Spawn(room, "normal", "7.0", 0.9F);
	Transmit(room, "7.0");
	EXPECT_EQ(Current(room).score - before, 40);
}

TEST(Scoring, ALostInterceptCostsTenFlooredAtZeroAndResetsTheChainButAnUnmatchedTransmissionDoesNot) {
	RoomHarness room;
	Load(room, "score_empty");
	room.World().set<Score>({.score = 5, .chainStep = 3});

	Transmit(room, "7.5");
	EXPECT_EQ(Current(room).score, 5);
	EXPECT_EQ(Current(room).chainStep, 3);

	Spawn(room, "normal", "7.0", 0.5F);
	room.World().get_mut<ShiftWindow>().frozen = false;
	room.StepSeconds(50.0F);
	EXPECT_EQ(Current(room).lost, 1);
	EXPECT_EQ(Current(room).score, 0);
	EXPECT_EQ(Current(room).chainStep, 0);
	EXPECT_EQ(Current(room).bestChainStep, 0);
}

TEST(Scoring, ParSumsEveryTicketThatWentOnAirIncludingLostOnesButNotStandByOnes) {
	RoomHarness room;
	Load(room, "score_pinned");
	room.World().get_mut<ShiftWindow>().frozen = false;
	room.StepSeconds(1.0F);
	// One NORMAL On Air; the HIGH and FLASH are still on Stand-by.
	EXPECT_EQ(Current(room).par, 20);

	room.StepSeconds(95.0F);
	// All three went On Air and ran out, and none of that takes par back.
	EXPECT_EQ(Current(room).lost, 3);
	EXPECT_EQ(Current(room).par, 20 + 30 + 40);
}

TEST(Scoring, StarsComeFromFractionsOfParAtTheHardStop) {
	RoomHarness room;
	Load(room, "score_empty");
	room.World().set<Score>({.score = 13, .par = 20});
	room.World().get_mut<ShiftWindow>().windowLeft = 0.01F;
	room.World().get_mut<ShiftWindow>().frozen = false;
	room.StepSeconds(0.5F);

	EXPECT_EQ(room.Window().phase, ShiftPhase::Ended);
	EXPECT_EQ(Current(room).stars, 2);
}

TEST(Scoring, AShiftWithAbsoluteThresholdsUsesThem) {
	RoomHarness room;
	Load(room, "score_absolute");
	room.World().set<Score>({.score = 12, .par = 1000});
	room.World().get_mut<ShiftWindow>().windowLeft = 0.01F;
	room.World().get_mut<ShiftWindow>().frozen = false;
	room.StepSeconds(0.5F);

	EXPECT_EQ(Current(room).stars, 2);
}

TEST(Scoring, AShiftGivingBothFractionsAndAbsoluteThresholdsIsALoadError) {
	RoomHarness room;
	const auto loaded = room.LoadShiftWithSeed(Fixture("score_both_stars"), kSeed);
	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), HasSubstr("both"));
}

TEST(Scoring, ReloadResetsTheScore) {
	RoomHarness room;
	Load(room, "score_empty");
	Spawn(room, "normal", "7.0", 0.9F);
	Transmit(room, "7.0");
	ASSERT_EQ(Current(room).score, 20);

	ASSERT_TRUE(room.Reload().has_value());
	EXPECT_EQ(Current(room).score, 0);
	EXPECT_EQ(Current(room).chainStep, 0);
}

} // namespace
