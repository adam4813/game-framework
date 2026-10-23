#pragma once

#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#include <flecs.h>
#include <glm/glm.hpp>

#include "engine/core/core.hpp"

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

// Marker: the viewport streaming centers on the WorldTransform of the entity carrying this tag (the
// player, a camera, or any chosen anchor), decoupling the tilemap from any specific follower.
struct TilemapFollowTarget {};

// TileDescriptor: data-driven definition of a single tile type
struct TileDescriptor {
	uint32_t id{0};                          // Unique tile ID
	std::string name;                        // Tile name (e.g., "grass", "forest", "path")
	glm::vec4 color{1.0F, 1.0F, 1.0F, 1.0F}; // RGBA tint (white = use raw texture colour)
	core::Rect tex_coords{};                 // Pixel rect {x, y, w, h} within the tileset image.
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

// TileSet: global registry of tile descriptors
struct TileSet {
	std::map<uint32_t, TileDescriptor> tiles;             // Tile ID -> Descriptor mapping
	std::unordered_map<std::string, uint32_t> name_to_id; // Tile name -> ID (populated by tileset loader)

	[[nodiscard]] const TileDescriptor* GetTile(const uint32_t id) const {
		const auto it = tiles.find(id);
		if (it != tiles.end()) {
			return &it->second;
		}
		return nullptr;
	}

	void RegisterTile(const TileDescriptor& descriptor) {
		tiles[descriptor.id] = descriptor;
		if (!descriptor.name.empty()) {
			name_to_id[descriptor.name] = descriptor.id;
		}
	}

	[[nodiscard]] std::optional<uint32_t> ResolveTileId(const std::string& name) const {
		if (name.empty()) {
			return std::nullopt; // Empty means no alias; use the ID if defined
		}
		if (const auto it = name_to_id.find(name); it != name_to_id.end()) {
			return it->second;
		}

		spdlog::warn("[Tileset] Unknown tile name '{}'", name);
		return std::nullopt;
	}
};

/// A single resolved grid painting operation. Tile references are stored as resolved IDs when
/// possible; a tile_name is preserved when name resolution wasn't available at parse time and
/// resolved lazily at bake time via TileSet.
struct TilemapOp {
	enum class Kind : uint8_t { Set, FillRect, HLine, VLine };
	Kind kind{Kind::Set};
	uint32_t tile_id{0};   ///< Resolved tile ID; valid when tile_name is empty
	std::string tile_name; ///< Original name if unresolved
	int x{0};              ///< Set/VLine column; FillRect x0; HLine x0
	int z{0};              ///< Set/HLine row;    FillRect z0; VLine z0
	int x1{0};             ///< FillRect x1 (exclusive); HLine x1 (inclusive)
	int z1{0};             ///< FillRect z1 (exclusive); VLine z1 (inclusive)
};

/// Declarative tilemap specification — the "recipe" half of the spec/baked-data split.
/// Holds all information needed to produce a Tilemap: grid dimensions, fill tile, and a flat list
/// of painting operations (tile names already resolved to IDs). Scripts and C++ can build or
/// patch TilemapSpec directly for procedural generation.
///
/// Populate via the "tilemap" level loader (reads from a map JSON file) or construct directly.
/// The TilemapBake observer converts a TilemapSpec + TileSet into a flat Tilemap automatically.
struct TilemapSpec {
	uint32_t width{0};
	uint32_t height{0};
	uint32_t tile_size_px{16};
	float pixels_per_world_unit{kDefaultPixelsPerWorldUnit};
	uint32_t fill_id{0};
	std::string fill_name; ///< Original name if unresolved
	std::vector<TilemapOp> ops;
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
	}; // Pixels mapped to one world unit; renderer + gameplay derive scale from this
	std::vector<uint32_t> tile_ids; // References to tile descriptors (not raw colors)

	// Optional per-tile movement override, indexed like tile_ids (z * width + x). A non-zero entry
	// forces that tile non-walkable regardless of its TileDescriptor — this is where the
	// world-object layer (buildings, trees, props) stamps collision that is decoupled from the flat
	// terrain tile underneath. Empty means "no overrides" (zero cost); when populated it is sized to
	// width*height.
	std::vector<uint8_t> blocked_override;
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

// Whether (x, z) is blocked by a movement override (e.g. a world-object footprint). An empty
// override vector means no overrides, so this is false. Out-of-range coordinates are not blocked
// here (bounds are handled by the caller / IsTileWalkable).
[[nodiscard]] inline bool IsTileBlocked(const Tilemap& tm, const int x, const int z) {
	if (tm.blocked_override.empty() || !InBounds(tm, x, z)) {
		return false;
	}
	const size_t idx = static_cast<size_t>(z) * tm.width + static_cast<size_t>(x);
	return idx < tm.blocked_override.size() && tm.blocked_override[idx] != 0;
}

// Combined walkability query: a tile is walkable when it is in bounds, not covered by a blocking
// movement override, and its TileDescriptor is walkable (an unknown descriptor is treated as
// walkable, matching the historical grid-movement behaviour). Out-of-range tiles are not walkable.
[[nodiscard]] inline bool IsTileWalkable(const Tilemap& tm, const TileSet& ts, const int x, const int z) {
	if (!InBounds(tm, x, z) || IsTileBlocked(tm, x, z)) {
		return false;
	}
	const TileDescriptor* descriptor = ts.GetTile(GetTileIdAt(tm, x, z));
	return descriptor == nullptr || descriptor->walkable;
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

// Enables viewport-sized ring-buffer rendering for a Tilemap.  Instead of baking the entire map
// into one mesh, the GPU buffer holds a fixed bw × bh window of tiles.  The streaming system
// repositions the mesh and refreshes UV data each frame the camera crosses a tile boundary, so
// only the visible area is ever on the GPU.  Set this component BEFORE setting Tilemap so that
// the mesh-builder observer sees it and builds a viewport-sized buffer.
//
// origin_tile_x / origin_tile_z: world-tile index at buffer slot (0,0); the sentinel
// std::numeric_limits<int>::min() means "not yet initialised" and forces a full UV upload on the
// first streaming update.
struct TilemapViewport {
	int buffer_width{0};  // 0 = compute from camera on first streaming update
	int buffer_height{0}; // 0 = compute from camera on first streaming update
	int origin_tile_x{std::numeric_limits<int>::min()};
	int origin_tile_z{std::numeric_limits<int>::min()};
};

} // namespace engine::tilemap
