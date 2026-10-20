#include <string>
#include <string_view>

#include <glm/glm.hpp>
#include <gtest/gtest.h>

#include "engine/input/input.hpp"
#include "engine/particles/particle_components.hpp"
#include "engine/spatial/spatial_components.hpp"
#include "room/item_components.hpp"
#include "room/operator_components.hpp"
#include "room/palette.hpp"
#include "room/room_components.hpp"
#include "room/station_components.hpp"
#include "room/ticket_simulation.hpp"
#include "room_harness.hpp"

namespace KeyCode = engine::input::KeyCode;
using engine::spatial::Transform;
using signal_room::Job;
using signal_room::JobPhase;
using signal_room::Operator;
using signal_room::OperatorMotion;
using signal_room::PartColour;
using signal_room::RoomOptions;
using signal_room::ReadState;
using signal_room::StationRead;
using signal_room::test::RoomHarness;

namespace {

constexpr float kNorth = 0.0F;
constexpr float kSouth = 3.1415927F;
constexpr glm::vec2 kBesideCounterSouthLeft{6.5F, 5.4F};
// The active Decoder (cell row 3) is worked from the north; Decoder2 below it is tarped.
constexpr glm::vec2 kNorthOfDecoder{6.5F, 2.6F};
constexpr glm::vec2 kSouthOfRadio1{1.6F, 1.4F};

std::string Fixture(const std::string_view name) {
	return std::string{SIGNAL_ROOM_FIXTURE_DIR} + "/" + std::string{name};
}

void Load(RoomHarness& room, const std::string_view fixture) {
	const auto loaded = room.LoadShift(Fixture(fixture));
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
}

flecs::entity Placement(const RoomHarness& room, const std::string_view name) {
	return room.World().lookup((std::string{"RoomRoot::"} + std::string{name}).c_str());
}

void StandAt(RoomHarness& room, const glm::vec2 position, const float yaw) {
	flecs::entity op;
	room.World().each([&](const flecs::entity e, const Operator&) { op = e; });
	op.get_mut<Transform>().position = {position.x, 0.0F, position.y};
	op.get_mut<OperatorMotion>().facing = yaw;
	room.Step();
}

void Interact(RoomHarness& room) {
	room.Tap(KeyCode::Space);
	room.Step(2);
}

const StationRead& ReadOf(const RoomHarness& room, const std::string_view station) {
	return Placement(room, station).get<StationRead>();
}

engine::core::Rgba Token(const RoomHarness& room, const char* token) {
	return *room.World().get<signal_room::Palette>().Get(token);
}

engine::core::Rgba LampColour(const RoomHarness& room, const std::string_view station) {
	return Placement(room, station).lookup("lamp").get<PartColour>().colour;
}

bool Same(const engine::core::Rgba a, const engine::core::Rgba b) {
	return a.r == b.r && a.g == b.g && a.b == b.b;
}

// Whether the lamp shows `token` on some frame within `seconds`.
bool LampShows(RoomHarness& room, const std::string_view station, const char* token, const float seconds) {
	const auto want = Token(room, token);
	for (int frame = 0; frame < static_cast<int>(seconds * 60.0F); ++frame) {
		room.Step();
		if (Same(LampColour(room, station), want)) {
			return true;
		}
	}
	return false;
}

void StartDecoding(RoomHarness& room) {
	StandAt(room, kBesideCounterSouthLeft, kSouth);
	Interact(room);
	StandAt(room, kNorthOfDecoder, kSouth);
	Interact(room);
	ASSERT_EQ(Placement(room, "Decoder").get<Job>().phase, JobPhase::Running);
}

void PutOnAir(RoomHarness& room, const std::string& frequency) {
	signal_room::SpawnTicket(room.World(), room.World().lookup("RoomRoot"), "normal", frequency, 0.0F, true, true);
	room.Step();
}

TEST(StationReads, TheDecoderLampIsOffIdleBlinksAmberRunningAndGlowsGreenWithOutputWaiting) {
	RoomHarness room;
	Load(room, "decode_route.shift.json");
	room.Step(3);
	EXPECT_TRUE(Same(LampColour(room, "Decoder"), Token(room, "lamp_off")));

	StartDecoding(room);
	EXPECT_TRUE(LampShows(room, "Decoder", "lamp_amber", 1.0F));
	EXPECT_TRUE(LampShows(room, "Decoder", "lamp_off", 1.0F));

	room.StepSeconds(6.5F);
	ASSERT_EQ(Placement(room, "Decoder").get<Job>().phase, JobPhase::Done);
	EXPECT_TRUE(Same(LampColour(room, "Decoder"), Token(room, "lamp_green")));
	room.StepSeconds(0.5F);
	EXPECT_TRUE(Same(LampColour(room, "Decoder"), Token(room, "lamp_green")));
}

TEST(StationReads, TheRiserBarFillTracksJobProgress) {
	RoomHarness room;
	Load(room, "decode_route.shift.json");
	StartDecoding(room);

	room.StepSeconds(3.0F);

	const auto bar = Placement(room, "Decoder").lookup("riser_bar");
	// Half of the 6 s Job, less the frames spent placing the card.
	EXPECT_NEAR(bar.get<Transform>().scale.x, 0.5F, 0.07F);
	room.StepSeconds(1.5F);
	EXPECT_NEAR(bar.get<Transform>().scale.x, 0.75F, 0.07F);
}

TEST(StationReads, ARadioIsOnAirWithATicketCoupledWhileCopyingResettingBrieflyAfterAndDarkOtherwise) {
	RoomHarness room;
	Load(room, "two_radios.shift.json");
	room.Step(3);
	EXPECT_EQ(ReadOf(room, "Radio1").state, ReadState::Dark);

	PutOnAir(room, "7.0");
	EXPECT_EQ(ReadOf(room, "Radio1").state, ReadState::OnAir);
	EXPECT_EQ(ReadOf(room, "Radio2").state, ReadState::Dark);

	StandAt(room, kSouthOfRadio1, kNorth);
	room.Hold(KeyCode::Space);
	room.StepSeconds(1.0F);
	EXPECT_EQ(ReadOf(room, "Radio1").state, ReadState::Coupled);
	EXPECT_TRUE(Same(LampColour(room, "Radio1"), Token(room, "freq_7_0")));

	room.Release(KeyCode::Space);
	room.StepSeconds(0.2F);
	EXPECT_EQ(ReadOf(room, "Radio1").state, ReadState::Resetting);
	room.StepSeconds(0.4F);
	EXPECT_EQ(ReadOf(room, "Radio1").state, ReadState::OnAir);
}

TEST(StationReads, TheDecoderReelsSpinFasterWhileCranked) {
	RoomHarness room;
	Load(room, "decode_route.shift.json");
	StartDecoding(room);

	const float before = ReadOf(room, "Decoder").reelAngle;
	room.StepSeconds(0.5F);
	const float uncranked = ReadOf(room, "Decoder").reelAngle - before;

	room.Hold(KeyCode::Space);
	room.Step(2);
	const float mid = ReadOf(room, "Decoder").reelAngle;
	room.StepSeconds(0.5F);
	const float cranked = ReadOf(room, "Decoder").reelAngle - mid;
	room.Release(KeyCode::Space);

	EXPECT_GT(uncranked, 0.0F);
	EXPECT_GT(cranked, uncranked * 2.0F);
	EXPECT_GT(ReadOf(room, "Decoder").crankAngle, 0.0F);
}

TEST(StationReads, ATransmissionBurstsConfettiUnderTheTelexAndFlashesItsLampGreen) {
	RoomHarness room;
	Load(room, "decode_route.shift.json");
	const auto telex = Placement(room, "Telex");
	EXPECT_EQ(room.World().count<engine::particles::Particle>(), 0);

	signal_room::RaiseTransmission(room.World(), "7.0", telex.id());
	room.Step(2);

	int confetti = 0;
	room.World().each([&](const flecs::entity e, const engine::particles::Particle&) {
		EXPECT_EQ(e.parent().parent(), telex) << e.path();
		++confetti;
	});
	EXPECT_GT(confetti, 0);
	EXPECT_TRUE(Same(LampColour(room, "Telex"), Token(room, "lamp_green")));
	EXPECT_FALSE(telex.lookup("paper").has(flecs::Disabled));
	room.StepSeconds(0.7F);
	EXPECT_TRUE(telex.lookup("paper").has(flecs::Disabled));
}

flecs::entity Carried(const RoomHarness& room) {
	flecs::entity item;
	room.World().each([&](const flecs::entity op, const Operator&) { item = op.target<signal_room::Carries>(); });
	return item;
}

void CopyAtRadio1(RoomHarness& room) {
	PutOnAir(room, "7.0");
	StandAt(room, kSouthOfRadio1, kNorth);
	room.Hold(KeyCode::Space);
	room.StepSeconds(2.3F);
	room.Release(KeyCode::Space);
	room.Step(2);
}

TEST(StationReads, WithAutoGrabAFinishedHoldJobLandsInTheOperatorsHands) {
	RoomHarness room;
	Load(room, "two_radios.shift.json");
	room.World().get_mut<RoomOptions>().autoGrab = true;

	CopyAtRadio1(room);

	ASSERT_TRUE(Carried(room));
	EXPECT_EQ(Carried(room).get<signal_room::Item>().id, "raw_card");
	EXPECT_EQ(Placement(room, "Radio1").get<Job>().phase, JobPhase::Idle);
}

TEST(StationReads, WithoutAutoGrabAFinishedHoldJobWaitsOnTheStation) {
	RoomHarness room;
	Load(room, "two_radios.shift.json");

	CopyAtRadio1(room);

	EXPECT_FALSE(Carried(room));
	EXPECT_EQ(Placement(room, "Radio1").get<Job>().phase, JobPhase::Done);
}

TEST(StationReads, AutoGrabNeverTakesFromAnUnattendedWalkAwayJob) {
	RoomHarness room;
	Load(room, "decode_route.shift.json");
	room.World().get_mut<RoomOptions>().autoGrab = true;
	StartDecoding(room);

	room.StepSeconds(6.5F);

	EXPECT_EQ(Placement(room, "Decoder").get<Job>().phase, JobPhase::Done);
	EXPECT_FALSE(Carried(room));
}

TEST(StationReads, TheDecoderCardSlidesFromTheWestSideToTheEastAsItProgresses) {
	RoomHarness room;
	Load(room, "decode_route.shift.json");
	StartDecoding(room);
	const auto decoder = Placement(room, "Decoder");
	const auto cardX = [&] { return decoder.target<signal_room::Holds>().get<Transform>().position.x - decoder.get<Transform>().position.x; };

	EXPECT_NEAR(cardX(), -0.85F, 0.1F);
	room.StepSeconds(3.0F);
	EXPECT_NEAR(cardX(), 0.0F, 0.2F);
	room.StepSeconds(3.5F);
	EXPECT_NEAR(cardX(), 0.85F, 0.01F);
}

TEST(StationReads, TheLockedDecoderIsATarpBoxStandingOnTheSharedTable) {
	RoomHarness room;
	Load(room, "decode_route.shift.json");
	const auto locked = Placement(room, "Decoder2");

	const auto tarp = locked.lookup("tarp");
	ASSERT_TRUE(tarp);
	EXPECT_NEAR(tarp.get<Transform>().position.y, 0.62F, 0.001F);
	EXPECT_FALSE(locked.lookup("lamp"));
	// The table top and legs stay: more than just the tarp's own parts.
	int parts = 0;
	locked.children([&](const flecs::entity) { ++parts; });
	EXPECT_GE(parts, 6);
	EXPECT_FALSE(Placement(room, "Decoder").lookup("tarp"));
}

} // namespace
