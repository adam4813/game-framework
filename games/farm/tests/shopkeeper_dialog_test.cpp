/// Integration test: shopkeeper dialog BUY / SELL flow (finding #17).
///
/// Verifies that after fixing the buy/sell coin inversion (#1):
///   - BUY path: coins decrease, item count increases
///   - SELL path: item count decreases, coins increase
///
/// Uses HeadlessWorld (EngineContext + HeadlessPlatform + dialog module) to run the
/// full dialog runner — same harness as farm_world_scene_integration_test.
#include <gtest/gtest.h>

#include <memory>

#include "engine/engine.hpp"
#include "farm/shop_helpers.hpp"
#include "farm/villager_system.hpp"
#include "game_time.hpp"
#include "player_inventory.hpp"

using namespace engine;
using namespace game;
using namespace game::farm;
using namespace engine::dialog;

namespace {

struct HeadlessWorld {
	std::unique_ptr<EngineContext> ctx;

	HeadlessWorld() {
		ctx = std::make_unique<EngineContext>(std::make_unique<platform::HeadlessPlatform>());
		ctx->GetModuleRegistry().Import(
			*ctx->GetWorld(),
			{"ai", "physics", "noise", "pathfinding", "save", "tilemap", "particles"}
		);
		ctx->GetWorld()->import<DialogModule>();
	}

	flecs::world& World() { return *ctx->GetWorld(); }
};

/// Find the first GraphEdge child of `parent` that has a DialogChoice with the given order.
flecs::entity FindChoiceByOrder(const flecs::entity parent, const int order) {
	flecs::entity found;
	parent.each<graph::GraphEdge>([&](const flecs::entity child) {
		if (child.has<DialogChoice>() && child.get<DialogChoice>().order == order) found = child;
	});
	return found;
}

} // namespace

class ShopkeeperDialogTest : public ::testing::Test {
protected:
	HeadlessWorld hw_;
	flecs::entity sceneRoot_;
	flecs::entity actor_;

	void SetUp() override {
		auto& world = hw_.World();
		RegisterGameTime(world);
		RegisterPlayerInventory(world);

		sceneRoot_ = world.entity("TestRoot");
		// RegisterVillagerSystems registers the inventory dialog handlers.
		RegisterVillagerSystems(world, sceneRoot_);

		actor_ = world.entity("TestActor");
		actor_.set<PlayerInventory>({});
	}
};

// ── BUY path ──────────────────────────────────────────────────────────────────

TEST_F(ShopkeeperDialogTest, BuyPath_DeductsCoinsAndAddsItem) {
	auto& world = hw_.World();

	// Start actor with 50 coins, 0 wheat_seeds.
	auto& inv = actor_.get_mut<PlayerInventory>();
	AddItem(inv, "coin", 50);

	// Build minimal buy tree: greet(line) → buy_choice(choice) → pay(take_coins,10)
	//                          → get_seed(give_item,"wheat_seed",5) → fin(end)
	DialogBuilder b;
	b.Node("greet").Line("Shop", "Buy or sell?");
	b.Node("greet").Child("buy_choice");
	b.Node("buy_choice").Choice("Buy wheat_seed (10c)", 0);
	b.Node("buy_choice").Child("pay");
	b.Node("pay").TakeCoins(10);
	b.Node("pay").Child("get_seed");
	b.Node("get_seed").GiveItem("wheat_seed", 5);
	b.Node("get_seed").Child("fin");
	b.Node("fin").End();

	const auto tree = b.Build(world, "buy_test_tree", sceneRoot_);
	ASSERT_TRUE(tree.is_valid());

	// Start dialog — runner pauses at "greet" (line).
	const flecs::entity runner = StartDialog(world, tree, actor_);
	ASSERT_TRUE(runner.is_valid() && runner.has<DialogRunner>()) << "Runner must be created and waiting at greet";
	EXPECT_TRUE(runner.get<DialogRunner>().waiting);

	// Find the buy_choice entity as an edge child of the root (greet) node.
	const flecs::entity buyChoice = FindChoiceByOrder(tree, 0);
	ASSERT_TRUE(buyChoice.is_valid()) << "Buy choice entity must exist as a child of greet";

	// Step 1: navigate to the buy_choice node.
	ResolveDialog(world, runner, buyChoice);
	ASSERT_TRUE(runner.is_alive() && runner.has<DialogRunner>())
		<< "Runner should still be alive after reaching choice node";
	EXPECT_TRUE(runner.get<DialogRunner>().waiting);

	// Step 2: confirm the choice (advance through pay → get_seed → fin → end).
	ResolveDialog(world, runner, {});

	// Dialog should have finished (pay, get_seed, and fin all auto-advance).
	EXPECT_EQ(CountItem(actor_.get<PlayerInventory>(), "coin"), 40) << "10 coins should be deducted for the purchase";
	EXPECT_EQ(CountItem(actor_.get<PlayerInventory>(), "wheat_seed"), 5)
		<< "5 wheat_seeds should be added to inventory";
}

// ── SELL path ─────────────────────────────────────────────────────────────────

TEST_F(ShopkeeperDialogTest, SellPath_RemovesItemAndAddsCoins) {
	auto& world = hw_.World();

	// Start actor with 5 coins and 3 wheat.
	auto& inv = actor_.get_mut<PlayerInventory>();
	AddItem(inv, "coin", 5);
	AddItem(inv, "wheat", 3);

	// Build minimal sell tree: greet(line) → sell_choice(choice) → take_wheat(take_item)
	//                           → give_coins(give_coins,8) → fin(end)
	DialogBuilder b;
	b.Node("greet").Line("Shop", "Buy or sell?");
	b.Node("greet").Child("sell_choice");
	b.Node("sell_choice").Choice("Sell wheat (+8c)", 0);
	b.Node("sell_choice").Child("take_wheat");
	b.Node("take_wheat").TakeItem("wheat", 1);
	b.Node("take_wheat").Child("give_coins_node");
	b.Node("give_coins_node").GiveCoins(8);
	b.Node("give_coins_node").Child("fin");
	b.Node("fin").End();

	const auto tree = b.Build(world, "sell_test_tree", sceneRoot_);
	ASSERT_TRUE(tree.is_valid());

	// Start dialog.
	const flecs::entity runner = StartDialog(world, tree, actor_);
	ASSERT_TRUE(runner.is_valid() && runner.has<DialogRunner>());
	EXPECT_TRUE(runner.get<DialogRunner>().waiting);

	// Navigate to sell_choice.
	const flecs::entity sellChoice = FindChoiceByOrder(tree, 0);
	ASSERT_TRUE(sellChoice.is_valid()) << "Sell choice entity must exist";

	ResolveDialog(world, runner, sellChoice);
	ASSERT_TRUE(runner.is_alive() && runner.has<DialogRunner>());
	EXPECT_TRUE(runner.get<DialogRunner>().waiting);

	// Confirm: runs take_wheat → give_coins → fin → end.
	ResolveDialog(world, runner, {});

	EXPECT_EQ(CountItem(actor_.get<PlayerInventory>(), "wheat"), 2) << "1 wheat should be removed after selling";
	EXPECT_EQ(CountItem(actor_.get<PlayerInventory>(), "coin"), 13) << "8 coins should be added (5 + 8 = 13)";
}
