/// Unit tests for the farm save / load round-trip (ticket 09).
///
/// Seams tested (per TDD skill – agreed before coding):
///   1. FarmSession, GameTime, PlayerInventory, TilledSoil/Crop/GrowthStage all round-trip
///      through SaveToString → LoadFromString with no platform dependency.
///   2. WorldData seed is preserved across the cycle.
///   3. Farm grid entities (TilledSoil, bare and with Crop+GrowthStage) are recreated.
///   4. New-game: LoadFromString with an empty/missing farm.tiles array leaves no TilledSoil.
#include <gtest/gtest.h>

#include <flecs.h>

#include "engine/engine.hpp"
#include "farm/farm_components.hpp"
#include "farm/farm_save.hpp"
#include "game_time.hpp"
#include "player_inventory.hpp"

using namespace engine;
using namespace game;
using namespace game::farm;

// ── Fixture ──────────────────────────────────────────────────────────────────

class FarmSaveTest : public ::testing::Test {
protected:
	flecs::world world_;
	flecs::entity playerEntity_;

	void SetUp() override {
		world_.import<save::SaveModule>();
		RegisterGameTime(world_);
		RegisterPlayerInventory(world_);
		RegisterFarmSave(world_);

		// Player entity that carries PlayerInventory (mirrors the real scene).
		playerEntity_ = world_.entity("Player").set<PlayerInventory>({});
	}

	// Helpers to count entities with a given component.
	int CountTilledSoil() const {
		int n = 0;
		world_.query<const TilledSoil>().each([&n](flecs::entity, const TilledSoil&) { ++n; });
		return n;
	}
	int CountCrops() const {
		int n = 0;
		world_.query<const Crop>().each([&n](flecs::entity, const Crop&) { ++n; });
		return n;
	}
};

// ── FarmSession seed ─────────────────────────────────────────────────────────

