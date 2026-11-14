#pragma once

#include <array>
#include <string>
#include <string_view>

#include <flecs.h>
#include <nlohmann/json.hpp>

#include "game_constants.hpp"

namespace game {

struct Item {
	std::string typeId;
	int count = 0;
};

inline void to_json(nlohmann::json& j, const Item& item) {
	j = nlohmann::json{{"typeId", item.typeId}, {"count", item.count}};
}

inline void from_json(const nlohmann::json& j, Item& item) {
	j.at("typeId").get_to(item.typeId);
	j.at("count").get_to(item.count);
}

struct PlayerInventory {
	std::array<Item, constats::kInventorySlots> slots{};
	int equippedSlot = 0;
};

inline void to_json(nlohmann::json& j, const PlayerInventory& inv) {
	j["equippedSlot"] = inv.equippedSlot;
	j["slots"] = nlohmann::json::array();
	for (const auto& slot : inv.slots) j["slots"].push_back(slot);
}

inline void from_json(const nlohmann::json& j, PlayerInventory& inv) {
	inv.equippedSlot = j.value("equippedSlot", 0);
	if (const auto it = j.find("slots"); it != j.end()) {
		const auto& arr = *it;
		for (size_t i = 0; i < inv.slots.size() && i < arr.size(); ++i) inv.slots[i] = arr[i].get<Item>();
	}
}

// Returns false when inventory is full and no existing stack matches typeId.
bool AddItem(PlayerInventory& inv, std::string_view typeId, int count = 1);

// Returns false when typeId is not present. Clears the slot when count reaches zero.
bool RemoveItem(PlayerInventory& inv, std::string_view typeId, int count = 1);

bool HasItem(const PlayerInventory& inv, std::string_view typeId);

// Returns nullptr when the equipped slot is empty.
const Item* GetEquippedItem(const PlayerInventory& inv);

// Clamps slot to [0, kInventorySlots - 1].
void SetEquippedSlot(PlayerInventory& inv, int slot);

// Registers Flecs component reflection and save binding (guarded: skipped when SaveRegistry absent).
void RegisterPlayerInventory(flecs::world& world);

} // namespace game
