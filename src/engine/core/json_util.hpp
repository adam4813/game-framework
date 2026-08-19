#pragma once

#include <fstream>
#include <optional>
#include <string>
#include <string_view>

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
