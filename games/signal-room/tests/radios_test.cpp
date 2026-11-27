#include <string>
#include <string_view>

#include <glm/glm.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engine/input/input.hpp"
#include "engine/spatial/spatial_components.hpp"
#include "room/item_components.hpp"
#include "room/operator_components.hpp"
#include "room/radio_components.hpp"
#include "room/station_components.hpp"
#include "room/ticket_simulation.hpp"
#include "room_harness.hpp"

namespace KeyCode = engine::input::KeyCode;
using engine::spatial::Transform;
using signal_room::AntennaCoupler;
using signal_room::Carries;
using signal_room::Holds;
using signal_room::Item;
using signal_room::Job;
using signal_room::JobPhase;
using signal_room::Operator;
using signal_room::OperatorMotion;
using signal_room::Radio;
using signal_room::Refusal;
using signal_room::Ticket;
using signal_room::test::RoomHarness;
using testing::HasSubstr;

namespace {

constexpr float kNorth = 0.0F;
constexpr float kSouth = 3.1415927F;
constexpr float kEast = -1.5707963F;
// Radio1 fills cells (1..2, 0), worked from the south.
constexpr glm::vec2 kSouthOfRadio1{1.6F, 1.4F};
constexpr glm::vec2 kNorthOfDecoder{6.5F, 2.6F};
constexpr glm::vec2 kWestOfTelex{9.6F, 3.5F};

std::string Fixture(const std::string_view name) {
	return std::string{SIGNAL_ROOM_FIXTURE_DIR} + "/" + std::string{name};
}

void Load(RoomHarness& room, const std::string_view fixture) {
	const auto loaded = room.LoadShift(Fixture(fixture));
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
}

flecs::entity FindOperator(const RoomHarness& room) {
	flecs::entity found;
	room.World().each([&](const flecs::entity e, const Operator&) { found = e; });
	return found;
}

flecs::entity Placement(const RoomHarness& room, const std::string_view name) {
	return room.World().lookup((std::string{"RoomRoot::"} + std::string{name}).c_str());
}

void StandAt(RoomHarness& room, const glm::vec2 position, const float yaw) {
	const auto op = FindOperator(room);
	op.get_mut<Transform>().position = {position.x, 0.0F, position.y};
	op.get_mut<OperatorMotion>().facing = yaw;
	room.Step();
}

void Interact(RoomHarness& room) {
	room.Tap(KeyCode::Space);
	room.Step(2);
}

bool Online(const RoomHarness& room, const std::string_view radio) {
	return Placement(room, radio).get<Radio>().online;
}

const Job& JobOf(const RoomHarness& room, const std::string_view station) {
	return Placement(room, station).get<Job>();
}

flecs::entity CardOn(const RoomHarness& room, const std::string_view station) {
	return Placement(room, station).target<Holds>();
}

void PutOnAir(RoomHarness& room, const std::string& frequency, const std::string& callsign = {}) {
	signal_room::SpawnTicket(
		room.World(),
		room.World().lookup("RoomRoot"),
		"normal",
		frequency,
		0.0F,
		true,
		true,
		callsign
	);
	room.Step();
}

struct Refusals {
	int count{0};
};

void Watch(RoomHarness& room, Refusals& refusals) {
	room.World().observer<Operator>().event<Refusal>().each([&refusals](flecs::iter&, size_t, Operator&) {
		++refusals.count;
	});
}

TEST(Radios, ARadioWithNoOnAirTicketRefusesAHoldAndMakesNoCard) {
	RoomHarness room;
	Refusals refusals;
	Watch(room, refusals);
	Load(room, "two_radios.shift.json");
	StandAt(room, kSouthOfRadio1, kNorth);
	EXPECT_FALSE(Online(room, "Radio1"));

	room.Hold(KeyCode::Space);
	room.StepSeconds(2.5F);
	room.Release(KeyCode::Space);

	EXPECT_GE(refusals.count, 1);
	EXPECT_EQ(JobOf(room, "Radio1").phase, JobPhase::Idle);
	EXPECT_FALSE(CardOn(room, "Radio1"));
}

TEST(Radios, HoldingTwoSecondsAtALitRadioCopiesARawCardInItsFrequency) {
	RoomHarness room;
	Load(room, "two_radios.shift.json");
	PutOnAir(room, "7.0");
	ASSERT_TRUE(Online(room, "Radio1"));
	StandAt(room, kSouthOfRadio1, kNorth);

	room.Hold(KeyCode::Space);
	room.StepSeconds(1.8F);
	EXPECT_EQ(JobOf(room, "Radio1").phase, JobPhase::Running);
	room.StepSeconds(0.4F);
	room.Release(KeyCode::Space);

	ASSERT_TRUE(CardOn(room, "Radio1"));
	EXPECT_EQ(CardOn(room, "Radio1").get<Item>().id, "raw_card");
	EXPECT_EQ(CardOn(room, "Radio1").get<Item>().frequency, "7.0");
}

TEST(Radios, ReleasingMidCopyKeepsProgressAndHoldingAgainFinishesIt) {
	RoomHarness room;
	Load(room, "two_radios.shift.json");
	PutOnAir(room, "7.0");
	StandAt(room, kSouthOfRadio1, kNorth);

	room.Hold(KeyCode::Space);
	room.StepSeconds(1.0F);
	room.Release(KeyCode::Space);
	room.StepSeconds(0.6F);
	EXPECT_NEAR(JobOf(room, "Radio1").progress, 0.5F, 0.05F);
	EXPECT_FALSE(CardOn(room, "Radio1"));

	room.Hold(KeyCode::Space);
	room.StepSeconds(1.1F);
	room.Release(KeyCode::Space);
	EXPECT_TRUE(CardOn(room, "Radio1"));
}

TEST(Radios, EngagingOneRadioDarkensTheOtherAndBothStayDarkForTheCouplerReset) {
	RoomHarness room;
	Load(room, "two_radios.shift.json");
	PutOnAir(room, "7.0");
	PutOnAir(room, "7.5");
	ASSERT_TRUE(Online(room, "Radio1"));
	ASSERT_TRUE(Online(room, "Radio2"));
	StandAt(room, kSouthOfRadio1, kNorth);

	room.Hold(KeyCode::Space);
	room.StepSeconds(0.5F);
	EXPECT_TRUE(Online(room, "Radio1"));
	EXPECT_FALSE(Online(room, "Radio2"));
	EXPECT_EQ(room.World().get<AntennaCoupler>().coupled, Placement(room, "Radio1").id());

	room.StepSeconds(1.7F);
	room.Release(KeyCode::Space);
	room.StepSeconds(0.2F);
	EXPECT_EQ(room.World().get<AntennaCoupler>().coupled, 0U);
	EXPECT_FALSE(Online(room, "Radio1"));
	EXPECT_FALSE(Online(room, "Radio2"));

	room.StepSeconds(0.4F);
	EXPECT_TRUE(Online(room, "Radio1"));
	EXPECT_TRUE(Online(room, "Radio2"));
}

TEST(Radios, ACopyFinishesEvenWhenItsTicketIsClearedMidHold) {
	RoomHarness room;
	Load(room, "two_radios.shift.json");
	PutOnAir(room, "7.0");
	StandAt(room, kSouthOfRadio1, kNorth);

	room.Hold(KeyCode::Space);
	room.StepSeconds(1.0F);
	room.World().delete_with<Ticket>();
	room.StepSeconds(1.2F);
	room.Release(KeyCode::Space);

	ASSERT_TRUE(CardOn(room, "Radio1"));
	EXPECT_EQ(CardOn(room, "Radio1").get<Item>().frequency, "7.0");
}

TEST(Radios, AFinishedCardWaitsOnTheRadioUntilPickedUpAndNoSecondCopyStarts) {
	RoomHarness room;
	Load(room, "two_radios.shift.json");
	PutOnAir(room, "7.0");
	StandAt(room, kSouthOfRadio1, kNorth);
	room.Hold(KeyCode::Space);
	room.StepSeconds(2.2F);
	room.Release(KeyCode::Space);
	room.StepSeconds(0.6F);
	ASSERT_EQ(JobOf(room, "Radio1").phase, JobPhase::Done);

	// A press picks the card up rather than starting a second copy.
	Interact(room);
	EXPECT_TRUE(FindOperator(room).target<Carries>());
	EXPECT_EQ(JobOf(room, "Radio1").phase, JobPhase::Idle);
}

TEST(Radios, ShiftOneRoutePlaysFromRadioThroughDecoderAndTelexToClearTheTicket) {
	RoomHarness room;
	const auto loaded = room.LoadShift("shifts/shift_01.shift.json");
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	PutOnAir(room, "7.0", "ROUTE");
	const auto routeOpen = [&] {
		bool found = false;
		for (const auto& ticket : room.OpenTickets()) {
			found = found || ticket.callsign == "ROUTE";
		}
		return found;
	};
	ASSERT_TRUE(routeOpen());

	StandAt(room, kSouthOfRadio1, kNorth);
	room.Hold(KeyCode::Space);
	room.StepSeconds(2.2F);
	room.Release(KeyCode::Space);
	room.StepSeconds(0.5F);
	Interact(room);
	ASSERT_EQ(FindOperator(room).target<Carries>().get<Item>().id, "raw_card");

	StandAt(room, kNorthOfDecoder, kSouth);
	Interact(room);
	room.StepSeconds(6.5F);
	ASSERT_EQ(JobOf(room, "Decoder").phase, JobPhase::Done);
	Interact(room);
	ASSERT_EQ(FindOperator(room).target<Carries>().get<Item>().id, "decoded_card");

	StandAt(room, kWestOfTelex, kEast);
	Interact(room);
	Interact(room);

	EXPECT_FALSE(routeOpen());
}

TEST(Radios, AShiftWithAnInServiceRadioLackingAFrequencyFailsNamingTheRadio) {
	RoomHarness room;
	const auto loaded = room.LoadShift(Fixture("radio_missing_frequency.shift.json"));
	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), HasSubstr("Radio1"));
}

