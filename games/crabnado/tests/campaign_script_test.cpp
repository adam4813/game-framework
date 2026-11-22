#include <filesystem>
#include <memory>
#include <string>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "arena_scene.hpp"
#include "campaign.hpp"
#include "campaign_script.hpp"
#include "crabnado.hpp"
#include "crabnado_module.hpp"
#include "enemy_types.hpp"
#include "engine/engine.hpp"
#include "horde.hpp"
#include "player.hpp"
#include "run.hpp"
#include "title_scene.hpp"

using namespace engine;
using namespace crabnado;

namespace {

constexpr float kFrame = 1.0F / 60.0F;

// The game on a headless platform with the arena pointed at the script fixture: an Act script that logs every event it
// hears into the banner, and Stages whose scripts end themselves, never end, throw, or don't compile.
struct ScriptWorld {
	std::unique_ptr<EngineContext> ctx =
		std::make_unique<EngineContext>(std::make_unique<platform::HeadlessPlatform>(CRABNADO_TEST_ASSET_DIR));

	ScriptWorld() {
		World().import<CrabnadoModule>();
		auto& source = World().get_mut<CampaignSource>();
		source.campaign_path = std::string{CRABNADO_TEST_FIXTURE_DIR} + "/campaign_scripts.json";
		source.scripts_path = std::string{CRABNADO_TEST_FIXTURE_DIR} + "/scripts/";
	}

	[[nodiscard]] flecs::world& World() const { return *ctx->GetWorld(); }
	[[nodiscard]] RunState& Run() const { return World().get_mut<RunState>(); }
	// What the Act script has logged so far (Banner() shows its trail).
	[[nodiscard]] std::string Trail() const { return World().get<CampaignScripts>().banner; }

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

	// The Remote API's way in: names an event, then one tick fires it.
	void Fire(const std::string& event, const std::string& arg0 = "", const std::string& arg1 = "") const {
		auto& scripts = World().get_mut<CampaignScripts>();
		scripts.fire_event = event;
		scripts.fire_arg0 = arg0;
		scripts.fire_arg1 = arg1;
		Step(kFrame * 2.0F);
	}

