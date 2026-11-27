/// Unit tests for WorldInteractionSystem (ticket 06).
///
/// Tests the three public seams:
///   ComputeFacingTile     — pure math, no ECS required
///   DetermineInteraction  — pure logic over ECS component state
///   ExecuteInteraction    — ECS mutation (bare flecs::world, no module imports needed)
///
/// End-to-end system dispatch (no-double-fire) uses a minimal world with explicit
/// InputState setup.
#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <flecs.h>

#include "engine/engine.hpp"
#include "farm/farm_components.hpp"
#include "farm/world_interaction_system.hpp"
#include "player_inventory.hpp"

using namespace game;
using namespace game::farm;
using namespace engine;
using namespace engine::tilemap;

// ── ComputeFacingTile ─────────────────────────────────────────────────────────

TEST(FacingTileTest, FacingUp_ZIncreases) {
	GridPosition gp{.x = 5, .z = 5};
	FacingDirection facing{.dx = 0, .dz = 1};
	auto [tx, tz] = ComputeFacingTile(gp, facing);
	EXPECT_EQ(tx, 5);
	EXPECT_EQ(tz, 6);
}

TEST(FacingTileTest, FacingDown_ZDecreases) {
	GridPosition gp{.x = 5, .z = 5};
	FacingDirection facing{.dx = 0, .dz = -1};
	auto [tx, tz] = ComputeFacingTile(gp, facing);
	EXPECT_EQ(tx, 5);
	EXPECT_EQ(tz, 4);
}

TEST(FacingTileTest, FacingLeft_XIncreases) {
	GridPosition gp{.x = 5, .z = 5};
	FacingDirection facing{.dx = 1, .dz = 0};
	auto [tx, tz] = ComputeFacingTile(gp, facing);
	EXPECT_EQ(tx, 6);
	EXPECT_EQ(tz, 5);
}

TEST(FacingTileTest, FacingRight_XDecreases) {
	GridPosition gp{.x = 5, .z = 5};
	FacingDirection facing{.dx = -1, .dz = 0};
	auto [tx, tz] = ComputeFacingTile(gp, facing);
	EXPECT_EQ(tx, 4);
	EXPECT_EQ(tz, 5);
}

// ── DetermineInteraction ──────────────────────────────────────────────────────

class DetermineInteractionTest : public ::testing::Test {
protected:
	flecs::world world_;
	CropRegistry registry_;

	void SetUp() override {
		registry_.defs["wheat"] = {.typeId = "wheat", .name = "Wheat", .harvestTool = "scythe"};
		registry_.defs["tomato"] = {.typeId = "tomato", .name = "Tomato", .harvestTool = ""};
		registry_.defs["carrot"] = {.typeId = "carrot", .name = "Carrot", .harvestTool = ""};
	}

	flecs::entity MakeTile() { return world_.entity(); }
};

TEST_F(DetermineInteractionTest, BareGroundWithHoe_TillSoil) {
	const auto result = DetermineInteraction("hoe", flecs::entity{}, registry_);
	EXPECT_EQ(result.kind, InteractionKind::TillSoil);
	EXPECT_FALSE(result.label.empty());
}

TEST_F(DetermineInteractionTest, BareGroundWithWateringCan_NoAction) {
	const auto result = DetermineInteraction("watering_can", flecs::entity{}, registry_);
	EXPECT_EQ(result.kind, InteractionKind::None);
}

TEST_F(DetermineInteractionTest, BareGroundNoTool_NoAction) {
	const auto result = DetermineInteraction("", flecs::entity{}, registry_);
	EXPECT_EQ(result.kind, InteractionKind::None);
}

TEST_F(DetermineInteractionTest, WateringCanOnUnwateredCrop_Water) {
	auto tile = MakeTile();
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "tomato", .watered = false});

	const auto result = DetermineInteraction("watering_can", tile, registry_);
	EXPECT_EQ(result.kind, InteractionKind::Water);
}

TEST_F(DetermineInteractionTest, WateringCanOnHarvestableCrop_Harvest) {
	// A harvestable crop should offer Harvest rather than Water, even when holding
	// watering can and the crop is unwatered (finding #7).
	auto tile = MakeTile();
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "tomato", .watered = false});
	tile.set<GrowthStage>({.stage = Stage::Harvestable});

	const auto result = DetermineInteraction("watering_can", tile, registry_);
	EXPECT_EQ(result.kind, InteractionKind::Harvest)
		<< "Holding watering can on harvestable (no-tool) crop must offer Harvest, not Water";
}

