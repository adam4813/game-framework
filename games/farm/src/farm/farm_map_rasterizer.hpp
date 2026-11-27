#pragma once

#include <cstdint>
#include <vector>

#include <glm/glm.hpp>

#include "engine/engine.hpp"
#include "world_data.hpp"

namespace game::farm {

// ── Biome-based rasterizer (used for unit tests + fallback) ──────────────────

/// Maps a Biome enum value to a flat RGBA colour.
[[nodiscard]] engine::core::Rgba BiomeToColor(Biome biome);

/// Rasterize the full biome grid into an outW×outH RGBA8 buffer. Nearest-neighbour, axes flipped
/// to match the in-world top-down camera orientation (+X = screen-left, +Z = screen-up).
[[nodiscard]] std::vector<uint8_t> RasterizeWorldMap(const WorldData& world, int outW, int outH);

/// Rasterize a local window centred at (centerX, centerZ) into a pre-allocated RGBA8 buffer.
void RasterizeLocalArea(
	const WorldData& world,
	int centerX,
	int centerZ,
	int radiusTiles,
	int outW,
	int outH,
	std::vector<uint8_t>& pixels
);

/// Allocating overload of RasterizeLocalArea.
[[nodiscard]] std::vector<uint8_t>
RasterizeLocalArea(const WorldData& world, int centerX, int centerZ, int radiusTiles, int outW, int outH);

// ── Tile-ID-based rasterizer (preferred in-game — shows roads, town floors, ruins) ─────────────

/// Maps a tile ID from assets/data/tilesets/basic.json to a minimap RGBA colour.
/// Unknown IDs map to a neutral grey. Stable as long as basic.json tile IDs don't change.
[[nodiscard]] engine::core::Rgba TileIdToMinimapColor(uint32_t tileId);

/// Rasterize the full tilemap into an outW×outH RGBA8 buffer.
/// Axes are flipped to match the in-world camera orientation, same as the biome rasterizer.
[[nodiscard]] std::vector<uint8_t> RasterizeWorldMapFromTilemap(const engine::tilemap::Tilemap& tm, int outW, int outH);

/// In-place: rasterize a local window around (centerX, centerZ) from the live tilemap.
/// Shows roads, stone-floor town centres and ruins rather than just biome colours.
void RasterizeLocalAreaFromTilemap(
	const engine::tilemap::Tilemap& tm,
	int centerX,
	int centerZ,
	int radiusTiles,
	int outW,
	int outH,
	std::vector<uint8_t>& pixels
);

// ── Pin helpers ───────────────────────────────────────────────────────────────

/// Returns the fractional [0,1]² position of a world grid coordinate on the full map.
/// Accounts for the axis flip: (gridW-1,gridH-1) maps to (0,0); (0,0) maps to (1,1).
[[nodiscard]] glm::vec2 ComputePinFraction(glm::ivec2 worldPos, int gridW, int gridH);

/// Returns the fractional [0,1]² position of a world grid coordinate within a local area view.
/// The center maps to (0.5,0.5). Values outside [0,1] are off-screen.
[[nodiscard]] glm::vec2 ComputeLocalPinFraction(glm::ivec2 worldPos, glm::ivec2 center, int radiusTiles);

} // namespace game::farm
