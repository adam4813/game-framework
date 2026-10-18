#pragma once

#include <expected>
#include <string>
#include <vector>

#include <flecs.h>

#include "shift.hpp"

namespace signal_room {

// Spawn a Shift's `counter_items` under `parent` (after BuildLayoutGreybox): for each, an Item named
// Item1, Item2, ... built from the ItemCatalogue singleton and its greybox shape, standing on and held
// by its Counter. The error names `shiftPath` and the first unknown Counter or Item id.
[[nodiscard]] std::expected<void, std::string> SpawnCounterItems(
	const flecs::world& world,
	flecs::entity parent,
	const std::vector<CounterItem>& counterItems,
	const std::string& shiftPath
);

} // namespace signal_room
