/// Unit tests for CropGrowthSystem and the Plant/Harvest interaction seams (ticket 07).
///
/// Seams tested:
///   DetermineInteraction   — seed-on-soil Plant action, season blocking, Harvestable gate
///   ExecuteInteraction     — Plant adds Crop+GrowthStage; Harvest removes both
///   CropGrowthSystem       — stage advancement (watered/unwatered), watered reset, reclaim
#include <gtest/gtest.h>

#include <flecs.h>

#include "farm/crop_growth_system.hpp"
#include "farm/farm_components.hpp"
#include "farm/world_interaction_system.hpp"
#include "game_time.hpp"
#include "player_inventory.hpp"

using namespace game;
using namespace game::farm;

// ── Test fixture ─────────────────────────────────────────────────────────────

class CropGrowthTest : public ::testing::Test {
protected:
	flecs::world world_;
	flecs::entity sceneRoot_;

	void SetUp() override {
		CropRegistry reg;

		reg.defs["wheat"] = {
			.typeId           = "wheat",
			.name             = "Wheat",
			.harvestTool      = "scythe",
			.seedItemId       = "seed_wheat",
			.stageDays        = {2, 3, 2}, // 7 total watered days to reach Harvestable
			.seasonsValid     = {},         // all seasons
			.requiresWatering = true,
		};
		reg.seedToCropId["seed_wheat"] = "wheat";

		reg.defs["tomato"] = {
			.typeId           = "tomato",
			.name             = "Tomato",
			.harvestTool      = "",
			.seedItemId       = "seed_tomato",
			.stageDays        = {2, 4, 2},
			.seasonsValid     = {Season::Summer}, // summer only
			.requiresWatering = true,
		};
		reg.seedToCropId["seed_tomato"] = "tomato";

		reg.defs["carrot"] = {
			.typeId           = "carrot",
			.name             = "Carrot",
			.harvestTool      = "",
			.seedItemId       = "seed_carrot",
			.stageDays        = {1, 2, 1}, // 4 total days, no watering needed
			.seasonsValid     = {Season::Spring, Season::Summer, Season::Autumn},
			.requiresWatering = false,
		};
		reg.seedToCropId["seed_carrot"] = "carrot";

		world_.set<CropRegistry>(std::move(reg));

		// Minimal GameTime — not using RegisterGameTime so TimeAdvanceSystem doesn't interfere.
		world_.set<GameTime>({
			.absoluteDay  = 0,
			.season       = Season::Spring,
			.daysPerSeason = 4, // short seasons make reclaim tests fast
		});

		sceneRoot_ = world_.entity("SceneRoot");
		RegisterCropGrowthSystem(world_, sceneRoot_);
	}

	// Advance the game by N days, triggering CropGrowthSystem once per day.
	void AdvanceDays(int n) {
		for (int i = 0; i < n; ++i) {
			world_.get_mut<GameTime>().absoluteDay++;
			world_.progress();
		}
	}

	flecs::entity MakeTilledSoil() {
		return world_.entity().set<TilledSoil>({});
	}

	// Creates a TilledSoil entity with Crop + GrowthStage planted today.
	flecs::entity MakeCrop(const std::string& typeId, bool watered = false) {
		auto e = MakeTilledSoil();
		e.set<Crop>({.typeId = typeId, .watered = watered});
		e.set<GrowthStage>({.stage = Stage::Planted, .daysAtStage = 0});
		return e;
	}

	const CropRegistry& Reg() { return world_.get<CropRegistry>(); }
};

// ── DetermineInteraction: Plant ───────────────────────────────────────────────

TEST_F(CropGrowthTest, PlantSeed_OnTilledSoil_ReturnsPlan) {
	auto tile = MakeTilledSoil();

	const auto action = DetermineInteraction("seed_wheat", tile, Reg(), Season::Spring);

	EXPECT_EQ(action.kind, InteractionKind::Plant);
	EXPECT_EQ(action.cropTypeId, "wheat");
	EXPECT_EQ(action.seedItemId, "seed_wheat");
	EXPECT_FALSE(action.label.empty());
}