	// Jumps the director to a Stage, as the Remote API does.
	void JumpTo(const int stage) const {
		Run().stage = stage;
		Step(kFrame * 2.0F);
	}
};

TEST(CampaignScripts, AnActScriptHearsTheActAndStageStartsAndKeepsItsStateThroughTheStages) {
	const ScriptWorld game;
	game.StartRun();
	game.Step(0.2F);
	EXPECT_NE(game.Trail().find("act;start0#1;"), std::string::npos) << game.Trail();
	EXPECT_EQ(game.Run().active_scripts, "act_probe,gate_stage");

	// gate_stage's After(2) timer calls EndStage(), which ends the `script`-gate Stage for reason "script".
	game.Step(2.2F);
	EXPECT_EQ(game.Run().stage, 1);
	// The Act's counter went on from 1 to 2 across the Stage change: its globals live as long as the Act.
	EXPECT_NE(game.Trail().find("end:script;start1#2;"), std::string::npos) << game.Trail();
	// Calls apply in order at the tick's end: EndStage() ended the Stage, then the new Stage's Spawn() was the last call.
	EXPECT_EQ(game.Run().last_script_call, "act_probe: Spawn(\"crab\", 3, \"crabnado\")");
	// The Stage's script is gone with its Stage.
	EXPECT_EQ(game.Run().active_scripts, "act_probe");
}

TEST(CampaignScripts, ASpawnCallThrowsEnemiesOfThatTypeAtTheEndOfTheTick) {
	const ScriptWorld game;
	game.StartRun();
	ASSERT_EQ(game.World().count<Enemy>(), 0);
	// Stage 1's OnStageStart calls Spawn("crab", 3, "crabnado") (the fixture's rate is 0, so nothing else throws).
	for (int frame = 0; frame < 600 && game.Run().stage < 1; ++frame) {
		game.Step(kFrame);
	}
	ASSERT_EQ(game.Run().stage, 1);
	EXPECT_EQ(game.World().count<Enemy>(), 3);
}

TEST(CampaignScripts, ADefeatFiresOnKillWithTheEnemyTypeAndItsSource) {
	const ScriptWorld game;
	game.StartRun();
	flecs::entity horde;
	game.World().query_builder<const Crabnado>().build().each([&horde](const flecs::entity crabnado, const Crabnado&) {
		horde = crabnado.target<ThrowsInto>();
	});
	ASSERT_TRUE(horde.is_valid());
	game.World().entity().child_of(horde).set<spatial::Transform>({}).set<Enemy>({.hp = 0.0F, .type = 0});
	game.Step(kFrame * 2.0F);
	const std::string type = game.World().get<EnemyRegistry>().types.at(0).id;
	EXPECT_NE(game.Trail().find("kill:" + type + "@crabnado;"), std::string::npos) << game.Trail();
}

TEST(CampaignScripts, TheRemoteApiFiresAnyEventByNameWithItsArguments) {
	const ScriptWorld game;
	game.StartRun();
	game.Fire("OnBossPhase", "crabnado", "2");
	EXPECT_NE(game.Trail().find("crabnado:phase2;"), std::string::npos) << game.Trail();
	// The request is consumed, and an unknown name does nothing.
	EXPECT_TRUE(game.World().get<CampaignScripts>().fire_event.empty());
	game.Fire("OnNothing");
	EXPECT_TRUE(game.World().get<CampaignScripts>().fire_event.empty());
	// The read-only getters see the run: Act 0, Stage 0.
	game.Fire("OnTimer", "read");
	EXPECT_NE(game.Trail().find("read:0/0/0;"), std::string::npos) << game.Trail();
}

TEST(CampaignScripts, SpawnBossBringsTheBossAshoreInItsBossStageAndOnBossStartFires) {
	const ScriptWorld game;
	game.StartRun();
	game.JumpTo(5);
	EXPECT_EQ(game.Run().phase, RunPhase::Boss);
	EXPECT_NE(game.Trail().find("start5#"), std::string::npos) << game.Trail();
	EXPECT_NE(game.Trail().find("boss:crabnado;"), std::string::npos) << game.Trail();
}

TEST(CampaignScripts, ACallThatDoesntFitThePhaseIsLoggedAndIgnored) {
	const ScriptWorld game;
	game.StartRun();
	game.JumpTo(5);
	ASSERT_EQ(game.Run().phase, RunPhase::Boss);

	game.Fire("OnTimer", "refuse_end");
	EXPECT_NE(game.Run().last_script_call.find("EndStage() (refused: in a Boss Stage)"), std::string::npos) << game.Run().last_script_call;
	EXPECT_EQ(game.Run().stage, 5);
	game.Fire("OnTimer", "refuse_boss");
	EXPECT_NE(game.Run().last_script_call.find("(refused: not this Act's boss)"), std::string::npos) << game.Run().last_script_call;
	game.Fire("OnTimer", "refuse_sting");
	EXPECT_NE(game.Run().last_script_call.find("(refused: unknown sting)"), std::string::npos) << game.Run().last_script_call;
	game.Fire("OnTimer", "refuse_spawn");
	EXPECT_NE(game.Run().last_script_call.find("(refused: unknown enemy type)"), std::string::npos) << game.Run().last_script_call;
	// A good call goes through and is recorded without a refusal.
	game.Fire("OnTimer", "sting");
	EXPECT_NE(game.Run().last_script_call.find("Sting(\"stage_sting\")"), std::string::npos) << game.Run().last_script_call;
	EXPECT_EQ(game.Run().last_script_call.find("refused"), std::string::npos) << game.Run().last_script_call;
}

TEST(CampaignScripts, EndStageIsRefusedInAMoltAndInATransition) {
	const ScriptWorld game;
	game.StartRun();
	game.JumpTo(1);
	ASSERT_EQ(game.Run().stage, 1);
	game.Run().phase = RunPhase::Molting;
	FireCampaignEvent(game.World(), "OnTimer", {std::string{"refuse_end"}});
	StepCampaignScripts(game.World(), 0.0F);
	EXPECT_NE(game.Run().last_script_call.find("(refused: in a molt)"), std::string::npos) << game.Run().last_script_call;
	EXPECT_EQ(game.Run().stage, 1);

	game.Run().phase = RunPhase::Transition;
	FireCampaignEvent(game.World(), "OnTimer", {std::string{"refuse_end"}});
	StepCampaignScripts(game.World(), 0.0F);
	EXPECT_NE(game.Run().last_script_call.find("(refused: in a transition)"), std::string::npos) << game.Run().last_script_call;
	EXPECT_EQ(game.Run().stage, 1);
}

TEST(CampaignScripts, AScriptGateWaitsForItsScriptHoweverLongItTakes) {
	const ScriptWorld game;
	game.StartRun();
	game.JumpTo(2);
	game.Step(75.0F);
	EXPECT_EQ(game.Run().stage, 2);
	EXPECT_EQ(game.Run().active_scripts, "act_probe,stuck_stage");
}

TEST(CampaignScripts, AHandlerThatThrowsDisablesItsScriptAndItsGateFallsBackToATimeGate) {
	const ScriptWorld game;
	game.StartRun();
	game.JumpTo(3);
	// dying_stage's OnStageStart threw: the script is out of the active list, the Act's script is untouched.
	EXPECT_EQ(game.Run().active_scripts, "act_probe");
	game.Step(55.0F);
	EXPECT_EQ(game.Run().stage, 3);
	game.Step(6.0F);
	EXPECT_EQ(game.Run().stage, 4);
	EXPECT_NE(game.Trail().find("end:time;"), std::string::npos) << game.Trail();
}

TEST(CampaignScripts, AStageScriptThatDoesntCompileIsSkippedAndTheStageRunsWithoutIt) {
	const ScriptWorld game;
	game.StartRun();
	game.JumpTo(4);
	EXPECT_EQ(game.Run().active_scripts, "act_probe");
	game.Step(5.5F);
	EXPECT_EQ(game.Run().stage, 5);
}

TEST(CampaignScripts, EveryArenaLoadRebuildsTheScriptsWithFreshState) {
	const ScriptWorld game;
	game.StartRun();
	game.Step(2.5F);
	ASSERT_EQ(game.Run().stage, 1);
	ASSERT_NE(game.Trail().find("start1#2;"), std::string::npos);

	scene::ActivateScene<TitleSceneTag>(game.World());
	game.Step(0.1F);
	game.StartRun();
	game.Step(0.2F);
	EXPECT_EQ(game.Run().stage, 0);
	// The trail starts over: the Act script is a new instance, not the old one with its globals.
	EXPECT_EQ(game.Trail().find("start1#2;"), std::string::npos) << game.Trail();
	EXPECT_NE(game.Trail().find("act;start0#1;"), std::string::npos) << game.Trail();
}

// -- Parsing --

const std::vector<std::string> kEnemies{"crab"};
const std::vector<std::string> kBosses{"crabnado"};

TEST(CampaignScriptParse, AScriptGateWithoutAScriptDropsItsStage) {
	const nlohmann::json doc = nlohmann::json::parse(R"({"acts":[{"id":"a","stages":[
		{"name":"Bad","gate":{"type":"script"}},{"name":"Fine","gate":{"type":"time","time":5}}]}]})");
	const CampaignParseResult result = ParseCampaign(doc, kEnemies, kBosses, SpawnerIds());
	ASSERT_EQ(result.campaign.acts.size(), 1U);
	ASSERT_EQ(result.campaign.acts[0].stages.size(), 1U);
	EXPECT_EQ(result.campaign.acts[0].stages[0].name, "Fine");
	ASSERT_EQ(result.errors.size(), 1U);
	EXPECT_NE(result.errors[0].find("a script gate needs a 'script'"), std::string::npos);
}

