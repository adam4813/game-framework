#pragma once

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

#include <glm/glm.hpp>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "core_types.hpp"

namespace engine::core {

// Open and parse a JSON file from disk. Returns std::nullopt (and logs the failure under `log_tag`)
// if the file cannot be opened or does not contain valid JSON. Centralises the open + parse +
// try/catch boilerplate that data loaders (levels, tilesets, ...) would otherwise each duplicate.
[[nodiscard]] inline std::optional<nlohmann::json>
LoadJsonFile(const std::string_view path, const std::string_view log_tag) {
	std::ifstream file{std::string{path}};
	if (!file) {
		spdlog::error("[{}] Cannot open file '{}'", log_tag, path);
		return std::nullopt;
	}
	try {
		nlohmann::json doc;
		file >> doc;
		return doc;
	}
	catch (const std::exception& ex) {
		spdlog::error("[{}] Failed to parse '{}': {}", log_tag, path, ex.what());
		return std::nullopt;
	}
}

// Load every `*.json` file in a directory (non-recursive), parsing each with LoadJsonFile and handing
// the parsed document plus the file's stem (its name without extension — a natural string id) to
// `on_file`. `on_file` returns true when it accepts the document; the count of accepted files is
// returned so a data loader can log an accurate "loaded N" summary. A missing directory, an
// unreadable entry, or an invalid JSON file is logged under `log_tag` and skipped — this never throws.
// Files are visited in sorted order so load results are deterministic across platforms/filesystems.
// This centralises the directory-scan + per-file parse boilerplate that content loaders (plays,
// modifiers, scenarios, ...) would otherwise each duplicate.
[[nodiscard]] inline int LoadJsonDirectory(
	const std::string_view dir_path,
	const std::string_view log_tag,
	const std::function<bool(std::string_view id, const nlohmann::json& doc)>& on_file
) {
	namespace fs = std::filesystem;
	std::error_code ec;
	const fs::path dir{dir_path};
	if (!fs::is_directory(dir, ec)) {
		spdlog::warn("[{}] Data directory '{}' not found", log_tag, dir_path);
		return 0;
	}

	std::vector<fs::path> files;
	for (fs::directory_iterator it{dir, ec}, end; it != end; it.increment(ec)) {
		if (ec) {
			spdlog::warn("[{}] Error scanning '{}': {}", log_tag, dir_path, ec.message());
			break;
		}
		if (it->is_regular_file(ec) && it->path().extension() == ".json") {
			files.push_back(it->path());
		}
	}
	std::ranges::sort(files);

	int loaded = 0;
	for (const auto& file : files) {
		const auto doc_opt = LoadJsonFile(file.string(), log_tag);
		if (!doc_opt) {
			continue;
		}
		if (on_file(file.stem().string(), *doc_opt)) {
			++loaded;
		}
	}
	return loaded;
}

// --- Shared JSON field parsers -----------------------------------------------------------------
// One home for turning JSON arrays into the engine's math/color/rect types, so data loaders don't
// each reinvent them. Each returns `def` when the key is absent. A key that is present but not an
// array of at least N numbers also returns `def`, and logs a warning naming the key, so a typo in
// data shows up in the log instead of silently becoming the default.

namespace detail {

// The array under `key` when it holds at least `count` numbers; nullptr (with a warning) when the
// key is present but malformed, and nullptr silently when it is absent.
[[nodiscard]] inline const nlohmann::json* NumberArray(const nlohmann::json& j, const char* key, const size_t count) {
	if (!j.contains(key)) {
		return nullptr;
	}
	const auto& a = j.at(key);
	const bool numbers =
		a.is_array()
		&& a.size() >= count
		&& std::all_of(a.begin(), a.begin() + static_cast<std::ptrdiff_t>(count), [](const nlohmann::json& v) {
			   return v.is_number();
		   });
	if (!numbers) {
		spdlog::warn("[Json] '{}' must be an array of {} numbers, got {}; using the default", key, count, a.dump());
		return nullptr;
	}
	return &a;
}

} // namespace detail

[[nodiscard]] inline glm::vec2 JVec2(const nlohmann::json& j, const char* key, const glm::vec2 def) {
	const auto* a = detail::NumberArray(j, key, 2);
	if (a == nullptr) {
		return def;
	}
	return {(*a)[0].get<float>(), (*a)[1].get<float>()};
}

[[nodiscard]] inline glm::vec3 JVec3(const nlohmann::json& j, const char* key, const glm::vec3 def) {
	const auto* a = detail::NumberArray(j, key, 3);
	if (a == nullptr) {
		return def;
	}
	return {(*a)[0].get<float>(), (*a)[1].get<float>(), (*a)[2].get<float>()};
}

// Parse a JSON [r, g, b, a] array (0-255) into a platform::Rgba.
[[nodiscard]] inline Rgba JRgba(const nlohmann::json& j, const char* key, const Rgba def) {
	const auto* a = detail::NumberArray(j, key, 4);
	if (a == nullptr) {
		return def;
	}
	return {
		.r = static_cast<std::uint8_t>((*a)[0].get<int>()),
		.g = static_cast<std::uint8_t>((*a)[1].get<int>()),
		.b = static_cast<std::uint8_t>((*a)[2].get<int>()),
		.a = static_cast<std::uint8_t>((*a)[3].get<int>())
	};
}

// Parse a JSON [r, g, b, a] array (0-255) into a normalized 0-1 glm::vec4 (tint/vertex-color form).
[[nodiscard]] inline glm::vec4 JColorNormalized(const nlohmann::json& j, const char* key, const glm::vec4 def) {
	const auto* a = detail::NumberArray(j, key, 4);
	if (a == nullptr) {
		return def;
	}
	return {
		(*a)[0].get<float>() / 255.0F,
		(*a)[1].get<float>() / 255.0F,
		(*a)[2].get<float>() / 255.0F,
		(*a)[3].get<float>() / 255.0F
	};
}

// Parse a JSON [x, y, w, h] pixel-rect array into a platform::Rect.
[[nodiscard]] inline Rect JRect(const nlohmann::json& j, const char* key, const Rect def = {}) {
	const auto* a = detail::NumberArray(j, key, 4);
	if (a == nullptr) {
		return def;
	}
	return {.x = (*a)[0].get<float>(), .y = (*a)[1].get<float>(), .w = (*a)[2].get<float>(), .h = (*a)[3].get<float>()};
}

} // namespace engine::core
