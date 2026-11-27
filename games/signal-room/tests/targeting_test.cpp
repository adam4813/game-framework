#include <map>
#include <numbers>
#include <string_view>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "engine/core/core_types.hpp"
#include "engine/render/render_components.hpp"
#include "engine/spatial/spatial_components.hpp"
#include "room/operator_components.hpp"
#include "room/room_tuning.hpp"
#include "room_harness.hpp"

using engine::core::Rgba;
using engine::render::Material;
using engine::spatial::Transform;
using signal_room::Operator;
using signal_room::OperatorMotion;
using signal_room::TargetingTuning;
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

// Stand the Operator at a floor position (x east, y south) facing `yaw`, and let one frame run.
void StandAt(RoomHarness& room, const glm::vec2 position, const float yaw) {
	const auto op = FindOperator(room);
	op.get_mut<Transform>().position = {position.x, 0.0F, position.y};
	op.get_mut<OperatorMotion>().facing = yaw;
	room.Step();
}

// The Operator's Target's name, or "" for no Target.
std::string_view TargetName(const RoomHarness& room) {
	const auto target = FindOperator(room).target<Targets>();
	return target ? std::string_view{target.name().c_str()} : std::string_view{};
}

TEST(Targeting, FacingACounterPointBlankTargetsIt) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// South of CounterSouthLeft (6,6), facing it.
	StandAt(room, {6.5F, 7.5F}, kNorth);

	EXPECT_EQ(TargetName(room), "CounterSouthLeft");
}

TEST(Targeting, FacingAnInServiceStationFromAWorkedSideTargetsIt) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// West of the Decoder (6-7, 3-4), worked from all four sides.
	StandAt(room, {5.5F, 3.5F}, kEast);

	EXPECT_EQ(TargetName(room), "Decoder");
}

TEST(Targeting, ATarpedKeypunchFacedPointBlankIsNeverTheTarget) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// South of the Tarped Keypunch (3,2), which would be worked from every side in service.
	StandAt(room, {3.5F, 3.5F}, kNorth);

	EXPECT_EQ(TargetName(room), "");
}

TEST(Targeting, FacingTheTelexFromItsUnworkedSouthSideGivesNoTarget) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(SIGNAL_ROOM_FIXTURE_DIR "/telex_open_south.shift.json").has_value());

	// South of the Telex (2,1), worked only from W, N and E.
	StandAt(room, {2.5F, 2.5F}, kNorth);

	EXPECT_EQ(TargetName(room), "");
}

TEST(Targeting, WithNothingInReachThereIsNoTarget) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// spawn_1 (1,5) facing north: open floor for more than 1.2 m all round the front.
	StandAt(room, {1.5F, 5.5F}, kNorth);

	EXPECT_EQ(TargetName(room), "");
}

TEST(Targeting, ASlightlyOffAimWithinTheConeTargetsTheNearestThing) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// South-west of CounterSouthLeft (6,6), aimed 20 degrees east of north: the ray lands on the open
	// cell (5,6), and the Counter's nearest corner is 0.71 m away, 25 degrees off the aim.
	StandAt(room, {5.5F, 7.5F}, -20.0F * kDegrees);

	EXPECT_EQ(TargetName(room), "CounterSouthLeft");
}

TEST(Targeting, ANarrowerConeFromLiveTuningMissesTheSameAim) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// As the Room panel's slider would: the Counter is 25 degrees off the aim, outside a 20 degree cone.
	room.World().get_mut<TargetingTuning>().coneHalfAngle = 20.0F;
	StandAt(room, {5.5F, 7.5F}, -20.0F * kDegrees);

	EXPECT_EQ(TargetName(room), "");
}

TEST(Targeting, ThingsOutsideTheConeAreNotTargeted) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());

	// The same spot facing west: the Counter is 135 degrees off the aim.
	StandAt(room, {5.5F, 7.5F}, 90.0F * kDegrees);

	EXPECT_EQ(TargetName(room), "");
}

TEST(Targeting, TheTargetFollowsTheOperatorAndClearsWhenItWalksAway) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	StandAt(room, {6.5F, 7.5F}, kNorth);
	ASSERT_EQ(TargetName(room), "CounterSouthLeft");

	StandAt(room, {7.5F, 7.5F}, kNorth);
	EXPECT_EQ(TargetName(room), "CounterSouthRight");

	StandAt(room, {7.5F, 7.5F}, 180.0F * kDegrees);
	EXPECT_EQ(TargetName(room), "");
}

// Every drawn colour in the Room, by entity.
std::map<flecs::entity_t, Rgba> DrawnColours(const RoomHarness& room) {
	std::map<flecs::entity_t, Rgba> colours;
	room.World().each([&](const flecs::entity e, const Material& material) { colours[e.id()] = material.color; });
	return colours;
}

bool IsPartOf(flecs::entity e, const flecs::entity owner) {
	for (; e; e = e.parent()) {
		if (e == owner) {
			return true;
		}
	}
	return false;
}

TEST(Targeting, OnlyTheTargetBrightensTowardTheOperatorColourAndRestoresAfter) {
	RoomHarness room;
	ASSERT_TRUE(room.LoadShift(kShift1).has_value());
	room.Step();
	ASSERT_EQ(TargetName(room), "");
	const auto before = DrawnColours(room);

	StandAt(room, {6.5F, 7.5F}, kNorth);
	room.Step(10);
	ASSERT_EQ(TargetName(room), "CounterSouthLeft");
	const auto counter = room.World().lookup("RoomRoot::CounterSouthLeft");
	ASSERT_TRUE(counter);

	int brightened = 0;
	for (const auto& [id, colour] : DrawnColours(room)) {
		const auto& was = before.at(id);
		if (colour.r == was.r && colour.g == was.g && colour.b == was.b) {
			continue;
		}
		EXPECT_TRUE(IsPartOf(room.World().entity(id), counter)) << room.World().entity(id).path();
		// The wood (123, 94, 68) and top (161, 132, 106) grow brighter and redder: toward a lightened
		// P1 brick red (178, 58, 44).
		EXPECT_GT(colour.r + colour.g + colour.b, was.r + was.g + was.b);
		EXPECT_GT(colour.r - colour.g, was.r - was.g);
		++brightened;
	}
	EXPECT_GT(brightened, 0);

	StandAt(room, {6.5F, 7.5F}, 180.0F * kDegrees);
	room.Step();
	ASSERT_EQ(TargetName(room), "");
	for (const auto& [id, colour] : DrawnColours(room)) {
		const auto& was = before.at(id);
		EXPECT_TRUE(colour.r == was.r && colour.g == was.g && colour.b == was.b)
			<< room.World().entity(id).path()
			<< " kept its brighten";
	}
}

} // namespace