TEST_F(FarmSaveTest, SeedRoundTrips) {
	world_.get_mut<FarmSession>().seed = 99'999;

	const std::string json = save::SaveToString(world_);

	world_.get_mut<FarmSession>().seed = 0;
	save::LoadFromString(world_, json);

	EXPECT_EQ(world_.get<FarmSession>().seed, 99'999);
}

// ── FarmSession player position ──────────────────────────────────────────────

TEST_F(FarmSaveTest, PlayerPositionRoundTrips) {
	auto& session = world_.get_mut<FarmSession>();
	session.playerX = 73;
	session.playerZ = 42;

	const std::string json = save::SaveToString(world_);

	world_.get_mut<FarmSession>().playerX = -1;
	world_.get_mut<FarmSession>().playerZ = -1;
	save::LoadFromString(world_, json);

	EXPECT_EQ(world_.get<FarmSession>().playerX, 73);
	EXPECT_EQ(world_.get<FarmSession>().playerZ, 42);
}

// ── GameTime ─────────────────────────────────────────────────────────────────

TEST_F(FarmSaveTest, GameTimeRoundTrips) {
	auto& gt = world_.get_mut<GameTime>();
	gt.tick = 1234;
	gt.day = 5;
	gt.hour = 14;
	gt.absoluteDay = 47;
	gt.season = Season::Autumn;
	gt.phase = DayPhase::Day;

	const std::string json = save::SaveToString(world_);

	auto& reset = world_.get_mut<GameTime>();
	reset.tick = 0;
	reset.day = 1;
	reset.hour = 0;
	reset.absoluteDay = 0;
	reset.season = Season::Spring;
	reset.phase = DayPhase::Night;

	save::LoadFromString(world_, json);

	const auto& gt2 = world_.get<GameTime>();
	EXPECT_EQ(gt2.tick, 1234);
	EXPECT_EQ(gt2.day, 5);
	EXPECT_EQ(gt2.hour, 14);
	EXPECT_EQ(gt2.absoluteDay, 47);
	EXPECT_EQ(gt2.season, Season::Autumn);
	EXPECT_EQ(gt2.phase, DayPhase::Day); // hour=14 → Day phase
}

// ── PlayerInventory ───────────────────────────────────────────────────────────

TEST_F(FarmSaveTest, PlayerInventoryRoundTrips) {
	auto& inv = playerEntity_.get_mut<PlayerInventory>();
	AddItem(inv, "hoe", 1);
	AddItem(inv, "watering_can", 1);
	AddItem(inv, "seed_wheat", 5);
	inv.equippedSlot = 2;

	const std::string json = save::SaveToString(world_);

	playerEntity_.set<PlayerInventory>({}); // clear
	save::LoadFromString(world_, json);

	const auto& inv2 = playerEntity_.get<PlayerInventory>();
	EXPECT_EQ(inv2.equippedSlot, 2);
	EXPECT_TRUE(HasItem(inv2, "hoe")) << "hoe should survive round-trip";
	EXPECT_TRUE(HasItem(inv2, "watering_can")) << "watering_can should survive round-trip";
	EXPECT_TRUE(HasItem(inv2, "seed_wheat")) << "seed_wheat should survive round-trip";
}

// ── TilledSoil (bare) ─────────────────────────────────────────────────────────

TEST_F(FarmSaveTest, TilledSoilRoundTrips) {
	world_.entity().set<TilledSoil>({.daysEmpty = 3}).set<tilemap::GridPosition>({.x = 10, .z = 20});

	const std::string json = save::SaveToString(world_);

	// Destroy and reload.
	{
		std::vector<flecs::entity> v;
		world_.query<TilledSoil>().each([&v](flecs::entity e, TilledSoil&) { v.push_back(e); });
		for (auto e : v) e.destruct();
	}

	save::LoadFromString(world_, json);

	ASSERT_EQ(CountTilledSoil(), 1) << "One TilledSoil entity should be restored";

	bool found = false;
	world_.query<const TilledSoil, const tilemap::GridPosition>().each(
		[&found](flecs::entity, const TilledSoil& soil, const tilemap::GridPosition& gp) {
			if (gp.x == 10 && gp.z == 20 && soil.daysEmpty == 3) found = true;
		}
	);
	EXPECT_TRUE(found) << "TilledSoil at (10,20) with daysEmpty=3 should be restored";
}

// ── Crop + GrowthStage ────────────────────────────────────────────────────────

TEST_F(FarmSaveTest, CropAndGrowthStageRoundTrip) {
	world_.entity()
		.set<TilledSoil>({.daysEmpty = 0})
		.set<tilemap::GridPosition>({.x = 5, .z = 7})
		.set<Crop>({.typeId = "wheat", .watered = true})
		.set<GrowthStage>({.stage = Stage::Mature, .daysAtStage = 2});

	const std::string json = save::SaveToString(world_);

	{
		std::vector<flecs::entity> v;
		world_.query<TilledSoil>().each([&v](flecs::entity e, TilledSoil&) { v.push_back(e); });
		for (auto e : v) e.destruct();
	}

	save::LoadFromString(world_, json);

	ASSERT_EQ(CountCrops(), 1) << "One Crop should be restored";

	bool foundCrop = false;
	bool foundGrowth = false;
	world_.query<const Crop, const GrowthStage, const tilemap::GridPosition>().each(
		[&](flecs::entity, const Crop& c, const GrowthStage& gs, const tilemap::GridPosition& gp) {
			if (gp.x == 5 && gp.z == 7) {
				foundCrop = (c.typeId == "wheat" && c.watered);
				foundGrowth = (gs.stage == Stage::Mature && gs.daysAtStage == 2);
			}
		}
	);
	EXPECT_TRUE(foundCrop) << "Crop values should survive round-trip";
	EXPECT_TRUE(foundGrowth) << "GrowthStage values should survive round-trip";
}

// ── Multiple tiles ────────────────────────────────────────────────────────────

TEST_F(FarmSaveTest, MultipleTilesRoundTrip) {
	// Three tiles: bare, with crop, with crop+growth
	world_.entity().set<TilledSoil>({.daysEmpty = 1}).set<tilemap::GridPosition>({.x = 1, .z = 1});
	world_.entity()
		.set<TilledSoil>({})
		.set<tilemap::GridPosition>({.x = 2, .z = 2})
		.set<Crop>({.typeId = "tomato", .watered = false});
	world_.entity()
		.set<TilledSoil>({})
		.set<tilemap::GridPosition>({.x = 3, .z = 3})
		.set<Crop>({.typeId = "carrot", .watered = false})
		.set<GrowthStage>({.stage = Stage::Sprouting, .daysAtStage = 1});

	const std::string json = save::SaveToString(world_);

	{
		std::vector<flecs::entity> v;
		world_.query<TilledSoil>().each([&v](flecs::entity e, TilledSoil&) { v.push_back(e); });
		for (auto e : v) e.destruct();
	}

	save::LoadFromString(world_, json);

	EXPECT_EQ(CountTilledSoil(), 3) << "All three tiles should be restored";
	EXPECT_EQ(CountCrops(), 2) << "Two planted tiles should be restored";
}

// ── Missing save data is tolerated ───────────────────────────────────────────

TEST_F(FarmSaveTest, MissingFarmTilesKeyLeavesNoEntities) {
	// A save JSON that doesn't contain "farm.tiles" at all should not crash
	// and should leave zero TilledSoil entities.
	const std::string json = R"({"version":1,"farm":{"seed":42}})";
	save::LoadFromString(world_, json);
	EXPECT_EQ(CountTilledSoil(), 0);
}

TEST_F(FarmSaveTest, EmptyFarmTilesArrayLeavesNoEntities) {
	// Pre-existing TilledSoil entities are destroyed when an empty array is loaded.
	// Note: "farm.tiles" dotted path maps to {"farm":{"tiles":[]}} in the JSON document.
	world_.entity().set<TilledSoil>({}).set<tilemap::GridPosition>({.x = 1, .z = 1});

	const std::string json = R"({"version":1,"farm":{"seed":1,"tiles":[]}})";
	save::LoadFromString(world_, json);
	EXPECT_EQ(CountTilledSoil(), 0);
}
