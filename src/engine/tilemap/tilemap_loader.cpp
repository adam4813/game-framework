#include "tilemap_loader.hpp"

#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/assets/assets.hpp"
#include "engine/core/core.hpp"
#include "tilemap_components.hpp"

namespace engine::tilemap {

namespace {

using json = nlohmann::json;

// Expand a JSON ops array into a flat list of TilemapOp
void ExpandOps(
	const json& ops,
	std::vector<TilemapOp>& out,
	const std::unordered_map<std::string, json>& brushes,
	const int ox,
	const int oz
) {
	for (const auto& op : ops) {
		const std::string kind = op.value("op", std::string{});
		try {
			if (kind == "stamp") {
				const auto brush_it = brushes.find(op.value("brush", std::string{}));
				if (brush_it == brushes.end()) {
					spdlog::warn("[Tilemap] Unknown brush '{}'", op.value("brush", std::string{}));
					continue;
				}
				const auto& at = op.at("at");
				ExpandOps(brush_it->second, out, brushes, ox + at[0].get<int>(), oz + at[1].get<int>());
				continue;
			}

			const auto& tile_val = op.at("tile");
			TilemapOp entry;

			if (tile_val.is_number_unsigned()) {
				entry.tile_id = tile_val.get<uint32_t>();
			}
			else if (tile_val.is_string()) {
				const std::string name = tile_val.get<std::string>();
				entry.tile_name = name;
			}
			else {
				spdlog::warn("[Tilemap] Unsupported tile reference type in op '{}'", kind);
				continue;
			}

			if (kind == "set") {
				const auto& at = op.at("at");
				entry.kind = TilemapOp::Kind::Set;
				entry.x = at[0].get<int>() + ox;
				entry.z = at[1].get<int>() + oz;
			}
			else if (kind == "rect") {
				const auto& r = op.at("rect");
				entry.kind = TilemapOp::Kind::FillRect;
				entry.x = r[0].get<int>() + ox;
				entry.z = r[1].get<int>() + oz;
				entry.x1 = r[2].get<int>() + ox;
				entry.z1 = r[3].get<int>() + oz;
			}
			else if (kind == "hline") {
				entry.kind = TilemapOp::Kind::HLine;
				entry.x = op.value("x0", 0) + ox;
				entry.z = op.value("y", 0) + oz;
				entry.x1 = op.value("x1", 0) + ox;
			}
			else if (kind == "vline") {
				entry.kind = TilemapOp::Kind::VLine;
				entry.x = op.value("x", 0) + ox;
				entry.z = op.value("y0", 0) + oz;
				entry.z1 = op.value("y1", 0) + oz;
			}
			else {
				spdlog::warn("[Tilemap] Unknown op '{}'", kind);
				continue;
			}
			out.push_back(std::move(entry));
		}
		catch (const std::exception& ex) {
			spdlog::warn("[Tilemap] Skipping malformed '{}' op: {}", kind.empty() ? "?" : kind, ex.what());
		}
	}
}

} // namespace

std::optional<TilemapSpec> ParseTilemapSpec(std::string_view map_path) {
	const auto doc_opt = core::LoadJsonFile(map_path, "Tilemap");
	if (!doc_opt) return std::nullopt;
	const json& doc = *doc_opt;

	const auto width = doc.value("width", 0U);
	const auto height = doc.value("height", 0U);
	if (width == 0 || height == 0) {
		spdlog::error("[Tilemap] Map '{}' has invalid dimensions {}x{}", map_path, width, height);
		return std::nullopt;
	}

	TilemapSpec spec;
	spec.width = width;
	spec.height = height;
	spec.tile_size_px = doc.value("tile_size_px", 16U);
	spec.pixels_per_world_unit = doc.value("pixels_per_world_unit", kDefaultPixelsPerWorldUnit);

	if (doc.contains("fill")) {
		if (const auto& fill_val = doc.at("fill"); fill_val.is_number_unsigned()) {
			spec.fill_id = fill_val.get<uint32_t>();
		}
		else if (fill_val.is_string()) {
			const std::string name = fill_val.get<std::string>();
			spec.fill_name = name;
		}
		else {
			spdlog::warn("[Tilemap] Unsupported tile reference type for fill'");
		}
	}

	std::unordered_map<std::string, json> brushes;
	if (doc.contains("brushes")) {
		for (const auto& [name, brush_ops] : doc.at("brushes").items()) {
			brushes[name] = brush_ops;
		}
	}
	if (doc.contains("ops")) {
		ExpandOps(doc.at("ops"), spec.ops, brushes, 0, 0);
	}

	spdlog::info("[Tilemap] Parsed spec ({}x{}, {} ops) from '{}'", width, height, spec.ops.size(), map_path);
	return spec;
}

Tilemap BakeSpec(const TilemapSpec& spec, const TileSet* tile_set) {
	Tilemap tm;
	tm.width = spec.width;
	tm.height = spec.height;
	tm.tile_size_px = spec.tile_size_px;
	tm.pixels_per_world_unit = spec.pixels_per_world_unit;

	const uint32_t fill_id = tile_set ? tile_set->ResolveTileId(spec.fill_name).value_or(spec.fill_id) : spec.fill_id;

	tm.tile_ids.assign(static_cast<size_t>(spec.width) * spec.height, fill_id);

	for (const auto& [kind, tile_id, tile_name, x, z, x1, z1] : spec.ops) {
		const uint32_t id = tile_set ? tile_set->ResolveTileId(tile_name).value_or(tile_id) : tile_id;

		switch (kind) {
		case TilemapOp::Kind::Set: SetTile(tm, x, z, id); break;
		case TilemapOp::Kind::FillRect: FillRect(tm, x, z, x1, z1, id); break;
		case TilemapOp::Kind::HLine: HLine(tm, z, x, x1, id); break;
		case TilemapOp::Kind::VLine: VLine(tm, x, z, z1, id); break;
		}
	}
	return tm;
}

nlohmann::json TilemapToJson(const Tilemap& tm) {
	return json{
		{"width", tm.width},
		{"height", tm.height},
		{"tile_size_px", tm.tile_size_px},
		{"pixels_per_world_unit", tm.pixels_per_world_unit},
		{"tile_ids", tm.tile_ids},
	};
}

Tilemap TilemapFromJson(const nlohmann::json& j) {
	Tilemap tm;
	tm.width = j.value("width", 0U);
	tm.height = j.value("height", 0U);
	tm.tile_size_px = j.value("tile_size_px", 16U);
	tm.pixels_per_world_unit = j.value("pixels_per_world_unit", kDefaultPixelsPerWorldUnit);
	if (j.contains("tile_ids") && j.at("tile_ids").is_array()) {
		const auto& ids = j.at("tile_ids");
		tm.tile_ids.reserve(ids.size());
		for (const auto& id : ids) {
			tm.tile_ids.push_back(id.get<uint32_t>());
		}
	}
	return tm;
}

} // namespace engine::tilemap
