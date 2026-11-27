#pragma once

#include <string_view>

#include "player_inventory.hpp"

namespace game::farm {

/// Returns the number of `typeId` items currently held in `inv`.
[[nodiscard]] int CountItem(const PlayerInventory& inv, std::string_view typeId);

/// Atomically buy `count` of `itemTypeId` at `coinCost` coins each.
/// Deducts `count * coinCost` "coin" items from `inv` and adds `count * itemTypeId` items.
/// Returns `false` (no mutation) when the player cannot afford the purchase.
[[nodiscard]] bool TryBuy(PlayerInventory& inv, std::string_view itemTypeId, int count, int coinCost);

/// Atomically sell `count` of `itemTypeId` at `coinGain` coins each.
/// Removes `count` of `itemTypeId` from `inv` and adds `count * coinGain` "coin" items.
/// Returns `false` (no mutation) when the player does not have enough of `itemTypeId`.
[[nodiscard]] bool TrySell(PlayerInventory& inv, std::string_view itemTypeId, int count, int coinGain);

} // namespace game::farm
