#pragma once

#include <expected>
#include <string>

namespace signal_room {

// One Shift file: a night in the bunker played in a named Layout. Later specs add the in-service
// Stations, Roster, Counter Items, Tickets and Modifiers to the same document.
struct Shift {
	std::string id;
	std::string name;
	std::string layout; // Layout file, asset-relative
};

// Parse a Shift file. The error names the file and what was wrong with it.
[[nodiscard]] std::expected<Shift, std::string> LoadShiftFile(const std::string& path);

} // namespace signal_room
