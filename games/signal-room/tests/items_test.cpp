#include <cmath>
#include <format>
#include <initializer_list>
#include <numbers>
#include <string_view>

#include <glm/glm.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engine/input/input.hpp"
#include "engine/particles/particle_components.hpp"
#include "engine/spatial/spatial_components.hpp"
#include "room/item_components.hpp"
#include "room/operator_components.hpp"
#include "room/room_tuning.hpp"
#include "room_harness.hpp"

namespace KeyCode = engine::input::KeyCode;
using engine::particles::Particle;
using engine::spatial::Transform;
using signal_room::Carries;
using signal_room::DustPuffTuning;
using signal_room::Holds;
using signal_room::Item;
using signal_room::Operator;
using signal_room::OperatorMotion;
using signal_room::Targets;
using signal_room::test::RoomHarness;

namespace {

constexpr std::string_view kShift1 = "shifts/shift_01.shift.json";
constexpr float kDegrees = std::numbers::pi_v<float> / 180.0F;
// Facings as yaws: 0 faces north and positive turns toward the west.
constexpr float kNorth = 0.0F;
constexpr float kEast = -90.0F * kDegrees;

flecs::entity FindOperator(const RoomHarness& room) {
	flecs::entity found;
	room.World().each([&](const flecs::entity e, const Operator&) { found = e; });
	return found;
}

flecs::entity Placement(const RoomHarness& room, const std::string_view name) {
	return room.World().lookup(std::format("RoomRoot::{}", name).c_str());
}

// Name of what an entity carries or holds through `Relationship`, or "" for nothing.
template<typename Relationship>
std::string_view Contents(const flecs::entity holder) {
	const auto item = holder.target<Relationship>();
	return item ? std::string_view{item.name().c_str()} : std::string_view{};
}

std::string_view Carried(const RoomHarness& room) { return Contents<Carries>(FindOperator(room)); }
std::string_view HeldBy(const RoomHarness& room, const std::string_view counter) {
	return Contents<Holds>(Placement(room, counter));
}

// How many Operators carry and Counters hold the named Item.
int HolderCount(const RoomHarness& room, const std::string_view itemName) {
	const auto item = Placement(room, itemName);
	return room.World().count(room.World().pair<Carries>(item)) + room.World().count(room.World().pair<Holds>(item));
}

std::string_view TargetName(const RoomHarness& room) {
	const auto target = FindOperator(room).target<Targets>();
	return target ? std::string_view{target.name().c_str()} : std::string_view{};
}

// Stand the Operator at a floor position (x east, y south) facing `yaw`, and let one frame run.
void StandAt(RoomHarness& room, const glm::vec2 position, const float yaw) {
	const auto op = FindOperator(room);
	op.get_mut<Transform>().position = {position.x, 0.0F, position.y};
	op.get_mut<OperatorMotion>().facing = yaw;
	room.Step();
}

glm::vec2 FloorPosition(const RoomHarness& room) {
	const auto& position = FindOperator(room).get<Transform>().position;
	return {position.x, position.z};
}

// Hold `keys` for `seconds` of 60 Hz frames, then release them and let the Operator stop.
void HoldFor(RoomHarness& room, const std::initializer_list<int> keys, const float seconds) {
	for (const int key : keys) {
		room.Hold(key);
	}
	room.Step(static_cast<int>(std::lround(seconds / signal_room::test::kFixedDelta)));
	for (const int key : keys) {
		room.Release(key);
	}
	room.Step(30);
}

void Interact(RoomHarness& room) {
	room.Tap(KeyCode::Space);
	room.Step(2);
}

// Shift 1 starts its placeholder Item on CounterSouthLeft (6,6). Stand south of it, facing it, and
// pick the Item up.
void PickUpFromCounterSouthLeft(RoomHarness& room) {
	StandAt(room, {6.5F, 7.5F}, kNorth);
	ASSERT_EQ(TargetName(room), "CounterSouthLeft");
	Interact(room);
	ASSERT_EQ(Carried(room), "Item1");
}

TEST(Items, PickUpWalkSetDownMovesTheItemToTheSecondCounter) {
	RoomHarness room;
	const auto loaded = room.LoadShift(kShift1);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	ASSERT_EQ(HeldBy(room, "CounterSouthLeft"), "Item1");
	EXPECT_EQ(HolderCount(room, "Item1"), 1);

	PickUpFromCounterSouthLeft(room);
	EXPECT_EQ(HeldBy(room, "CounterSouthLeft"), "");
	EXPECT_EQ(HolderCount(room, "Item1"), 1);

	// One cell east, then up against CounterSouthRight (7,6) to face it.
	HoldFor(room, {KeyCode::D}, 0.22F);
	HoldFor(room, {KeyCode::W}, 0.1F);
	ASSERT_EQ(TargetName(room), "CounterSouthRight");
	Interact(room);

	EXPECT_EQ(Carried(room), "");
	EXPECT_EQ(HeldBy(room, "CounterSouthLeft"), "");
	EXPECT_EQ(HeldBy(room, "CounterSouthRight"), "Item1");
	EXPECT_EQ(HolderCount(room, "Item1"), 1);
	// It rests on CounterSouthRight's cell (7,6).
	const auto& resting = Placement(room, "Item1").get<Transform>().position;
	EXPECT_FLOAT_EQ(resting.x, 7.5F);
	EXPECT_FLOAT_EQ(resting.z, 6.5F);
}

TEST(Items, TheCarriedItemIsShownInFrontOfTheOperatorTiltedTowardTheCamera) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	PickUpFromCounterSouthLeft(room);