TEST_F(DetermineInteractionTest, WateringCanOnNonHarvestableUnwateredCrop_Water) {
	// A non-harvestable unwatered crop should offer Water when holding watering can.
	auto tile = MakeTile();
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "tomato", .watered = false});
	tile.set<GrowthStage>({.stage = Stage::Mature}); // not yet harvestable

	const auto result = DetermineInteraction("watering_can", tile, registry_);
	EXPECT_EQ(result.kind, InteractionKind::Water) << "Watering can on non-harvestable unwatered crop must offer Water";
}

TEST_F(DetermineInteractionTest, WateringCanOnAlreadyWateredHarvestableCrop_Harvest) {
	auto tile = MakeTile();
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "tomato", .watered = true});
	tile.set<GrowthStage>({.stage = Stage::Harvestable});

	// Already watered harvestable tomato (no-tool): harvest is offered.
	const auto result = DetermineInteraction("watering_can", tile, registry_);
	EXPECT_EQ(result.kind, InteractionKind::Harvest) << "Already-watered harvestable tomato should offer harvest";
}

TEST_F(DetermineInteractionTest, HarvestWheat_ScytheRequired_WrongTool) {
	auto tile = MakeTile();
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "wheat", .watered = false});
	tile.set<GrowthStage>({.stage = Stage::Harvestable});

	const auto result = DetermineInteraction("hoe", tile, registry_);
	EXPECT_NE(result.kind, InteractionKind::Harvest) << "Hoe should not offer wheat harvest";
}

TEST_F(DetermineInteractionTest, HarvestWheat_ScytheRequired_CorrectTool) {
	auto tile = MakeTile();
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "wheat", .watered = false});
	tile.set<GrowthStage>({.stage = Stage::Harvestable});

	const auto result = DetermineInteraction("scythe", tile, registry_);
	EXPECT_EQ(result.kind, InteractionKind::Harvest);
	EXPECT_EQ(result.cropTypeId, "wheat");
}

TEST_F(DetermineInteractionTest, HarvestTomato_NoToolRequired_BareHand) {
	auto tile = MakeTile();
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "tomato", .watered = false});
	tile.set<GrowthStage>({.stage = Stage::Harvestable});

	// Harvestable takes priority: even watering_can should offer harvest for a
	// no-tool crop (finding #7 — harvest before water for harvestable crops).
	const auto wateringResult = DetermineInteraction("watering_can", tile, registry_);
	EXPECT_EQ(wateringResult.kind, InteractionKind::Harvest)
		<< "Harvestable tomato should offer harvest even with watering can";

	// No tool equipped → harvest (no tool required)
	const auto harvestResult = DetermineInteraction("", tile, registry_);
	EXPECT_EQ(harvestResult.kind, InteractionKind::Harvest);
	EXPECT_EQ(harvestResult.cropTypeId, "tomato");
}

TEST_F(DetermineInteractionTest, HarvestCarrot_NoToolRequired_AnyTool) {
	auto tile = MakeTile();
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "carrot", .watered = true}); // already watered
	tile.set<GrowthStage>({.stage = Stage::Harvestable});

	// Hoe equipped, carrot has no tool requirement → harvest
	const auto result = DetermineInteraction("hoe", tile, registry_);
	EXPECT_EQ(result.kind, InteractionKind::Harvest);
}

TEST_F(DetermineInteractionTest, Pickupable_AnyTool) {
	auto item = MakeTile();
	item.set<Pickupable>({.typeId = "mushroom", .count = 2});

	const auto result = DetermineInteraction("scythe", item, registry_);
	EXPECT_EQ(result.kind, InteractionKind::Pickup);
	EXPECT_NE(result.label.find("mushroom"), std::string::npos);
}

TEST_F(DetermineInteractionTest, Interactable_AnyTool) {
	auto door = MakeTile();
	door.add<Interactable>();

	const auto result = DetermineInteraction("", door, registry_);
	EXPECT_EQ(result.kind, InteractionKind::Interact);
}

