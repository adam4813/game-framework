#include "farm_map_rasterizer.hpp"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>

namespace game::farm {

namespace {

// Biome → flat minimap colour. Index = static_cast<uint8_t>(Biome).
// Order must match Biome enum: Ocean=0, Lake=1, Beach=2, Plains=3,
//                              Forest=4, Hills=5, Mountain=6, River=7
constexpr std::array<engine::core::Rgba, 8> kBiomeColors = {{
	{.r = 60,  .g = 110, .b = 200, .a = 255}, // Ocean   – deep blue
	{.r = 80,  .g = 140, .b = 210, .a = 255}, // Lake    – mid blue
	{.r = 220, .g = 205, .b = 145, .a = 255}, // Beach   – sandy yellow
	{.r = 110, .g = 170, .b = 80,  .a = 255}, // Plains  – bright green
	{.r = 45,  .g = 115, .b = 55,  .a = 255}, // Forest  – dark green
	{.r = 95,  .g = 130, .b = 65,  .a = 255}, // Hills   – muted green
	{.r = 105, .g = 100, .b = 95,  .a = 255}, // Mountain – stone grey
	{.r = 90,  .g = 140, .b = 200, .a = 255}, // River   – lighter blue
}};
static_assert(
	kBiomeColors.size() == static_cast<std::size_t>(Biome::Count_),
	"kBiomeColors must have one entry per Biome enumerator"
);

} // namespace

engine::core::Rgba BiomeToColor(const Biome biome) {
	const auto idx = static_cast<std::size_t>(biome);
	if (idx < kBiomeColors.size())
		return kBiomeColors[idx];
	return {.r = 180, .g = 180, .b = 180, .a = 255};
}

std::vector<uint8_t> RasterizeWorldMap(const WorldData& world, const int outW, const int outH) {
	std::vector<uint8_t> rgba(static_cast<std::size_t>(outW) * outH * 4, 255);
	const int gridW = world.grid.width;
	const int gridH = world.grid.height;
	if (gridW <= 0 || gridH <= 0 || outW <= 0 || outH <= 0)
		return rgba;

	for (int oy = 0; oy < outH; ++oy) {
		// Flip both axes so the rendered map matches the in-world top-down camera orientation.
		const int gz = (gridH - 1) - static_cast<int>(static_cast<int64_t>(oy) * gridH / outH);
		for (int ox = 0; ox < outW; ++ox) {
			const int gx = (gridW - 1) - static_cast<int>(static_cast<int64_t>(ox) * gridW / outW);
			const Biome biome = world.grid.BiomeAt(
				std::clamp(gx, 0, gridW - 1),
				std::clamp(gz, 0, gridH - 1)
			);
			const auto c = BiomeToColor(biome);
			const std::size_t idx = (static_cast<std::size_t>(oy) * outW + ox) * 4;
			rgba[idx + 0] = c.r;
			rgba[idx + 1] = c.g;
			rgba[idx + 2] = c.b;
			rgba[idx + 3] = 255;
		}
	}
	return rgba;
}

std::vector<uint8_t> RasterizeLocalArea(
	const WorldData& world,
	const int centerX,
	const int centerZ,
	const int radiusTiles,
	const int outW,
	const int outH
) {
	std::vector<uint8_t> rgba(static_cast<std::size_t>(outW) * outH * 4, 255);
	RasterizeLocalArea(world, centerX, centerZ, radiusTiles, outW, outH, rgba);
	return rgba;
}

void RasterizeLocalArea(
	const WorldData& world,
	const int centerX,
	const int centerZ,
	const int radiusTiles,
	const int outW,
	const int outH,
	std::vector<uint8_t>& rgba
) {
	const int gridW = world.grid.width;
	const int gridH = world.grid.height;
	if (radiusTiles <= 0 || outW <= 0 || outH <= 0)
		return;

	const int tileSpan = radiusTiles * 2;
	const int startX   = centerX - radiusTiles;
	const int startZ   = centerZ - radiusTiles;

	for (int oy = 0; oy < outH; ++oy) {
		// Flip both axes so the minimap matches the in-world top-down camera orientation.
		const int tz = (startZ + tileSpan - 1) - static_cast<int>(static_cast<int64_t>(oy) * tileSpan / outH);
		for (int ox = 0; ox < outW; ++ox) {
			const int    tx = (startX + tileSpan - 1) - static_cast<int>(static_cast<int64_t>(ox) * tileSpan / outW);
			const Biome  biome = (tx >= 0 && tx < gridW && tz >= 0 && tz < gridH)
			                       ? world.grid.BiomeAt(tx, tz)
			                       : Biome::Ocean;
			const auto   c    = BiomeToColor(biome);
			const std::size_t idx = (static_cast<std::size_t>(oy) * outW + ox) * 4;
			rgba[idx + 0] = c.r;
			rgba[idx + 1] = c.g;
			rgba[idx + 2] = c.b;
			rgba[idx + 3] = 255;
		}
	}
}

glm::vec2 ComputePinFraction(const glm::ivec2 worldPos, const int gridW, const int gridH) {
	if (gridW <= 0 || gridH <= 0)
		return {0.0F, 0.0F};
	// Flip both axes to match the RasterizeWorldMap flip (camera has +X = screen-left).
	return {
		1.0F - (static_cast<float>(worldPos.x) + 0.5F) / static_cast<float>(gridW),
		1.0F - (static_cast<float>(worldPos.y) + 0.5F) / static_cast<float>(gridH),
	};
}

glm::vec2 ComputeLocalPinFraction(
	const glm::ivec2 worldPos,
	const glm::ivec2 center,
	const int radiusTiles
) {
	if (radiusTiles <= 0)
		return {0.5F, 0.5F};
	const float span = static_cast<float>(radiusTiles * 2);
	// Flip both axes to match the RasterizeLocalArea flip (camera has +X = screen-left).
	return {
		1.0F - ((static_cast<float>(worldPos.x - center.x) + static_cast<float>(radiusTiles)) / span),
		1.0F - ((static_cast<float>(worldPos.y - center.y) + static_cast<float>(radiusTiles)) / span),
	};
}

// ── Tile-ID-based rasterizer ──────────────────────────────────────────────────

namespace {

// Minimap colours per tile ID, indexed by the IDs in assets/data/tilesets/basic.json.
// Matches BiomeToColor for terrain tiles; adds distinct colours for road/town/ruin overlays.
constexpr std::array<engine::core::Rgba, 10> kTileMinimapColors = {{
	{.r = 110, .g = 170,  .b = 80, .a = 255}, // 0 grass        – plains green
	{ .r = 45, .g = 115,  .b = 55, .a = 255}, // 1 forest       – dark green
	{.r = 170, .g = 150, .b = 110, .a = 255}, // 2 path         – dirt-tan road
	{ .r = 60, .g = 110, .b = 200, .a = 255}, // 3 water        – blue
	{.r = 170, .g = 165, .b = 160, .a = 255}, // 4 stone_floor  – town light grey
	{.r = 105, .g = 100,  .b = 95, .a = 255}, // 5 stone_wall   – mountain dark grey
	{.r = 220, .g = 205, .b = 145, .a = 255}, // 6 sand         – beach sandy
	{ .r = 75, .g = 105,  .b = 55, .a = 255}, // 7 deep_forest  – hills / deep forest
	{.r = 150, .g = 110,  .b = 75, .a = 255}, // 8 ruins        – ruin brown
	{.r = 140, .g = 100,  .b = 60, .a = 255}, // 9 tilled_soil  – dark brown earth
}};
constexpr engine::core::Rgba kUnknownTileColor = {.r = 128, .g = 128, .b = 128, .a = 255};

} // namespace

engine::core::Rgba TileIdToMinimapColor(const uint32_t tileId) {
	if (tileId < kTileMinimapColors.size())
		return kTileMinimapColors[tileId];
	return kUnknownTileColor;
}

std::vector<uint8_t> RasterizeWorldMapFromTilemap(
	const engine::tilemap::Tilemap& tm,
	const int outW,
	const int outH
) {
	std::vector<uint8_t> rgba(static_cast<std::size_t>(outW) * outH * 4, 255);
	const int gridW = static_cast<int>(tm.width);
	const int gridH = static_cast<int>(tm.height);
	if (gridW <= 0 || gridH <= 0 || outW <= 0 || outH <= 0)
		return rgba;

	for (int oy = 0; oy < outH; ++oy) {
		// Flip both axes to match the in-world top-down camera orientation.
		const int gz = (gridH - 1) - static_cast<int>(static_cast<int64_t>(oy) * gridH / outH);
		for (int ox = 0; ox < outW; ++ox) {
			const int gx = (gridW - 1) - static_cast<int>(static_cast<int64_t>(ox) * gridW / outW);
			const uint32_t tileId = engine::tilemap::GetTileIdAt(tm, gx, gz);
			const auto c = TileIdToMinimapColor(
				(tileId == engine::tilemap::kInvalidTileId) ? 3u : tileId
			);
			const std::size_t idx = (static_cast<std::size_t>(oy) * outW + ox) * 4;
			rgba[idx + 0] = c.r;
			rgba[idx + 1] = c.g;
			rgba[idx + 2] = c.b;
			rgba[idx + 3] = 255;
		}
	}
	return rgba;
}

void RasterizeLocalAreaFromTilemap(
	const engine::tilemap::Tilemap& tm,
	const int centerX,
	const int centerZ,
	const int radiusTiles,
	const int outW,
	const int outH,
	std::vector<uint8_t>& rgba
) {
	const int gridW = static_cast<int>(tm.width);
	const int gridH = static_cast<int>(tm.height);
	if (radiusTiles <= 0 || outW <= 0 || outH <= 0)
		return;

	const int tileSpan = radiusTiles * 2;
	const int startX   = centerX - radiusTiles;
	const int startZ   = centerZ - radiusTiles;

	for (int oy = 0; oy < outH; ++oy) {
		const int tz = (startZ + tileSpan - 1) - static_cast<int>(static_cast<int64_t>(oy) * tileSpan / outH);
		for (int ox = 0; ox < outW; ++ox) {
			const int tx = (startX + tileSpan - 1) - static_cast<int>(static_cast<int64_t>(ox) * tileSpan / outW);
			const bool inBounds = (tx >= 0 && tx < gridW && tz >= 0 && tz < gridH);
			const uint32_t tileId = inBounds
				? engine::tilemap::GetTileIdAt(tm, tx, tz)
				: 3u; // water tile for out-of-bounds border
			const auto c = TileIdToMinimapColor(
				(tileId == engine::tilemap::kInvalidTileId) ? 3u : tileId
			);
			const std::size_t idx = (static_cast<std::size_t>(oy) * outW + ox) * 4;
			rgba[idx + 0] = c.r;
			rgba[idx + 1] = c.g;
			rgba[idx + 2] = c.b;
			rgba[idx + 3] = 255;
		}
	}
}

} // namespace game::farm
