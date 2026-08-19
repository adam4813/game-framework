#pragma once

#include <cstdint>
#include <flecs.h>
#include <functional>
#include <glm/glm.hpp>
#include <map>
#include <string>
#include <vector>

#include "engine/platform/platform.hpp"

namespace engine::tilemap {

// Default pixels-per-world-unit for a tilemap whose art is authored at this scale. Stored per
// tilemap (see Tilemap::pixels_per_world_unit) so it stays configurable/data-driven; this is only
// the fallback used when a tilemap doesn't override it.
inline constexpr float kDefaultPixelsPerWorldUnit = 32.0F;

// Sentinel returned by GetTileIdAt for out-of-range / malformed lookups. Never present in a
// registry, so registry.GetTile(kInvalidTileId) resolves to nullptr.
inline constexpr uint32_t kInvalidTileId = 0xFFFFFFFFU;

// Callback function types for tile events
// Entity is the entity that entered/is on/left the tile
// TileX, TileZ are the tile grid coordinates
using TileCallback = std::function<void(flecs::entity, int, int)>;

// GridPosition: tile-space coordinates for an entity on the grid
struct GridPosition {
	int x{0};
	int z{0};
};

// TileDescriptor: data-driven definition of a single tile type
struct TileDescriptor {
	uint32_t id{0};                          // Unique tile ID
	std::string name;                        // Tile name (e.g., "grass", "forest", "path")
	glm::vec4 color{1.0F, 1.0F, 1.0F, 1.0F}; // RGBA tint (white = use raw texture colour)
	platform::Rect tex_coords{};             // Pixel rect {x, y, w, h} within the tileset image.
	bool walkable{true};                     // Whether entities can pass through this tile

	// Tile event callbacks (optional)
	TileCallback onenter; // Fire when entity enters the tile
	TileCallback within;  // Fire every frame while entity is on the tile
	TileCallback onleave; // Fire when entity leaves the tile
};

// Internal: CPU-side mesh data for tilemap rendering (cleared after upload)
struct TilemapMeshData {
	std::vector<glm::vec3> vertices;
	std::vector<uint32_t> indices;
	std::vector<glm::vec4> colors;
	std::vector<glm::vec2> uvs;
};

// TileRegistry: global registry of tile descriptors
struct TileRegistry {
	std::map<uint32_t, TileDescriptor> tiles; // Tile ID -> Descriptor mapping

	[[nodiscard]] const TileDescriptor* GetTile(const uint32_t id) const {
		const auto it = tiles.find(id);
		if (it != tiles.end()) {
			return &it->second;
		}
		return nullptr;
	}

	void RegisterTile(const TileDescriptor& descriptor) { tiles[descriptor.id] = descriptor; }
};

// TileSet component: the tileset texture used to draw this tilemap.
struct TileSet {
	std::string texture_path;      // Path to the tileset texture
	uint32_t image_width_px{128};  // Full texture width in pixels
	uint32_t image_height_px{256}; // Full texture height in pixels
};

// Tilemap component: a 2D grid of tile descriptors
// tile_ids is a flat array indexed as: tile_ids[y * width + x]
// Each tile_id references a TileDescriptor in the global tile registry
struct Tilemap {
	uint32_t width{0};
	uint32_t height{0};
	uint32_t tile_size_px{16}; // Tile art size in pixels (world size = tile_size_px / pixels_per_world_unit)
	float pixels_per_world_unit{
		kDefaultPixelsPerWorldUnit
	};                              // Pixels mapped to one world unit; renderer + gameplay derive scale from this
	std::vector<uint32_t> tile_ids; // References to tile descriptors (not raw colors)
};

// World-space size of one tile in `tm` — the single source of truth both the renderer mesh and the
// gameplay grid stride derive from, so changing tile_size_px / pixels_per_world_unit keeps them synced.
[[nodiscard]] inline float TileWorldSize(const Tilemap& tm) {
	return static_cast<float>(tm.tile_size_px) / tm.pixels_per_world_unit;
}

// Whether (x, z) is inside the tilemap grid.
[[nodiscard]] inline bool InBounds(const Tilemap& tm, const int x, const int z) {
	return x >= 0 && z >= 0 && x < static_cast<int>(tm.width) && z < static_cast<int>(tm.height);
}

// Tile id at (x, z), or kInvalidTileId if out of range or the grid is malformed (tile_ids shorter
// than width*height). Keeps the flat-array indexing + bounds check in one place.
[[nodiscard]] inline uint32_t GetTileIdAt(const Tilemap& tm, const int x, const int z) {
	if (!InBounds(tm, x, z)) {
		return kInvalidTileId;
	}
	const size_t idx = static_cast<size_t>(z) * tm.width + static_cast<size_t>(x);
	return idx < tm.tile_ids.size() ? tm.tile_ids[idx] : kInvalidTileId;
}

// --- Grid mutation operations ------------------------------------------------------------------
// Reusable primitives for authoring/editing a tilemap grid, at load time OR at runtime. They mutate
// `tm.tile_ids` in place (which must already be sized to width*height); the map loader dispatches
// JSON ops to these, and gameplay code can call them directly to change the map on the fly. After a
// runtime edit, re-set/`modified<Tilemap>()` the component to trigger a mesh rebuild.

// Set a single tile. Out-of-range coordinates are a no-op.
inline void SetTile(Tilemap& tm, const int x, const int z, const uint32_t id) {
	if (!InBounds(tm, x, z)) {
		return;
	}
	const size_t idx = static_cast<size_t>(z) * tm.width + static_cast<size_t>(x);
	if (idx < tm.tile_ids.size()) {
		tm.tile_ids[idx] = id;
	}
}

// Fill the rectangle [x0, x1) x [z0, z1) (exclusive of x1/z1) with `id`.
inline void FillRect(Tilemap& tm, const int x0, const int z0, const int x1, const int z1, const uint32_t id) {
	for (int z = z0; z < z1; ++z) {
		for (int x = x0; x < x1; ++x) {
			SetTile(tm, x, z, id);
		}
	}
}

// Fill the horizontal run from x0 to x1 (inclusive of both ends) on row z.
inline void HLine(Tilemap& tm, const int z, const int x0, const int x1, const uint32_t id) {
	const int step = (x1 >= x0) ? 1 : -1;
	for (int x = x0; x != x1 + step; x += step) {
		SetTile(tm, x, z, id);
	}
}

// Fill the vertical run from z0 to z1 (inclusive of both ends) on column x.
inline void VLine(Tilemap& tm, const int x, const int z0, const int z1, const uint32_t id) {
	const int step = (z1 >= z0) ? 1 : -1;
	for (int z = z0; z != z1 + step; z += step) {
		SetTile(tm, x, z, id);
	}
}

// TileCallbackState: tracks which tile an entity is currently on (for callback firing)
// Added dynamically to entities that enter tiles with callbacks
struct TileCallbackState {
	int current_tile_x{-1};
	int current_tile_z{-1};
};

} // namespace engine::tilemap