	// Facing north from (6.5, 7.5): the Item is ahead (north, -z) at hand height, its top turned
	// toward the camera in the south.
	const auto& item = Placement(room, "Item1").get<Transform>();
	EXPECT_FLOAT_EQ(item.position.x, 6.5F);
	EXPECT_LT(item.position.z, 7.5F);
	EXPECT_GT(item.position.z, 7.5F - 0.6F);
	EXPECT_GT(item.position.y, 0.4F);
	EXPECT_GT(item.rotation.x, 0.0F);

	// It goes where the Operator goes.
	HoldFor(room, {KeyCode::A}, 0.3F);
	const auto& carried = Placement(room, "Item1").get<Transform>().position;
	EXPECT_LT(carried.x, FloorPosition(room).x);
	EXPECT_LT(carried.x, 6.0F);
}

TEST(Items, SettingDownOnAnOccupiedCounterRefusesAndTheItemStaysCarried) {
	RoomHarness room;
	const auto loaded = room.LoadShift(SIGNAL_ROOM_FIXTURE_DIR "/two_items.shift.json");
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	ASSERT_EQ(HeldBy(room, "CounterSouthRight"), "Item2");
	PickUpFromCounterSouthLeft(room);

	StandAt(room, {7.5F, 7.5F}, kNorth);
	ASSERT_EQ(TargetName(room), "CounterSouthRight");
	Interact(room);

	EXPECT_EQ(Carried(room), "Item1");
	EXPECT_EQ(HeldBy(room, "CounterSouthRight"), "Item2");
	EXPECT_EQ(HolderCount(room, "Item1"), 1);
}

TEST(Items, PressingEmptyHandedAtAnEmptyCounterDoesNothing) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	StandAt(room, {7.5F, 7.5F}, kNorth);
	ASSERT_EQ(TargetName(room), "CounterSouthRight");
	Interact(room);

	EXPECT_EQ(Carried(room), "");
	EXPECT_EQ(HeldBy(room, "CounterSouthRight"), "");
	EXPECT_EQ(HeldBy(room, "CounterSouthLeft"), "Item1");
}

TEST(Items, PressingAtAnInServiceStationDoesNothing) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// West of the Decoder (6-7, 3-4), facing it: empty-handed, then carrying.
	StandAt(room, {5.5F, 3.5F}, kEast);
	ASSERT_EQ(TargetName(room), "Decoder");
	Interact(room);
	EXPECT_EQ(Carried(room), "");
	EXPECT_EQ(HeldBy(room, "CounterSouthLeft"), "Item1");

	PickUpFromCounterSouthLeft(room);
	StandAt(room, {5.5F, 3.5F}, kEast);
	ASSERT_EQ(TargetName(room), "Decoder");
	Interact(room);
	EXPECT_EQ(Carried(room), "Item1");
	EXPECT_EQ(HolderCount(room, "Item1"), 1);
}

