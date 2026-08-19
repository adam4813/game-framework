#!/usr/bin/env python3
"""
Generate tilemap and player textures using Pillow.

Usage:
    python Tools/generate_tilemap_textures.py

Generates:
    - assets/textures/tileset_basic.png (256x256, 8-tile grid, 2x4)
    - assets/textures/player_sprite.png (64x64)
    - assets/textures/tilemap_bg.png (512x512)
"""

import os
import sys
from pathlib import Path
from PIL import Image, ImageDraw
import numpy as np


OUTPUT_DIR = "assets/textures"


def create_directory_if_needed():
    """Ensure output directory exists."""
    Path(OUTPUT_DIR).mkdir(parents=True, exist_ok=True)


def solid_with_noise(width, height, base_color, noise_amplitude=5):
    """
    Create an image with a solid color plus subtle noise.

    Args:
        width: Image width in pixels
        height: Image height in pixels
        base_color: Tuple of (R, G, B) values
        noise_amplitude: Maximum deviation from base color

    Returns:
        PIL Image with noise applied
    """
    # Create base image
    img = Image.new("RGB", (width, height), base_color)
    pixels = img.load()

    # Add subtle noise
    np.random.seed(hash(base_color) % (2**32))  # Deterministic noise per color
    for y in range(height):
        for x in range(width):
            noise = np.random.randint(-noise_amplitude, noise_amplitude + 1, 3)
            r, g, b = base_color
            r = max(0, min(255, r + noise[0]))
            g = max(0, min(255, g + noise[1]))
            b = max(0, min(255, b + noise[2]))
            pixels[x, y] = (r, g, b)

    return img


def _make_grass(size):
    """Tile 0 – bright open grass with subtle variation."""
    return solid_with_noise(size, size, (70, 150, 50), noise_amplitude=14)


def _make_forest(size):
    """Tile 1 – dense forest with tree-crown blobs."""
    img = solid_with_noise(size, size, (32, 105, 32), noise_amplitude=8)
    draw = ImageDraw.Draw(img)
    np.random.seed(42)
    for _ in range(6):
        cx = int(np.random.randint(8, size - 8))
        cy = int(np.random.randint(8, size - 8))
        r  = int(np.random.randint(7, 13))
        shade = int(np.random.randint(0, 18))
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(14 + shade, 78 + shade, 14))
    return img


def _make_path(size):
    """Tile 2 – dirt road, warm tan-brown."""
    return solid_with_noise(size, size, (155, 118, 68), noise_amplitude=18)


def _make_water(size):
    """Tile 3 – water with lighter horizontal ripple bands."""
    img = solid_with_noise(size, size, (48, 108, 200), noise_amplitude=8)
    pixels = img.load()
    for y in range(size):
        if y % 10 < 4:
            for x in range(size):
                r, g, b = pixels[x, y]
                pixels[x, y] = (min(255, r + 22), min(255, g + 22), min(255, b + 28))
    return img


def _make_stone_floor(size):
    """Tile 4 – castle interior stone flags with grid lines."""
    img = solid_with_noise(size, size, (158, 158, 158), noise_amplitude=10)
    draw = ImageDraw.Draw(img)
    grid = 16
    line_color = (115, 115, 115)
    for x in range(0, size + 1, grid):
        draw.line([(x, 0), (x, size)], fill=line_color, width=1)
    for y in range(0, size + 1, grid):
        draw.line([(0, y), (size, y)], fill=line_color, width=1)
    return img


def _make_stone_wall(size):
    """Tile 5 – castle walls with a brick pattern."""
    img = solid_with_noise(size, size, (88, 88, 88), noise_amplitude=8)
    draw = ImageDraw.Draw(img)
    mortar = (55, 55, 55)
    bh, bw, mg = 10, 20, 2
    for row_i, y0 in enumerate(range(0, size, bh + mg)):
        draw.rectangle([0, y0, size, y0 + mg - 1], fill=mortar)
        offset = (bw // 2) if (row_i % 2 == 1) else 0
        for x0 in range(-offset, size, bw + mg):
            draw.line([(x0, y0), (x0, min(y0 + bh, size))], fill=mortar, width=mg)
    return img


def _make_sand(size):
    """Tile 6 – sandy ground, warm yellow-tan."""
    return solid_with_noise(size, size, (212, 188, 118), noise_amplitude=14)


def _make_deep_forest(size):
    """Tile 7 – dense dark forest border with denser tree crowns."""
    img = solid_with_noise(size, size, (16, 62, 16), noise_amplitude=6)
    draw = ImageDraw.Draw(img)
    np.random.seed(99)
    for _ in range(9):
        cx = int(np.random.randint(6, size - 6))
        cy = int(np.random.randint(6, size - 6))
        r  = int(np.random.randint(5, 10))
        draw.ellipse([cx - r, cy - r, cx + r, cy + r], fill=(8, 42, 8))
    return img


def generate_tileset():
    """
    Generate tileset_basic.png: 128x256 with 8-tile grid (2x4).

    Tile layout (ID → terrain):
      0 = GRASS        1 = FOREST
      2 = PATH         3 = WATER
      4 = STONE_FLOOR  5 = STONE_WALL
      6 = SAND         7 = DEEP_FOREST
    """
    tile_size = 64
    grid_w, grid_h = 2, 4
    width, height = tile_size * grid_w, tile_size * grid_h

    img = Image.new("RGB", (width, height), (0, 0, 0))

    tile_funcs = [
        _make_grass,
        _make_forest,
        _make_path,
        _make_water,
        _make_stone_floor,
        _make_stone_wall,
        _make_sand,
        _make_deep_forest,
    ]

    for idx, func in enumerate(tile_funcs):
        row = idx // grid_w
        col = idx % grid_w
        tile_img = func(tile_size)
        img.paste(tile_img, (col * tile_size, row * tile_size))

    output_path = os.path.join(OUTPUT_DIR, "tileset_basic.png")
    img.save(output_path)
    print(f"[OK] Generated {output_path}")


def generate_player_sprite():
    """
    Generate player_sprite.png: 64x64 green square with subtle noise.
    """
    width, height = 64, 64
    base_color = (100, 200, 100)  # Green

    img = solid_with_noise(width, height, base_color, noise_amplitude=5)

    output_path = os.path.join(OUTPUT_DIR, "player_sprite.png")
    img.save(output_path)
    print(f"[OK] Generated {output_path}")


def generate_tilemap_bg():
    """
    Generate tilemap_bg.png: 512x512 light gray with subtle grid pattern.
    Grid spacing: 32px, line width: 1px
    """
    width, height = 512, 512
    base_color = (220, 220, 220)  # Light gray

    img = Image.new("RGB", (width, height), base_color)
    draw = ImageDraw.Draw(img)

    grid_spacing = 32
    line_color = (180, 180, 180)  # Darker gray for grid lines
    line_width = 1

    # Draw vertical lines
    for x in range(0, width, grid_spacing):
        draw.line([(x, 0), (x, height)], fill=line_color, width=line_width)

    # Draw horizontal lines
    for y in range(0, height, grid_spacing):
        draw.line([(0, y), (width, y)], fill=line_color, width=line_width)

    output_path = os.path.join(OUTPUT_DIR, "tilemap_bg.png")
    img.save(output_path)
    print(f"[OK] Generated {output_path}")


def main():
    """Generate all textures."""
    create_directory_if_needed()
    print("Generating tilemap textures...")
    generate_tileset()
    generate_player_sprite()
    generate_tilemap_bg()
    print("\n[OK] All textures generated successfully!")


if __name__ == "__main__":
    main()
