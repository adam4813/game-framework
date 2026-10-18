#include "item_spawn.hpp"

#include <algorithm>
#include <format>
#include <string_view>

#include "greybox_catalogue.hpp"
#include "greybox_model.hpp"
#include "item_catalogue.hpp"
#include "item_components.hpp"
#include "item_placement.hpp"
#include "palette.hpp"
#include "placements.hpp"
#include "room_components.hpp"
#include "ticket_rules.hpp"

namespace signal_room {

namespace {

// Stripe colour of an Item with no frequency (a striped kind spawned without one).
constexpr engine::core::Rgba kUnstripedColour{.r = 128, .g = 128, .b = 128, .a = 255};

engine::core::Rgba StripeColour(const flecs::world& world, const std::string& frequency) {
	const auto* entry = world.get<FrequencyTable>().Find(frequency);
	const auto* colour = entry != nullptr ? world.get<Palette>().Get(entry->colour) : nullptr;
	return colour != nullptr ? *colour : kUnstripedColour;
}

} // namespace

flecs::entity SpawnItem(
	const flecs::world& world,
	const flecs::entity parent,
	const std::string& itemId,
	const std::string& frequency
) {
	const auto& kind = *world.get<ItemCatalogue>().Get(itemId);
	const auto item = world.entity().child_of(parent).set<Item>(
		{.id = itemId, .frequency = frequency, .carrySpeed = kind.carrySpeed, .tiltToCamera = kind.tiltToCamera}
	);
	const auto& model = *world.get<GreyboxCatalogue>().items.Get(kind.shape);
	BuildGreyboxModel(
		world,
		item,
		model,
		glm::vec3{1.0F},
		kind.frequencyStripe ? StripeColour(world, frequency) : engine::core::Rgba{}
	);
	return item;
}

std::expected<void, std::string> SpawnCounterItems(
	const flecs::world& world,
	const flecs::entity parent,
	const std::vector<CounterItem>& counterItems,
	const std::string& shiftPath
) {
	const auto counters = PlacementsOf<Counter>(world, parent);
	const auto& items = world.get<ItemCatalogue>();

	for (size_t i = 0; i < counterItems.size(); ++i) {
		const auto& [counterName, itemId, frequency] = counterItems[i];
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
		if (!items.Contains(itemId)) {
			return std::unexpected(
				std::format("Shift file {}: counter_items puts unknown Item '{}' on {}", shiftPath, itemId, counterName)
			);
		}
		if (!frequency.empty() && world.get<FrequencyTable>().Find(frequency) == nullptr) {
			return std::unexpected(
				std::format(
					"Shift file {}: counter_items puts an Item with unknown frequency '{}' on {}",
					shiftPath,
					frequency,
					counterName
				)
			);
		}

		const auto item = SpawnItem(world, parent, itemId, frequency);
		item.set_name(std::format("Item{}", i + 1).c_str());
		PlaceOnCounter(item, counter->first);
		counter->first.add<Holds>(item);
	}
	return {};
}

} // namespace signal_room
