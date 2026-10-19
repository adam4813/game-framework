#pragma once

#include <expected>
#include <string>
#include <utility>
#include <vector>

#include "modifiers.hpp"
#include "ticket_rules.hpp"

namespace signal_room {

// One Operator a Shift puts on the floor: the Layout spawn marker it starts on and its identity
// colour's palette token.
struct RosterEntry {
	std::string spawn;
	std::string colour;
};

// An Item a Shift starts on a Counter: the Counter's placement name, the Item catalogue id and the
// frequency label it carries ("" for none).
struct CounterItem {
	std::string counter;
	std::string item;
	std::string frequency;
};

// A Ticket the schedule brings at a fixed time. `at` is in Window seconds from the start; `overfill` is a
// fraction of the tier's On Air time, as for generated Tickets.
struct PinnedTicket {
	float at{0.0F};
	std::string tier;
	std::string frequency;
	float overfill{0.0F};
	bool unlosable{false};
};

// A Shift's Collection Schedule: a seeded generator plus pinned entries. The weights are (id, weight)
// pairs; without tier weights the generator is off and only pinned entries arrive.
struct Schedule {
	float intervalMin{25.0F}; // s between generated arrivals
	float intervalMax{35.0F};
	std::vector<std::pair<std::string, float>> tierWeights;
	std::vector<std::pair<std::string, float>> frequencyWeights;
	float minRepeatGap{0.0F};         // s before the same frequency may arrive again
	std::vector<PinnedTicket> pinned; // sorted by `at`
};

// One Shift file: a night in the bunker played in a named Layout, with the Layout's Stations it
// puts in service (every other Station stands Tarped), its Roster, in order, and the Items its
// Counters start with, the frequency of each Radio, the Window length and the Collection Schedule.
// Later specs add Modifiers to the same document.
struct Shift {
	std::string id;
	std::string name;
	std::string layout;                 // Layout file, asset-relative
	std::vector<std::string> inService; // Station placement names
	std::vector<RosterEntry> roster;
	std::vector<CounterItem> counterItems;
	std::vector<std::pair<std::string, std::string>> radios; // Radio placement name -> frequency label
	float window{240.0F};                                    // s
	Schedule schedule;
	std::vector<float> starFractions; // empty: the Ticket rules' default
	std::vector<float> starAbsolute;  // [1, 2, 3 stars] scores, empty: use fractions of par
};

// Parse a Shift file. The error names the file and what was wrong with it.
[[nodiscard]] std::expected<Shift, std::string> LoadShiftFile(const std::string& path, const ActiveModifiers& modifiers = {});

// Check a Shift's tiers and frequencies against the rules and the frequency table, and that every frequency
// the schedule uses is on an in-service Radio. The error names the file and the entry.
[[nodiscard]] std::expected<void, std::string> ValidateShiftTickets(
	const Shift& shift,
	const TicketRules& rules,
	const FrequencyTable& frequencies,
	const std::string& path
);

} // namespace signal_room
