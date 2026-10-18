#include "greybox_catalogue.hpp"

#include <algorithm>
#include <format>
#include <string_view>

#include <nlohmann/json.hpp>

#include "json_file.hpp"

using namespace engine;
using nlohmann::json;

namespace signal_room {

namespace {

// Load errors carry the context the parse is in ("stations.radio part 3") and are prefixed with the
// file path once at the top.
using Result = std::expected<void, std::string>;

std::expected<PartShape, std::string> ParseShape(const std::string& shape) {
	if (shape == "cube") {
		return PartShape::Cube;
	}
	if (shape == "sphere") {
		return PartShape::Sphere;
	}
	if (shape == "capsule") {
		return PartShape::Capsule;
	}
	return std::unexpected(std::format("unknown shape '{}' (cube, sphere, capsule)", shape));
}

// An optional [x, y, z] field: `def` when absent, an error when present but not three numbers (the
// engine's JVec3 would silently fall back to the default).
std::expected<glm::vec3, std::string>
ReadVec3(const json& j, const char* key, const glm::vec3 def, const std::string_view context) {
	if (!j.contains(key)) {
		return def;
	}
	const auto& a = j.at(key);
	if (!a.is_array() || a.size() != 3 || !std::ranges::all_of(a, [](const json& v) { return v.is_number(); })) {
		return std::unexpected(std::format("{}: {} must be [x, y, z]", context, key));
	}
	return glm::vec3{a[0].get<float>(), a[1].get<float>(), a[2].get<float>()};
}

// The palette colour whose token is `j[key]`.
std::expected<core::Rgba, std::string>
ResolveColour(const Palette& palette, const json& j, const std::string_view context, const char* key = "colour") {
	const auto token = j.value(key, std::string{});
	const auto* colour = palette.Get(token);
	if (colour == nullptr) {
		return std::unexpected(std::format("{} uses unknown palette colour '{}'", context, token));
	}
	return *colour;
}

std::expected<GreyboxModel, std::string> ParseModel(const Palette& palette, const json& j, const std::string& context) {
	GreyboxModel model{
		.fitFootprint = j.value("fit", std::string{}) == "footprint",
		.height = j.value("height", GreyboxModel{}.height),
	};
	const auto parts = j.value("parts", json::array());
	for (size_t i = 0; i < parts.size(); ++i) {
		const auto& p = parts[i];
		const auto partContext = std::format("{} part {}", context, i);
		const auto shape = ParseShape(p.value("shape", std::string{"cube"}));
		if (!shape) {
			return std::unexpected(std::format("{}: {}", partContext, shape.error()));
		}
		const auto colour = ResolveColour(palette, p, partContext);
		if (!colour) {
			return std::unexpected(colour.error());
		}
		const auto offset = ReadVec3(p, "offset", glm::vec3{0.0F}, partContext);
		const auto size = ReadVec3(p, "size", glm::vec3{1.0F}, partContext);
		const auto rotation = ReadVec3(p, "rotation", glm::vec3{0.0F}, partContext);
		for (const auto* vec : {&offset, &size, &rotation}) {
			if (!*vec) {
				return std::unexpected(vec->error());
			}
		}
		const auto parent = p.value("parent", std::string{});
		const auto earlier = [&](const GreyboxPart& part) { return part.name == parent; };
		if (!parent.empty() && std::ranges::none_of(model.parts, earlier)) {
			return std::unexpected(std::format("{}: parent '{}' is not an earlier named part", partContext, parent));
		}
		model.parts.push_back({
			.name = p.value("name", std::string{}),
			.parent = parent,
			.shape = *shape,
			.offset = *offset,
			.size = *size,
			.rotation = *rotation,
			.colour = *colour,
			.visible = p.value("visible", true),
		});
	}
	return model;
}

Result ParseModels(const Palette& palette, const json& doc, const char* section, core::Registry<GreyboxModel>& out) {
	for (const auto& [kind, value] : doc.value(section, json::object()).items()) {
		auto model = ParseModel(palette, value, std::format("{}.{}", section, kind));
		if (!model) {
			return std::unexpected(model.error());
		}
		out.Add(kind, std::move(*model));
	}
	return {};
}

Result ParseWalls(const Palette& palette, const json& doc, core::Registry<WallStyle>& out) {
	for (const auto& [heightClass, value] : doc.value("walls", json::object()).items()) {
		const auto colour = ResolveColour(palette, value, std::format("walls.{}", heightClass));
		if (!colour) {
			return std::unexpected(colour.error());
		}
		out.Add(
			heightClass,
			{.height = value.value("height", WallStyle{}.height),
			 .thickness = value.value("thickness", WallStyle{}.thickness),
			 .colour = *colour}
		);
	}
	return {};
}

Result ParseFloor(const Palette& palette, const json& doc, FloorStyle& out) {
	const auto floor = doc.value("floor", json::object());
	for (const auto& [key, colour] :
		 {std::pair{"grout", &out.grout}, std::pair{"tile", &out.tile}, std::pair{"tile_alt", &out.tileAlt}}) {
		const auto resolved = ResolveColour(palette, floor, std::format("floor.{}", key), key);
		if (!resolved) {
			return std::unexpected(resolved.error());
		}
		*colour = *resolved;
	}
	return {};
}

} // namespace

std::expected<GreyboxCatalogue, std::string> LoadGreyboxCatalogueFile(const std::string& path, const Palette& palette) {
	const auto doc = ReadJsonFile(path, "Greybox");
	if (!doc) {
		return std::unexpected(doc.error());
	}

	GreyboxCatalogue catalogue;
	auto counter = ParseModel(palette, doc->value("counter", json::object()), "counter");
	if (!counter) {
		return std::unexpected(std::format("Greybox file {}: {}", path, counter.error()));
	}
	catalogue.counter = std::move(*counter);

	for (const auto& result :
		 {ParseModels(palette, *doc, "stations", catalogue.stations),
		  ParseModels(palette, *doc, "props", catalogue.props),
		  ParseWalls(palette, *doc, catalogue.walls),
		  ParseFloor(palette, *doc, catalogue.floor)}) {
		if (!result) {
			return std::unexpected(std::format("Greybox file {}: {}", path, result.error()));
		}
	}
	if (!catalogue.props.Contains(kTarpPropKind)) {
		return std::unexpected(std::format("Greybox file {} has no props.{} entry", path, kTarpPropKind));
	}
	return catalogue;
}

} // namespace signal_room
