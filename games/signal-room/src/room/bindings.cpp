#include "bindings.hpp"

#include <algorithm>
#include <format>
#include <optional>

#include <nlohmann/json.hpp>

#include "engine/input/key_names.hpp"
#include "json_file.hpp"

using nlohmann::json;

namespace signal_room {

bool Bindings::IsDown(const engine::input::InputState& input, const Action action) const {
	return std::ranges::any_of(keys[static_cast<size_t>(action)], [&](const int key) { return input.IsKeyDown(key); });
}

bool Bindings::WasPressed(const engine::input::InputState& input, const Action action) const {
	return std::ranges::any_of(keys[static_cast<size_t>(action)], [&](const int key) {
		return input.WasKeyPressed(key);
	});
}

std::expected<Bindings, std::string> LoadBindingsFile(const std::string& path) {
	const auto doc = ReadJsonFile(path, "Bindings");
	if (!doc) {
		return std::unexpected(doc.error());
	}

	const auto actions = doc->value("actions", json::object());
	Bindings bindings;
	for (size_t i = 0; i < kActionCount; ++i) {
		const auto* action = kActionNames[i];
		if (!actions.contains(action) || !actions.at(action).is_array()) {
			return std::unexpected(std::format("Bindings file {} has no key list for action '{}'", path, action));
		}
		for (const auto& name : actions.at(action)) {
			const auto key = name.is_string() ? engine::input::KeyCodeFromName(name.get<std::string>()) : std::nullopt;
			if (!key) {
				return std::unexpected(
					std::format("Bindings file {}: action '{}' names unknown key {}", path, action, name.dump())
				);
			}
			bindings.keys[i].push_back(*key);
		}
	}
	return bindings;
}

} // namespace signal_room
