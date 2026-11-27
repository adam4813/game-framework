/// Unit tests for shop TryBuy / TrySell pure functions (ticket 08).
///
/// Seam: TryBuy(inv, typeId, count, price) and TrySell(inv, typeId, count, gain)
/// Tests verify: correct item/coin deltas on success, no mutation on failure,
/// insufficient-funds rejection, and insufficient-stock rejection.
#include <gtest/gtest.h>

#include "farm/shop_helpers.hpp"
#include "player_inventory.hpp"

using namespace game;
using namespace game::farm;

// ── CountItem ─────────────────────────────────────────────────────────────────

TEST(CountItemTest, EmptyInventory_ReturnsZero) {
	PlayerInventory inv{};
	EXPECT_EQ(CountItem(inv, "coin"), 0);
}

TEST(CountItemTest, AfterAddItem_ReturnsCount) {
	PlayerInventory inv{};
	AddItem(inv, "coin", 50);
	EXPECT_EQ(CountItem(inv, "coin"), 50);
}

// ── TryBuy ────────────────────────────────────────────────────────────────────

class TryBuyTest : public ::testing::Test {
protected:
	PlayerInventory inv_;

	void SetUp() override {
		// Start with 20 coins and no seeds.
		AddItem(inv_, "coin", 20);
	}
};

TEST_F(TryBuyTest, SufficientCoins_ReturnsTrue) { EXPECT_TRUE(TryBuy(inv_, "wheat_seed", 1, 10)); }

TEST_F(TryBuyTest, SufficientCoins_DeductsCoins) {
	ASSERT_TRUE(TryBuy(inv_, "wheat_seed", 1, 10));
	EXPECT_EQ(CountItem(inv_, "coin"), 10) << "20 coins - 10 cost = 10 remaining";
}

TEST_F(TryBuyTest, SufficientCoins_AddsItem) {
	ASSERT_TRUE(TryBuy(inv_, "wheat_seed", 1, 10));
	EXPECT_TRUE(HasItem(inv_, "wheat_seed"));
	EXPECT_EQ(CountItem(inv_, "wheat_seed"), 1);
}

TEST_F(TryBuyTest, SufficientCoins_MultiCount_DeductsAll) {
	ASSERT_TRUE(TryBuy(inv_, "wheat_seed", 2, 5)); // 2 × 5 = 10 coins
	EXPECT_EQ(CountItem(inv_, "coin"), 10);
	EXPECT_EQ(CountItem(inv_, "wheat_seed"), 2);
}

TEST_F(TryBuyTest, ExactCoins_Succeeds) {
	EXPECT_TRUE(TryBuy(inv_, "wheat_seed", 1, 20)); // exact
	EXPECT_EQ(CountItem(inv_, "coin"), 0);
}

TEST_F(TryBuyTest, InsufficientCoins_ReturnsFalse) {
	EXPECT_FALSE(TryBuy(inv_, "wheat_seed", 1, 21)); // costs 21, only have 20
}

TEST_F(TryBuyTest, InsufficientCoins_NoMutation) {
	ASSERT_FALSE(TryBuy(inv_, "wheat_seed", 1, 21));
	EXPECT_EQ(CountItem(inv_, "coin"), 20) << "Coins unchanged on failed purchase";
	EXPECT_FALSE(HasItem(inv_, "wheat_seed")) << "No seeds added on failed purchase";
}

TEST_F(TryBuyTest, NoCoins_ReturnsFalse) {
	PlayerInventory empty{};
	EXPECT_FALSE(TryBuy(empty, "wheat_seed", 1, 10));
}

// ── TrySell ───────────────────────────────────────────────────────────────────

class TrySellTest : public ::testing::Test {
protected:
	PlayerInventory inv_;

	void SetUp() override {
		// Start with 3 wheat and 5 coins.
		AddItem(inv_, "wheat", 3);
		AddItem(inv_, "coin", 5);
	}
};

TEST_F(TrySellTest, HasCrops_ReturnsTrue) { EXPECT_TRUE(TrySell(inv_, "wheat", 1, 8)); }

TEST_F(TrySellTest, HasCrops_RemovesCrops) {
	ASSERT_TRUE(TrySell(inv_, "wheat", 1, 8));
	EXPECT_EQ(CountItem(inv_, "wheat"), 2) << "3 wheat - 1 sold = 2 remaining";
}

TEST_F(TrySellTest, HasCrops_AddsCoins) {
	ASSERT_TRUE(TrySell(inv_, "wheat", 1, 8));
	EXPECT_EQ(CountItem(inv_, "coin"), 13) << "5 existing + 8 earned = 13";
}

TEST_F(TrySellTest, SellMultiple_CorrectDelta) {
	ASSERT_TRUE(TrySell(inv_, "wheat", 3, 8)); // sell all 3
	EXPECT_EQ(CountItem(inv_, "wheat"), 0);
	EXPECT_EQ(CountItem(inv_, "coin"), 29) << "5 + 3×8 = 29";
}

TEST_F(TrySellTest, InsufficientCrops_ReturnsFalse) {
	EXPECT_FALSE(TrySell(inv_, "wheat", 4, 8)); // only have 3
}

TEST_F(TrySellTest, InsufficientCrops_NoMutation) {
	ASSERT_FALSE(TrySell(inv_, "wheat", 4, 8));
	EXPECT_EQ(CountItem(inv_, "wheat"), 3) << "Crops unchanged on failed sale";
	EXPECT_EQ(CountItem(inv_, "coin"), 5) << "Coins unchanged on failed sale";
}

TEST_F(TrySellTest, NoCrops_ReturnsFalse) {
	PlayerInventory empty{};
	EXPECT_FALSE(TrySell(empty, "wheat", 1, 8));
}

TEST_F(TrySellTest, SellDifferentCrop_NoCrossContamination) {
	EXPECT_FALSE(TrySell(inv_, "tomato", 1, 5)) << "Can't sell tomato when only wheat present";
	EXPECT_EQ(CountItem(inv_, "wheat"), 3) << "Wheat untouched";
}
