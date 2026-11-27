#pragma once

#include <expected>
#include <string>
#include <vector>

#include <flecs.h>

#include "shift.hpp"

namespace signal_room {

// Spawn one Item of catalogue kind `itemId` under `parent`, built from its greybox shape. A striped
// kind draws its `identity` part in `frequency`'s colour. The Item has no holder and no position yet:
// the caller adds the (Holds) or (Carries) pair and places it. Precondition: `itemId` is in the
// ItemCatalogue.
flecs::entity
SpawnItem(const flecs::world& world, flecs::entity parent, const std::string& itemId, const std::string& frequency);

// Spawn a Shift's `counter_items` under `parent` (after BuildLayoutGreybox): for each, an Item named
// Item1, Item2, ... standing on and held by its Counter. The error names `shiftPath` and the first
// unknown Counter, Item id or frequency.
[[nodiscard]] std::expected<void, std::string> SpawnCounterItems(
	const flecs::world& world,
	flecs::entity parent,
	const std::vector<CounterItem>& counterItems,
	const std::string& shiftPath
);

} // namespace signal_room