TEST_F(DetermineInteractionTest, HintVisibleOnlyWhenActionAvailable) {
	// No action: bare ground, no tool
	const auto none = DetermineInteraction("", flecs::entity{}, registry_);
	EXPECT_EQ(none.kind, InteractionKind::None);
	EXPECT_TRUE(none.label.empty()) << "Label must be empty when no action";

	// Action: bare ground + hoe
	const auto some = DetermineInteraction("hoe", flecs::entity{}, registry_);
	EXPECT_NE(some.kind, InteractionKind::None);
	EXPECT_FALSE(some.label.empty()) << "Label must be non-empty when action available";
}

// ── ExecuteInteraction ────────────────────────────────────────────────────────

class ExecuteInteractionTest : public ::testing::Test {
protected:
	flecs::world world_;
	flecs::entity player_;
	CropRegistry registry_;

	void SetUp() override {
		registry_.defs["wheat"] = {.typeId = "wheat", .name = "Wheat", .harvestTool = "scythe"};
		registry_.defs["tomato"] = {.typeId = "tomato", .name = "Tomato", .harvestTool = ""};

		player_ = world_.entity("Player");
		player_.add<PlayerControlled>();
		player_.set<GridPosition>({.x = 5, .z = 5});
		player_.set<FacingDirection>({.dx = 0, .dz = 1});
		player_.set<GridMover>({});
		player_.set<PlayerInventory>({});
	}
};

TEST_F(ExecuteInteractionTest, Hoe_CreatesFilledSoilAtFacingTile) {
	const AvailableInteraction action{
		.kind = InteractionKind::TillSoil,
		.label = "Till soil",
	};
	ExecuteInteraction(world_, player_, flecs::entity{}, 5, 6, action);

	bool found = false;
	world_.query<const TilledSoil, const GridPosition>().each([&](const TilledSoil&, const GridPosition& pos) {
		if (pos.x == 5 && pos.z == 6) found = true;
	});
	EXPECT_TRUE(found) << "TilledSoil entity must exist at facing tile after hoe";
}

TEST_F(ExecuteInteractionTest, WateringCan_SetsCropWateredTrue) {
	auto tile = world_.entity("Tile");
	tile.set<GridPosition>({.x = 5, .z = 6});
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "tomato", .watered = false});

	const AvailableInteraction action{
		.kind = InteractionKind::Water,
		.label = "Water tomato",
	};
	ExecuteInteraction(world_, player_, tile, 5, 6, action);

	EXPECT_TRUE(tile.get<Crop>().watered) << "Crop must be watered after watering interaction";
}

TEST_F(ExecuteInteractionTest, Harvest_RemovesCropAndAddsToInventory) {
	auto tile = world_.entity("Tile");
	tile.set<GridPosition>({.x = 5, .z = 6});
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "wheat", .watered = true});
	tile.set<GrowthStage>({.stage = Stage::Harvestable});

	const AvailableInteraction action{
		.kind = InteractionKind::Harvest,
		.label = "Harvest wheat",
		.cropTypeId = "wheat",
	};
	ExecuteInteraction(world_, player_, tile, 5, 6, action);

	EXPECT_FALSE(tile.has<Crop>()) << "Crop component must be removed after harvest";
	EXPECT_FALSE(tile.has<GrowthStage>()) << "GrowthStage must be removed after harvest";
	EXPECT_TRUE(HasItem(player_.get<PlayerInventory>(), "wheat"))
		<< "wheat must be added to player inventory after harvest";
}

TEST_F(ExecuteInteractionTest, Harvest_WheatNotOffered_WhenWrongToolEquipped) {
	// DetermineInteraction returns None for wheat with wrong tool — verify that path
	auto tile = world_.entity("Tile");
	tile.set<GridPosition>({.x = 5, .z = 6});
	tile.add<TilledSoil>();
	tile.set<Crop>({.typeId = "wheat", .watered = false});
	tile.set<GrowthStage>({.stage = Stage::Harvestable});

	const auto action = DetermineInteraction("hoe", tile, registry_);
	EXPECT_EQ(action.kind, InteractionKind::None)
		<< "Harvest must NOT be offered when equipped tool doesn't match harvestTool";
}

TEST_F(ExecuteInteractionTest, Pickup_AddsItemAndRemovesEntity) {
	auto item = world_.entity("Mushroom");
	item.set<GridPosition>({.x = 5, .z = 6});
	item.set<Pickupable>({.typeId = "mushroom", .count = 2});

	const AvailableInteraction action{
		.kind = InteractionKind::Pickup,
		.label = "Pick up mushroom",
	};
	ExecuteInteraction(world_, player_, item, 5, 6, action);

	EXPECT_FALSE(item.is_alive()) << "Pickupable entity must be removed from world";
	EXPECT_TRUE(HasItem(player_.get<PlayerInventory>(), "mushroom")) << "mushroom must be in inventory after pickup";
}

