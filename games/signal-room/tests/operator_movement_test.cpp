#include <algorithm>
#include <cmath>
#include <initializer_list>
#include <numbers>
#include <string_view>

#include <glm/glm.hpp>
#include <gmock/gmock.h>
#include <gtest/gtest.h>

#include "engine/input/input.hpp"
#include "engine/spatial/spatial_components.hpp"
#include "room/movement_tuning.hpp"
#include "room/operator_components.hpp"
#include "room_harness.hpp"

namespace KeyCode = engine::input::KeyCode;
using engine::spatial::Transform;
using signal_room::MovementTuning;
using signal_room::Operator;
using signal_room::OperatorMotion;
using signal_room::test::RoomHarness;

namespace {

constexpr std::string_view kShift1 = "shifts/shift_01.shift.json";

flecs::entity FindOperator(const RoomHarness& room) {
	flecs::entity found;
	room.World().each([&](const flecs::entity e, const Operator&) { found = e; });
	return found;
}

TEST(OperatorMovement, Shift1SpawnsOneOperatorAtSpawn1InP1Colour) {
	RoomHarness room;
	const auto loaded = room.LoadShift(kShift1);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();

	int operators = 0;
	room.World().each([&](const Operator&) { ++operators; });
	EXPECT_EQ(operators, 1);

	const auto op = FindOperator(room);
	ASSERT_TRUE(op);
	EXPECT_EQ(op.get<Operator>().colour, "p1");
	// spawn_1 is cell (1,5): its centre on the 1 m grid at the origin.
	const auto& position = op.get<Transform>().position;
	EXPECT_FLOAT_EQ(position.x, 1.5F);
	EXPECT_FLOAT_EQ(position.z, 5.5F);
}

// The Operator's floor position: x east, y south (the Transform's z).
glm::vec2 FloorPosition(const RoomHarness& room) {
	const auto& position = FindOperator(room).get<Transform>().position;
	return {position.x, position.z};
}

// Hold `keys` for `seconds` of 60 Hz frames, then release them.
void HoldFor(RoomHarness& room, const std::initializer_list<int> keys, const float seconds) {
	for (const int key : keys) {
		room.Hold(key);
	}
	room.Step(static_cast<int>(std::lround(seconds / signal_room::test::kFixedDelta)));
	for (const int key : keys) {
		room.Release(key);
	}
}

TEST(OperatorMovement, OneSecondWalkCoversAboutWalkSpeed) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	const auto start = FloorPosition(room);

	HoldFor(room, {KeyCode::D}, 1.0F);

	// 4.5 m/s, less what the ~0.08 s ramp up loses (about 0.18 m).
	const auto moved = FloorPosition(room) - start;
	EXPECT_NEAR(moved.x, 4.5F, 0.3F);
	EXPECT_LE(moved.x, 4.5F);
	EXPECT_NEAR(moved.y, 0.0F, 1e-4F);
}

TEST(OperatorMovement, ArrowKeysWalkLikeWasd) {
	RoomHarness wasd;
	RoomHarness arrows;
	ASSERT_TRUE(wasd.LoadShift(kShift1).has_value());
	ASSERT_TRUE(arrows.LoadShift(kShift1).has_value());

	HoldFor(wasd, {KeyCode::W}, 0.5F);
	HoldFor(arrows, {KeyCode::Up}, 0.5F);

	EXPECT_LT(FloorPosition(wasd).y, 5.0F);
	EXPECT_FLOAT_EQ(FloorPosition(arrows).y, FloorPosition(wasd).y);
}

TEST(OperatorMovement, DiagonalIsNoFasterThanStraight) {
	RoomHarness straight;
	RoomHarness diagonal;
	ASSERT_TRUE(straight.LoadShift(kShift1).has_value());
	ASSERT_TRUE(diagonal.LoadShift(kShift1).has_value());
	const auto start = FloorPosition(straight);

	HoldFor(straight, {KeyCode::W}, 0.5F);
	HoldFor(diagonal, {KeyCode::W, KeyCode::D}, 0.5F);

	const float straightDistance = glm::length(FloorPosition(straight) - start);
	const auto diagonalMove = FloorPosition(diagonal) - start;
	EXPECT_GT(diagonalMove.x, 0.5F);
	EXPECT_LT(diagonalMove.y, -0.5F);
	EXPECT_LE(glm::length(diagonalMove), straightDistance + 1e-4F);
}

