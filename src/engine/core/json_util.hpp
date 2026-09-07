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

#include "engine/platform/platform.hpp"

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
	std::sort(files.begin(), files.end());

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
// each reinvent them. Each returns `def` when the key is absent or the array is the wrong shape.

[[nodiscard]] inline glm::vec2 JVec2(const nlohmann::json& j, const char* key, const glm::vec2 def) {
	if (!j.contains(key)) {
		return def;
	}
	const auto& a = j.at(key);
	if (!a.is_array() || a.size() < 2) {
		return def;
	}
	return {a[0].get<float>(), a[1].get<float>()};
}

[[nodiscard]] inline glm::vec3 JVec3(const nlohmann::json& j, const char* key, const glm::vec3 def) {
	if (!j.contains(key)) {
		return def;
	}
	const auto& a = j.at(key);
	if (!a.is_array() || a.size() < 3) {
		return def;
	}
	return {a[0].get<float>(), a[1].get<float>(), a[2].get<float>()};
}

// Parse a JSON [r, g, b, a] array (0-255) into a platform::Rgba.
[[nodiscard]] inline platform::Rgba JRgba(const nlohmann::json& j, const char* key, const platform::Rgba def) {
	if (!j.contains(key)) {
		return def;
	}
	const auto& a = j.at(key);
	if (!a.is_array() || a.size() < 4) {
		return def;
	}
	return {
		static_cast<std::uint8_t>(a[0].get<int>()),
		static_cast<std::uint8_t>(a[1].get<int>()),
		static_cast<std::uint8_t>(a[2].get<int>()),
		static_cast<std::uint8_t>(a[3].get<int>())
	};
}

// Parse a JSON [r, g, b, a] array (0-255) into a normalized 0-1 glm::vec4 (tint/vertex-color form).
[[nodiscard]] inline glm::vec4 JColorNormalized(const nlohmann::json& j, const char* key, const glm::vec4 def) {
	if (!j.contains(key)) {
		return def;
	}
	const auto& a = j.at(key);
	if (!a.is_array() || a.size() < 4) {
		return def;
	}
	return {
		a[0].get<float>() / 255.0F,
		a[1].get<float>() / 255.0F,
		a[2].get<float>() / 255.0F,
		a[3].get<float>() / 255.0F
	};
}

// Parse a JSON [x, y, w, h] pixel-rect array into a platform::Rect.
[[nodiscard]] inline platform::Rect JRect(const nlohmann::json& j, const char* key, const platform::Rect def = {}) {
	if (!j.contains(key)) {
		return def;
	}
	const auto& a = j.at(key);
	if (!a.is_array() || a.size() < 4) {
		return def;
	}
	return {a[0].get<float>(), a[1].get<float>(), a[2].get<float>(), a[3].get<float>()};
}

} // namespace engine::core
