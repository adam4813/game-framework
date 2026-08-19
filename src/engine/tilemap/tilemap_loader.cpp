#include "tilemap_loader.hpp"

#include <optional>
#include <string>
#include <unordered_map>

#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "engine/assets/assets_module.hpp"
#include "engine/core/json_util.hpp"
#include "engine/platform/platform.hpp"
#include "tilemap_components.hpp"

namespace engine::tilemap {

namespace {

using json = nlohmann::json;

// Resolve a JSON tile reference (a string tile name or a numeric id) to a tile id.
std::optional<uint32_t> ResolveTile(const json& value, const LoadedTileset& tileset) {
	if (value.is_number_unsigned()) {
		return value.get<uint32_t>();
	}
	if (value.is_string()) {
		const auto it = tileset.name_to_id.find(value.get<std::string>());
		if (it != tileset.name_to_id.end()) {
			return it->second;
		}
		spdlog::warn("[Tilemap] Unknown tile name '{}' in map ops", value.get<std::string>());
	}
	return std::nullopt;
}

// Apply a list of grid ops to `tm`, offset by (ox, oy) so brush ops stamp relative to an origin.
// Primitive ops dispatch straight to the reusable Tilemap grid operations; only the `stamp`
// composite (expand a named brush at an offset) is handled here.
//
// TODO: `stamp` brushes are tied to specific tileset tiles, so a shared brush library arguably
// belongs with the tileset rather than each map file. For now brushes are declared inline in the map
// JSON and expanded here — revisit once tileset/brush ownership is settled.
void ApplyOps(
	const json& ops,
	Tilemap& tm,
	const LoadedTileset& tileset,
	const std::unordered_map<std::string, json>& brushes,
	const int ox,
	const int oy
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
				ApplyOps(brush_it->second, tm, tileset, brushes, ox + at[0].get<int>(), oy + at[1].get<int>());
				continue;
			}

			const auto tile = ResolveTile(op.at("tile"), tileset);
			if (!tile) {
				continue; // ResolveTile already warned
			}
			const uint32_t id = *tile;

			if (kind == "rect") {
				const auto& r = op.at("rect"); // [x0, y0, x1, y1], exclusive of x1/y1
				FillRect(
					tm,
					r[0].get<int>() + ox,
					r[1].get<int>() + oy,
					r[2].get<int>() + ox,
					r[3].get<int>() + oy,
					id
				);
			}
			else if (kind == "hline") {
				HLine(tm, op.value("y", 0) + oy, op.value("x0", 0) + ox, op.value("x1", 0) + ox, id);
			}
			else if (kind == "vline") {
				VLine(tm, op.value("x", 0) + ox, op.value("y0", 0) + oy, op.value("y1", 0) + oy, id);
			}
			else if (kind == "set") {
				const auto& at = op.at("at");
				SetTile(tm, at[0].get<int>() + ox, at[1].get<int>() + oy, id);
			}
			else {
				spdlog::warn("[Tilemap] Unknown op '{}'", kind);
			}
		}
		catch (const std::exception& ex) {
			// A malformed op (missing key, wrong type, short array) skips itself instead of aborting
			// the whole map build.
			spdlog::warn("[Tilemap] Skipping malformed '{}' op: {}", kind.empty() ? "?" : kind, ex.what());
		}
	}
}

} // namespace

