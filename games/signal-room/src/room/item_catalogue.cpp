#include "item_catalogue.hpp"

#include <format>

#include <nlohmann/json.hpp>

#include "greybox_catalogue.hpp"
#include "json_file.hpp"

using nlohmann::json;

namespace signal_room {

std::expected<ItemCatalogue, std::string>
LoadItemCatalogueFile(const std::string& path, const GreyboxCatalogue& greybox) {
	const auto doc = ReadJsonFile(path, "Item catalogue");
	if (!doc) {
		return std::unexpected(doc.error());
	}

	ItemCatalogue catalogue;
	for (const auto& [id, entry] : doc->value("items", json::object()).items()) {
		const auto itemError = [&](const std::string& problem) {
			return std::unexpected(std::format("Item catalogue file {}: Item '{}' {}", path, id, problem));
		};
		if (!entry.is_object()) {
			return itemError("must be an object");
		}
		const auto shape = entry.value("shape", std::string{});
		if (!greybox.items.Contains(shape)) {
			return itemError(std::format("uses unknown greybox shape '{}'", shape));
		}
		const auto carrySpeed = entry.value("carry_speed", json(ItemKind{}.carrySpeed));
		if (!carrySpeed.is_number() || carrySpeed.get<float>() <= 0.0F) {
			return itemError("carry_speed must be a positive number");
		}
		const auto tilt = entry.value("tilt_to_camera", json(false));
		if (!tilt.is_boolean()) {
			return itemError("tilt_to_camera must be true or false");
		}
		const auto stripe = entry.value("frequency_stripe", json(false));
		if (!stripe.is_boolean()) {
			return itemError("frequency_stripe must be true or false");
		}
		catalogue.Add(
			id,
			{.shape = shape,
			 .carrySpeed = carrySpeed.get<float>(),
			 .tiltToCamera = tilt.get<bool>(),
			 .frequencyStripe = stripe.get<bool>()}
		);
	}
	return catalogue;
}

} // namespace signal_room
