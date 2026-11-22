#include <memory>
#include <string>

#include <gtest/gtest.h>

#include "arena_scene.hpp"
#include "campaign.hpp"
#include "crabnado_constants.hpp"
#include "crabnado_module.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "arena.hpp"
#include "run.hpp"
#include "transition.hpp"
#include "upgrades.hpp"

using namespace engine;
using namespace crabnado;

namespace {

constexpr float kFrame = 1.0F / 60.0F;

// The game on a headless platform with the arena pointed at the gate fixture (one Stage per gate type and a boss).
struct DirectorWorld {
	std::unique_ptr<EngineContext> ctx =
		std::make_unique<EngineContext>(std::make_unique<platform::HeadlessPlatform>(CRABNADO_TEST_ASSET_DIR));

	explicit DirectorWorld(const std::string& fixture = "campaign_gates.json") {
		World().import<CrabnadoModule>();
		World().get_mut<CampaignSource>().campaign_path = std::string{CRABNADO_TEST_FIXTURE_DIR} + "/" + fixture;
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

// Jumps to the Boss Stage of the current Act and ends the boss the way its retreat does.
void BeatTheBoss(const DirectorWorld& game) {
	game.Run().stage = static_cast<int>(CurrentAct(game.World())->stages.size());
	game.Step(0.1F);
	game.Run().phase = RunPhase::Boss;
	ClearAct(game.World());
}

TEST(CampaignDirector, BeatingAnActsBossRunsTheTransitionAndStartsTheNextActsCountdown) {
	const DirectorWorld game("campaign_two_acts.json");
	game.StartRun();
	game.Run().kills = 12;
	game.Run().molt_level = 3;
	game.Step(2.0F);
	BeatTheBoss(game);

	// ACT CLEAR holds the scene paused with every clock standing still, and notes the split.
	EXPECT_EQ(game.Run().phase, RunPhase::Transition);
	EXPECT_EQ(game.Run().step, TransitionStep::ActClear);
	EXPECT_GT(game.Run().splits[0], 2.0F);
	const float clock = game.Run().clock;
	game.Step(constants::kActClearTime - 0.5F);
	EXPECT_EQ(game.Run().step, TransitionStep::ActClear);
	EXPECT_FLOAT_EQ(game.Run().clock, clock);

	// Then the comic, the arena reset and the free molt, which leaves the molt bar alone.
	game.Step(1.0F);
	EXPECT_EQ(game.Run().phase, RunPhase::Transition);
	EXPECT_EQ(game.Run().step, TransitionStep::Molt);
	EXPECT_EQ(game.Run().act, 1);
	EXPECT_EQ(game.Run().stage, 0);
	EXPECT_EQ(game.Run().molt_level, 3);
	EXPECT_EQ(game.Run().kills, 12);
	EXPECT_FALSE(game.World().get<MoltOffer>().ids[0].empty());
	EXPECT_FLOAT_EQ(game.Run().clock, clock);
	EXPECT_EQ(game.World().get<Rocks>().circles.size(), 10U);

	// Picking an upgrade leads into the new Act's countdown, then play.
	ChooseUpgrade(game.World(), 0);
	EXPECT_EQ(game.Run().phase, RunPhase::Countdown);
	game.Step(constants::kCountdownTime + 0.5F);
	EXPECT_EQ(game.Run().phase, RunPhase::Playing);
	game.Step(0.5F);
	EXPECT_EQ(game.Run().act, 1);
	EXPECT_EQ(game.Run().stage, 0);
	EXPECT_GT(game.Run().act_clock, 0.0F);
	EXPECT_LT(game.Run().act_clock, 1.0F);
	EXPECT_EQ(game.Run().molt_level, 3);
}

TEST(CampaignDirector, TheNextActStartsAtFullHealthInTheCentreWithTheHordeCleared) {
	const DirectorWorld game("campaign_two_acts.json");
	game.StartRun();
	const flecs::entity player = FindPlayer(game.World());
	player.get_mut<PlayerStats>().max_hp = 150.0F;
	player.get_mut<PlayerStats>().hp = 20.0F;
	player.get_mut<spatial::Transform>().position = {30.0F, 0.0F, 10.0F};
	BeatTheBoss(game);
	game.Step(constants::kActClearTime + 0.5F);

	EXPECT_FLOAT_EQ(player.get<PlayerStats>().hp, 150.0F);
	EXPECT_FLOAT_EQ(player.get<spatial::Transform>().position.x, 0.0F);
	EXPECT_FLOAT_EQ(player.get<spatial::Transform>().position.z, 0.0F);
	EXPECT_EQ(game.World().count<Enemy>(), 0);
}

TEST(CampaignDirector, BeatingTheLastActsBossWinsTheRunAfterItsEnding) {
	const DirectorWorld game("campaign_two_acts.json");
	game.StartRun();
	BeatTheBoss(game);
	game.Step(constants::kActClearTime + 0.5F);
	ChooseUpgrade(game.World(), 0);
	game.Step(constants::kCountdownTime + 1.0F);
	ASSERT_EQ(game.Run().act, 1);
	BeatTheBoss(game);
	EXPECT_EQ(game.Run().phase, RunPhase::Transition);
	game.Step(constants::kActClearTime + 0.5F);
	// The ending plays before the win; Skip goes straight to Results.
	EXPECT_EQ(game.Run().phase, RunPhase::Transition);
	EXPECT_EQ(game.Run().step, TransitionStep::Comic);
	game.Step(2.0F);
	const flecs::entity skip = game.World().lookup("ArenaUI::StoryComic::SkipButton");
	ASSERT_TRUE(skip.is_valid());
	skip.get<ui::OnClick>().callback(skip);
	game.Step(0.1F);
	EXPECT_EQ(game.Run().phase, RunPhase::Won);
	EXPECT_FALSE(game.World().lookup("ArenaUI::StoryComic").is_valid());
	EXPECT_GT(game.Run().splits[1], game.Run().splits[0]);
}

} // namespace
