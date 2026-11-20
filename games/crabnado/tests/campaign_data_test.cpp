#include <algorithm>
#include <cmath>
#include <string>
#include <string_view>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include "boss_registry.hpp"
#include "campaign.hpp"
#include "engine/engine.hpp"
#include "offscreen.hpp"

namespace {

using nlohmann::json;
using namespace crabnado;

constexpr std::string_view kDataDir = CRABNADO_TEST_ASSET_DIR "/data/crabnado/";

json LoadShipped(const std::string& name) {
	const auto doc = engine::core::LoadJsonFile(std::string{kDataDir} + name, "campaign_data_test");
	EXPECT_TRUE(doc.has_value()) << name;
	return doc.value_or(json::object());
}

std::vector<std::string> KeysOf(const json& doc) {
	std::vector<std::string> keys;
	for (const auto& item : doc.items()) {
		keys.push_back(item.key());
	}
	return keys;
}

TEST(CampaignData, ShippedFilesResolveEveryReference) {
	const json enemies = LoadShipped("enemies.json");
	const BossParseResult bosses = ParseBosses(LoadShipped("bosses.json"));
	EXPECT_TRUE(bosses.errors.empty()) << ::testing::PrintToString(bosses.errors);

	std::vector<std::string> bossIds;
	for (const BossDef& boss : bosses.registry.bosses) {
		bossIds.push_back(boss.id);
	}
	const CampaignParseResult campaign = ParseCampaign(LoadShipped("campaign.json"), KeysOf(enemies), bossIds, SpawnerIds());
	EXPECT_TRUE(campaign.errors.empty()) << ::testing::PrintToString(campaign.errors);
	ASSERT_FALSE(campaign.campaign.acts.empty());
	for (const Act& act : campaign.campaign.acts) {
		EXPECT_TRUE(act.boss.has_value()) << act.id;
	}
}

TEST(CampaignData, ActOneIsCrabThenShrimpThenTheBoss) {
	const json enemies = LoadShipped("enemies.json");
	const CampaignParseResult campaign = ParseCampaign(LoadShipped("campaign.json"), KeysOf(enemies), std::vector<std::string>{"crabnado"}, SpawnerIds());
	ASSERT_FALSE(campaign.campaign.acts.empty());
	const Act& act = campaign.campaign.acts[0];
	ASSERT_EQ(act.stages.size(), 2U);
	EXPECT_EQ(act.stages[0].name, "Crab");
	EXPECT_FLOAT_EQ(act.stages[0].gate.time, 25.0F);
	EXPECT_EQ(act.stages[0].spawns.size(), 1U);
	EXPECT_EQ(act.stages[1].name, "Shrimp");
	EXPECT_EQ(act.stages[1].spawns.size(), 2U);
	// Today's run: 1.0 + 0.035 * clock, so the Shrimp Stage starts where the Crab Stage ended.
	EXPECT_FLOAT_EQ(SpawnRate(act.stages[0].rate, 25.0F), 1.875F);
	EXPECT_FLOAT_EQ(SpawnRate(act.stages[1].rate, 0.0F), 1.875F);
	// The boss runs at 0.35 of the rate at 300 s (1.0 + 0.035 * 300 = 11.5).
	EXPECT_FLOAT_EQ(BossSpawnRate(act), 11.5F * 0.35F);
}

TEST(CampaignParse, AnUnknownEnemyTypeDropsOnlyThatEntry) {
	const json doc = json::parse(R"({"acts":[{"id":"a","stages":[
		{"gate":{"type":"time","time":10},"spawns":{"crab":5,"kraken":9}}]}]})");
	const CampaignParseResult result = ParseCampaign(doc, std::vector<std::string>{"crab"}, std::vector<std::string>{}, SpawnerIds());
	ASSERT_EQ(result.campaign.acts.size(), 1U);
	EXPECT_EQ(result.campaign.acts[0].stages[0].spawns.size(), 1U);
	EXPECT_EQ(result.errors.size(), 1U);
}