TEST(OperatorMovement, AQuarterTurnTakesAboutTwoFrames) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	constexpr float kEast = -std::numbers::pi_v<float> / 2.0F;

	// From spawn, facing north (yaw 0). A 180-degree turn takes 0.06 s, so a 90-degree one takes 0.03 s:
	// about 1.8 frames at 60 Hz.
	room.Hold(KeyCode::D);
	room.Step();
	const float afterOne = FindOperator(room).get<OperatorMotion>().facing;
	EXPECT_GT(afterOne, kEast);
	EXPECT_LT(afterOne, 0.0F);

	room.Step();
	EXPECT_NEAR(FindOperator(room).get<OperatorMotion>().facing, kEast, 1e-4F);
}

TEST(OperatorMovement, LiveTuningAndCarrySpeedScaleTheWalk) {
	RoomHarness tuned;
	RoomHarness carrying;
	ASSERT_TRUE(tuned.LoadShift(kShift1).has_value());
	ASSERT_TRUE(carrying.LoadShift(kShift1).has_value());
	const auto start = FloorPosition(tuned);

	// As the Room panel's slider would, mid-Shift.
	tuned.World().get_mut<MovementTuning>().walkSpeed = 2.25F;
	FindOperator(carrying).get_mut<Operator>().carrySpeed = 0.5F;
	HoldFor(tuned, {KeyCode::D}, 1.0F);
	HoldFor(carrying, {KeyCode::D}, 1.0F);

	// Half of 4.5 m/s, and the 0.08 s ramp up still takes 0.08 s, losing half of it: 2.25 * (1 - 0.04).
	EXPECT_NEAR(FloorPosition(tuned).x - start.x, 2.16F, 0.02F);
	EXPECT_NEAR(FloorPosition(carrying).x - start.x, 2.16F, 0.02F);
}

// Walk east along row 5 from spawn_1 for 1 s, dashing at each of `dashFrames` (frames from the start).
float WalkEastDashingAt(const std::initializer_list<int> dashFrames) {
	RoomHarness room;
	EXPECT_TRUE(room.LoadShift(kShift1).has_value());
	room.Hold(KeyCode::D);
	for (int frame = 0; frame < 60; ++frame) {
		if (std::ranges::find(dashFrames, frame) != dashFrames.end()) {
			room.Tap(KeyCode::LeftShift);
		}
		room.Step();
	}
	return FloorPosition(room).x;
}

TEST(OperatorMovement, DashCoversMoreGroundThanWalking) {
	const float walked = WalkEastDashingAt({});
	const float dashed = WalkEastDashingAt({15});

	// 0.2 s at 2.2x walk adds about 1.1 m.
	EXPECT_GT(dashed - walked, 0.8F);
}

TEST(OperatorMovement, SecondDashInsideTheCooldownDoesNothing) {
	const float once = WalkEastDashingAt({15});
	// 24 frames (0.4 s) later the first dash has ended but its 0.5 s cooldown is still running.
	const float twice = WalkEastDashingAt({15, 39});

	EXPECT_FLOAT_EQ(twice, once);
}

TEST(OperatorMovement, DashAfterTheCooldownWorksAgain) {
	const float once = WalkEastDashingAt({0});
	// 0.2 s dash + 0.5 s cooldown = 42 frames.
	const float twice = WalkEastDashingAt({0, 45});

	EXPECT_GT(twice - once, 0.8F);
}

TEST(OperatorMovement, DashWithNoInputGoesTheWayTheOperatorFaces) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	const auto start = FloorPosition(room);

	// Operators spawn facing north.
	room.Tap(KeyCode::LeftShift);
	room.Step(30);

	const auto moved = FloorPosition(room) - start;
	EXPECT_LT(moved.y, -1.5F);
	EXPECT_NEAR(moved.x, 0.0F, 1e-4F);
}

