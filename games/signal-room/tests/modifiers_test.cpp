#include <string>
#include <string_view>

#include <glm/glm.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engine/input/input.hpp"
#include "engine/spatial/spatial_components.hpp"
#include "room/item_components.hpp"
#include "room/operator_components.hpp"
#include "room/room_components.hpp"
#include "room/ticket_simulation.hpp"
#include "room_harness.hpp"

namespace KeyCode = engine::input::KeyCode;
using engine::spatial::Transform;
using signal_room::Carries;
using signal_room::Holds;
using signal_room::Item;
using signal_room::ModifierForce;
using signal_room::Operator;
using signal_room::OperatorMotion;
using signal_room::Tarped;
using signal_room::test::RoomHarness;
using testing::HasSubstr;

namespace {

constexpr float kNorth = 0.0F;
constexpr float kEast = -1.5707963F;
constexpr float kSouth = 3.1415927F;
constexpr glm::vec2 kSouthOfRadio1{1.6F, 1.4F};
// The Keypunch is cell (3,2), worked from the south.
constexpr glm::vec2 kSouthOfKeypunch{3.5F, 3.4F};
constexpr glm::vec2 kBesideCounterSouthLeft{6.5F, 5.4F};
constexpr glm::vec2 kWestOfTelex{9.6F, 3.5F};

std::string Fixture(const std::string_view name) {
	return std::string{SIGNAL_ROOM_FIXTURE_DIR} + "/" + std::string{name};
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

void HoldFor(RoomHarness& room, const float seconds) {
	room.Hold(KeyCode::Space);
	room.StepSeconds(seconds);
	room.Release(KeyCode::Space);
	room.Step();
}

void PutOnAir(RoomHarness& room) {
	signal_room::SpawnTicket(room.World(), room.World().lookup("RoomRoot"), "normal", "7.0", 0.0F, true);
	room.Step();
}

std::string CarriedId(const RoomHarness& room) {
	const auto item = FindOperator(room).target<Carries>();
	return item ? item.get<Item>().id : std::string{};
}

TEST(Modifiers, KeypunchOnPutsTheKeypunchInServiceAndTheRadioGivesAMorseTape) {
	RoomHarness room;
	const auto loaded = room.LoadShiftWithModifiers(Fixture("radio_route.shift.json"), {{.id = "keypunch", .on = true}});
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	EXPECT_FALSE(Placement(room, "Keypunch").has<Tarped>());
	PutOnAir(room);

	StandAt(room, kSouthOfRadio1, kNorth);
	HoldFor(room, 2.3F);
	StandAt(room, kSouthOfRadio1, kNorth);
	Interact(room);
	ASSERT_EQ(CarriedId(room), "morse_tape");

	StandAt(room, kSouthOfKeypunch, kNorth);
	Interact(room);
	ASSERT_EQ(CarriedId(room), "");
	HoldFor(room, 1.7F);

	const auto card = Placement(room, "Keypunch").target<Holds>();
	ASSERT_TRUE(card);
	EXPECT_EQ(card.get<Item>().id, "raw_card");
	EXPECT_EQ(card.get<Item>().frequency, "7.0");
}

TEST(Modifiers, KeypunchOffLeavesItTarpedAndTheRadioGivesARawCard) {
	RoomHarness room;
	const auto loaded = room.LoadShiftWithModifiers(Fixture("radio_route.shift.json"), {});
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	EXPECT_TRUE(Placement(room, "Keypunch").has<Tarped>());
	PutOnAir(room);

	StandAt(room, kSouthOfRadio1, kNorth);
	HoldFor(room, 2.3F);
	const auto card = Placement(room, "Radio1").target<Holds>();
	ASSERT_TRUE(card);
	EXPECT_EQ(card.get<Item>().id, "raw_card");
}

TEST(Modifiers, TelexAutostampTransmitsWithoutAPress) {
	RoomHarness room;
	const auto loaded =
		room.LoadShiftWithModifiers(Fixture("autostamp_route.shift.json"), {{.id = "telex_autostamp", .on = true}});
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	PutOnAir(room);
	ASSERT_EQ(room.OpenTickets().size(), 1U);

	StandAt(room, kBesideCounterSouthLeft, kSouth);
	Interact(room);
	ASSERT_EQ(CarriedId(room), "decoded_card");
	StandAt(room, kWestOfTelex, kEast);
	Interact(room);
	room.Step(2);

	EXPECT_TRUE(room.OpenTickets().empty());
}

TEST(Modifiers, TheToggleForcingAShiftsModifierOffLeavesItOff) {
	RoomHarness room;
	const auto loaded = room.LoadShiftWithModifiers(
		Fixture("radio_route_keypunch.shift.json"),
		{{.id = "keypunch", .on = false}}
	);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	EXPECT_TRUE(Placement(room, "Keypunch").has<Tarped>());
}

TEST(Modifiers, TheShiftListingAModifierTurnsItOnWithoutAToggle) {
	RoomHarness room;
	const auto loaded = room.LoadShiftWithModifiers(Fixture("radio_route_keypunch.shift.json"), {});
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	EXPECT_FALSE(Placement(room, "Keypunch").has<Tarped>());
}

TEST(Modifiers, AnOverrideWhosePathHasNoParentFailsTheLoadNamingTheModifier) {
	RoomHarness room;
	const auto loaded = room.LoadShiftWithModifiers(
		Fixture("radio_route.shift.json"),
		{{.id = "broken", .on = true}},
		Fixture("modifiers_bad_parent.json")
	);
	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), HasSubstr("Modifier 'broken' override 0"));
	EXPECT_THAT(loaded.error(), HasSubstr("/nope/deeper"));
}

TEST(Modifiers, AnUnknownModifierInTheToggleFailsTheLoadNamingIt) {
	RoomHarness room;
	const auto loaded = room.LoadShiftWithModifiers(Fixture("radio_route.shift.json"), {{.id = "typo", .on = true}});
	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), HasSubstr("'typo'"));
}

} // namespace