TEST(CampaignParse, AnUnknownSpawnerDropsTheEntryAndAStageDefaultFallsBack) {
	const json doc = json::parse(R"({"acts":[{"id":"a","stages":[
		{"gate":{"type":"time","time":10},"source":"nowhere","spawns":{"crab":{"weight":5,"source":"elsewhere"},"shrimp":3}}]}]})");
	const CampaignParseResult result =
		ParseCampaign(doc, std::vector<std::string>{"crab", "shrimp"}, std::vector<std::string>{}, SpawnerIds());
	const Stage& stage = result.campaign.acts.at(0).stages.at(0);
	EXPECT_EQ(stage.source, "crabnado");
	ASSERT_EQ(stage.spawns.size(), 1U);
	EXPECT_EQ(stage.spawns[0].type, 1);
	EXPECT_EQ(result.errors.size(), 2U);
}

TEST(CampaignParse, AnOffscreenEntryNeedsTheActsOffscreenBlock) {
	const json doc = json::parse(R"({"acts":[
		{"id":"with","offscreen":{"volley_min":2,"volley_max":4,"radius":90,"min_turn_deg":45},"stages":[
			{"gate":{"type":"time","time":10},"source":"offscreen","spawns":{"crab":5}}]},
		{"id":"without","stages":[
			{"gate":{"type":"time","time":10},"source":"offscreen","spawns":{"crab":5}}]}]})");
	const CampaignParseResult result = ParseCampaign(doc, std::vector<std::string>{"crab"}, std::vector<std::string>{}, SpawnerIds());
	ASSERT_EQ(result.campaign.acts.size(), 2U);
	const Act& with = result.campaign.acts[0];
	ASSERT_TRUE(with.offscreen.has_value());
	EXPECT_EQ(with.offscreen->volley_min, 2);
	EXPECT_EQ(with.offscreen->volley_max, 4);
	EXPECT_FLOAT_EQ(with.offscreen->radius, 90.0F);
	EXPECT_FLOAT_EQ(with.offscreen->min_turn_deg, 45.0F);
	EXPECT_EQ(with.stages[0].spawns.size(), 1U);
	EXPECT_TRUE(result.campaign.acts[1].stages[0].spawns.empty());
	EXPECT_EQ(result.errors.size(), 1U);
}

TEST(CampaignParse, AnActWithoutACrabnadoRejectsCrabnadoSpawnsInNormalStagesOnly) {
	const json doc = json::parse(R"({"acts":[{"id":"a","crabnado":false,"offscreen":{},"stages":[
		{"gate":{"type":"time","time":10},"spawns":{"crab":{"weight":5,"source":"crabnado"},"shrimp":{"weight":3,"source":"offscreen"}}}],
		"boss":{"id":"crabnado","spawns":{"crab":5}}}]})");
	const CampaignParseResult result =
		ParseCampaign(doc, std::vector<std::string>{"crab", "shrimp"}, std::vector<std::string>{"crabnado"}, SpawnerIds());
	const Act& act = result.campaign.acts.at(0);
	EXPECT_FALSE(act.crabnado);
	ASSERT_EQ(act.stages[0].spawns.size(), 1U);
	EXPECT_EQ(act.stages[0].spawns[0].source, "offscreen");
	ASSERT_TRUE(act.boss.has_value());
	EXPECT_EQ(act.boss->spawns.size(), 1U);
	EXPECT_EQ(result.errors.size(), 1U);
}

TEST(CampaignParse, ABadOffscreenBlockIsDroppedAndReported) {
	const json doc = json::parse(R"({"acts":[{"id":"a","offscreen":{"volley_min":5,"volley_max":2},"stages":[
		{"gate":{"type":"time","time":10},"source":"offscreen","spawns":{"crab":5}}]}]})");
	const CampaignParseResult result = ParseCampaign(doc, std::vector<std::string>{"crab"}, std::vector<std::string>{}, SpawnerIds());
	const Act& act = result.campaign.acts.at(0);
	EXPECT_FALSE(act.offscreen.has_value());
	EXPECT_TRUE(act.stages[0].spawns.empty());
	EXPECT_EQ(result.errors.size(), 2U);
}

