#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "engine/engine.hpp"
#include "engine/input/input.hpp"
#include "engine/spatial/spatial_components.hpp"
#include "room/operator_components.hpp"
#include "room/score_components.hpp"
#include "room/shift_flow.hpp"
#include "room/ticket_components.hpp"
#include "room_harness.hpp"

namespace KeyCode = engine::input::KeyCode;
using signal_room::FlowLine;
using signal_room::FlowUi;
using signal_room::Score;
using signal_room::ShiftPhase;
using signal_room::test::RoomHarness;

namespace {

constexpr std::string_view kShift1 = "shifts/shift_01.shift.json";

// A Room at its Briefing, as a player sees it after loading Shift 1.
void LoadAtBriefing(RoomHarness& room) {
	room.KeepBriefing();
	const auto loaded = room.LoadShift(kShift1);
	ASSERT_TRUE(loaded.has_value()) << loaded.error();
	room.Step();
}

// The text of the Briefing or results line `key`.
std::string Line(const RoomHarness& room, const std::string& key) {
	std::string text;
	room.World().query<const FlowLine, const engine::ui::Label>().each([&](const FlowLine& line, const engine::ui::Label& label) {
		if (line.key == key) {
			text = label.text;
		}
	});
	return text;
}

glm::vec3 OperatorPosition(const RoomHarness& room) {
	glm::vec3 position{};
	room.World().query<const signal_room::Operator, const engine::spatial::Transform>().each(
		[&](const signal_room::Operator&, const engine::spatial::Transform& transform) { position = transform.position; }
	);
	return position;
}

void PressInteract(RoomHarness& room) {
	room.Tap(KeyCode::Space);
	room.Step(2);
}

TEST(ShiftFlow, Shift1LoadsIntoBriefingAndTheRoomHolds) {
	RoomHarness room;
	LoadAtBriefing(room);
	ASSERT_EQ(room.Window().phase, ShiftPhase::Briefing);
	EXPECT_TRUE(room.World().get<FlowUi>().briefing.get<engine::ui::Modal>().open);
	EXPECT_FALSE(room.World().get<FlowUi>().results.get<engine::ui::Modal>().open);

	const auto before = OperatorPosition(room);
	room.Hold(KeyCode::D);
	room.StepSeconds(1.0F);
	room.Release(KeyCode::D);
	EXPECT_EQ(OperatorPosition(room), before);
	EXPECT_FLOAT_EQ(room.Window().windowLeft, room.Window().window);

	PressInteract(room);
	EXPECT_EQ(room.Window().phase, ShiftPhase::Running);
	EXPECT_FALSE(room.World().get<FlowUi>().briefing.get<engine::ui::Modal>().open);

	room.Hold(KeyCode::D);
	room.StepSeconds(0.5F);
	room.Release(KeyCode::D);
	EXPECT_NE(OperatorPosition(room), before);
	EXPECT_LT(room.Window().windowLeft, room.Window().window);
}

TEST(ShiftFlow, TheBriefingShowsTheRouteTheStationsMake) {
	RoomHarness room;
	LoadAtBriefing(room);
	EXPECT_NE(Line(room, "route").find("Radio > Decoder > Telex"), std::string::npos) << Line(room, "route");
}

TEST(ShiftFlow, TheHardStopShowsTheResultsWithTheScoreParAndStars) {
	RoomHarness room;
	LoadAtBriefing(room);
	PressInteract(room);
	room.World().set<Score>({.score = 13, .par = 20});
	room.World().get_mut<signal_room::ShiftWindow>().windowLeft = 0.01F;
	room.StepSeconds(0.5F);

	ASSERT_EQ(room.Window().phase, ShiftPhase::Ended);
	EXPECT_TRUE(room.World().get<FlowUi>().results.get<engine::ui::Modal>().open);
	EXPECT_EQ(Line(room, "score"), "Score: 13");
	EXPECT_EQ(Line(room, "par"), "Par: 20");
	EXPECT_EQ(Line(room, "stars"), "Stars: **-");
}

TEST(ShiftFlow, RetryStartsAFreshAttemptAtTheBriefing) {
	RoomHarness room;
	LoadAtBriefing(room);
	PressInteract(room);
	room.World().set<Score>({.score = 13, .par = 20});
	room.World().get_mut<signal_room::ShiftWindow>().windowLeft = 0.01F;
	room.StepSeconds(1.0F);
	ASSERT_EQ(room.Window().phase, ShiftPhase::Ended);

	PressInteract(room);
	EXPECT_EQ(room.Window().phase, ShiftPhase::Briefing);
	EXPECT_EQ(room.World().get<Score>().score, 0);
	EXPECT_TRUE(room.OpenTickets().empty());
	EXPECT_FLOAT_EQ(room.Window().windowLeft, room.Window().window);
}

TEST(ShiftFlow, ReloadReturnsToTheBriefing) {
	RoomHarness room;
	LoadAtBriefing(room);
	PressInteract(room);
	ASSERT_EQ(room.Window().phase, ShiftPhase::Running);

	const auto reloaded = room.Reload();
	ASSERT_TRUE(reloaded.has_value()) << reloaded.error();
	room.Step();
	EXPECT_EQ(room.Window().phase, ShiftPhase::Briefing);
	EXPECT_TRUE(room.World().get<FlowUi>().briefing.get<engine::ui::Modal>().open);
}

} // namespace