TEST(Items, InteractingTurnsTheOperatorToFaceTheCounterWithoutMovingIt) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// South-west of CounterSouthLeft (6,6), aimed 20 degrees east of north: the cone picks the Counter,
	// whose nearest corner (6,7) is 45 degrees east of north.
	StandAt(room, {5.5F, 7.5F}, -20.0F * kDegrees);
	ASSERT_EQ(TargetName(room), "CounterSouthLeft");
	Interact(room);

	ASSERT_EQ(Carried(room), "Item1");
	EXPECT_NEAR(FindOperator(room).get<OperatorMotion>().facing, -45.0F * kDegrees, 1e-4F);
	EXPECT_FLOAT_EQ(FloorPosition(room).x, 5.5F);
	EXPECT_FLOAT_EQ(FloorPosition(room).y, 7.5F);
}

// Pick up Shift 1's Item, then walk west along row 7 for 0.5 s, dashing on the fifth frame if asked.
// Returns how far the Operator went.
float CarryWestFor(RoomHarness& room, const bool dash, const float carrySpeed = 1.0F) {
	EXPECT_TRUE(room.LoadShift(kShift1).has_value());
	PickUpFromCounterSouthLeft(room);
	Placement(room, "Item1").get_mut<Item>().carrySpeed = carrySpeed;
	const float start = FloorPosition(room).x;
	room.Hold(KeyCode::A);
	for (int frame = 0; frame < 30; ++frame) {
		if (dash && frame == 5) {
			room.Tap(KeyCode::LeftShift);
		}
		room.Step();
	}
	room.Release(KeyCode::A);
	EXPECT_EQ(Carried(room), "Item1");
	return start - FloorPosition(room).x;
}

TEST(Items, DashingWhileCarryingCoversMoreGround) {
	RoomHarness walking;
	RoomHarness dashing;
	const float walked = CarryWestFor(walking, false);
	const float dashed = CarryWestFor(dashing, true);

	// 0.2 s at 2.2x walk adds about 1.1 m.
	EXPECT_GT(dashed - walked, 0.8F);
}

TEST(Items, AnItemWithCarrySpeedHalfHalvesTheWalkDistance) {
	RoomHarness full;
	RoomHarness half;
	const float fullDistance = CarryWestFor(full, false);
	const float halfDistance = CarryWestFor(half, false, 0.5F);

	// 0.5 s at 4.5 m/s, less the ramp up: about 2.1 m, and half that at half speed.
	EXPECT_NEAR(fullDistance, 2.07F, 0.05F);
	EXPECT_NEAR(halfDistance, fullDistance * 0.5F, 0.02F);
}

TEST(Items, ASetDownPuffsTheTunedAmountOfDustOffTheCounter) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	room.World().get_mut<DustPuffTuning>().count = 5.0F;
	PickUpFromCounterSouthLeft(room);
	// Picking up raises no dust.
	EXPECT_EQ(room.World().count<Particle>(), 0);

	StandAt(room, {7.5F, 7.5F}, kNorth);
	Interact(room);
	ASSERT_EQ(HeldBy(room, "CounterSouthRight"), "Item1");

	int puffed = 0;
	room.World().each([&](const flecs::entity e, const Particle&) {
		EXPECT_EQ(e.parent().parent(), Placement(room, "CounterSouthRight")) << e.path();
		++puffed;
	});
	EXPECT_EQ(puffed, 5);
}

TEST(Items, ReloadRestoresTheStartingCounterItems) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	PickUpFromCounterSouthLeft(room);

	const auto reloaded = room.Reload();

	ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
	EXPECT_EQ(Carried(room), "");
	EXPECT_EQ(HeldBy(room, "CounterSouthLeft"), "Item1");
	EXPECT_EQ(HolderCount(room, "Item1"), 1);
}

TEST(Items, AShiftPuttingAnItemOnAnUnknownCounterFailsNamingIt) {
	RoomHarness room;

	const auto loaded = room.LoadShift(SIGNAL_ROOM_FIXTURE_DIR "/unknown_counter_item.shift.json");

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), testing::HasSubstr("CounterNowhere"));
}

TEST(Items, AShiftPlacingAnUnknownItemIdFailsNamingIt) {
	RoomHarness room;

	const auto loaded = room.LoadShift(SIGNAL_ROOM_FIXTURE_DIR "/unknown_item.shift.json");

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), testing::HasSubstr("no_such_item"));
}

} // namespace