TEST(Radios, EngagingReleasingAndComingBackOnlineRaiseTheirEventsAndMovingDisconnects) {
	RoomHarness room;
	int engaged = 0;
	int released = 0;
	int online = 0;
	room.World().observer<Radio>().event<signal_room::CouplerEngaged>().each([&](Radio&) { ++engaged; });
	room.World().observer<Radio>().event<signal_room::CouplerReleased>().each([&](Radio&) { ++released; });
	room.World().observer<Radio>().event<signal_room::RadioOnline>().each([&](Radio&) { ++online; });
	Load(room, "two_radios.shift.json");
	PutOnAir(room, "7.0");
	EXPECT_EQ(online, 1);
	StandAt(room, kSouthOfRadio1, kNorth);

	room.Hold(KeyCode::Space);
	room.StepSeconds(0.5F);
	EXPECT_EQ(engaged, 1);
	EXPECT_EQ(released, 0);

	room.Hold(KeyCode::D);
	room.Step(2);
	room.Release(KeyCode::D);
	room.Release(KeyCode::Space);
	EXPECT_EQ(released, 1);
	EXPECT_EQ(room.World().get<AntennaCoupler>().coupled, 0U);

	room.StepSeconds(0.6F);
	EXPECT_EQ(online, 2);
}

} // namespace
