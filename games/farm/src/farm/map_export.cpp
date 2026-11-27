#include "map_export.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>

// File-local copy: the static (WASM) raylib build ships its own stb_image_write, so exported
// symbols would collide at link time.
#define STB_IMAGE_WRITE_STATIC
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>

#include "world_data.hpp"

namespace game::farm {
namespace {

struct Rgb {
	uint8_t r{0};
	uint8_t g{0};
	uint8_t b{0};
};

Rgb BiomeColour(const Biome biome) {
	switch (biome) {
	case Biome::Ocean: return {.r = 34, .g = 64, .b = 120};
	case Biome::Lake: return {.r = 60, .g = 110, .b = 180};
	case Biome::River: return {.r = 90, .g = 150, .b = 210};
	case Biome::Beach: return {.r = 210, .g = 200, .b = 140};
	case Biome::Plains: return {.r = 108, .g = 160, .b = 80};
	case Biome::Forest: return {.r = 52, .g = 104, .b = 56};
	case Biome::Hills: return {.r = 140, .g = 120, .b = 80};
	case Biome::Mountain: return {.r = 150, .g = 150, .b = 158};
	case Biome::Count_: break;
	}
	return {.r = 0, .g = 0, .b = 0};
}

// A 5x7 bitmap font: 7 rows per glyph, low 5 bits per row (bit 4 = leftmost column). Covers the
// characters that appear in town names plus digits and a few symbols; unknown chars render blank.
using Glyph = std::array<uint8_t, 7>;

const Glyph& GlyphFor(char c) {
	static const Glyph kBlank{0, 0, 0, 0, 0, 0, 0};
	// clang-format off
	static const std::array<Glyph, 26> kLetters{{
		{0b01110,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}, // A
		{0b11110,0b10001,0b10001,0b11110,0b10001,0b10001,0b11110}, // B
		{0b01110,0b10001,0b10000,0b10000,0b10000,0b10001,0b01110}, // C
		{0b11110,0b10001,0b10001,0b10001,0b10001,0b10001,0b11110}, // D
		{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b11111}, // E
		{0b11111,0b10000,0b10000,0b11110,0b10000,0b10000,0b10000}, // F
		{0b01110,0b10001,0b10000,0b10111,0b10001,0b10001,0b01111}, // G
		{0b10001,0b10001,0b10001,0b11111,0b10001,0b10001,0b10001}, // H
		{0b01110,0b00100,0b00100,0b00100,0b00100,0b00100,0b01110}, // I
		{0b00111,0b00010,0b00010,0b00010,0b00010,0b10010,0b01100}, // J
		{0b10001,0b10010,0b10100,0b11000,0b10100,0b10010,0b10001}, // K
		{0b10000,0b10000,0b10000,0b10000,0b10000,0b10000,0b11111}, // L
		{0b10001,0b11011,0b10101,0b10101,0b10001,0b10001,0b10001}, // M
		{0b10001,0b10001,0b11001,0b10101,0b10011,0b10001,0b10001}, // N
		{0b01110,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}, // O
		{0b11110,0b10001,0b10001,0b11110,0b10000,0b10000,0b10000}, // P
		{0b01110,0b10001,0b10001,0b10001,0b10101,0b10010,0b01101}, // Q
		{0b11110,0b10001,0b10001,0b11110,0b10100,0b10010,0b10001}, // R
		{0b01111,0b10000,0b10000,0b01110,0b00001,0b00001,0b11110}, // S
		{0b11111,0b00100,0b00100,0b00100,0b00100,0b00100,0b00100}, // T
		{0b10001,0b10001,0b10001,0b10001,0b10001,0b10001,0b01110}, // U
		{0b10001,0b10001,0b10001,0b10001,0b10001,0b01010,0b00100}, // V
		{0b10001,0b10001,0b10001,0b10101,0b10101,0b11011,0b10001}, // W
		{0b10001,0b10001,0b01010,0b00100,0b01010,0b10001,0b10001}, // X
		{0b10001,0b10001,0b01010,0b00100,0b00100,0b00100,0b00100}, // Y
		{0b11111,0b00001,0b00010,0b00100,0b01000,0b10000,0b11111}, // Z
	}};
	static const std::array<Glyph, 10> kDigits{{
		{0b01110,0b10001,0b10011,0b10101,0b11001,0b10001,0b01110}, // 0
		{0b00100,0b01100,0b00100,0b00100,0b00100,0b00100,0b01110}, // 1
		{0b01110,0b10001,0b00001,0b00010,0b00100,0b01000,0b11111}, // 2
		{0b11111,0b00010,0b00100,0b00010,0b00001,0b10001,0b01110}, // 3
		{0b00010,0b00110,0b01010,0b10010,0b11111,0b00010,0b00010}, // 4
		{0b11111,0b10000,0b11110,0b00001,0b00001,0b10001,0b01110}, // 5
		{0b00110,0b01000,0b10000,0b11110,0b10001,0b10001,0b01110}, // 6
		{0b11111,0b00001,0b00010,0b00100,0b01000,0b01000,0b01000}, // 7
		{0b01110,0b10001,0b10001,0b01110,0b10001,0b10001,0b01110}, // 8
		{0b01110,0b10001,0b10001,0b01111,0b00001,0b00010,0b01100}, // 9
	}};
	// clang-format on
	static const Glyph kHyphen{0, 0, 0, 0b11111, 0, 0, 0};
	static const Glyph kPeriod{0, 0, 0, 0, 0, 0, 0b00100};

	if (c >= 'a' && c <= 'z') {
		c = static_cast<char>(c - 'a' + 'A');
	}
	if (c >= 'A' && c <= 'Z') {
		return kLetters[static_cast<size_t>(c - 'A')];
	}
	if (c >= '0' && c <= '9') {
		return kDigits[static_cast<size_t>(c - '0')];
	}
	if (c == '-') {
		return kHyphen;
	}
	if (c == '.') {
		return kPeriod;
	}
	return kBlank;
}

struct Image {
	int w{0};
	int h{0};
	std::vector<uint8_t> rgba;