TEST_F(CropGrowthTest, PlantSeed_OnTilledSoilWithExistingCrop_NoAction) {
	auto tile = MakeCrop("wheat");
	tile.get_mut<GrowthStage>().stage = Stage::Sprouting;

	// Already has a crop — can't plant on top of it
	const auto action = DetermineInteraction("seed_wheat", tile, Reg(), Season::Spring);
	EXPECT_NE(action.kind, InteractionKind::Plant);
}

// ── DetermineInteraction: seasonal blocking ───────────────────────────────────

TEST_F(CropGrowthTest, Tomato_BlockedOutOfSeason_Spring) {
	auto tile = MakeTilledSoil();
	const auto action = DetermineInteraction("seed_tomato", tile, Reg(), Season::Spring);
	EXPECT_EQ(action.kind, InteractionKind::None)
		<< "Tomato is summer-only; planting in Spring should be blocked";
}

TEST_F(CropGrowthTest, Tomato_BlockedOutOfSeason_Winter) {
	auto tile = MakeTilledSoil();
	const auto action = DetermineInteraction("seed_tomato", tile, Reg(), Season::Winter);
	EXPECT_EQ(action.kind, InteractionKind::None);
}

TEST_F(CropGrowthTest, Tomato_AllowedInSummer) {
	auto tile = MakeTilledSoil();
	const auto action = DetermineInteraction("seed_tomato", tile, Reg(), Season::Summer);
	EXPECT_EQ(action.kind, InteractionKind::Plant);
}

TEST_F(CropGrowthTest, Wheat_AllSeasons_AllAllowed) {
	auto spring = MakeTilledSoil();
	auto winter = MakeTilledSoil();
	EXPECT_EQ(DetermineInteraction("seed_wheat", spring, Reg(), Season::Spring).kind, InteractionKind::Plant);
	EXPECT_EQ(DetermineInteraction("seed_wheat", winter, Reg(), Season::Winter).kind, InteractionKind::Plant);
}

TEST_F(CropGrowthTest, Carrot_BlockedInWinter) {
	auto tile = MakeTilledSoil();
	const auto action = DetermineInteraction("seed_carrot", tile, Reg(), Season::Winter);
	EXPECT_EQ(action.kind, InteractionKind::None)
		<< "Carrot is valid spring/summer/autumn only";
}

// ── ExecuteInteraction: Plant ─────────────────────────────────────────────────

TEST_F(CropGrowthTest, ExecutePlant_AddsCropAndGrowthStageToTile) {
	auto tile = MakeTilledSoil();
	auto player = world_.entity();
	player.set<PlayerInventory>({});
	AddItem(player.get_mut<PlayerInventory>(), "seed_wheat", 1);

	const AvailableInteraction action{
		.kind       = InteractionKind::Plant,
		.label      = "Plant Wheat",
		.cropTypeId = "wheat",
		.seedItemId = "seed_wheat",
	};
	ExecuteInteraction(world_, player, tile, 0, 0, action);

	ASSERT_TRUE(tile.has<Crop>())        << "Crop must be added to tile after planting";
	ASSERT_TRUE(tile.has<GrowthStage>()) << "GrowthStage must be added to tile after planting";
	EXPECT_EQ(tile.get<GrowthStage>().stage, Stage::Planted);
	EXPECT_EQ(tile.get<GrowthStage>().daysAtStage, 0);
	EXPECT_EQ(tile.get<Crop>().typeId, "wheat");
}

TEST_F(CropGrowthTest, ExecutePlant_ConsumesSeedFromInventory) {
	auto tile = MakeTilledSoil();
	auto player = world_.entity();
	player.set<PlayerInventory>({});
	AddItem(player.get_mut<PlayerInventory>(), "seed_wheat", 2);

	const AvailableInteraction action{
		.kind       = InteractionKind::Plant,
		.cropTypeId = "wheat",
		.seedItemId = "seed_wheat",
	};
	ExecuteInteraction(world_, player, tile, 0, 0, action);

	// Started with 2; one consumed — still has it (count=1)
	EXPECT_TRUE(HasItem(player.get<PlayerInventory>(), "seed_wheat"))
		<< "One seed used; one should remain";
}

