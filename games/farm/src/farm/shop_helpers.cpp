#include "shop_helpers.hpp"

#include "player_inventory.hpp"

namespace game::farm {

int CountItem(const PlayerInventory& inv, std::string_view typeId) {
	for (const auto& slot : inv.slots) {
		if (slot.typeId == typeId)
			return slot.count;
	}
	return 0;
}

bool TryBuy(PlayerInventory& inv, std::string_view itemTypeId, int count, int coinCost) {
	const int total = count * coinCost;
	if (CountItem(inv, "coin") < total)
		return false;
	RemoveItem(inv, "coin", total);
	AddItem(inv, itemTypeId, count);
	return true;
}

bool TrySell(PlayerInventory& inv, std::string_view itemTypeId, int count, int coinGain) {
	if (CountItem(inv, itemTypeId) < count)
		return false;
	RemoveItem(inv, itemTypeId, count);
	AddItem(inv, "coin", count * coinGain);
	return true;
}

} // namespace game::farm