	void Put(const int x, const int y, const Rgb c) {
		if (x < 0 || y < 0 || x >= w || y >= h) {
			return;
		}
		const size_t i = (static_cast<size_t>(y) * w + x) * 4;
		rgba[i] = c.r;
		rgba[i + 1] = c.g;
		rgba[i + 2] = c.b;
		rgba[i + 3] = 255;
	}

	void FillRect(const int x, const int y, const int rw, const int rh, const Rgb c) {
		for (int yy = y; yy < y + rh; ++yy) {
			for (int xx = x; xx < x + rw; ++xx) {
				Put(xx, yy, c);
			}
		}
	}

	void FillCircle(const int cx, const int cy, const int radius, const Rgb c) {
		for (int yy = -radius; yy <= radius; ++yy) {
			for (int xx = -radius; xx <= radius; ++xx) {
				if (xx * xx + yy * yy <= radius * radius) {
					Put(cx + xx, cy + yy, c);
				}
			}
		}
	}

	void Line(int x0, int y0, const int x1, const int y1, const int thickness, const Rgb c) {
		const int dx = std::abs(x1 - x0);
		const int dy = -std::abs(y1 - y0);
		const int sx = x0 < x1 ? 1 : -1;
		const int sy = y0 < y1 ? 1 : -1;
		int err = dx + dy;
		const int t = std::max(1, thickness) / 2;
		while (true) {
			FillRect(x0 - t, y0 - t, thickness, thickness, c);
			if (x0 == x1 && y0 == y1) {
				break;
			}
			const int e2 = 2 * err;
			if (e2 >= dy) {
				err += dy;
				x0 += sx;
			}
			if (e2 <= dx) {
				err += dx;
				y0 += sy;
			}
		}
	}

	void Glyph5x7(const int x, const int y, const char ch, const int scale, const Rgb c) {
		const Glyph& g = GlyphFor(ch);
		for (int row = 0; row < 7; ++row) {
			for (int col = 0; col < 5; ++col) {
				if ((g[static_cast<size_t>(row)] >> (4 - col)) & 1) {
					FillRect(x + col * scale, y + row * scale, scale, scale, c);
				}
			}
		}
	}

