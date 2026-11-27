#include "farm_tilemap_builder.hpp"

#include <algorithm>

#include <spdlog/spdlog.h>

#include "town_layout.hpp"
#include "town_style.hpp"

namespace game::farm {

std::string_view BiomeToTileName(const Biome biome) {
	switch (biome) {
	case Biome::Ocean: [[fallthrough]];
	case Biome::Lake: [[fallthrough]];
	case Biome::River: return "water";
	case Biome::Beach: return "sand";
	case Biome::Plains: return "grass";
	case Biome::Forest: return "forest";
	case Biome::Hills: return "deep_forest";
	case Biome::Mountain: return "stone_wall";
	case Biome::Count_: break;
	}
	return "grass";
}

engine::tilemap::Tilemap BuildTilemapFromWorldData(
	const WorldData& world,
	const engine::tilemap::TileSet& tileset,
	const TownStyleTable& styles
) {
	const auto resolve = [&](const std::string_view name) -> uint32_t {
		const auto id = tileset.ResolveTileId(std::string(name));
		if (!id) {
			spdlog::warn("[FarmTilemapBuilder] Unknown tile name '{}'; falling back to grass", name);
			return 0;
		}
		return *id;
	};

	const int w = world.grid.width;
	const int h = world.grid.height;

	engine::tilemap::Tilemap tm;
	tm.width = static_cast<uint32_t>(w);
	tm.height = static_cast<uint32_t>(h);
	tm.tile_size_px = 16;
	tm.pixels_per_world_unit = 32.0F;
	tm.tile_ids.resize(static_cast<size_t>(w) * static_cast<size_t>(h));

	// Pass 1: base biome layer (row-major: z is row, x is column — matches Grid::Index(x, y)).
	for (int z = 0; z < h; ++z) {
		for (int x = 0; x < w; ++x) {
			const Biome biome = world.grid.BiomeAt(x, z);
			tm.tile_ids[static_cast<size_t>(z) * w + x] = resolve(BiomeToTileName(biome));
		}
	}

	// Pass 2: paint road paths over terrain — 3 tiles wide so they're navigable through forests.
	const uint32_t pathId = resolve("path");
	for (const auto& seg : world.roads.segments) {
		for (const auto& pt : seg.path) {
			// 3×3 square centred on each path tile.
			engine::tilemap::FillRect(tm, pt.x - 1, pt.y - 1, pt.x + 2, pt.y + 2, pathId);
		}
	}

	// Pass 3: town layouts — walkable plots + street spine/branches + decorative field plots. A living
	// town's plots are paved with a single dominant ground material rolled from the style table (keyed
	// by the town's biome + size tier), so small towns read as dirt and larger ones as cobble/brick.
	// Ruined towns use "ruins"; farm plots use "tilled_soil"; the street spine/branches use "path".
	const uint32_t ruinsId = resolve("ruins");
	const uint32_t tilledId = resolve("tilled_soil");
	const uint32_t pathTileId = resolve("path");
	for (const auto& layout : world.townLayouts) {
		uint32_t floorTile = ruinsId;
		if (!layout.ruined) {
			const TownSite* site = world.FindTown(layout.townId);
			const int population = (site != nullptr) ? site->size : 0;
			const glm::ivec2 at = (site != nullptr) ? site->location : layout.landmark;
			const Biome biome = world.grid.InBounds(at.x, at.y) ? world.grid.BiomeAt(at.x, at.y) : Biome::Plains;
			const std::string material = PickFloorMaterial(
				styles,
				biome,
				TierForPopulation(population),
				DeriveTownSeed(world.seed, layout.townId) ^ 0x464C4F52u
			);
			floorTile = resolve(material);
		}
		for (const auto& tile : layout.floorTiles) {
			engine::tilemap::SetTile(tm, tile.x, tile.y, floorTile);
		}
		for (const auto& field : layout.fieldTiles) {
			engine::tilemap::SetTile(tm, field.x, field.y, tilledId);
		}
		for (const auto& street : layout.pathTiles) {
			engine::tilemap::SetTile(tm, street.x, street.y, pathTileId);
		}
	}

	// Pass 3b: bridge decks — clear the water's non-walkable state along each span by paving it with a
	// walkable path tile, so the player and NPCs can cross (ticket 08). A bridge deck is the only
	// walkable water in the world; its upright structure is rendered from the world-object layer.
	for (const auto& bridge : world.bridges) {
		for (const auto& tile : bridge.deck) {
			engine::tilemap::SetTile(tm, tile.x, tile.y, pathId);
		}
	}

	// Pass 4: world-object footprints — stamp blocking objects into the movement-override layer so a
	// covered tile is non-walkable regardless of the (flat) terrain tile beneath it. The override
	// vector is allocated lazily on the first blocking object so object-free worlds pay nothing.
	for (const auto& obj : world.objects) {
		if (!obj.blocksMovement) {
			continue;
		}
		if (tm.blocked_override.empty()) {
			tm.blocked_override.assign(static_cast<size_t>(w) * static_cast<size_t>(h), 0);
		}
		for (const auto& tile : obj.footprint) {
			if (engine::tilemap::InBounds(tm, tile.x, tile.y)) {
				tm.blocked_override[static_cast<size_t>(tile.y) * w + tile.x] = 1;
			}
		}
	}

	return tm;
}

} // namespace game::farm