TEST(OffscreenSpawner, EachVolleyComesFromAtLeastTheMinimumTurnAwayFromTheLast) {
	flecs::world world;
	world.set<engine::ecs::RngState>({});
	constexpr float kPi = 3.14159265F;
	constexpr float kMinTurn = 60.0F;
	float last = 0.0F;
	for (int i = 0; i < 500; ++i) {
		const float next = PickVolleyBearing(world, true, last, kMinTurn);
		const float turnDeg = std::abs(std::remainder(next - last, 2.0F * kPi)) * 180.0F / kPi;
		EXPECT_GE(turnDeg, kMinTurn - 0.01F);
		last = next;
	}
}

TEST(CampaignParse, AnUnknownBossIdLeavesTheActWithoutABossStage) {
	const json doc = json::parse(R"({"acts":[{"id":"a","stages":[{"gate":{"type":"time","time":10}}],
		"boss":{"id":"typo","spawns":{}}}]})");
	const CampaignParseResult result = ParseCampaign(doc, std::vector<std::string>{}, std::vector<std::string>{"crabnado"}, SpawnerIds());
	ASSERT_EQ(result.campaign.acts.size(), 1U);
	EXPECT_FALSE(result.campaign.acts[0].boss.has_value());
	EXPECT_EQ(result.errors.size(), 1U);
}

TEST(CampaignParse, ParsesEveryGateType) {
	const json doc = json::parse(R"({"acts":[{"id":"a","stages":[
		{"gate":{"type":"time","time":1}},{"gate":{"type":"quota","kills":2}},
		{"gate":{"type":"both","time":3,"kills":4,"bonus":{"type":"heal"}}},{"gate":{"type":"script","script":"x"}}]}]})");
	const CampaignParseResult result = ParseCampaign(doc, std::vector<std::string>{}, std::vector<std::string>{}, SpawnerIds());
	EXPECT_TRUE(result.errors.empty()) << ::testing::PrintToString(result.errors);
	ASSERT_EQ(result.campaign.acts.at(0).stages.size(), 4U);
}

TEST(CampaignParse, ABothGateReadsItsBonusAndStatesItsChallenge) {
	const json doc = json::parse(R"({"acts":[{"id":"a","stages":[
		{"gate":{"type":"both","time":60,"kills":40,"bonus":{"type":"shells","amount":9}}},
		{"gate":{"type":"both","time":60,"kills":40,"bonus":{"type":"heal"}}},
		{"gate":{"type":"both","time":60,"kills":40,"bonus":{"type":"nonsense"}}}]}]})");
	const CampaignParseResult result = ParseCampaign(doc, std::vector<std::string>{}, std::vector<std::string>{}, SpawnerIds());
	const auto& stages = result.campaign.acts.at(0).stages;
	ASSERT_EQ(stages.size(), 2U);
	EXPECT_EQ(result.errors.size(), 1U);
	EXPECT_EQ(stages[0].gate.bonus.type, BonusType::Shells);
	EXPECT_FLOAT_EQ(stages[0].gate.bonus.amount.value_or(0.0F), 9.0F);
	EXPECT_FALSE(stages[1].gate.bonus.amount.has_value());
	EXPECT_EQ(ChallengeText(stages[0].gate), "Defeat 40 in 1:00!");
	EXPECT_EQ(ChallengeText(Gate{}), "");
}

TEST(CampaignParse, AMissingSeaKeyMeansTheEventNeverHappens) {
	const json doc = json::parse(R"({"acts":[{"id":"a","sea":{"rogue_at":50},"stages":[{"gate":{"type":"time","time":1}}]}]})");
	const CampaignParseResult result = ParseCampaign(doc, std::vector<std::string>{}, std::vector<std::string>{}, SpawnerIds());
	const ActSea& sea = result.campaign.acts.at(0).sea;
	EXPECT_FALSE(sea.cross_swell_at.has_value());
	EXPECT_TRUE(sea.rogue_at.has_value());
}

