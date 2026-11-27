#!/usr/bin/env python3
"""
Generate farm-world upright-object sprite atlas using Pillow.

Usage (run from repo root):
    python games/farm/tools/generate_farm_object_sprites.py

Generates:
    games/farm/assets/textures/farm_objects.png  — 256x256 sprite atlas, 4x4 grid of 64x64 tiles

Tile layout (col, row → sprite name):
  (0,0) tree        broadleaf tree (oak/birch)
  (1,0) tree_dark   conifer tree   (pine/fir)
  (2,0) building    stone building (house/shop/hall/landmark/well)
  (3,0) wood        wooden structure (barn/signpost)
  (0,1) character   humanoid figure (villager/NPC)
  (1,1) animal      quadruped animal (cow/deer/sheep/etc.)
  (2,1) rubble      broken ruins
  (3,1) (reserved)
  (0-3, 2-3) reserved for future use

UV positions in the 256x256 atlas (pixel coords for tilesets/farm_objects.json):
  tree:      (0,   0,  64, 64)
  tree_dark: (64,  0,  64, 64)
  building:  (128, 0,  64, 64)
  wood:      (192, 0,  64, 64)
  character: (0,   64, 64, 64)
  animal:    (64,  64, 64, 64)
  rubble:    (128, 64, 64, 64)
"""

import os
from pathlib import Path
from PIL import Image, ImageDraw
import numpy as np

OUTPUT_DIR = "games/farm/assets/textures"
TILE = 64
COLS = 8
ROWS = 8


def _noise(img, base, amp=8):
    px = img.load()
    np.random.seed(hash(base) % (2 ** 32))
    for y in range(img.height):
        for x in range(img.width):
            d = np.random.randint(-amp, amp + 1, 3)
            r, g, b = base
            px[x, y] = (
                max(0, min(255, r + int(d[0]))),
                max(0, min(255, g + int(d[1]))),
                max(0, min(255, b + int(d[2]))),
            )


def _solid(size, color, amp=8):
    img = Image.new("RGB", (size, size), color)
    _noise(img, color, amp)
    return img


# ─── Sprite functions ─────────────────────────────────────────────────────────

