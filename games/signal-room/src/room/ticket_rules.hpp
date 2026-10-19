#pragma once

#include <expected>
#include <string>
#include <vector>

#include "engine/core/core.hpp"
#include "modifiers.hpp"
#include "palette.hpp"

namespace signal_room {

// One Ticket tier (a data row, so new tiers need no C++). The overfill range is a fraction of
// `onAirTime` a Ticket's gauge starts above 100%.
struct TierRule {
	std::string label;
	float onAirTime{90.0F}; // s
	int base{20};           // score for a Clear before the band share and chain step
	float overfillMin{0.0F};
	float overfillMax{0.0F};
};

// A gauge band: the first one whose `above` the On Air fraction exceeds; a Clear scores `share` of base.
struct GaugeBand {
	std::string id;
	float above{0.0F};
	float share{1.0F};
};

// World singleton loaded from the ticket rules file. Live tuning edits tier times in memory.
struct TicketRules {
	engine::core::Registry<TierRule> tiers;
	std::vector<GaugeBand> bands;
	std::vector<float> handlerChain;
	int lostInterceptPenalty{10};
	int rackCap{5};
	float lastCall{20.0F};           // s of Window left at which new On Air Tickets stop
	float emptyRackWaitFactor{0.5F}; // the wait to the next arrival drains at 1 / this when the rack is empty
	std::vector<float> starFractions;
	std::vector<std::string> callsigns;
};

// Parse the ticket rules file. The error names the file and the bad entry.
[[nodiscard]] std::expected<TicketRules, std::string> LoadTicketRulesFile(const std::string& path, const ActiveModifiers& modifiers = {});

// One frequency: its label ("7.0") and the palette token of its colour.
struct Frequency {
	std::string label;
	std::string colour;
};

// World singleton: the frequencies, ordered low to high.
struct FrequencyTable {
	std::vector<Frequency> entries;

	[[nodiscard]] const Frequency* Find(const std::string& label) const;
};

// Parse the frequency table file, checking every colour token against `palette`.
[[nodiscard]] std::expected<FrequencyTable, std::string>
LoadFrequencyTableFile(const std::string& path, const Palette& palette);

} // namespace signal_room
