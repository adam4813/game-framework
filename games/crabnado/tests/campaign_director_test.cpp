#include <memory>

#include <gtest/gtest.h>

#include "arena_scene.hpp"
#include "campaign.hpp"
#include "crabnado_constants.hpp"
#include "crabnado_module.hpp"
#include "engine/engine.hpp"
#include "player.hpp"
#include "run.hpp"

using namespace engine;
using namespace crabnado;

namespace {

constexpr float kFrame = 1.0F / 60.0F;

// The game on a headless platform with the arena pointed at the gate fixture (one Stage per gate type and a boss).
struct DirectorWorld {
	std::unique_ptr<EngineContext> ctx =
		std::make_unique<EngineContext>(std::make_unique<platform::HeadlessPlatform>(CRABNADO_TEST_ASSET_DIR));

	DirectorWorld() {
		World().import<CrabnadoModule>();
		World().get_mut<CampaignSource>().campaign_path = CRABNADO_TEST_FIXTURE_DIR "/campaign_gates.json";
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }
	[[nodiscard]] RunState& Run() const { return World().get_mut<RunState>(); }

	void Step(const float seconds) const {
		for (float t = 0.0F; t < seconds; t += kFrame) {
			fixed_step::RunFrame(World(), kFrame);
		}
	}

	// Opens the arena and runs its countdown out, with a player the fixture's horde can't kill.
	void StartRun() const {
		scene::ActivateScene<ArenaSceneTag>(World());
		for (int frame = 0; frame < 1200 && Run().phase != RunPhase::Playing; ++frame) {
			fixed_step::RunFrame(World(), kFrame);
		}
		const flecs::entity player = FindPlayer(World());
		ASSERT_TRUE(player.is_valid());
		player.get_mut<PlayerStats>().hp = 1.0e9F;
		player.get_mut<PlayerStats>().max_hp = 1.0e9F;
	}
};

TEST(CampaignDirector, EachGateTypeEndsItsStageAndTheBossFollowsAfterTheStormUp) {
	const DirectorWorld game;
	game.StartRun();
	ASSERT_EQ(game.Run().phase, RunPhase::Playing);
	ASSERT_EQ(game.Run().stage, 0);

	// time: ends at X on the Stage's own clock.
	game.Step(9.0F);
	EXPECT_EQ(game.Run().stage, 0);
	game.Step(1.5F);
	EXPECT_EQ(game.Run().stage, 1);
	EXPECT_LT(game.Run().stage_clock, 1.0F);
	EXPECT_GT(game.Run().stage_banner, 0.0F);

	// quota: no clock, so it waits however long it takes, and ends at Y kills.
	game.Step(15.0F);
	EXPECT_EQ(game.Run().stage, 1);
	EXPECT_GT(game.Run().stage_clock, 14.0F);
	game.Run().kills += 4;
	game.Step(0.2F);
	EXPECT_EQ(game.Run().stage, 1);
	game.Run().kills += 1;
	game.Step(0.2F);
	EXPECT_EQ(game.Run().stage, 2);
	EXPECT_LT(game.Run().stage_clock, 1.0F);

	// both, quota first: kills in the Stage pay the bonus (a reroll) and clear the challenge. Earlier kills don't count.
	EXPECT_EQ(game.Run().rerolls, 0);
	game.Run().kills += 3;
	game.Step(0.2F);
	EXPECT_EQ(game.Run().stage, 2);
	game.Run().kills += 1;
	game.Step(0.2F);
	EXPECT_EQ(game.Run().stage, 3);
	EXPECT_EQ(game.Run().rerolls, 2);
	EXPECT_GT(game.Run().challenge_cleared, 0.0F);

	// both, time first: the time runs out, nothing is paid, and the last Stage gates into the storm-up.
	game.Run().kills += 10;
	const int shellsBefore = game.Run().shells;
	game.Step(5.5F);
	EXPECT_EQ(game.Run().stage, 4);
	EXPECT_EQ(game.Run().rerolls, 2);
	EXPECT_EQ(game.Run().shells, shellsBefore);
	EXPECT_EQ(game.Run().phase, RunPhase::Playing);
	EXPECT_GT(game.Run().storm_up, 0.0F);

	// The storm-up counts down, then the boss lands.
	game.Step(constants::kStormUpTime - 1.0F);
	EXPECT_EQ(game.Run().phase, RunPhase::Playing);
	game.Step(1.5F);
	EXPECT_EQ(game.Run().phase, RunPhase::Boss);
	EXPECT_EQ(game.Run().storm_up, 0.0F);
}

TEST(CampaignDirector, AMoltPausesEveryClockAndResumesThePhaseItLeft) {
	const DirectorWorld game;
	game.StartRun();
	game.Step(2.0F);
	const float stageClock = game.Run().stage_clock;
	const float actClock = game.Run().act_clock;
	const float clock = game.Run().clock;

	game.Run().resume_phase = RunPhase::Playing;
	game.Run().phase = RunPhase::Molting;
	game.Step(3.0F);
	EXPECT_FLOAT_EQ(game.Run().stage_clock, stageClock);
	EXPECT_FLOAT_EQ(game.Run().act_clock, actClock);
	EXPECT_FLOAT_EQ(game.Run().clock, clock);
}

} // namespace
