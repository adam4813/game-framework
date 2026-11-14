/// Integration test: FarmWorldScene loads headlessly, player entity exists, tilemap non-empty.
/// Also verifies ticket-08 NPC villager spawning: count, Interactable tag, VillagerSchedule,
/// and DialogTrigger on each villager.
///
/// Uses EngineContext + HeadlessPlatform (no GPU/window) — same harness as scene_equivalence.
#include <gtest/gtest.h>

#include <memory>
#include <set>
#include <utility>

#include "engine/engine.hpp"
#include "engine/engine_context.hpp"
#include "engine/tilemap/tilemap_components.hpp"
#include "farm/farm_components.hpp"
#include "farm/farm_world_scene.hpp"
#include "farm/villager_components.hpp"
#include "game_time.hpp"
#include "player_inventory.hpp"

using namespace engine;
using namespace game;
using namespace game::farm;
using engine::tilemap::GridMover;
using engine::tilemap::PlayerControlled;
using engine::tilemap::RegisterGridMovementLoaders;

namespace {

/// Bootstraps EngineContext with HeadlessPlatform + all optional modules (mirrors TestWorld in
/// tests/scene_equivalence/main.cpp). Kept in an anonymous struct so each test fixture gets
/// its own isolated world.
struct HeadlessWorld {
	std::unique_ptr<EngineContext> ctx;

	HeadlessWorld() {
		ctx = std::make_unique<EngineContext>(std::make_unique<platform::HeadlessPlatform>());
		ctx->GetModuleRegistry().Import(
			*ctx->GetWorld(),
			{"ai", "physics", "noise", "pathfinding", "save", "tilemap", "particles"}
		);
		// Dialog module (game-level) is needed for VillagerSystem dialog trees and
		// DialogTrigger validation in ticket-08 integration tests.
		ctx->GetWorld()->import<dialog::DialogModule>();
	}

	flecs::world& World() { return *ctx->GetWorld(); }
};

} // namespace

class FarmWorldSceneIntegrationTest : public ::testing::Test {
protected:
	HeadlessWorld hw_;

	void SetUp() override {
		auto& world = hw_.World();
		// Wire the game-level singletons that FarmWorldScene depends on.
		RegisterGameTime(world);
		RegisterPlayerInventory(world);
		RegisterGridMovementLoaders(world);

		// Run headless load with a fixed seed so the test is deterministic.
		scene_.LoadHeadless(world, 42);
	}

	FarmWorldScene scene_;
};

// ── Tilemap ──────────────────────────────────────────────────────────────────────

TEST_F(FarmWorldSceneIntegrationTest, TilemapEntityExists) {
	const flecs::entity tilemap = hw_.World().lookup("FarmWorldSceneRoot::FarmWorldTilemap");
	EXPECT_TRUE(tilemap.is_valid()) << "FarmWorldTilemap entity should exist after LoadHeadless";
}

TEST_F(FarmWorldSceneIntegrationTest, TilemapIsNonEmpty) {
	const flecs::entity tilemap = hw_.World().lookup("FarmWorldSceneRoot::FarmWorldTilemap");
	ASSERT_TRUE(tilemap.is_valid());
	const auto* tm = tilemap.try_get<tilemap::Tilemap>();
	ASSERT_NE(tm, nullptr) << "Tilemap entity should carry a Tilemap component";
	EXPECT_GT(tm->tile_ids.size(), 0u) << "Tilemap tile_ids must be populated";
	EXPECT_GT(tm->width * tm->height, 0u) << "Tilemap dimensions must be > 0";
	EXPECT_EQ(tm->tile_ids.size(), static_cast<size_t>(tm->width) * tm->height)
		<< "tile_ids size must equal width × height";
}

// ── Player ──────────────────────────────────────────────────────────────────────

TEST_F(FarmWorldSceneIntegrationTest, PlayerEntityExists) {
	const flecs::entity player = hw_.World().lookup("FarmWorldSceneRoot::Player");
	EXPECT_TRUE(player.is_valid()) << "Player entity should exist after LoadHeadless";
}

TEST_F(FarmWorldSceneIntegrationTest, PlayerHasPlayerControlled) {
	const flecs::entity player = hw_.World().lookup("FarmWorldSceneRoot::Player");
	ASSERT_TRUE(player.is_valid());
	EXPECT_TRUE(player.has<PlayerControlled>()) << "Player should carry the PlayerControlled tag";
}

TEST_F(FarmWorldSceneIntegrationTest, PlayerHasGridMover) {
	const flecs::entity player = hw_.World().lookup("FarmWorldSceneRoot::Player");
	ASSERT_TRUE(player.is_valid());
	EXPECT_TRUE(player.has<GridMover>()) << "Player should carry a GridMover component";
}