TEST(CampaignScriptParse, AScriptTheCheckRejectsIsDroppedFromItsActOrStageAndReported) {
	const nlohmann::json doc = nlohmann::json::parse(R"({"acts":[{"id":"a","script":"ghost","stages":[
		{"name":"Timed","gate":{"type":"time","time":5},"script":"ghost"},
		{"name":"Scripted","gate":{"type":"script"},"script":"ghost"},
		{"name":"Real","gate":{"type":"script"},"script":"real"}]}]})");
	const auto exists = [](const std::string_view id) { return id == "real"; };
	const CampaignParseResult result = ParseCampaign(doc, kEnemies, kBosses, SpawnerIds(), exists);
	ASSERT_EQ(result.campaign.acts.size(), 1U);
	const Act& act = result.campaign.acts[0];
	EXPECT_TRUE(act.script.empty());
	ASSERT_EQ(act.stages.size(), 2U);
	EXPECT_TRUE(act.stages[0].script.empty());
	EXPECT_EQ(act.stages[1].script, "real");
	EXPECT_EQ(result.errors.size(), 4U) << ::testing::PrintToString(result.errors);
}

TEST(CampaignScriptParse, AScriptsHandlersAreCheckedAgainstTheEventTable) {
	const HandlerReport report = AnalyzeHandlers(std::vector<std::string>{
		"void OnKill(const string&in, const string&in)", "void OnTimer(const string&in)", "void OnKill(const string&in)",
		"void Helper(int)", "void OnStageEnd(string)"});
	EXPECT_EQ(report.defined, (std::vector<std::string>{"OnKill", "OnTimer"}));
	// A handler with the wrong parameters, or an On* name that is no event, is reported; a plain helper is not.
	EXPECT_EQ(report.unmatched, (std::vector<std::string>{"void OnKill(const string&in)", "void OnStageEnd(string)"}));
}

