#include "shift.hpp"

#include <algorithm>
#include <format>

#include <nlohmann/json.hpp>

#include "json_file.hpp"

namespace signal_room {

namespace {

using Weights = std::vector<std::pair<std::string, float>>;

std::expected<Weights, std::string>
ReadWeights(const nlohmann::json& schedule, const char* key, const std::string& path) {
	Weights weights;
	const auto object = schedule.value(key, nlohmann::json::object());
	if (!object.is_object()) {
		return std::unexpected(std::format("Shift file {}: schedule.{} must be {{\"<id>\": weight}}", path, key));
	}
	for (const auto& [id, weight] : object.items()) {
		if (!weight.is_number() || weight.get<float>() < 0.0F) {
			return std::unexpected(std::format("Shift file {}: schedule.{}.{} must be a number >= 0", path, key, id));
		}
		weights.emplace_back(id, weight.get<float>());
	}
	return weights;
}

std::expected<Schedule, std::string> ReadSchedule(const nlohmann::json& schedule, const std::string& path) {
	Schedule result;
	if (!schedule.is_object()) {
		return std::unexpected(std::format("Shift file {}: schedule must be an object", path));
	}
	const auto interval = schedule.value("interval", nlohmann::json::array({result.intervalMin, result.intervalMax}));
	if (!interval.is_array()
		|| interval.size() != 2
		|| !interval[0].is_number()
		|| !interval[1].is_number()
		|| interval[0].get<float>() <= 0.0F
		|| interval[1].get<float>() < interval[0].get<float>()) {
		return std::unexpected(
			std::format("Shift file {}: schedule.interval must be [min, max] with 0 < min <= max", path)
		);
	}
	result.intervalMin = interval[0].get<float>();
	result.intervalMax = interval[1].get<float>();
	result.minRepeatGap = schedule.value("min_repeat_gap", 0.0F);

	auto tiers = ReadWeights(schedule, "tier_weights", path);
	if (!tiers) {
		return std::unexpected(tiers.error());
	}
	auto frequencies = ReadWeights(schedule, "frequency_weights", path);
	if (!frequencies) {
		return std::unexpected(frequencies.error());
	}
	result.tierWeights = std::move(*tiers);
	result.frequencyWeights = std::move(*frequencies);

	int index = 0;
	for (const auto& entry : schedule.value("pinned", nlohmann::json::array())) {
		const bool valid = entry.is_object()
						   && entry.value("at", nlohmann::json{}).is_number()
						   && entry.value("tier", nlohmann::json{}).is_string()
						   && entry.value("frequency", nlohmann::json{}).is_string()
						   && entry.value("overfill", 0.0F) >= 0.0F
						   && entry.value("at", 0.0F) >= 0.0F;
		if (!valid) {
			return std::unexpected(
				std::format(
					"Shift file {}: schedule.pinned[{}] must be {{\"at\": s, \"tier\": id, \"frequency\": label, "
					"\"overfill\": fraction, \"unlosable\": bool}}",
					path,
					index
				)
			);
		}
		result.pinned.push_back(
			{.at = entry.at("at"),
			 .tier = entry.at("tier"),
			 .frequency = entry.at("frequency"),
			 .overfill = entry.value("overfill", 0.0F),
			 .unlosable = entry.value("unlosable", false)}
		);
		++index;
	}
	std::ranges::stable_sort(result.pinned, {}, &PinnedTicket::at);
	return result;
}

} // namespace

std::expected<Shift, std::string> LoadShiftFile(const std::string& path, const ActiveModifiers& modifiers) {
	const auto doc = ReadGameDocument(path, "Shift", "shift", modifiers);
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

	for (const auto& entry : doc->value("roster", nlohmann::json::array())) {
		if (!entry.is_object()
			|| !entry.value("spawn", nlohmann::json{}).is_string()
			|| !entry.value("colour", nlohmann::json{}).is_string()) {
			return std::unexpected(
				std::format("Shift file {}: roster entries must be {{\"spawn\": name, \"colour\": token}}", path)
			);
		}
		shift.roster.push_back({.spawn = entry.at("spawn"), .colour = entry.at("colour")});
	}

	const auto counterItems = doc->value("counter_items", nlohmann::json::object());
	if (!counterItems.is_object()) {
		return std::unexpected(
			std::format(
				"Shift file {}: counter_items must be {{\"<Counter name>\": \"<Item id>\" or {{\"item\": id, "
				"\"frequency\": label}}}}",
				path
			)
		);
	}
	for (const auto& [counter, entry] : counterItems.items()) {
		const bool plain = entry.is_string();
		const bool striped = entry.is_object() && entry.value("item", nlohmann::json{}).is_string();
		if (!plain && !striped) {
			return std::unexpected(
				std::format(
					"Shift file {}: counter_items.{} must be an Item id or {{\"item\", \"frequency\"}}",
					path,
					counter
				)
			);
		}
		shift.counterItems.push_back(
			plain ? CounterItem{.counter = counter, .item = entry.get<std::string>()}
				  : CounterItem{
						.counter = counter,
						.item = entry.at("item").get<std::string>(),
						.frequency = entry.value("frequency", std::string{})
					}
		);
	}

	const auto radios = doc->value("radios", nlohmann::json::object());
	if (!radios.is_object()) {
		return std::unexpected(
			std::format("Shift file {}: radios must be {{\"<Radio name>\": \"<frequency label>\"}}", path)
		);
	}
	for (const auto& [radio, frequency] : radios.items()) {
		if (!frequency.is_string()) {
			return std::unexpected(std::format("Shift file {}: radios.{} must be a frequency label", path, radio));
		}
		shift.radios.emplace_back(radio, frequency.get<std::string>());
	}

	shift.window = doc->value("window", shift.window);
	if (shift.window <= 0.0F) {
		return std::unexpected(std::format("Shift file {}: window must be a positive number of seconds", path));
	}
	if (doc->contains("schedule")) {
		auto schedule = ReadSchedule(doc->at("schedule"), path);
		if (!schedule) {
			return std::unexpected(schedule.error());
		}
		shift.schedule = std::move(*schedule);
	}
	if (doc->contains("stars")) {
		const auto& stars = doc->at("stars");
		const auto read = [&](const char* key, std::vector<float>& out) {
			const auto list = stars.is_object() ? stars.value(key, nlohmann::json::array()) : nlohmann::json{};
			if (!list.is_array() || (!list.empty() && list.size() != 3)) {
				return false;
			}
			return std::ranges::all_of(list, [&](const auto& n) {
				if (!n.is_number()) {
					return false;
				}
				out.push_back(n.template get<float>());
				return true;
			});
		};
		if (!read("fractions", shift.starFractions) || !read("absolute", shift.starAbsolute)) {
			return std::unexpected(
				std::format("Shift file {}: stars must be {{\"fractions\": [3 numbers]}} or {{\"absolute\": [3 numbers]}}", path)
			);
		}
		if (!shift.starFractions.empty() && !shift.starAbsolute.empty()) {
			return std::unexpected(std::format("Shift file {}: stars gives both fractions and absolute", path));
		}
	}
	return shift;
}

std::expected<void, std::string> ValidateShiftTickets(
	const Shift& shift,
	const TicketRules& rules,
	const FrequencyTable& frequencies,
	const std::string& path
) {
	for (const auto& [radio, label] : shift.radios) {
		if (!frequencies.Find(label)) {
			return std::unexpected(
				std::format("Shift file {}: radios.{} names unknown frequency '{}'", path, radio, label)
			);
		}
	}
	// Frequencies on an in-service Radio. A Radio missing from in_service may still carry one (it's Tarped).
	const auto isLive = [&](const std::string& label) {
		return std::ranges::any_of(shift.radios, [&](const auto& radio) {
			return radio.second == label && std::ranges::contains(shift.inService, radio.first);
		});
	};
	const auto& schedule = shift.schedule;
	for (const auto& [tier, weight] : schedule.tierWeights) {
		if (!rules.tiers.Contains(tier)) {
			return std::unexpected(
				std::format("Shift file {}: schedule.tier_weights names unknown tier '{}'", path, tier)
			);
		}
	}
	for (const auto& [label, weight] : schedule.frequencyWeights) {
		if (!frequencies.Find(label)) {
			return std::unexpected(
				std::format("Shift file {}: schedule.frequency_weights names unknown frequency '{}'", path, label)
			);
		}
		if (!isLive(label)) {
			return std::unexpected(
				std::format(
					"Shift file {}: schedule.frequency_weights frequency '{}' is on no in-service Radio",
					path,
					label
				)
			);
		}
	}
	if (!schedule.tierWeights.empty() && schedule.frequencyWeights.empty()) {
		return std::unexpected(std::format("Shift file {}: schedule has tier_weights but no frequency_weights", path));
	}
	for (size_t i = 0; i < schedule.pinned.size(); ++i) {
		const auto& pinned = schedule.pinned[i];
		if (!rules.tiers.Contains(pinned.tier)) {
			return std::unexpected(
				std::format(
					"Shift file {}: schedule.pinned entry at {} s names unknown tier '{}'",
					path,
					pinned.at,
					pinned.tier
				)
			);
		}
		if (!frequencies.Find(pinned.frequency)) {
			return std::unexpected(
				std::format(
					"Shift file {}: schedule.pinned entry at {} s names unknown frequency '{}'",
					path,
					pinned.at,
					pinned.frequency
				)
			);
		}
		if (!isLive(pinned.frequency)) {
			return std::unexpected(
				std::format(
					"Shift file {}: schedule.pinned entry at {} s: frequency '{}' is on no in-service Radio",
					path,
					pinned.at,
					pinned.frequency
				)
			);
		}
	}
	return {};
}

} // namespace signal_room
