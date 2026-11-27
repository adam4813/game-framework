#include <gtest/gtest.h>
#include <flecs.h>

#include "game_time.hpp"

class GameTimeTest : public ::testing::Test {
protected:
	flecs::world world;

	void SetUp() override {
		game::RegisterGameTime(world);
		// Use fast test thresholds: 1 tick = 1 hour, 3 days per season.
		auto& gt       = world.get_mut<game::GameTime>();
		gt.ticksPerHour  = 1;
		gt.daysPerSeason = 3;
	}

	void Progress(int n = 1) {
		for (int i = 0; i < n; ++i)
			world.progress();
	}

	[[nodiscard]] const game::GameTime& GT() const { return world.get<game::GameTime>(); }
};

// ── Tick ────────────────────────────────────────────────────────────────────────

TEST_F(GameTimeTest, tick_increments_each_frame) {
	Progress(3);
	EXPECT_EQ(GT().tick, 3);
}

// ── Hour ────────────────────────────────────────────────────────────────────────

TEST_F(GameTimeTest, hour_advances_with_ticks) {
	Progress(7);
	EXPECT_EQ(GT().hour, 7);
}

// ── Day rollover ────────────────────────────────────────────────────────────────

TEST_F(GameTimeTest, hour_resets_and_day_advances_after_24_hours) {
	Progress(24);
	EXPECT_EQ(GT().hour, 0);
	EXPECT_EQ(GT().day, 2);
	EXPECT_EQ(GT().absoluteDay, 1);
}

// ── Season rollover ─────────────────────────────────────────────────────────────

TEST_F(GameTimeTest, season_advances_after_days_per_season) {
	Progress(24 * 3); // 3 full days
	EXPECT_EQ(GT().season, game::Season::Summer);
	EXPECT_EQ(GT().day, 1);
}

TEST_F(GameTimeTest, seasons_cycle_through_all_four) {
	Progress(24 * 3 * 4); // 12 days = 1 full year
	EXPECT_EQ(GT().season, game::Season::Spring);
}

// ── Phase derivation ────────────────────────────────────────────────────────────

TEST_F(GameTimeTest, initial_phase_is_night_at_hour_zero) {
	EXPECT_EQ(GT().phase, game::DayPhase::Night);
}

TEST_F(GameTimeTest, phase_is_dawn_at_hour_5) {
	Progress(5);
	EXPECT_EQ(GT().hour, 5);
	EXPECT_EQ(GT().phase, game::DayPhase::Dawn);
}

TEST_F(GameTimeTest, phase_is_day_at_hour_8) {
	Progress(8);
	EXPECT_EQ(GT().phase, game::DayPhase::Day);
}

TEST_F(GameTimeTest, phase_is_day_at_hour_17) {
	Progress(17);
	EXPECT_EQ(GT().phase, game::DayPhase::Day);
}

TEST_F(GameTimeTest, phase_is_dusk_at_hour_18) {
	Progress(18);
	EXPECT_EQ(GT().phase, game::DayPhase::Dusk);
}

TEST_F(GameTimeTest, phase_is_night_at_hour_21) {
	Progress(21);
	EXPECT_EQ(GT().phase, game::DayPhase::Night);
}

// ── Time hooks ─────────────────────────────────────────────────────────────────

// Hook fires the first tick the clock reaches its hour, with the correct name in the event.
TEST_F(GameTimeTest, hook_fires_at_registered_hour_with_correct_name) {
	int count = 0;
	std::string capturedName;

	world.observer("HookFireObserver")
		.event<game::TimeHookFired>()
		.with<game::GameTime>()
		.run([&count, &capturedName](flecs::iter& it) {
			while (it.next()) {
				const auto* ev = it.param<game::TimeHookFired>();
				if (ev) {
					++count;
					capturedName = ev->name;
				}
			}
		});

	game::AddTimeHook(world, "dawn", 5);

	Progress(4); // hours 1-4: hook should not fire yet
	EXPECT_EQ(count, 0);

	Progress(1); // hour 5: hook fires
	EXPECT_EQ(GT().hour, 5);
	EXPECT_EQ(count, 1);
	EXPECT_EQ(capturedName, "dawn");
}

// Hook does not re-fire for the remainder of the same day.
TEST_F(GameTimeTest, hook_does_not_refire_during_same_day) {
	int count = 0;
	world.observer("HookNoRefireObserver")
		.event<game::TimeHookFired>()
		.with<game::GameTime>()
		.run([&count](flecs::iter& it) {
			while (it.next()) {
				if (it.param<game::TimeHookFired>()) ++count;
			}
		});

	game::AddTimeHook(world, "dawn", 5);

	Progress(5); // fires once at hour 5
	EXPECT_EQ(count, 1);

	Progress(18); // hours 6-23 (still absoluteDay 0)
	EXPECT_EQ(GT().hour, 23);
	EXPECT_EQ(count, 1); // no re-fire
}

// Hook re-arms on the next day and fires again at the same hour.
TEST_F(GameTimeTest, hook_refires_on_next_day) {
	int count = 0;
	world.observer("HookRefireObserver")
		.event<game::TimeHookFired>()
		.with<game::GameTime>()
		.run([&count](flecs::iter& it) {
			while (it.next()) {
				if (it.param<game::TimeHookFired>()) ++count;
			}
		});

	game::AddTimeHook(world, "dawn", 5);

	Progress(5);  // day 0, hour 5 → fires; count=1
	EXPECT_EQ(count, 1);

	// Advance 24 more ticks: tick 6-24 cross into day 1, ticks 25-29 reach hour 5 of day 1.
	Progress(24);
	EXPECT_EQ(GT().absoluteDay, 1);
	EXPECT_EQ(count, 2);
}