def make_tree(size):
    """Broadleaf tree – rounded crown over a short trunk."""
    img = _solid(size, (34, 100, 34), amp=10)
    draw = ImageDraw.Draw(img)
    cx = size // 2
    tw = max(4, size // 9)
    draw.rectangle([cx - tw, size // 2, cx + tw, size - 2], fill=(110, 72, 38))
    for dx, dy, r in [(-size//7, 0, size//4), (size//7, 0, size//4), (0, -size//7, size//3)]:
        draw.ellipse([cx+dx-r, size//3+dy-r, cx+dx+r, size//3+dy+r], fill=(28, 110, 28))
    return img


def make_tree_dark(size):
    """Conifer / pine – dark stacked cone tiers with a trunk."""
    img = _solid(size, (14, 55, 14), amp=6)
    draw = ImageDraw.Draw(img)
    cx = size // 2
    draw.rectangle([cx - 3, size * 3 // 4, cx + 3, size - 2], fill=(70, 45, 20))
    for ty, hw in [(size//8, size//8), (size//4, size//5), (size*2//5, size//3)]:
        draw.polygon([(cx - hw, ty + hw*2), (cx, ty), (cx + hw, ty + hw*2)], fill=(10, 44, 10))
    return img


def make_building(size):
    """Stone building facade – grey flagstone wall, window, door."""
    img = _solid(size, (118, 118, 122), amp=6)
    draw = ImageDraw.Draw(img)
    draw.rectangle([0, 0, size, size // 6], fill=(78, 78, 82))
    wx, wy = size // 5, size // 4
    draw.rectangle([wx, wy, wx + size//4, wy + size//5], fill=(55, 80, 120))
    draw.rectangle([wx+2, wy+2, wx+size//4-2, wy+size//5-2], fill=(130, 170, 215))
    dx = size // 2 - size//10
    draw.rectangle([dx, size*3//4, dx + size//5, size-1], fill=(72, 48, 28))
    return img


def make_wood_structure(size):
    """Wooden structure – warm brown planks with roof triangle."""
    img = _solid(size, (145, 92, 42), amp=14)
    draw = ImageDraw.Draw(img)
    for y in range(0, size, 10):
        draw.line([(0, y), (size, y)], fill=(95, 60, 22), width=2)
    draw.rectangle([2, 0, 8, size-1], fill=(88, 55, 20))
    draw.rectangle([size-8, 0, size-2, size-1], fill=(88, 55, 20))
    draw.polygon([(0, size//5), (size//2, 2), (size, size//5)], fill=(75, 46, 16))
    return img


def make_character(size):
    """Humanoid figure – villager / NPC silhouette."""
    img = _solid(size, (148, 112, 62), amp=4)
    draw = ImageDraw.Draw(img)
    cx = size // 2
    hr = size // 8
    draw.ellipse([cx-hr, size//8, cx+hr, size//8+hr*2], fill=(218, 176, 124))
    draw.rectangle([cx-8, size//3, cx+8, size*2//3], fill=(68, 98, 162))
    draw.rectangle([cx-8, size*2//3, cx-2, size-4], fill=(48, 68, 108))
    draw.rectangle([cx+2, size*2//3, cx+8, size-4], fill=(48, 68, 108))
    draw.rectangle([cx-14, size//3, cx-8, size//2+2], fill=(68, 98, 162))
    draw.rectangle([cx+8, size//3, cx+14, size//2+2], fill=(68, 98, 162))
    return img


def make_animal(size):
    """Generic quadruped – cow / deer / sheep etc."""
    img = _solid(size, (200, 175, 110), amp=10)
    draw = ImageDraw.Draw(img)
    bx1, by1, bx2, by2 = size//6, size//3, size*5//6, size*2//3
    draw.ellipse([bx1, by1, bx2, by2], fill=(155, 105, 55))
    hx = size*5//6
    draw.ellipse([hx-10, by1-5, hx+8, by1+16], fill=(138, 92, 46))
    for lx in [size//5, size*2//5, size*3//5, size*4//5]:
        draw.rectangle([lx-3, by2, lx+3, by2 + size//6], fill=(118, 76, 36))
    draw.ellipse([size//6-5, by1+4, size//6+4, by1+13], fill=(138, 92, 46))
    return img


def make_rubble(size):
    """Broken stone rubble / ruins."""
    img = _solid(size, (108, 88, 68), amp=14)
    draw = ImageDraw.Draw(img)
    np.random.seed(77)
    for _ in range(10):
        x = int(np.random.randint(2, size - 14))
        y = int(np.random.randint(2, size - 10))
        w = int(np.random.randint(8, 20))
        h = int(np.random.randint(5, 14))
        s = int(np.random.randint(0, 32))
        draw.rectangle([x, y, x+w, y+h], fill=(82+s, 64+s, 50+s))
    return img


# ─── Village building sprites ──────────────────────────────────────────────────
# A shared facade helper draws a wall block, a roof, a door, and optional windows so each building
# kind reads distinctly (different wall/roof colours + roof shapes) while staying cheap to author.

def _facade(size, wall, roof, roof_style="gable", door=(74, 50, 30), windows=2, window_col=(150, 185, 215)):
    img = _solid(size, wall, amp=6)
    draw = ImageDraw.Draw(img)
    roof_dark = tuple(max(0, c - 24) for c in roof)
    if roof_style == "gable":
        draw.polygon([(0, size // 3), (size // 2, 2), (size, size // 3)], fill=roof)
        draw.rectangle([0, size // 3, size, size // 3 + 3], fill=roof_dark)
    elif roof_style == "flat":
        draw.rectangle([0, 0, size, size // 5], fill=roof)
        draw.rectangle([0, size // 5, size, size // 5 + 2], fill=roof_dark)
    elif roof_style == "spire":
        draw.polygon([(size // 2, 0), (size * 3 // 8, size // 3), (size * 5 // 8, size // 3)], fill=roof)
        draw.rectangle([0, size // 3, size, size // 3 + 3], fill=roof_dark)
    # Door, centred along the base.
    dw = size // 5
    dx = size // 2 - dw // 2
    draw.rectangle([dx, size * 3 // 4, dx + dw, size - 1], fill=door)
    # Windows.
    ww = size // 7
    wy = size // 2
    if windows >= 1:
        draw.rectangle([size // 6, wy, size // 6 + ww, wy + ww], fill=window_col)
    if windows >= 2:
        draw.rectangle([size * 5 // 7, wy, size * 5 // 7 + ww, wy + ww], fill=window_col)
    return img


def make_house(size):
    """Residential cottage/house/longhouse – warm plaster walls, red gable roof."""
    return _facade(size, (178, 150, 112), (156, 72, 52), "gable")


def make_shop(size):
    """Shop – blue-roofed storefront with a wide display window."""
    img = _facade(size, (172, 162, 132), (74, 116, 150), "gable", windows=0)
    draw = ImageDraw.Draw(img)
    draw.rectangle([size // 6, size // 2, size * 2 // 3, size * 3 // 4 - 2], fill=(160, 195, 220))
    for i, x in enumerate(range(size // 6, size * 2 // 3, 8)):  # awning stripes
        draw.rectangle([x, size // 2 - 6, x + 4, size // 2], fill=(190, 80, 70) if i % 2 else (230, 225, 215))
    return img


def make_barn(size):
    """Barn – deep red boards, big hayloft doors, cross-braced."""
    img = _solid(size, (158, 60, 46), amp=10)
    draw = ImageDraw.Draw(img)
    draw.polygon([(0, size // 4), (size // 2, 2), (size, size // 4)], fill=(96, 72, 52))
    draw.rectangle([size // 3, size // 2, size * 2 // 3, size - 1], fill=(120, 44, 34))
    draw.line([(size // 3, size // 2), (size * 2 // 3, size - 1)], fill=(210, 205, 195), width=2)
    draw.line([(size * 2 // 3, size // 2), (size // 3, size - 1)], fill=(210, 205, 195), width=2)
    return img


def make_tavern(size):
    """Tavern – timber-framed, brown thatch roof, a hanging sign."""
    img = _facade(size, (150, 112, 74), (112, 84, 54), "gable")
    draw = ImageDraw.Draw(img)
    for x in range(size // 6, size, size // 5):  # timber framing
        draw.line([(x, size // 3), (x, size * 3 // 4)], fill=(90, 62, 36), width=2)
    draw.rectangle([size - 16, size // 3 + 4, size - 4, size // 2], fill=(60, 40, 22))  # sign
    return img


def make_smithy(size):
    """Smithy – grey stone, chimney with a warm forge glow."""
    img = _facade(size, (96, 96, 102), (70, 62, 56), "gable", windows=1, window_col=(230, 140, 60))
    draw = ImageDraw.Draw(img)
    draw.rectangle([size - 18, 2, size - 8, size // 3], fill=(70, 66, 62))  # chimney
    draw.ellipse([size - 20, -4, size - 6, 8], fill=(240, 180, 90))         # smoke/glow
    return img


def make_hall(size):
    """Town hall – pale stone, tall slate roof, a banner over the door."""
    img = _facade(size, (124, 124, 132), (92, 72, 112), "gable")
    draw = ImageDraw.Draw(img)
    draw.rectangle([size // 2 - 4, size * 3 // 4 - 10, size // 2 + 4, size - 1], fill=(170, 60, 60))  # banner
    return img


def make_chapel(size):
    """Chapel – pale walls, steep spire topped with a cross."""
    img = _facade(size, (200, 196, 186), (120, 110, 140), "spire", windows=1)
    draw = ImageDraw.Draw(img)
    cx = size // 2
    draw.rectangle([cx - 1, size // 12, cx + 1, size // 3], fill=(60, 55, 70))      # cross vertical
    draw.rectangle([cx - 5, size // 7, cx + 5, size // 7 + 2], fill=(60, 55, 70))   # cross arms
    return img


def make_market(size):
    """Market – open stalls under a striped awning."""
    img = _solid(size, (182, 168, 138), amp=8)
    draw = ImageDraw.Draw(img)
    for i, x in enumerate(range(0, size, size // 6)):  # striped awning
        draw.rectangle([x, size // 4, x + size // 6, size // 4 + size // 6],
                       fill=(200, 80, 70) if i % 2 else (235, 230, 220))
    draw.rectangle([size // 8, size // 2, size * 7 // 8, size * 3 // 5], fill=(130, 96, 60))  # counter
    for x in (size // 8, size * 7 // 8 - 4):  # posts
        draw.rectangle([x, size // 4, x + 4, size - 1], fill=(110, 80, 48))
    return img


def make_temple(size):
    """Temple – marble columns beneath a pediment roof."""
    img = _solid(size, (214, 209, 194), amp=5)
    draw = ImageDraw.Draw(img)
    draw.polygon([(2, size // 3), (size // 2, 4), (size - 2, size // 3)], fill=(176, 170, 152))
    draw.rectangle([0, size // 3, size, size // 3 + 4], fill=(150, 144, 126))
    for x in range(size // 6, size * 5 // 6, size // 5):  # columns
        draw.rectangle([x, size // 3 + 6, x + size // 12, size - 1], fill=(228, 224, 212))
        draw.rectangle([x - 1, size // 3 + 6, x + size // 12 + 1, size // 3 + 9], fill=(190, 185, 168))
    return img


def make_guardhouse(size):
    """Guardhouse – blocky grey stone tower with battlements and a flag."""
    img = _solid(size, (104, 104, 112), amp=6)
    draw = ImageDraw.Draw(img)
    for x in range(0, size, size // 5):  # crenellations
        draw.rectangle([x, 0, x + size // 10, size // 10], fill=(80, 80, 88))
    draw.rectangle([size // 2 - size // 10, size * 3 // 5, size // 2 + size // 10, size - 1], fill=(60, 50, 38))  # door
    draw.rectangle([size - 6, 2, size - 4, size // 3], fill=(70, 70, 78))  # flagpole
    draw.polygon([(size - 4, 2), (size - 4, size // 8), (size + 8, size // 16)], fill=(180, 60, 60))
    return img


def make_well(size):
    """Village well – round stone kerb under a little peaked roof on posts."""
    img = _solid(size, (70, 150, 50), amp=10)  # sits on grass
    draw = ImageDraw.Draw(img)
    cx = size // 2
    draw.polygon([(cx - size // 3, size // 3), (cx, size // 12), (cx + size // 3, size // 3)], fill=(130, 80, 48))
    for px in (cx - size // 4, cx + size // 4):  # posts
        draw.rectangle([px - 2, size // 3, px + 2, size * 2 // 3], fill=(96, 64, 36))
    draw.ellipse([cx - size // 4, size // 2, cx + size // 4, size - 2], fill=(120, 120, 126))  # kerb
    draw.ellipse([cx - size // 5, size // 2 + 3, cx + size // 5, size - 5], fill=(40, 60, 90))  # water
    return img


def make_landmark(size):
    """Generic monument – a pale stone obelisk on a base."""
    img = _solid(size, (70, 150, 50), amp=10)
    draw = ImageDraw.Draw(img)
    cx = size // 2
    draw.polygon([(cx - 6, size - 6), (cx, size // 8), (cx + 6, size - 6)], fill=(190, 186, 172))
    draw.rectangle([cx - size // 5, size - 10, cx + size // 5, size - 2], fill=(150, 146, 132))
    return img


def make_signpost(size):
    """Wooden signpost – a post with a board."""
    img = _solid(size, (70, 150, 50), amp=10)
    draw = ImageDraw.Draw(img)
    cx = size // 2
    draw.rectangle([cx - 2, size // 4, cx + 2, size - 2], fill=(100, 68, 38))
    draw.rectangle([cx - size // 3, size // 4, cx + size // 4, size // 4 + size // 6], fill=(150, 112, 64))
    return img


SPRITES = [
    (0, 0, make_tree),
    (1, 0, make_tree_dark),
    (2, 0, make_building),
    (3, 0, make_wood_structure),
    (0, 1, make_character),
    (1, 1, make_animal),
    (2, 1, make_rubble),

    (0, 2, make_well),
    (1, 2, make_landmark),
    (2, 2, make_house),
    (3, 2, make_shop),
    (4, 2, make_barn),
    (5, 2, make_tavern),
    (6, 2, make_smithy),
    (7, 2, make_hall),
    (0, 3, make_chapel),
    (1, 3, make_market),
    (2, 3, make_temple),
    (3, 3, make_guardhouse),
    (4, 3, make_signpost),
]


def main():
    Path(OUTPUT_DIR).mkdir(parents=True, exist_ok=True)
    atlas = Image.new("RGB", (COLS * TILE, ROWS * TILE), (0, 0, 0))
    for col, row, fn in SPRITES:
        atlas.paste(fn(TILE), (col * TILE, row * TILE))
    out = os.path.join(OUTPUT_DIR, "farm_objects.png")
    atlas.save(out)
    print(f"[OK] Generated {out}  ({COLS*TILE}x{ROWS*TILE}, {COLS}x{ROWS} tiles of {TILE}px)")


if __name__ == "__main__":
    main()
