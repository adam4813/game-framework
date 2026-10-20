#include <cmath>
#include <numbers>
#include <string>
#include <string_view>

#include <glm/glm.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engine/input/input.hpp"
#include "engine/spatial/spatial_components.hpp"
#include "room/item_catalogue.hpp"
#include "room/item_components.hpp"
#include "room/operator_components.hpp"
#include "room/station_catalogue.hpp"
#include "room/station_components.hpp"
#include "room/ticket_simulation.hpp"
#include "room_harness.hpp"

namespace KeyCode = engine::input::KeyCode;
using engine::spatial::Transform;
using signal_room::Carries;
using signal_room::Holding;
using signal_room::Holds;
using signal_room::Item;
using signal_room::Job;
using signal_room::JobPhase;
using signal_room::Operator;
using signal_room::OperatorMotion;
using signal_room::Refusal;
using signal_room::RefusalReason;
using signal_room::TicketState;
using signal_room::Transmission;
using signal_room::test::RoomHarness;
using testing::HasSubstr;

namespace {

constexpr float kDegrees = std::numbers::pi_v<float> / 180.0F;
// Facings as yaws: 0 faces north and positive turns toward the west.
constexpr float kNorth = 0.0F;
constexpr float kEast = -90.0F * kDegrees;
constexpr float kSouth = 180.0F * kDegrees;

// The Decoder fills cells (6..7, 3..4); the Counters south of it are (6,6) and (7,6). Standing in row 5
// the Operator works the Decoder to its north and a Counter to its south.
constexpr glm::vec2 kBesideCounterSouthLeft{6.5F, 5.4F};
// The active Decoder is cell row 3 (Decoder2, tarped, is row 4), worked from the north.
constexpr glm::vec2 kNorthOfDecoder{6.5F, 2.6F};
constexpr glm::vec2 kBesideCounterSouthRight{7.5F, 5.4F};
// The Telex is cell (10,3), worked from the west.
constexpr glm::vec2 kWestOfTelex{9.6F, 3.5F};

std::string Fixture(const std::string_view name) {
	return std::string{SIGNAL_ROOM_FIXTURE_DIR} + "/" + std::string{name};
}

// The Refusals raised, newest last. Declare it before the harness: the observer captures it.
struct Refusals {
	int count{0};
	RefusalReason last{RefusalReason::NothingToDo};
};

void Watch(RoomHarness& room, Refusals& refusals) {
	room.World().observer<Operator>().event<Refusal>().each([&refusals](flecs::iter& it, size_t, Operator&) {
		++refusals.count;
		refusals.last = it.param<Refusal>()->reason;
	});
}

void LoadDecodeRoute(RoomHarness& room) {
	const auto loaded = room.LoadShift(Fixture("decode_route.shift.json"));
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

// The Item the Operator carries, or an invalid entity.
flecs::entity Carried(const RoomHarness& room) { return FindOperator(room).target<Carries>(); }

std::string CarriedId(const RoomHarness& room) {
	const auto item = Carried(room);
	return item ? item.get<Item>().id : std::string{};
}

const Job& JobOf(const RoomHarness& room, const std::string_view station) {
	return Placement(room, station).get<Job>();
}

// The Item a Station holds, or an invalid entity.
flecs::entity HeldBy(const RoomHarness& room, const std::string_view station) {
	return Placement(room, station).target<Holds>();
}

void PickUp(RoomHarness& room, const glm::vec2 standing) {
	StandAt(room, standing, kSouth);
	Interact(room);
}

void PlaceOnDecoder(RoomHarness& room, const glm::vec2 standing) {
	StandAt(room, standing, kSouth);
	Interact(room);
}

// Pick up the card on CounterSouthLeft and place it on the Decoder.
void StartDecoding(RoomHarness& room) {
	PickUp(room, kBesideCounterSouthLeft);
	ASSERT_EQ(CarriedId(room), "raw_card");
	PlaceOnDecoder(room, kNorthOfDecoder);
	ASSERT_EQ(JobOf(room, "Decoder").phase, JobPhase::Running);
}

// Walk the Decoded Card from a finished Decoder into the Operator's hands.
void TakeDecodedCard(RoomHarness& room) {
	StandAt(room, kNorthOfDecoder, kSouth);
	Interact(room);
	ASSERT_EQ(CarriedId(room), "decoded_card");
}

TEST(Stations, ARawCardPlacedOnTheDecoderBecomesADecodedCardAfterSixSeconds) {
	RoomHarness room;
	LoadDecodeRoute(room);
	StartDecoding(room);
	EXPECT_EQ(JobOf(room, "Decoder").transform, "decode");

	room.StepSeconds(5.5F);
	EXPECT_EQ(JobOf(room, "Decoder").phase, JobPhase::Running);
	room.StepSeconds(1.0F);

	EXPECT_EQ(JobOf(room, "Decoder").phase, JobPhase::Done);
	const auto output = HeldBy(room, "Decoder");
	ASSERT_TRUE(output);
	EXPECT_EQ(output.get<Item>().id, "decoded_card");
	EXPECT_EQ(output.get<Item>().frequency, "7.0");
}

TEST(Stations, HoldingAtTheRunningDecoderFinishesItInAboutTwoAndAHalfSeconds) {
	RoomHarness room;
	LoadDecodeRoute(room);
	StartDecoding(room);

	room.Hold(KeyCode::Space);
	room.StepSeconds(2.2F);
	EXPECT_EQ(JobOf(room, "Decoder").phase, JobPhase::Running);
	room.StepSeconds(0.5F);
	EXPECT_EQ(JobOf(room, "Decoder").phase, JobPhase::Done);
	room.Release(KeyCode::Space);
}

TEST(Stations, ReleasingTheCrankMidJobKeepsItsProgress) {
	RoomHarness room;
	LoadDecodeRoute(room);
	StartDecoding(room);

	room.Hold(KeyCode::Space);
	room.StepSeconds(1.25F);
	room.Release(KeyCode::Space);
	room.Step();
	// Half the 2.5 s crank.
	EXPECT_NEAR(JobOf(room, "Decoder").progress, 0.5F, 0.03F);
	EXPECT_FALSE(FindOperator(room).has<Holding>());

	// Back at the normal 6 s pace: another 0.5 s adds a twelfth.
	room.StepSeconds(0.5F);
	EXPECT_NEAR(JobOf(room, "Decoder").progress, 0.5F + 0.5F / 6.0F, 0.03F);
}

TEST(Stations, TheDecodedCardCannotBePickedUpWhileCarryingButCanOnceEmptyHanded) {
	RoomHarness room;
	Refusals refusals;
	Watch(room, refusals);
	LoadDecodeRoute(room);
	StartDecoding(room);
	room.StepSeconds(6.5F);
	ASSERT_EQ(JobOf(room, "Decoder").phase, JobPhase::Done);

	PickUp(room, kBesideCounterSouthRight);
	ASSERT_EQ(CarriedId(room), "raw_card");
	StandAt(room, kNorthOfDecoder, kSouth);
	Interact(room);
	EXPECT_EQ(CarriedId(room), "raw_card");
	EXPECT_EQ(HeldBy(room, "Decoder").get<Item>().id, "decoded_card");
	EXPECT_EQ(refusals.count, 1);

	// Set the raw card back down, then take the decoded one.
	StandAt(room, kBesideCounterSouthRight, kSouth);
	Interact(room);
	ASSERT_EQ(CarriedId(room), "");
	TakeDecodedCard(room);
	EXPECT_EQ(JobOf(room, "Decoder").phase, JobPhase::Idle);
	EXPECT_FALSE(HeldBy(room, "Decoder"));
}

TEST(Stations, ASecondRawCardIsRefusedAtABusyDecoderAndStaysCarried) {
	RoomHarness room;
	Refusals refusals;
	Watch(room, refusals);
	LoadDecodeRoute(room);
	StartDecoding(room);

	PickUp(room, kBesideCounterSouthRight);
	StandAt(room, kNorthOfDecoder, kSouth);
	Interact(room);

	EXPECT_EQ(CarriedId(room), "raw_card");
	EXPECT_EQ(refusals.count, 1);
	EXPECT_EQ(refusals.last, RefusalReason::Busy);
	EXPECT_EQ(JobOf(room, "Decoder").phase, JobPhase::Running);
}

TEST(Stations, ADecodedCardOfferedToTheDecoderIsRefused) {
	RoomHarness room;
	Refusals refusals;
	Watch(room, refusals);
	LoadDecodeRoute(room);
	StartDecoding(room);
	room.StepSeconds(6.5F);
	TakeDecodedCard(room);

	PlaceOnDecoder(room, kNorthOfDecoder);

	EXPECT_EQ(CarriedId(room), "decoded_card");
	EXPECT_EQ(refusals.count, 1);
	EXPECT_EQ(refusals.last, RefusalReason::WrongItem);
	EXPECT_EQ(JobOf(room, "Decoder").phase, JobPhase::Idle);
}

TEST(Stations, StampingADecodedCardOnTheTelexClearsTheOnAirTicketOfItsFrequency) {
	RoomHarness room;
	LoadDecodeRoute(room);
	signal_room::SpawnTicket(room.World(), room.World().lookup("RoomRoot"), "normal", "7.0", 0.0F, true);
	room.Step();
	ASSERT_EQ(room.OpenTickets().size(), 1U);
	ASSERT_EQ(room.OpenTickets().front().state, TicketState::OnAir);

	StartDecoding(room);
	room.StepSeconds(6.5F);
	TakeDecodedCard(room);

	StandAt(room, kWestOfTelex, kEast);
	Interact(room);
	EXPECT_EQ(CarriedId(room), "");
	EXPECT_EQ(JobOf(room, "Telex").phase, JobPhase::Running);
	EXPECT_EQ(room.OpenTickets().size(), 1U);

	Interact(room);
	EXPECT_TRUE(room.OpenTickets().empty());
	EXPECT_EQ(JobOf(room, "Telex").phase, JobPhase::Idle);
	EXPECT_FALSE(HeldBy(room, "Telex"));
}

TEST(Stations, MovingDuringACrankEndsTheHold) {
	RoomHarness room;
	LoadDecodeRoute(room);
	StartDecoding(room);

	room.Hold(KeyCode::Space);
	room.StepSeconds(0.5F);
	ASSERT_TRUE(FindOperator(room).has<Holding>());

	room.Hold(KeyCode::D);
	room.Step(2);
	EXPECT_FALSE(FindOperator(room).has<Holding>());
	room.Release(KeyCode::D);
	room.Release(KeyCode::Space);
}

TEST(Stations, PressingEmptyHandedAtAnIdleDecoderRaisesARefusal) {
	RoomHarness room;
	Refusals refusals;
	Watch(room, refusals);
	LoadDecodeRoute(room);

	StandAt(room, kNorthOfDecoder, kSouth);
	Interact(room);

	EXPECT_EQ(refusals.count, 1);
	EXPECT_EQ(refusals.last, RefusalReason::NothingToDo);
	EXPECT_EQ(JobOf(room, "Decoder").phase, JobPhase::Idle);
}

TEST(Stations, ACounterThatCannotActRefusesThePress) {
	RoomHarness room;
	Refusals refusals;
	Watch(room, refusals);
	LoadDecodeRoute(room);
	PickUp(room, kBesideCounterSouthLeft);

	// Empty-handed at the now empty Counter is not possible; carrying at the full one is.
	StandAt(room, kBesideCounterSouthRight, kSouth);
	Interact(room);

	EXPECT_EQ(CarriedId(room), "raw_card");
	EXPECT_EQ(refusals.count, 1);
	EXPECT_EQ(refusals.last, RefusalReason::CounterFull);
}

TEST(Stations, AStationCatalogueWithADuplicateInputSetFailsNamingTheKind) {
	signal_room::ItemCatalogue items;
	items.Add("raw_card", {});
	items.Add("decoded_card", {});

	const auto loaded = signal_room::LoadStationCatalogueFile(Fixture("stations_duplicate_inputs.json"), items);

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), HasSubstr("decoder"));
	EXPECT_THAT(loaded.error(), HasSubstr("input set"));
}

TEST(Stations, AStationCatalogueNamingAnUnknownItemOrVerbFailsNamingTheEntry) {
	signal_room::ItemCatalogue items;
	items.Add("raw_card", {});
	items.Add("decoded_card", {});

	const auto unknownItem = signal_room::LoadStationCatalogueFile(Fixture("stations_unknown_item.json"), items);
	ASSERT_FALSE(unknownItem.has_value());
	EXPECT_THAT(unknownItem.error(), HasSubstr("no_such_card"));
	EXPECT_THAT(unknownItem.error(), HasSubstr("decode"));

	const auto unknownVerb = signal_room::LoadStationCatalogueFile(Fixture("stations_unknown_verb.json"), items);
	ASSERT_FALSE(unknownVerb.has_value());
	EXPECT_THAT(unknownVerb.error(), HasSubstr("shake"));
	EXPECT_THAT(unknownVerb.error(), HasSubstr("decode"));
}

} // namespace