void PlaceOperator(const RoomHarness& room, const glm::vec2 position) {
	FindOperator(room).get_mut<Transform>().position = {position.x, 0.0F, position.y};
}

// Hold `keys` for `seconds`, calling `check` with the Operator's position after every frame.
template<typename Check>
void HoldAndCheckEachFrame(RoomHarness& room, const std::initializer_list<int> keys, const float seconds, Check check) {
	for (const int key : keys) {
		room.Hold(key);
	}
	const int frames = static_cast<int>(std::lround(seconds / signal_room::test::kFixedDelta));
	for (int frame = 0; frame < frames; ++frame) {
		room.Step();
		check(FloorPosition(room));
	}
	for (const int key : keys) {
		room.Release(key);
	}
}

constexpr float kRadius = 0.3F;
constexpr float kSlack = 1e-3F;

TEST(OperatorMovement, WalkingAndDashingIntoTheDecoderNeverEntersItsFootprint) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	// West of the Decoder, whose 2x2 footprint covers x 6-8, z 3-5.
	PlaceOperator(room, {4.0F, 4.0F});
	room.Tap(KeyCode::LeftShift);

	HoldAndCheckEachFrame(room, {KeyCode::D}, 2.0F, [](const glm::vec2 p) { ASSERT_LE(p.x + kRadius, 6.0F + kSlack); });

	EXPECT_NEAR(FloorPosition(room).x, 6.0F - kRadius, 0.01F);
	EXPECT_NEAR(FloorPosition(room).y, 4.0F, 1e-4F);
}

TEST(OperatorMovement, AnAngledApproachSlidesAlongTheDecoder) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	PlaceOperator(room, {6.0F - kRadius, 3.3F});

	// Down-right into the Decoder's west face: blocked east, free south.
	HoldAndCheckEachFrame(room, {KeyCode::D, KeyCode::S}, 0.4F, [](const glm::vec2 p) {
		ASSERT_LE(p.x + kRadius, 6.0F + kSlack);
	});

	EXPECT_NEAR(FloorPosition(room).x, 6.0F - kRadius, 0.01F);
	EXPECT_GT(FloorPosition(room).y, 4.0F);
}

TEST(OperatorMovement, WalkingIntoATarpedStationNeverEntersItsFootprint) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	// South of the Tarped Keypunch at cell (3,2), which covers z 2-3.
	PlaceOperator(room, {3.5F, 4.5F});

	HoldAndCheckEachFrame(room, {KeyCode::W}, 1.0F, [](const glm::vec2 p) { ASSERT_GE(p.y - kRadius, 3.0F - kSlack); });

	EXPECT_NEAR(FloorPosition(room).y, 3.0F + kRadius, 0.01F);
}

TEST(OperatorMovement, TheOperatorCannotCrossTheOpenSouthEdge) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// From spawn_1 (1,5) straight south across the desk-edge row to the floor's edge at z 9.
	HoldAndCheckEachFrame(room, {KeyCode::S}, 2.0F, [](const glm::vec2 p) { ASSERT_LE(p.y + kRadius, 9.0F + kSlack); });

	EXPECT_NEAR(FloorPosition(room).y, 9.0F - kRadius, 0.01F);
}

TEST(OperatorMovement, TheStormWindowCellsAreWalkable) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	// Between Radio2 (4-5,0) and Radio3 (8-9,0), on the line between window cells (6,0) and (7,0).
	PlaceOperator(room, {7.0F, 2.5F});

	HoldFor(room, {KeyCode::W}, 1.0F);

	// Up to the north wall at z 0.
	EXPECT_NEAR(FloorPosition(room).y, kRadius, 0.01F);
	EXPECT_NEAR(FloorPosition(room).x, 7.0F, 1e-4F);
}

TEST(OperatorMovement, RosterNamingAMissingSpawnMarkerFailsWithThatName) {
	RoomHarness room;

	const auto loaded = room.LoadShift(SIGNAL_ROOM_FIXTURE_DIR "/missing_spawn.shift.json");

	ASSERT_FALSE(loaded.has_value());
	EXPECT_THAT(loaded.error(), testing::HasSubstr("spawn_9"));
	EXPECT_FALSE(FindOperator(room));
}

} // namespace