// -- Shipped scripts --

// Compiles `path` headlessly and lists its global functions' declarations; empty (and a failed test) if it doesn't load.
std::vector<std::string> Declarations(const ScriptWorld& game, const std::string& path) {
	auto& backend = *game.World().get_mut<scripting::ScriptBackendSingleton>().backend;
	scripting::IScriptInstance* instance = backend.CreateInstanceFromFile(path, game.World().entity());
	EXPECT_NE(instance, nullptr) << path << " didn't compile";
	if (instance == nullptr) {
		return {};
	}
	std::vector<std::string> declarations = instance->FunctionDeclarations();
	backend.DestroyInstance(instance);
	return declarations;
}

TEST(CampaignScriptData, TheTypoFixtureCompilesButItsMismatchedHandlerIsReported) {
	const ScriptWorld game;
	const HandlerReport report = AnalyzeHandlers(Declarations(game, std::string{CRABNADO_TEST_FIXTURE_DIR} + "/scripts/typo_handler.as"));
	EXPECT_EQ(report.defined, (std::vector<std::string>{"OnStageStart"}));
	EXPECT_EQ(report.unmatched.size(), 1U);
}

TEST(CampaignScriptData, EveryScriptTheShippedCampaignNamesExistsCompilesAndDefinesOnlyEventHandlers) {
	const ScriptWorld game;
	const std::string assets = CRABNADO_TEST_ASSET_DIR;
	const auto doc = core::LoadJsonFile(ResolveSourcePath(assets, std::string{constants::kCampaignPath}), "campaign_script_test");
	ASSERT_TRUE(doc.has_value());
	const CampaignParseResult campaign = ParseCampaign(*doc, std::vector<std::string>{"crab", "shrimp", "lobster"}, std::vector<std::string>{"crabnado", "crabnado_enraged", "captain_crab"}, SpawnerIds());

	std::vector<std::string> ids;
	for (const Act& act : campaign.campaign.acts) {
		ids.push_back(act.script);
		for (const Stage& stage : act.stages) {
			ids.push_back(stage.script);
		}
	}
	// The shipped scripts live in the default scripts directory, not the fixture's.
	const std::string directory = ResolveSourcePath(assets, std::string{constants::kScriptsDir});
	for (const std::string& id : ids) {
		if (id.empty()) {
			continue;
		}
		const std::string path = directory + id + ".as";
		ASSERT_TRUE(std::filesystem::exists(path)) << path;
		const HandlerReport report = AnalyzeHandlers(Declarations(game, path));
		EXPECT_TRUE(report.unmatched.empty()) << path << ": " << ::testing::PrintToString(report.unmatched);
	}
}

} // namespace
