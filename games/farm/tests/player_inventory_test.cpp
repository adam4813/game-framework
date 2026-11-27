#include <gtest/gtest.h>

#include "player_inventory.hpp"

using namespace game;
using namespace game::constats;

class PlayerInventoryTest : public ::testing::Test {
protected:
	PlayerInventory inv{};
};

// ── Add / Stack ─────────────────────────────────────────────────────────────

TEST_F(PlayerInventoryTest, AddItem_StacksOntoExistingSlot_WhenSameTypeId) {
	ASSERT_TRUE(AddItem(inv, "wheat", 3));
	ASSERT_TRUE(AddItem(inv, "wheat", 2));
	EXPECT_EQ(inv.slots[0].typeId, "wheat");
	EXPECT_EQ(inv.slots[0].count, 5);
	EXPECT_TRUE(inv.slots[1].typeId.empty()); // only one slot consumed
}

TEST_F(PlayerInventoryTest, AddItem_FillsNewSlot_WhenTypeIdIsDifferent) {
	ASSERT_TRUE(AddItem(inv, "wheat", 1));
	ASSERT_TRUE(AddItem(inv, "seeds", 1));
	EXPECT_EQ(inv.slots[0].typeId, "wheat");
	EXPECT_EQ(inv.slots[1].typeId, "seeds");
}

TEST_F(PlayerInventoryTest, AddItem_ReturnsFalse_WhenInventoryFull) {
	for (int i = 0; i < kInventorySlots; ++i) ASSERT_TRUE(AddItem(inv, "item_" + std::to_string(i), 1));
	EXPECT_FALSE(AddItem(inv, "overflow", 1));
}

TEST_F(PlayerInventoryTest, AddItem_Succeeds_WhenExistingSlotMatchesAndInventoryFull) {
	for (int i = 0; i < kInventorySlots; ++i) ASSERT_TRUE(AddItem(inv, "item_" + std::to_string(i), 1));
	// Stacking onto an existing slot must still succeed even when all slots are occupied.
	EXPECT_TRUE(AddItem(inv, "item_0", 99));
}

// ── Remove ───────────────────────────────────────────────────────────────────

TEST_F(PlayerInventoryTest, RemoveItem_ReducesCount) {
	ASSERT_TRUE(AddItem(inv, "wood", 5));
	ASSERT_TRUE(RemoveItem(inv, "wood", 2));
	EXPECT_EQ(inv.slots[0].count, 3);
}

TEST_F(PlayerInventoryTest, RemoveItem_ClearsSlot_WhenCountReachesZero) {
	ASSERT_TRUE(AddItem(inv, "seeds", 2));
	ASSERT_TRUE(RemoveItem(inv, "seeds", 2));
	EXPECT_TRUE(inv.slots[0].typeId.empty());
	EXPECT_EQ(inv.slots[0].count, 0);
}

TEST_F(PlayerInventoryTest, RemoveItem_ReturnsFalse_WhenTypeIdAbsent) {
	EXPECT_FALSE(RemoveItem(inv, "nonexistent", 1));
}

// ── HasItem ──────────────────────────────────────────────────────────────────

TEST_F(PlayerInventoryTest, HasItem_ReturnsFalse_WhenEmpty) { EXPECT_FALSE(HasItem(inv, "wheat")); }

TEST_F(PlayerInventoryTest, HasItem_ReturnsTrue_AfterAdd) {
	ASSERT_TRUE(AddItem(inv, "hoe", 1));
	EXPECT_TRUE(HasItem(inv, "hoe"));
	EXPECT_FALSE(HasItem(inv, "shovel"));
}

TEST_F(PlayerInventoryTest, HasItem_ReturnsFalse_AfterRemoveToZero) {
	ASSERT_TRUE(AddItem(inv, "carrot", 1));
	ASSERT_TRUE(RemoveItem(inv, "carrot", 1));
	EXPECT_FALSE(HasItem(inv, "carrot"));
}

// ── EquippedSlot ─────────────────────────────────────────────────────────────

TEST_F(PlayerInventoryTest, EquippedSlot_DefaultsToZero) { EXPECT_EQ(inv.equippedSlot, 0); }

TEST_F(PlayerInventoryTest, SetEquippedSlot_Clamps_BelowZero) {
	SetEquippedSlot(inv, -5);
	EXPECT_EQ(inv.equippedSlot, 0);
}

TEST_F(PlayerInventoryTest, SetEquippedSlot_Clamps_AboveMax) {
	SetEquippedSlot(inv, 1000);
	EXPECT_EQ(inv.equippedSlot, kInventorySlots - 1);
}

TEST_F(PlayerInventoryTest, SetEquippedSlot_AcceptsValidIndex) {
	SetEquippedSlot(inv, 5);
	EXPECT_EQ(inv.equippedSlot, 5);
}

// ── GetEquippedItem ───────────────────────────────────────────────────────────

TEST_F(PlayerInventoryTest, GetEquippedItem_ReturnsNull_WhenSlotEmpty) { EXPECT_EQ(GetEquippedItem(inv), nullptr); }

TEST_F(PlayerInventoryTest, GetEquippedItem_ReturnsItem_WhenEquippedSlotOccupied) {
	ASSERT_TRUE(AddItem(inv, "watering_can", 1));
	SetEquippedSlot(inv, 0);
	const Item* item = GetEquippedItem(inv);
	ASSERT_NE(item, nullptr);
	EXPECT_EQ(item->typeId, "watering_can");
}

TEST_F(PlayerInventoryTest, GetEquippedItem_RespectsEquippedSlotIndex) {
	ASSERT_TRUE(AddItem(inv, "wheat", 1)); // slot 0
	ASSERT_TRUE(AddItem(inv, "seeds", 1)); // slot 1
	SetEquippedSlot(inv, 1);
	const Item* item = GetEquippedItem(inv);
	ASSERT_NE(item, nullptr);
	EXPECT_EQ(item->typeId, "seeds");
}
