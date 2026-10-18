#include "shift.hpp"

#include <format>

#include <nlohmann/json.hpp>

#include "json_file.hpp"

namespace signal_room {

std::expected<Shift, std::string> LoadShiftFile(const std::string& path) {
	const auto doc = ReadJsonFile(path, "Shift");
	if (!doc) {
		return std::unexpected(doc.error());
	}

	Shift shift{
		.id = doc->value("id", std::string{}),
		.name = doc->value("name", std::string{}),
		.layout = doc->value("layout", std::string{}),
	};
	if (shift.layout.empty()) {
		return std::unexpected(std::format("Shift file {} names no layout", path));
	}

	for (const auto& name : doc->value("in_service", nlohmann::json::array())) {
		if (!name.is_string()) {
			return std::unexpected(std::format("Shift file {}: in_service entries must be Station names", path));
		}
		shift.inService.push_back(name.get<std::string>());
	}
	return shift;
}

} // namespace signal_room
