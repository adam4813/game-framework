#pragma once

#include <optional>
#include <string_view>

#include <nlohmann/json.hpp>

#include "tilemap_components.hpp"

namespace engine::tilemap {

/// Parse a map ops JSON file into a TilemapSpec, resolving tile names to IDs via `name_to_id`.
/// Stamps (brush references) are fully expanded — the resulting ops list is flat.
/// Unresolved names are stored as `TilemapOp::tile_name` for late resolution at bake time.
[[nodiscard]] std::optional<TilemapSpec> ParseTilemapSpec(std::string_view map_path);

/// Bake a TilemapSpec into a flat Tilemap (fill + ops applied). Pure data transform — no file I/O.
/// Optionally pass a TileSet to resolve any tile_name entries left unresolved at parse time.
[[nodiscard]] Tilemap BakeSpec(const TilemapSpec& spec, const TileSet* tile_set = nullptr);

/// Serialize a baked Tilemap into a JSON object: { width, height, tile_size_px,
/// pixels_per_world_unit, tile_ids[] }. This is the inverse of TilemapFromJson and matches the
/// "tilemap" level-loader schema, so a saved grid round-trips through disk unchanged.
[[nodiscard]] nlohmann::json TilemapToJson(const Tilemap& tm);

/// Deserialize a Tilemap from a JSON object produced by TilemapToJson (or authored in the
/// "tilemap" level-loader schema). Missing fields fall back to the Tilemap defaults; a malformed
/// or wrong-sized tile_ids array yields an empty grid (caller can validate via width*height).
[[nodiscard]] Tilemap TilemapFromJson(const nlohmann::json& j);

} // namespace engine::tilemap
