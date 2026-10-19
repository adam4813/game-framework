#include "modifiers.hpp"

#include <algorithm>
#include <array>
#include <format>

#include "json_file.hpp"

using nlohmann::json;

namespace signal_room {

namespace {

constexpr std::array<std::string_view, 5> kDocuments{"shift", "stations", "items", "tickets", "tuning"};

std::string Where(const std::string_view modifier, const size_t index, const std::string_view path) {
	return std::format("Modifier '{}' override {} (path '{}')", modifier, index, path);
}

} // namespace

std::expected<ModifierCatalogue, std::string> LoadModifierCatalogueFile(const std::string& path) {
	const auto doc = ReadJsonFile(path, "Modifier catalogue");
	if (!doc) {
		return std::unexpected(doc.error());
	}

	ModifierCatalogue catalogue;
	for (const auto& [id, entry] : doc->value("modifiers", json::object()).items()) {
		if (!entry.is_object()) {
			return std::unexpected(std::format("Modifier catalogue file {}: Modifier '{}' must be an object", path, id));
		}
		Modifier modifier{
			.name = entry.value("name", id),
			.description = entry.value("description", std::string{}),
		};
		size_t index = 0;
		for (const auto& item : entry.value("overrides", json::array())) {
			const bool valid = item.is_object()
							   && item.value("doc", json{}).is_string()
							   && item.value("path", json{}).is_string()
							   && item.contains("set");
			if (!valid) {
				return std::unexpected(
					std::format(
						"Modifier catalogue file {}: Modifier '{}' override {} must be {{\"doc\", \"path\", \"set\"}}",
						path,
						id,
						index
					)
				);
			}
			ModifierOverride entryOverride{.doc = item.at("doc"), .path = item.at("path"), .set = item.at("set")};
			if (!std::ranges::contains(kDocuments, std::string_view{entryOverride.doc})) {
				return std::unexpected(
					std::format(
						"Modifier catalogue file {}: {} names unknown document '{}'",
						path,
						Where(id, index, entryOverride.path),
						entryOverride.doc
					)
				);
			}
			modifier.overrides.push_back(std::move(entryOverride));
			++index;
		}
		catalogue.Add(id, std::move(modifier));
	}
	return catalogue;
}

std::expected<std::vector<std::string>, std::string> ReadShiftModifierIds(const std::string& shiftPath) {
	const auto doc = ReadJsonFile(shiftPath, "Shift");
	if (!doc) {
		return std::unexpected(doc.error());
	}
	std::vector<std::string> ids;
	for (const auto& id : doc->value("modifiers", json::array())) {
		if (!id.is_string()) {
			return std::unexpected(std::format("Shift file {}: modifiers entries must be Modifier ids", shiftPath));
		}
		ids.push_back(id.get<std::string>());
	}
	return ids;
}

std::expected<ActiveModifiers, std::string> ResolveActiveModifiers(
	const ModifierCatalogue& catalogue,
	const std::vector<std::string>& shiftIds,
	const ModifierToggle& toggle
) {
	for (const auto& id : shiftIds) {
		if (!catalogue.Contains(id)) {
			return std::unexpected(std::format("Shift lists unknown Modifier '{}'", id));
		}
	}
	for (const auto& [id, on] : toggle.forced) {
		if (!catalogue.Contains(id)) {
			return std::unexpected(std::format("Modifier toggle names unknown Modifier '{}'", id));
		}
	}

	const auto forced = [&](const std::string& id) -> const ModifierForce* {
		const auto it = std::ranges::find(toggle.forced, id, &ModifierForce::id);
		return it == toggle.forced.end() ? nullptr : &*it;
	};

	ActiveModifiers active;
	for (const auto& id : catalogue.Ids()) {
		const auto* force = forced(id);
		const bool on = force ? force->on : std::ranges::contains(shiftIds, id);
		if (!on) {
			continue;
		}
		active.ids.push_back(id);
		size_t index = 0;
		for (const auto& override : catalogue.items.at(id).overrides) {
			active.overrides.push_back({.modifier = id, .index = index++, .override = override});
		}
	}
	return active;
}

std::expected<json, std::string> ReadGameDocument(
	const std::string& path,
	const std::string_view fileKind,
	const std::string_view doc,
	const ActiveModifiers& active
) {
	auto result = ReadJsonFile(path, fileKind);
	if (!result) {
		return result;
	}
	for (const auto& [modifier, index, override] : active.overrides) {
		if (override.doc != doc) {
			continue;
		}
		try {
			const json::json_pointer pointer{override.path};
			// `set` replaces an existing path, else adds (a new member, or `-` to append).
			const auto op = result->contains(pointer) ? "replace" : "add";
			result->patch_inplace(json::array({{{"op", op}, {"path", override.path}, {"value", override.set}}}));
		}
		catch (const std::exception& ex) {
			return std::unexpected(
				std::format("{} in the {} file {}: {}", Where(modifier, index, override.path), fileKind, path, ex.what())
			);
		}
	}
	return result;
}

} // namespace signal_room
