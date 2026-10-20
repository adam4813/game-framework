#include <string>
#include <string_view>
#include <vector>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "engine/input/input.hpp"
#include "engine/spatial/spatial_components.hpp"
#include "room/guidance.hpp"
#include "room/item_components.hpp"
#include "room/operator_components.hpp"
#include "room/room_tuning.hpp"
#include "room_harness.hpp"

namespace KeyCode = engine::input::KeyCode;
using engine::spatial::Transform;
using signal_room::Bubble;
using signal_room::Carries;
using signal_room::Chevron;
using signal_room::Operator;
using signal_room::OperatorMotion;
using signal_room::test::RoomHarness;

namespace {

constexpr float kSouth = 3.1415927F;
constexpr float kEast = -1.5707964F;
constexpr glm::vec2 kBesideCounterSouthLeft{6.5F, 5.4F};
constexpr glm::vec2 kNorthOfDecoder{6.5F, 2.6F};
constexpr glm::vec2 kWestOfTelex{9.6F, 3.5F};

void Load(RoomHarness& room) {
	const auto loaded = room.LoadShift(std::string{SIGNAL_ROOM_FIXTURE_DIR} + "/decode_route.shift.json");
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

void StandAt(RoomHarness& room, const glm::vec2 position, const float yaw = kSouth) {
	const auto op = FindOperator(room);
	op.get_mut<Transform>().position = {position.x, 0.0F, position.y};
	op.get_mut<OperatorMotion>().facing = yaw;
	room.Step();
}

void Interact(RoomHarness& room) {
	room.Tap(KeyCode::Space);
	room.Step(2);
}

// The stations the Chevrons float over, one entry per Chevron.
std::vector<flecs::entity_t> ChevronStations(const RoomHarness& room) {
	std::vector<flecs::entity_t> stations;
	room.World().each([&](const Chevron& chevron) { stations.push_back(chevron.station); });
	return stations;
}

const Bubble& BubbleOf(const RoomHarness& room) {
	const Bubble* found = nullptr;
	room.World().each([&](const Bubble& bubble) { found = &bubble; });
	return *found;
}

TEST(Guidance, CarryingARawCardPutsAChevronOverTheDecoderOnlyAndNoneOnceItIsBusy) {
	RoomHarness room;
	Load(room);
	room.Step(2);
	EXPECT_TRUE(ChevronStations(room).empty());

	StandAt(room, kBesideCounterSouthLeft);
	Interact(room);
	EXPECT_EQ(ChevronStations(room), std::vector{Placement(room, "Decoder").id()});

	StandAt(room, kNorthOfDecoder);
	Interact(room);
	room.Step(2);
	EXPECT_TRUE(ChevronStations(room).empty());
}

TEST(Guidance, CarryingADecodedCardPutsAChevronOverTheTelex) {
	RoomHarness room;
	Load(room);
	StandAt(room, kBesideCounterSouthLeft);
	Interact(room);
	StandAt(room, kNorthOfDecoder);
	Interact(room);
	room.StepSeconds(7.0F);
	Interact(room);
	ASSERT_EQ(FindOperator(room).target<Carries>().get<signal_room::Item>().id, "decoded_card");
	room.Step(2);
	EXPECT_EQ(ChevronStations(room), std::vector{Placement(room, "Telex").id()});
}

TEST(Guidance, ARefusedPressShowsTheCrossUntilItsDurationRunsOut) {
	RoomHarness room;
	Load(room);
	StandAt(room, kBesideCounterSouthLeft);
	Interact(room);
	EXPECT_EQ(BubbleOf(room).timeLeft, 0.0F) << "a successful press shows no cross";

	// A Raw Card is the wrong Item for the Telex.
	StandAt(room, kWestOfTelex, kEast);
	Interact(room);
	EXPECT_GT(BubbleOf(room).timeLeft, 0.0F);
	EXPECT_EQ(BubbleOf(room).glyph, "refuse");

	room.StepSeconds(room.World().get<signal_room::GuidanceTuning>().bubbleDuration + 0.2F);
	EXPECT_EQ(BubbleOf(room).timeLeft, 0.0F);
}

} // namespace
