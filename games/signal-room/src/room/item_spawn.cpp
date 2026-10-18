#include "item_spawn.hpp"

#include <algorithm>
#include <format>
#include <string_view>

#include "greybox_catalogue.hpp"
#include "greybox_model.hpp"
#include "item_catalogue.hpp"
#include "item_components.hpp"
#include "item_placement.hpp"
#include "placements.hpp"
#include "room_components.hpp"

namespace signal_room {

std::expected<void, std::string> SpawnCounterItems(
	const flecs::world& world,
	const flecs::entity parent,
	const std::vector<CounterItem>& counterItems,
	const std::string& shiftPath
) {
	const auto counters = PlacementsOf<Counter>(world, parent);
	const auto& items = world.get<ItemCatalogue>();
	const auto& greybox = world.get<GreyboxCatalogue>();

	for (size_t i = 0; i < counterItems.size(); ++i) {
		const auto& [counterName, itemId] = counterItems[i];
		const auto counter = std::ranges::find_if(counters, [&](const auto& placement) {
			return std::string_view{placement.first.name().c_str()} == counterName;
		});
		if (counter == counters.end()) {
			return std::unexpected(
				std::format(
					"Shift file {}: counter_items names Counter '{}', but its Layout has none",
					shiftPath,
					counterName
				)
			);
		}
		const auto* kind = items.Get(itemId);
		if (kind == nullptr) {
			return std::unexpected(
				std::format("Shift file {}: counter_items puts unknown Item '{}' on {}", shiftPath, itemId, counterName)
			);
		}

		const auto item =
			world.entity()
				.child_of(parent)
				.set_name(std::format("Item{}", i + 1).c_str())
				.set<Item>({.id = itemId, .carrySpeed = kind->carrySpeed, .tiltToCamera = kind->tiltToCamera});
		PlaceOnCounter(item, counter->first);
		BuildGreyboxModel(world, item, *greybox.items.Get(kind->shape));
		counter->first.add<Holds>(item);
	}
	return {};
}

} // namespace signal_room