TEST(BossParse, AMissingTunableSkipsTheBossWithNoDefault) {
	const json doc = json::parse(R"({"crabnado":{"behavior":"slam","speed":6}})");
	const BossParseResult result = ParseBosses(doc);
	EXPECT_TRUE(result.registry.bosses.empty());
	EXPECT_EQ(result.errors.size(), 1U);
}

TEST(BossParse, TheShippedEnragedBossCarriesTheTicketsStartingValues) {
	const BossParseResult result = ParseBosses(LoadShipped("bosses.json"));
	const auto found = std::ranges::find(result.registry.bosses, "crabnado_enraged", &BossDef::id);
	ASSERT_NE(found, result.registry.bosses.end());
	EXPECT_EQ(found->behavior, "enraged");
	EXPECT_FLOAT_EQ(found->enraged.hp, 40.0F);
	EXPECT_FLOAT_EQ(found->enraged.slam_damage, 10.0F);
	EXPECT_FLOAT_EQ(found->enraged.daze_time, 4.0F);
	EXPECT_EQ(found->enraged.volley_ring, 6);
	EXPECT_FLOAT_EQ(found->enraged.suction_radius, 30.0F);
	EXPECT_EQ(found->slam.storm_color.r, 120);
}

TEST(BossParse, AnEnragedBossMissingAnAttackTunableIsSkipped) {
	json doc = LoadShipped("bosses.json");
	doc["crabnado_enraged"].erase("suction_time");
	const BossParseResult result = ParseBosses(doc);
	EXPECT_EQ(result.errors.size(), 1U);
	EXPECT_EQ(std::ranges::count(result.registry.bosses, "crabnado_enraged", &BossDef::id), 0);
}

TEST(Rates, AMissingMaxMeansNoCapAndAMaxCaps) {
	EXPECT_FLOAT_EQ(SpawnRate({.base = 1.0F, .growth = 1.0F, .max = std::nullopt}, 1000.0F), 1001.0F);
	EXPECT_FLOAT_EQ(SpawnRate({.base = 1.0F, .growth = 1.0F, .max = 10.0F}, 1000.0F), 10.0F);
}

TEST(HpScale, AStageScaleReplacesTheActsAndBothRunOnTheActClock) {
	Act act;
	act.hp_scale = {.mult = 1.0F, .growth_seconds = 240.0F};
	Stage tough;
	tough.hp_scale = HpScale{.mult = 2.0F, .growth_seconds = 100.0F};
	EXPECT_FLOAT_EQ(EnemyHpScale(act, nullptr, 240.0F), 2.0F);
	EXPECT_FLOAT_EQ(EnemyHpScale(act, &tough, 100.0F), 4.0F);
}

} // namespace

TEST(CampaignParse, AnInfestedCrestEntryNeedsNoWeightAndReadsItsTuning) {
	const json doc = json::parse(R"({"acts":[{"id":"a","stages":[{"gate":{"type":"time","time":10},"source":"crabnado","spawns":{
		"crab":5,"shrimp":{"source":"infested_crest","chance":0.35,"riders":[3,6],"min_gap":12},
		"lobster":{"source":"infested_crest","chance":2}}}]}]})");
	const CampaignParseResult result = ParseCampaign(doc, std::vector<std::string>{"crab", "shrimp", "lobster"}, std::vector<std::string>{}, SpawnerIds());
	const auto& spawns = result.campaign.acts.at(0).stages.at(0).spawns;
	ASSERT_EQ(spawns.size(), 2U);
	EXPECT_EQ(spawns[1].source, "infested_crest");
	EXPECT_FLOAT_EQ(spawns[1].weight, 0.0F);
	EXPECT_FLOAT_EQ(spawns[1].chance, 0.35F);
	EXPECT_EQ(spawns[1].riders_min, 3);
	EXPECT_EQ(spawns[1].riders_max, 6);
	EXPECT_FLOAT_EQ(spawns[1].min_gap, 12.0F);
	EXPECT_EQ(result.errors.size(), 1U); // the chance of 2 is dropped
}
