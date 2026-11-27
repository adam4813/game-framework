/// Unit tests for VillagerSchedule::SelectWaypoint (pure function, ticket 08).
///
/// Seam: SelectWaypoint(schedule, hour) → waypoint
/// Tests verify: correct entry selection by hour, midnight wrap, single entry,
/// and empty schedule edge case.
#include <gtest/gtest.h>

#include <glm/glm.hpp>

#include "farm/villager_components.hpp"
#include "farm/villager_schedule.hpp"

using namespace game::farm;

// ── Fixture ──────────────────────────────────────────────────────────────────

/// Three-entry schedule: 06:00→{2,3}, 12:00→{5,7}, 20:00→{1,0}
static VillagerSchedule MakeThreeEntrySchedule() {
	VillagerSchedule s;
	s.entries = {
		{.startHour = 6,  .waypoint = {2, 3}},
		{.startHour = 12, .waypoint = {5, 7}},
		{.startHour = 20, .waypoint = {1, 0}},
	};
	return s;
}

// ── Empty schedule ────────────────────────────────────────────────────────────

TEST(VillagerScheduleTest, EmptySchedule_ReturnsZeroOrigin) {
	const VillagerSchedule s;
	const auto wp = SelectWaypoint(s, 10);
	EXPECT_EQ(wp, glm::ivec2(0, 0));
}

// ── Single-entry schedule ─────────────────────────────────────────────────────

TEST(VillagerScheduleTest, SingleEntry_AnyHour_ReturnsOnlyEntry) {
	VillagerSchedule s;
	s.entries = {{.startHour = 8, .waypoint = {4, 4}}};

	EXPECT_EQ(SelectWaypoint(s, 8),  glm::ivec2(4, 4));
	EXPECT_EQ(SelectWaypoint(s, 12), glm::ivec2(4, 4));
	EXPECT_EQ(SelectWaypoint(s, 3),  glm::ivec2(4, 4)) << "Before single entry → still that entry";
}

// ── Three-entry schedule ──────────────────────────────────────────────────────

TEST(VillagerScheduleTest, AtFirstEntryHour_ReturnsFirstEntry) {
	const auto s = MakeThreeEntrySchedule();
	EXPECT_EQ(SelectWaypoint(s, 6), glm::ivec2(2, 3));
}

TEST(VillagerScheduleTest, BetweenFirstAndSecond_ReturnsFirstEntry) {
	const auto s = MakeThreeEntrySchedule();
	EXPECT_EQ(SelectWaypoint(s, 9), glm::ivec2(2, 3));
	EXPECT_EQ(SelectWaypoint(s, 11), glm::ivec2(2, 3));
}

TEST(VillagerScheduleTest, AtSecondEntryHour_ReturnsSecondEntry) {
	const auto s = MakeThreeEntrySchedule();
	EXPECT_EQ(SelectWaypoint(s, 12), glm::ivec2(5, 7));
}

TEST(VillagerScheduleTest, BetweenSecondAndThird_ReturnsSecondEntry) {
	const auto s = MakeThreeEntrySchedule();
	EXPECT_EQ(SelectWaypoint(s, 15), glm::ivec2(5, 7));
	EXPECT_EQ(SelectWaypoint(s, 19), glm::ivec2(5, 7));
}

TEST(VillagerScheduleTest, AtThirdEntryHour_ReturnsThirdEntry) {
	const auto s = MakeThreeEntrySchedule();
	EXPECT_EQ(SelectWaypoint(s, 20), glm::ivec2(1, 0));
}

TEST(VillagerScheduleTest, AfterLastEntry_ReturnsLastEntry) {
	const auto s = MakeThreeEntrySchedule();
	EXPECT_EQ(SelectWaypoint(s, 22), glm::ivec2(1, 0));
	EXPECT_EQ(SelectWaypoint(s, 23), glm::ivec2(1, 0));
}

TEST(VillagerScheduleTest, BeforeFirstEntry_ReturnsLastEntry_MidnightWrap) {
	const auto s = MakeThreeEntrySchedule();
	// At 03:00 the villager is still where it was late the previous night (last entry).
	EXPECT_EQ(SelectWaypoint(s, 0),  glm::ivec2(1, 0)) << "Midnight → last entry of previous day";
	EXPECT_EQ(SelectWaypoint(s, 3),  glm::ivec2(1, 0)) << "Early morning → last entry";
	EXPECT_EQ(SelectWaypoint(s, 5),  glm::ivec2(1, 0)) << "One hour before dawn → last entry";
}