TEST_F(FarmWorldSceneIntegrationTest, PlayerSpawnsAtStartingVillage) {
	const flecs::entity player = hw_.World().lookup("FarmWorldSceneRoot::Player");
	ASSERT_TRUE(player.is_valid());
	const auto* gp = player.try_get<tilemap::GridPosition>();
	ASSERT_NE(gp, nullptr);
	// Grid position must be within the world bounds (160×160 default).
	EXPECT_GE(gp->x, 0);
	EXPECT_GE(gp->z, 0);
	EXPECT_LT(gp->x, 160);
	EXPECT_LT(gp->z, 160);
}

TEST_F(FarmWorldSceneIntegrationTest, PlayerHasInventory) {
	const flecs::entity player = hw_.World().lookup("FarmWorldSceneRoot::Player");
	ASSERT_TRUE(player.is_valid());
	EXPECT_TRUE(player.has<PlayerInventory>()) << "Player should carry a PlayerInventory component";
}

// ── Villagers (ticket 08) ───────────────────────────────────────────────────────

TEST_F(FarmWorldSceneIntegrationTest, AtLeastThreeVillagersSpawned) {
	int count = 0;
	hw_.World().query<const VillagerTag>().each([&](flecs::entity, const VillagerTag&) { ++count; });
	EXPECT_GE(count, 3) << "At least 3 villager entities must spawn at scene load";
}

TEST_F(FarmWorldSceneIntegrationTest, VillagersHaveInteractableTag) {
	bool allHave = true;
	hw_.World().query<const VillagerTag>().each([&](flecs::entity e, const VillagerTag&) {
		if (!e.has<Interactable>()) {
			allHave = false;
		}
	});
	EXPECT_TRUE(allHave) << "Every villager must carry the Interactable tag";
}

TEST_F(FarmWorldSceneIntegrationTest, VillagersHaveScheduleWithAtLeastTwoEntries) {
	bool allHave = true;
	hw_.World().query<const VillagerTag, const VillagerSchedule>().each(
		[&](flecs::entity, const VillagerTag&, const VillagerSchedule& sched) {
			if (sched.entries.size() < 2) allHave = false;
		}
	);
	EXPECT_TRUE(allHave) << "Every villager must have a schedule with >= 2 waypoints";
}

TEST_F(FarmWorldSceneIntegrationTest, VillagersHaveDialogTrigger) {
	bool allHave = true;
	hw_.World().query<const VillagerTag, const DialogTrigger>().each(
		[&](flecs::entity, const VillagerTag&, const DialogTrigger& trigger) {
			if (!trigger.tree.is_valid()) allHave = false;
		}
	);
	EXPECT_TRUE(allHave) << "Every villager must have a DialogTrigger with a valid tree entity";
}

TEST_F(FarmWorldSceneIntegrationTest, VillagersHaveGridPosition) {
	bool allHave = true;
	hw_.World().query<const VillagerTag>().each([&](flecs::entity e, const VillagerTag&) {
		if (!e.has<tilemap::GridPosition>()) allHave = false;
	});
	EXPECT_TRUE(allHave) << "Every villager must have a GridPosition for waypoint movement";
}

TEST_F(FarmWorldSceneIntegrationTest, VillagersHaveName) {
	bool allHave = true;
	hw_.World().query<const VillagerTag, const VillagerName>().each(
		[&](flecs::entity, const VillagerTag&, const VillagerName& name) {
			if (name.name.empty()) allHave = false;
		}
	);
	EXPECT_TRUE(allHave) << "Every villager must have a non-empty VillagerName";
}

// ── Multi-town villager spread (ticket 09) ────────────────────────────────────────

// Every villager is parented to a per-town TownRoot entity (grouped by settlement).
TEST_F(FarmWorldSceneIntegrationTest, EveryVillagerBelongsToATownRoot) {
	bool allHave = true;
	hw_.World().query<const VillagerTag>().each([&](flecs::entity e, const VillagerTag&) {
		const flecs::entity parent = e.parent();
		if (!parent.is_valid() || !parent.has<TownRoot>()) allHave = false;
	});
	EXPECT_TRUE(allHave) << "Every villager must be a child of a TownRoot (town-location) entity";
}

// Required AC: villagers exist as children of at least two distinct town locations.
TEST_F(FarmWorldSceneIntegrationTest, VillagersSpanAtLeastTwoTownLocations) {
	std::set<std::pair<int, int>> townLocations;
	hw_.World().query<const VillagerTag>().each([&](flecs::entity e, const VillagerTag&) {
		const flecs::entity parent = e.parent();
		if (parent.is_valid()) {
			if (const auto* root = parent.try_get<TownRoot>()) {
				townLocations.emplace(root->location.x, root->location.y);
			}
		}
	});
	EXPECT_GE(townLocations.size(), 2u) << "villagers must populate at least two distinct town locations at scene load";
}
