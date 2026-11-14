#pragma once

#include "engine/engine.hpp"
#include "town_style.hpp"
#include "world_data.hpp"

namespace game::farm {

/// Map a Biome enum value to its base tile name in the shared tileset.
/// Roads and town tiles are painted on top of the base layer.
[[nodiscard]] std::string_view BiomeToTileName(Biome biome);

/// Build a baked Tilemap from a WorldData grid. The TileSet must contain every tile name
/// emitted by BiomeToTileName plus "path", "ruins", "tilled_soil", and the floor materials the
/// town-style table can roll ("dirt", "cobblestone", "brick", …).
/// Pass 1: base biome layer. Pass 2: roads as "path". Pass 3: each living town's plots are paved
/// with a dominant ground material rolled from `styles` (keyed by the town's biome + size tier);
/// ruined towns use "ruins", farm plots use "tilled_soil", and the town's street spine/branches
/// are painted as "path" on top. Pass 4: blocking world-object footprints populate the Tilemap's
/// blocked_override movement layer (collision decoupled from the flat terrain tile).
/// Unknown tile names fall back to grass (id 0). `styles` defaults to the built-in palette so
/// callers that don't theme towns get sensible dirt/stone/brick grounds for free.
[[nodiscard]] engine::tilemap::Tilemap BuildTilemapFromWorldData(
	const WorldData& world,
	const engine::tilemap::TileSet& tileset,
	const TownStyleTable& styles = DefaultTownStyleTable()
);

} // namespace game::farm
