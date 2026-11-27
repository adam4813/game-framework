#pragma once

#include <expected>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace signal_room {

// Read and parse a game data file. The error starts with `fileKind` ("Shift", "Palette", ...) and
// names the path: "Shift file not found: <path>" or "Shift file <path> is not valid JSON: <why>".
[[nodiscard]] std::expected<nlohmann::json, std::string>
ReadJsonFile(const std::string& path, std::string_view fileKind);

} // namespace signal_room
