#include "tilemap_nav_adapter.hpp"

#include <spdlog/spdlog.h>

namespace engine::tilemap {

std::unique_ptr<pathfinding::GridNavGraph>
BuildNavGraphFromTilemap(const Tilemap& tilemap, const TileSet& tileset, const glm::vec3 origin) {
	auto graph = std::make_unique<pathfinding::GridNavGraph>();
	graph->Resize(static_cast<int>(tilemap.width), static_cast<int>(tilemap.height));
	graph->cell_size = TileWorldSize(tilemap);
	graph->origin = origin;

	for (int z = 0; z < static_cast<int>(tilemap.height); ++z) {
		for (int x = 0; x < static_cast<int>(tilemap.width); ++x) {
			const uint32_t tile_id = GetTileIdAt(tilemap, x, z);
			if (tile_id == kInvalidTileId) {
				graph->SetWalkable(x, z, false);
				continue;
			}
			const TileDescriptor* desc = tileset.GetTile(tile_id);
			graph->SetWalkable(x, z, desc ? desc->walkable : true);
		}
	}

	spdlog::debug(
		"[TilemapNavAdapter] Built nav graph: {}x{} cells, cell_size={:.3f}",
		tilemap.width,
		tilemap.height,
		graph->cell_size
	);
	graph->BuildAdjacency();
	return graph;
}

} // namespace engine::tilemap