TEST_F(CropGrowthTest, ExecutePlant_LastSeed_RemovedFromInventory) {
	auto tile = MakeTilledSoil();
	auto player = world_.entity();
	player.set<PlayerInventory>({});
	AddItem(player.get_mut<PlayerInventory>(), "seed_wheat", 1);

	const AvailableInteraction action{
		.kind       = InteractionKind::Plant,
		.cropTypeId = "wheat",
		.seedItemId = "seed_wheat",
	};
	ExecuteInteraction(world_, player, tile, 0, 0, action);

	EXPECT_FALSE(HasItem(player.get<PlayerInventory>(), "seed_wheat"))
		<< "Last seed must be removed from inventory after planting";
}

// ── CropGrowthSystem: watered vs unwatered ────────────────────────────────────

TEST_F(CropGrowthTest, WateredWheat_IncrementsDaysAtStage) {
	auto e = MakeCrop("wheat", /*watered=*/true);

	AdvanceDays(1);

	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Planted) << "Should still be Planted after 1/2 days";
	EXPECT_EQ(e.get<GrowthStage>().daysAtStage, 1);
}

TEST_F(CropGrowthTest, UnwateredWheat_DoesNotAdvance) {
	auto e = MakeCrop("wheat", /*watered=*/false);

	AdvanceDays(3); // more than stageDays[0]=2, but unwatered

	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Planted) << "Unwatered wheat must not advance";
	EXPECT_EQ(e.get<GrowthStage>().daysAtStage, 0);
}

TEST_F(CropGrowthTest, UnwateredCarrot_AdvancesAnyway) {
	auto e = MakeCrop("carrot", /*watered=*/false); // carrot: requiresWatering=false

	AdvanceDays(1); // stageDays[0]=1 → should advance to Sprouting

	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Sprouting)
		<< "Carrot (no watering requirement) must advance when unwatered";
}

// ── CropGrowthSystem: watered flag reset ─────────────────────────────────────

TEST_F(CropGrowthTest, WateredFlag_ResetToFalseAfterDayAdvance) {
	auto e = MakeCrop("wheat", /*watered=*/true);

	AdvanceDays(1);

	EXPECT_FALSE(e.get<Crop>().watered)
		<< "watered flag must be reset to false at the start of each new day";
}

// ── CropGrowthSystem: full stage progression ─────────────────────────────────

TEST_F(CropGrowthTest, WheatGrowsThroughAllStages_Watered) {
	auto e = MakeCrop("wheat");

	// wheat stageDays: [2, 3, 2]
	// Planted → Sprouting (2 days)
	e.get_mut<Crop>().watered = true; AdvanceDays(1);
	e.get_mut<Crop>().watered = true; AdvanceDays(1);
	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Sprouting) << "Should be Sprouting after 2 watered days";

	// Sprouting → Mature (3 days)
	e.get_mut<Crop>().watered = true; AdvanceDays(1);
	e.get_mut<Crop>().watered = true; AdvanceDays(1);
	e.get_mut<Crop>().watered = true; AdvanceDays(1);
	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Mature) << "Should be Mature after 3 more watered days";

	// Mature → Harvestable (2 days)
	e.get_mut<Crop>().watered = true; AdvanceDays(1);
	e.get_mut<Crop>().watered = true; AdvanceDays(1);
	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Harvestable) << "Should be Harvestable after 2 more watered days";
}

TEST_F(CropGrowthTest, CarrotGrowsThroughAllStages_Unwatered) {
	auto e = MakeCrop("carrot"); // requiresWatering=false, stageDays=[1,2,1]

	AdvanceDays(1); // Planted → Sprouting
	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Sprouting);

	AdvanceDays(2); // Sprouting → Mature
	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Mature);

	AdvanceDays(1); // Mature → Harvestable
	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Harvestable);
}

TEST_F(CropGrowthTest, Harvestable_DoesNotAdvanceFurther) {
	auto e = MakeCrop("carrot");
	AdvanceDays(4); // enough to reach Harvestable and then some

	EXPECT_EQ(e.get<GrowthStage>().stage, Stage::Harvestable)
		<< "Stage must not advance beyond Harvestable";
}

// ── DetermineInteraction: Harvestable gate ────────────────────────────────────

