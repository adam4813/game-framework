#include "ticket_rules.hpp"

#include <algorithm>
#include <format>

#include <nlohmann/json.hpp>

#include "json_file.hpp"

using nlohmann::json;

namespace signal_room {

namespace {

// A list of numbers under `key`, or an error naming the file and key.
std::expected<std::vector<float>, std::string> ReadNumbers(const json& doc, const char* key, const std::string& path) {
	std::vector<float> numbers;
	for (const auto& value : doc.value(key, json::array())) {
		if (!value.is_number()) {
			return std::unexpected(std::format("Ticket rules file {}: {} must be a list of numbers", path, key));
		}
		numbers.push_back(value.get<float>());
	}
	return numbers;
}

std::expected<TierRule, std::string> ReadTier(const std::string& id, const json& entry, const std::string& path) {
	const auto fail = [&](const char* what) {
		return std::unexpected(std::format("Ticket rules file {}: tier '{}' {}", path, id, what));
	};
	if (!entry.is_object()
		|| !entry.value("on_air_time", json{}).is_number()
		|| !entry.value("base", json{}).is_number()) {
		return fail("needs numbers on_air_time and base");
	}
	const auto overfill = entry.value("overfill", json::array({0.0F, 0.0F}));
	if (!overfill.is_array() || overfill.size() != 2 || !overfill[0].is_number() || !overfill[1].is_number()) {
		return fail("overfill must be [min, max]");
	}
	TierRule tier{
		.label = entry.value("label", id),
		.onAirTime = entry.at("on_air_time").get<float>(),
		.base = entry.at("base").get<int>(),
		.overfillMin = overfill[0].get<float>(),
		.overfillMax = overfill[1].get<float>(),
	};
	if (tier.onAirTime <= 0.0F || tier.overfillMin < 0.0F || tier.overfillMax < tier.overfillMin) {
		return fail("needs a positive on_air_time and 0 <= overfill min <= max");
	}
	return tier;
}

} // namespace

std::expected<TicketRules, std::string> LoadTicketRulesFile(const std::string& path, const ActiveModifiers& modifiers) {
	const auto doc = ReadGameDocument(path, "Ticket rules", "tickets", modifiers);
	if (!doc) {
		return std::unexpected(doc.error());
	}

	TicketRules rules;
	const auto tiers = doc->value("tiers", json::object());
	if (!tiers.is_object() || tiers.empty()) {
		return std::unexpected(std::format("Ticket rules file {} has no \"tiers\" object", path));
	}
	for (const auto& [id, entry] : tiers.items()) {
		auto tier = ReadTier(id, entry, path);
		if (!tier) {
			return std::unexpected(tier.error());
		}
		rules.tiers.Add(id, std::move(*tier));
	}

	for (const auto& band : doc->value("bands", json::array())) {
		if (!band.is_object() || !band.value("above", json{}).is_number() || !band.value("share", json{}).is_number()) {
			return std::unexpected(std::format("Ticket rules file {}: bands need numbers 'above' and 'share'", path));
		}
		rules.bands.push_back(
			{.id = band.value("id", std::string{}), .above = band.at("above"), .share = band.at("share")}
		);
	}

	auto chain = ReadNumbers(*doc, "handler_chain", path);
	if (!chain) {
		return std::unexpected(chain.error());
	}
	auto stars = ReadNumbers(*doc, "star_fractions", path);
	if (!stars) {
		return std::unexpected(stars.error());
	}
	rules.handlerChain = std::move(*chain);
	rules.starFractions = std::move(*stars);

	rules.lostInterceptPenalty = doc->value("lost_intercept_penalty", rules.lostInterceptPenalty);
	rules.rackCap = doc->value("rack_cap", rules.rackCap);
	rules.lastCall = doc->value("last_call", rules.lastCall);
	rules.emptyRackWaitFactor = doc->value("empty_rack_wait_factor", rules.emptyRackWaitFactor);
	if (rules.rackCap < 1 || rules.lastCall < 0.0F || rules.emptyRackWaitFactor <= 0.0F) {
		return std::unexpected(
			std::format(
				"Ticket rules file {}: rack_cap must be >= 1, last_call >= 0 and empty_rack_wait_factor > 0",
				path
			)
		);
	}

	for (const auto& callsign : doc->value("callsigns", json::array())) {
		if (!callsign.is_string()) {
			return std::unexpected(std::format("Ticket rules file {}: callsigns must be strings", path));
		}
		rules.callsigns.push_back(callsign.get<std::string>());
	}
	if (rules.callsigns.empty()) {
		return std::unexpected(std::format("Ticket rules file {} has no \"callsigns\"", path));
	}
	return rules;
}

const Frequency* FrequencyTable::Find(const std::string& label) const {
	const auto found = std::ranges::find(entries, label, &Frequency::label);
	return found == entries.end() ? nullptr : &*found;
}

std::expected<FrequencyTable, std::string> LoadFrequencyTableFile(const std::string& path, const Palette& palette) {
	const auto doc = ReadJsonFile(path, "Frequency table");
	if (!doc) {
		return std::unexpected(doc.error());
	}
	FrequencyTable table;
	for (const auto& entry : doc->value("frequencies", json::array())) {
		if (!entry.is_object()
			|| !entry.value("label", json{}).is_string()
			|| !entry.value("colour", json{}).is_string()) {
			return std::unexpected(
				std::format("Frequency table {}: entries must be {{\"label\": text, \"colour\": token}}", path)
			);
		}
		Frequency frequency{.label = entry.at("label"), .colour = entry.at("colour")};
		if (!palette.Contains(frequency.colour)) {
			return std::unexpected(
				std::format(
					"Frequency table {}: frequency '{}' names unknown colour '{}'",
					path,
					frequency.label,
					frequency.colour
				)
			);
		}
		if (table.Find(frequency.label) != nullptr) {
			return std::unexpected(
				std::format("Frequency table {}: frequency '{}' is listed twice", path, frequency.label)
			);
		}
		table.entries.push_back(std::move(frequency));
	}
	if (table.entries.empty()) {
		return std::unexpected(std::format("Frequency table {} has no \"frequencies\"", path));
	}
	return table;
}

} // namespace signal_room
