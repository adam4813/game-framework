#include "palette.hpp"

#include <algorithm>
#include <format>

#include <nlohmann/json.hpp>

#include "json_file.hpp"

using namespace engine;

namespace signal_room {

std::expected<Palette, std::string> LoadPaletteFile(const std::string& path) {
	const auto doc = ReadJsonFile(path, "Palette");
	if (!doc) {
		return std::unexpected(doc.error());
	}
	if (!doc->contains("colours") || !doc->at("colours").is_object()) {
		return std::unexpected(std::format("Palette file {} has no \"colours\" object", path));
	}

	Palette palette;
	for (const auto& [token, value] : doc->at("colours").items()) {
		const bool rgb = value.is_array() && (value.size() == 3 || value.size() == 4);
		if (!rgb || !std::ranges::all_of(value, [](const nlohmann::json& c) { return c.is_number_integer(); })) {
			return std::unexpected(
				std::format("Palette file {}: colour '{}' must be [r, g, b] or [r, g, b, a] (0-255)", path, token)
			);
		}
		palette.Add(
			token,
			{.r = value[0].get<std::uint8_t>(),
			 .g = value[1].get<std::uint8_t>(),
			 .b = value[2].get<std::uint8_t>(),
			 .a = value.size() == 4 ? value[3].get<std::uint8_t>() : std::uint8_t{255}}
		);
	}
	return palette;
}

} // namespace signal_room