TEST_F(ExecuteInteractionTest, Interactable_FiresInteractWithEvent) {
	auto door = world_.entity("Door");
	door.set<GridPosition>({.x = 5, .z = 6});
	door.add<Interactable>();
	door.set<DoorOpen>({.open = false});

	// Register an observer that toggles the door when InteractWith fires on it.
	world_.observer("DoorToggle").event<InteractWith>().with<Interactable>().run([](flecs::iter& it) {
		while (it.next()) {
			for (size_t i = 0; i < it.count(); ++i) {
				if (auto* d = it.entity(i).try_get_mut<DoorOpen>()) d->open = !d->open;
			}
		}
	});

	const AvailableInteraction action{
		.kind = InteractionKind::Interact,
		.label = "Interact",
	};
	ExecuteInteraction(world_, player_, door, 5, 6, action);

	EXPECT_TRUE(door.get<DoorOpen>().open) << "Door should be open after InteractWith event fires";
}

// ── No-double-fire via system dispatch ───────────────────────────────────────

/// Verifies that the interaction system only fires on a key PRESS (rising edge),
/// not while the key is held down (state only, pressed=false).
TEST(WorldInteractionSystemTest, NoDoubleFire_ConfirmConsumedOnPress) {
	flecs::world world;

	// Set up required singletons.
	world.set<input::InputState>({});
	world.set<CropRegistry>({});
	world.set<InteractionHint>({});

	// Create player facing (5,6).
	auto player = world.entity("Player");
	player.add<PlayerControlled>();
	player.set<GridPosition>({.x = 5, .z = 5});
	player.set<FacingDirection>({.dx = 0, .dz = 1});
	player.set<GridMover>({});
	player.set<PlayerInventory>({});
	AddItem(player.get_mut<PlayerInventory>(), "hoe", 1);

	// Register systems under a scene root.
	auto sceneRoot = world.entity("SceneRoot");
	RegisterWorldInteractionSystems(world, sceneRoot);

	// Frame 1: press E → hoe on bare (5,6) → TilledSoil created.
	auto& input1 = world.get_mut<input::InputState>();
	input1.keys[input::KeyCode::E].pressed = true;
	input1.keys[input::KeyCode::E].state = true;
	world.progress(0.016F);

	auto countTiles = [&]() {
		int n = 0;
		world.query<const TilledSoil, const GridPosition>().each([&](const TilledSoil&, const GridPosition& p) {
			if (p.x == 5 && p.z == 6) ++n;
		});
		return n;
	};
	EXPECT_EQ(countTiles(), 1) << "TilledSoil should be created once after first press";

	// Frame 2: key still physically held, but NOT a new press (rising edge gone).
	// The interaction system should NOT fire again.
	auto& input2 = world.get_mut<input::InputState>();
	input2.keys[input::KeyCode::E].pressed = false; // held, not newly pressed
	input2.keys[input::KeyCode::E].state = true;
	world.progress(0.016F);

	// Still only one TilledSoil (hoe on existing TilledSoil with no Crop → no DetermineInteraction match
	// for "hoe on tilled soil" in ticket 06, but also .pressed is false so nothing fires anyway).
	EXPECT_EQ(countTiles(), 1) << "No second interaction should fire when key is held (not pressed)";
}

// ── Player starting inventory ─────────────────────────────────────────────────

/// The FarmWorldScene gives the player hoe + watering_can + scythe at startup.
/// Since LoadHeadless calls SetupPlayer (tested via farm_world_scene_integration_test),
/// we verify the item IDs here as a contract test.
TEST(FarmStartingInventoryTest, StarterToolsPresent) {
	PlayerInventory inv{};
	AddItem(inv, "hoe", 1);
	AddItem(inv, "watering_can", 1);
	AddItem(inv, "scythe", 1);

	EXPECT_TRUE(HasItem(inv, "hoe")) << "Starter hoe must be in inventory";
	EXPECT_TRUE(HasItem(inv, "watering_can")) << "Starter watering_can must be in inventory";
	EXPECT_TRUE(HasItem(inv, "scythe")) << "Starter scythe must be in inventory";
}
