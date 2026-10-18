#include "shift.hpp"

#include <format>
#include <fstream>

#include <nlohmann/json.hpp>

namespace signal_room {

std::expected<Shift, std::string> LoadShiftFile(const std::string& path) {
	std::ifstream file{path};
	if (!file) {
		return std::unexpected(std::format("Shift file not found: {}", path));
	}

	nlohmann::json doc;
	try {
		file >> doc;
	}
	catch (const std::exception& ex) {
		return std::unexpected(std::format("Shift file {} is not valid JSON: {}", path, ex.what()));
	}

	Shift shift{
		.id = doc.value("id", std::string{}),
		.name = doc.value("name", std::string{}),
		.layout = doc.value("layout", std::string{}),
	};
	if (shift.layout.empty()) {
		return std::unexpected(std::format("Shift file {} names no layout", path));
	}
	return shift;
}

} // namespace signal_room
