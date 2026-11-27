#include "station_catalogue.hpp"

#include <algorithm>
#include <format>

#include <nlohmann/json.hpp>

#include "item_catalogue.hpp"
#include "json_file.hpp"

using nlohmann::json;

namespace signal_room {

namespace {

// Verbs cross the data boundary by name.
std::expected<Verb, std::string> ParseVerb(const std::string& name) {
	if (name == "instant") {
		return Verb::Instant;
	}
	if (name == "tap") {
		return Verb::Tap;
	}
	if (name == "hold") {
		return Verb::Hold;
	}
	if (name == "walk_away") {
		return Verb::WalkAway;
	}
	return std::unexpected(std::format("uses unknown verb '{}'", name));
}

// A list of known Item ids under `key`.
std::expected<std::vector<std::string>, std::string>
ReadItemIds(const json& entry, const char* key, const ItemCatalogue& items) {
	std::vector<std::string> ids;
	for (const auto& value : entry.value(key, json::array())) {
		if (!value.is_string()) {
			return std::unexpected(std::format("{} must be a list of Item ids", key));
		}
		auto id = value.get<std::string>();
		if (!items.Contains(id)) {
			return std::unexpected(std::format("{} lists unknown Item '{}'", key, id));
		}
		ids.push_back(std::move(id));
	}
	return ids;
}

// A positive number under `key`, or `fallback` when it is absent.
std::expected<float, std::string> ReadPositive(const json& entry, const char* key, const float fallback) {
	if (!entry.contains(key)) {
		return fallback;
	}
	const auto& value = entry.at(key);
	if (!value.is_number() || value.get<float>() <= 0.0F) {
		return std::unexpected(std::format("{} must be a positive number", key));
	}
	return value.get<float>();
}

std::expected<StationTransform, std::string> ReadTransform(const json& entry, const ItemCatalogue& items) {
	if (!entry.is_object()) {
		return std::unexpected("must be an object");
	}
	StationTransform transform{.id = entry.value("id", std::string{})};
	if (transform.id.empty()) {
		return std::unexpected("needs an id");
	}
	auto inputs = ReadItemIds(entry, "inputs", items);
	if (!inputs) {
		return std::unexpected(inputs.error());
	}
	auto outputs = ReadItemIds(entry, "outputs", items);
	if (!outputs) {
		return std::unexpected(outputs.error());
	}
	transform.inputs = std::move(*inputs);
	transform.outputs = std::move(*outputs);

	const auto verb = ParseVerb(entry.value("verb", std::string{}));
	if (!verb) {
		return std::unexpected(verb.error());
	}
	transform.verb = *verb;

	const auto duration = ReadPositive(entry, "duration", 0.0F);
	const auto crank = ReadPositive(entry, "crank_duration", 0.0F);
	const auto taps = ReadPositive(entry, "taps", 1.0F);
	for (const auto& number : {duration, crank, taps}) {
		if (!number) {
			return std::unexpected(number.error());
		}
	}
	transform.duration = *duration;
	transform.crankDuration = *crank;
	transform.taps = static_cast<int>(*taps);
	const bool timed = transform.verb == Verb::Hold || transform.verb == Verb::WalkAway;
	if (timed && transform.duration <= 0.0F) {
		return std::unexpected("needs a positive duration");
	}
	if (transform.crankDuration > 0.0F && transform.verb != Verb::WalkAway) {
		return std::unexpected("has crank_duration, which only walk_away uses");
	}

	transform.transmit = entry.value("transmit", false);
	if (transform.transmit && !transform.outputs.empty()) {
		return std::unexpected("transmits, so it cannot have outputs");
	}
	return transform;
}

} // namespace

std::expected<StationCatalogue, std::string>
LoadStationCatalogueFile(const std::string& path, const ItemCatalogue& items, const ActiveModifiers& modifiers) {
	const auto doc = ReadGameDocument(path, "Station catalogue", "stations", modifiers);
	if (!doc) {
		return std::unexpected(doc.error());
	}

	StationCatalogue catalogue;
	for (const auto& [kindId, kindEntry] : doc->value("stations", json::object()).items()) {
		StationKind kind{.displayName = kindEntry.value("display_name", kindId)};
		for (const auto& entry : kindEntry.value("transforms", json::array())) {
			auto transform = ReadTransform(entry, items);
			if (!transform) {
				return std::unexpected(
					std::format(
						"Station catalogue file {}: Station '{}' Transform '{}' {}",
						path,
						kindId,
						entry.is_object() ? entry.value("id", std::string{}) : std::string{},
						transform.error()
					)
				);
			}
			// Two Transforms of one kind may not take the same Items, whatever order they are placed in.
			auto sortedInputs = transform->inputs;
			std::ranges::sort(sortedInputs);
			const bool duplicate = std::ranges::any_of(kind.transforms, [&](const StationTransform& other) {
				auto otherInputs = other.inputs;
				std::ranges::sort(otherInputs);
				return otherInputs == sortedInputs;
			});
			if (duplicate) {
				return std::unexpected(
					std::format(
						"Station catalogue file {}: Station '{}' Transform '{}' shares its input set with another "
						"Transform of the kind",
						path,
						kindId,
						transform->id
					)
				);
			}
			kind.transforms.push_back(std::move(*transform));
		}
		catalogue.Add(kindId, std::move(kind));
	}
	if (catalogue.Empty()) {
		return std::unexpected(std::format("Station catalogue file {} has no \"stations\"", path));
	}
	return catalogue;
}

} // namespace signal_room
