#include "json_file.hpp"

#include <format>
#include <fstream>

namespace signal_room {

std::expected<nlohmann::json, std::string> ReadJsonFile(const std::string& path, const std::string_view fileKind) {
	std::ifstream file{path};
	if (!file) {
		return std::unexpected(std::format("{} file not found: {}", fileKind, path));
	}

	nlohmann::json doc;
	try {
		file >> doc;
	}
	catch (const std::exception& ex) {
		return std::unexpected(std::format("{} file {} is not valid JSON: {}", fileKind, path, ex.what()));
	}
	return doc;
}

} // namespace signal_room