TEST_F(CropGrowthTest, HarvestNotOffered_WhenStageIsNotHarvestable) {
	auto tile = MakeCrop("tomato"); // Planted stage, no tool required for tomato
	tile.get_mut<GrowthStage>().stage = Stage::Sprouting;

	const auto action = DetermineInteraction("", tile, Reg(), Season::Spring);
	EXPECT_NE(action.kind, InteractionKind::Harvest)
		<< "Harvest must not be offered when crop is not Harvestable";
}

TEST_F(CropGrowthTest, HarvestOffered_WhenHarvestable_NoTool) {
	auto tile = MakeCrop("tomato");
	tile.get_mut<GrowthStage>().stage = Stage::Harvestable;

	const auto action = DetermineInteraction("", tile, Reg(), Season::Spring);
	EXPECT_EQ(action.kind, InteractionKind::Harvest);
	EXPECT_EQ(action.cropTypeId, "tomato");
}

TEST_F(CropGrowthTest, HarvestOffered_WhenHarvestable_CorrectTool) {
	auto tile = MakeCrop("wheat");
	tile.get_mut<GrowthStage>().stage = Stage::Harvestable;

	const auto action = DetermineInteraction("scythe", tile, Reg(), Season::Spring);
	EXPECT_EQ(action.kind, InteractionKind::Harvest);
}

TEST_F(CropGrowthTest, HarvestNotOffered_WhenHarvestable_WrongTool) {
	auto tile = MakeCrop("wheat");
	tile.get_mut<GrowthStage>().stage = Stage::Harvestable;

	const auto action = DetermineInteraction("hoe", tile, Reg(), Season::Spring);
	EXPECT_NE(action.kind, InteractionKind::Harvest)
		<< "Wheat requires scythe; hoe should not offer harvest";
}

// ── ExecuteInteraction: Harvest removes Crop + GrowthStage ───────────────────

TEST_F(CropGrowthTest, ExecuteHarvest_RemovesCropAndGrowthStage) {
	auto tile = MakeCrop("tomato");
	tile.get_mut<GrowthStage>().stage = Stage::Harvestable;
	auto player = world_.entity();
	player.set<PlayerInventory>({});

	const AvailableInteraction action{
		.kind       = InteractionKind::Harvest,
		.label      = "Harvest tomato",
		.cropTypeId = "tomato",
	};
	ExecuteInteraction(world_, player, tile, 0, 0, action);

	EXPECT_FALSE(tile.has<Crop>())        << "Crop must be removed after harvest";
	EXPECT_FALSE(tile.has<GrowthStage>()) << "GrowthStage must be removed after harvest";
	EXPECT_TRUE(HasItem(player.get<PlayerInventory>(), "tomato"))
		<< "Harvested item must be added to inventory";
}

// ── CropGrowthSystem: bare-ground reclaim ────────────────────────────────────

TEST_F(CropGrowthTest, TilledSoil_ReclaimedAfterFullSeason) {
	auto tile = MakeTilledSoil(); // daysEmpty = 0, daysPerSeason = 4

	AdvanceDays(4); // daysEmpty increments 1 per day → 4 >= 4 → remove TilledSoil

	EXPECT_FALSE(tile.has<TilledSoil>())
		<< "TilledSoil must revert to bare ground after a full season without a crop";
}

TEST_F(CropGrowthTest, TilledSoil_NotReclaimedBeforeFullSeason) {
	auto tile = MakeTilledSoil();

	AdvanceDays(3); // 3 < 4

	EXPECT_TRUE(tile.has<TilledSoil>())
		<< "TilledSoil must NOT be reclaimed before a full season has passed";
}

TEST_F(CropGrowthTest, TilledSoilWithCrop_DaysEmptyNotIncremented) {
	auto tile = MakeCrop("carrot"); // tile has TilledSoil + Crop

	AdvanceDays(4); // carrot reaches Harvestable; daysEmpty should stay 0

	// Crop is still present (not harvested), so daysEmpty should remain 0
	EXPECT_EQ(tile.get<TilledSoil>().daysEmpty, 0)
		<< "daysEmpty must not increment while a crop is present";
}
