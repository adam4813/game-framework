#pragma once

#include <expected>
#include <string>
#include <vector>

namespace signal_room {

// One Operator a Shift puts on the floor: the Layout spawn marker it starts on and its identity
// colour's palette token.
struct RosterEntry {
	std::string spawn;
	std::string colour;
};

// An Item a Shift starts on a Counter: the Counter's placement name and the Item catalogue id.
struct CounterItem {
	std::string counter;
	std::string item;
};

// One Shift file: a night in the bunker played in a named Layout, with the Layout's Stations it
// puts in service (every other Station stands Tarped), its Roster, in order, and the Items its
// Counters start with. Later specs add Tickets and Modifiers to the same document.
struct Shift {
	std::string id;
	std::string name;
	std::string layout;                 // Layout file, asset-relative
	std::vector<std::string> inService; // Station placement names
	std::vector<RosterEntry> roster;
	std::vector<CounterItem> counterItems;
};

// Parse a Shift file. The error names the file and what was wrong with it.
[[nodiscard]] std::expected<Shift, std::string> LoadShiftFile(const std::string& path);

} // namespace signal_room
