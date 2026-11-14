#include "player_inventory.hpp"

#include <algorithm>

#include "engine/engine.hpp"

using namespace engine;

namespace game {

bool AddItem(PlayerInventory& inv, std::string_view typeId, int count) {
	// Stack onto existing slot first.
	for (auto& slot : inv.slots) {
		if (slot.typeId == typeId) {
			slot.count += count;
			return true;
		}
	}
	// Fill first empty slot.
	for (auto& slot : inv.slots) {
		if (slot.typeId.empty()) {
			slot.typeId = std::string(typeId);
			slot.count = count;
			return true;
		}
	}
	return false;
}

bool RemoveItem(PlayerInventory& inv, std::string_view typeId, int count) {
	for (auto& slot : inv.slots) {
		if (slot.typeId != typeId) continue;
		slot.count -= count;
		if (slot.count <= 0) slot = {};
		return true;
	}
	return false;
}

bool HasItem(const PlayerInventory& inv, std::string_view typeId) {
	return std::ranges::any_of(inv.slots, [&](const Item& s) { return s.typeId == typeId && s.count > 0; });
}

const Item* GetEquippedItem(const PlayerInventory& inv) {
	const auto& slot = inv.slots[inv.equippedSlot];
	return slot.count > 0 ? &slot : nullptr;
}

void SetEquippedSlot(PlayerInventory& inv, int slot) {
	inv.equippedSlot = std::clamp(slot, 0, constats::kInventorySlots - 1);
}

void RegisterPlayerInventory(flecs::world& world) {
	world.component<Item>().member<std::string>("typeId").member<int>("count");

	world.component<PlayerInventory>().member<int>("equippedSlot");

	if (!world.has<save::SaveRegistry>()) return;

	auto& reg = world.get_mut<save::SaveRegistry>();
	reg.Field("player.inventory")
		.Get([](const flecs::world& w) -> save::Json {
			save::Json result = nullptr;
			w.query_builder<const PlayerInventory>().build().each([&result](flecs::entity, const PlayerInventory& inv) {
				if (result.is_null()) result = inv;
			});
			return result;
		})
		.Set([](flecs::world& w, const save::Json& j) {
			if (j.is_null()) return;
			w.query_builder<PlayerInventory>().build().each([&j](flecs::entity, PlayerInventory& inv) {
				inv = j.get<PlayerInventory>();
			});
		});
}

} // namespace game