	// Draw text with a 1px dark shadow for legibility over any biome.
	void Text(const int x, const int y, const std::string& s, const int scale, const Rgb c) {
		int cx = x;
		for (const char ch : s) {
			Glyph5x7(cx + scale, y + scale, ch, scale, {.r = 10, .g = 12, .b = 16});
			Glyph5x7(cx, y, ch, scale, c);
			cx += 6 * scale;
		}
	}
};

} // namespace

bool WriteWorldImagePng(const WorldData& world, const std::string& path, const int cellPixels) {
	const int cp = std::max(1, cellPixels);
	constexpr int kMarginTiles = 10; // ocean frame so the landmass reads as a continent and edge
									 // labels have room instead of being clipped
	const int off = kMarginTiles * cp;
	Image img;
	img.w = (world.grid.width + 2 * kMarginTiles) * cp;
	img.h = (world.grid.height + 2 * kMarginTiles) * cp;
	if (img.w <= 0 || img.h <= 0) {
		return false;
	}
	img.rgba.assign(static_cast<size_t>(img.w) * img.h * 4, 255);
	img.FillRect(0, 0, img.w, img.h, BiomeColour(Biome::Ocean)); // ocean frame

	const auto clampTextX = [&](const int x, const int textW) {
		return std::clamp(x, 2, std::max(2, img.w - textW - 2));
	};

	// Biomes.
	for (int y = 0; y < world.grid.height; ++y) {
		for (int x = 0; x < world.grid.width; ++x) {
			img.FillRect(off + x * cp, off + y * cp, cp, cp, BiomeColour(world.grid.BiomeAt(x, y)));
		}
	}

	// Named bodies of water, at their centroid.
	const int waterScale = std::max(1, cp / 3);
	for (const WaterBody& body : world.waterBodies) {
		if (body.name.empty()) {
			continue;
		}
		const int tw = static_cast<int>(body.name.size()) * 6 * waterScale;
		const int tx = clampTextX(off + body.centroid.x * cp + cp / 2 - tw / 2, tw);
		img.Text(tx, off + body.centroid.y * cp + cp / 2, body.name, waterScale, {.r = 205, .g = 225, .b = 245});
	}

	const auto townCentre = [&](const glm::ivec2 tile) {
		return glm::ivec2{off + tile.x * cp + cp / 2, off + tile.y * cp + cp / 2};
	};

	// Roads.
	for (const RoadSegment& seg : world.roads.segments) {
		const TownSite* a = world.FindTown(seg.townA);
		const TownSite* b = world.FindTown(seg.townB);
		if (a == nullptr || b == nullptr) {
			continue;
		}
		const glm::ivec2 pa = townCentre(a->location);
		const glm::ivec2 pb = townCentre(b->location);
		img.Line(pa.x, pa.y, pb.x, pb.y, std::max(2, cp / 3), {.r = 70, .g = 50, .b = 36});
	}

	// Town pins + names.
	const int pinR = std::max(3, cp);
	const int nameScale = std::max(1, cp / 3);
	for (const TownSite& t : world.towns) {
		const glm::ivec2 c = townCentre(t.location);
		const Rgb fill =
			t.status == TownStatus::Living ? Rgb{.r = 90, .g = 210, .b = 90} : Rgb{.r = 170, .g = 70, .b = 70};
		img.FillCircle(c.x, c.y, pinR + 1, {.r = 20, .g = 20, .b = 20});
		img.FillCircle(c.x, c.y, pinR, fill);
		// Place the label to the right of the pin, flipping to the left if it would run off the edge.
		const int tw = static_cast<int>(t.name.size()) * 6 * nameScale;
		int tx = c.x + pinR + 2;
		if (tx + tw > img.w - 2) {
			tx = c.x - pinR - 2 - tw;
		}
		img.Text(std::max(2, tx), c.y - 3 * nameScale, t.name, nameScale, {.r = 245, .g = 245, .b = 245});
	}

	return stbi_write_png(path.c_str(), img.w, img.h, 4, img.rgba.data(), img.w * 4) != 0;
}

} // namespace game::farm
