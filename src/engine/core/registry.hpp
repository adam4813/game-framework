#pragma once

#include <algorithm>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <nlohmann/json.hpp>

namespace engine::core {

// A string-id keyed collection of immutable definitions loaded from data. Generalises the
// "PlayRegistry / ModifierRegistry / ScenarioRegistry" pattern any content-driven game needs: parse
// a folder of JSON once at load, then look definitions up by their stable string id at runtime.
//
// `T` is a plain-data definition struct. The registry owns its values; Get returns a pointer that
// stays valid until the registry is cleared/destroyed (never store it across a reload). Kept
// deliberately small — it is a typed map with a couple of conveniences, not a service.
template<typename T>
struct Registry {
	std::unordered_map<std::string, T> items;

	// Insert or replace the definition stored under `id`.
	void Add(std::string id, T value) { items.insert_or_assign(std::move(id), std::move(value)); }

	// Look a definition up by id. Returns nullptr when absent so callers branch on the pointer.
	[[nodiscard]] const T* Get(const std::string& id) const {
		const auto it = items.find(id);
		return it == items.end() ? nullptr : &it->second;
	}

	[[nodiscard]] bool Contains(const std::string& id) const { return items.contains(id); }

	[[nodiscard]] std::size_t Count() const { return items.size(); }

	[[nodiscard]] bool Empty() const { return items.empty(); }

	// All registered ids, sorted so iteration order is deterministic across platforms.
	[[nodiscard]] std::vector<std::string> Ids() const {
		std::vector<std::string> ids;
		ids.reserve(items.size());
		for (const auto& [id, _] : items) {
			ids.push_back(id);
		}
		std::ranges::sort(ids);
		return ids;
	}
};

// Turn a directory of `*.json` definition files into a Registry<T>. Each file's stem is its id and
// `parse` converts the parsed document into a T (returning std::nullopt to reject a malformed
// entry, which is logged and skipped under `log_tag`). Returns the count of accepted files. This
// fuses LoadJsonDirectory + the per-file parse + registry insert that every data loader repeats.
template<typename T>
int LoadRegistryFromDirectory(
	const std::string_view dir_path,
	const std::string_view log_tag,
	Registry<T>& registry,
	const std::function<std::optional<T>(std::string_view id, const nlohmann::json& doc)>& parse
) {
	return LoadJsonDirectory(dir_path, log_tag, [&](const std::string_view id, const nlohmann::json& doc) {
		auto parsed = parse(id, doc);
		if (!parsed) {
			return false;
		}
		registry.Add(std::string{id}, std::move(*parsed));
		return true;
	});
}

} // namespace engine::core
