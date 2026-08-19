#pragma once

#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>

#include <flecs.h>

#include "tilemap_components.hpp"

namespace engine::tilemap {

// A parsed tileset: the TileSet component (texture + atlas dimensions), the tile registry
// (descriptors keyed by id), and a name → id lookup for resolving tile references in map ops. The
// caller owns the registry and may inject tile behaviour (onenter/within/onleave) into it before it
// is placed on an entity.
struct LoadedTileset {
	TileSet info;
	TileRegistry registry;
	std::unordered_map<std::string, std::uint32_t> name_to_id;
};

// Load and parse a JSON tileset file (see assets/data/tilesets/*.json). Kept separate from the map
// loader so the caller can load the tileset, inject tile behaviour into its registry, and then build
// the grid. Returns std::nullopt on failure (missing file / malformed JSON).
[[nodiscard]] std::optional<LoadedTileset> LoadTileset(const flecs::world& world, std::string_view tileset_path);

// Map-tileset pair: BuildTilemap returns both to ensure they stay in sync.
// The map JSON's "tileset" field drives which tileset is loaded.
struct MapResult {
	Tilemap tilemap;
	LoadedTileset tileset;
};

// Build both tileset and tilemap from a JSON map file (see assets/data/maps/*.json).
// The map must declare a "tileset" field with the path to its tileset JSON.
// The map declares width/height/tile scale, a default `fill` tile, optional
// reusable `brushes`, and an `ops` list of grid primitives (`rect`, `hline`, `vline`, `set`,
// `stamp`). Returns std::nullopt if tileset load fails or map ops fail.
[[nodiscard]] std::optional<MapResult> BuildMap(const flecs::world& world, std::string_view map_path);

} // namespace engine::tilemap