std::optional<LoadedTileset> LoadTileset(const flecs::world& world, const std::string_view tileset_path) {
	const std::string asset_dir = world.get<platform::PlatformRef>().ptr->AssetDirectory();

	const auto doc_opt = core::LoadJsonFile(tileset_path, "Tileset");
	if (!doc_opt) {
		return std::nullopt;
	}
	const json& doc = *doc_opt;

	LoadedTileset out;
	const auto cell = doc.value("tile_size_px", 64U);
	const auto per_row = doc.value("tiles_per_row", 1U);
	const auto per_col = doc.value("tiles_per_col", 1U);
	out.info.texture_path = asset_dir + "/" + doc.value("texture", std::string{});
	// Atlas dimensions default to (cells * cell size) but may be overridden explicitly.
	out.info.image_width_px = doc.value("image_width_px", per_row * cell);
	out.info.image_height_px = doc.value("image_height_px", per_col * cell);

	if (!doc.contains("tiles")) {
		spdlog::warn("[Tileset] '{}' has no 'tiles' array", tileset_path);
		return out;
	}
	for (const auto& tile_def : doc.at("tiles")) {
		TileDescriptor td{};
		td.id = tile_def.value("id", 0U);
		td.name = tile_def.value("name", std::string{});
		td.color = core::JColorNormalized(tile_def, "color", {1.0F, 1.0F, 1.0F, 1.0F});
		td.tex_coords = core::JRect(tile_def, "tex_coords");
		td.walkable = tile_def.value("walkable", true);
		if (!td.name.empty()) {
			out.name_to_id[td.name] = td.id;
		}
		out.registry.RegisterTile(td);
	}
	spdlog::info("[Tileset] Loaded {} tile descriptors from '{}'", out.registry.tiles.size(), tileset_path);
	return out;
}

std::optional<Tilemap> BuildTilemapGrid(std::string_view map_path, const LoadedTileset& tileset) {
	const auto doc_opt = core::LoadJsonFile(map_path, "Tilemap");
	if (!doc_opt) {
		return std::nullopt;
	}
	const json& doc = *doc_opt;

	const auto width = doc.value("width", 0U);
	const auto height = doc.value("height", 0U);
	if (width == 0 || height == 0) {
		spdlog::error("[Tilemap] Map '{}' has invalid dimensions {}x{}", map_path, width, height);
		return std::nullopt;
	}

	uint32_t fill_id = 0;
	if (doc.contains("fill")) {
		if (const auto resolved = ResolveTile(doc.at("fill"), tileset)) {
			fill_id = *resolved;
		}
		else {
			spdlog::warn("[Tilemap] Unresolved fill tile in map '{}' — defaulting to id 0", map_path);
		}
	}

	Tilemap tilemap;
	tilemap.width = width;
	tilemap.height = height;
	tilemap.tile_size_px = doc.value("tile_size_px", 16U);
	tilemap.pixels_per_world_unit = doc.value("pixels_per_world_unit", kDefaultPixelsPerWorldUnit);
	tilemap.tile_ids.assign(static_cast<size_t>(width) * height, fill_id);

	std::unordered_map<std::string, json> brushes;
	if (doc.contains("brushes")) {
		for (const auto& [name, brush_ops] : doc.at("brushes").items()) {
			brushes[name] = brush_ops;
		}
	}
	if (doc.contains("ops")) {
		ApplyOps(doc.at("ops"), tilemap, tileset, brushes, 0, 0);
	}

	spdlog::info("[Tilemap] Built grid ({}x{}) from '{}'", width, height, map_path);
	return tilemap;
}

std::optional<MapResult> BuildMap(const flecs::world& world, std::string_view map_path) {
	// Load map JSON to read the tileset path
	const auto doc_opt = core::LoadJsonFile(map_path, "Map");
	if (!doc_opt) {
		return std::nullopt;
	}
	const json& doc = *doc_opt;

	// Map must declare its tileset in the "tileset" field
	const std::string tileset_path = doc.value("tileset", std::string{});
	if (tileset_path.empty()) {
		spdlog::error("[Map] Map '{}' has no 'tileset' field", map_path);
		return std::nullopt;
	}

	// Load the tileset from the map's tileset field
	auto tileset = LoadTileset(world, assets::ResolveAsset(world, tileset_path));
	if (!tileset) {
		spdlog::error("[Map] Failed to load tileset '{}' from map '{}'", tileset_path, map_path);
		return std::nullopt;
	}

	// Build the tilemap grid using the loaded tileset
	auto tilemap = BuildTilemapGrid(map_path, *tileset);
	if (!tilemap) {
		spdlog::error("[Map] Failed to build tilemap grid from '{}'", map_path);
		return std::nullopt;
	}

	return MapResult{.tilemap = *tilemap, .tileset = *tileset};
}

} // namespace engine::tilemap
