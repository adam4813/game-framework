#!/usr/bin/env python3
"""Generate simple, tileable N64-era textures for the football prototype.

Low-res, limited-palette, dithered/noisy art meant to be tiled at native resolution by the engine's
DrawTexturedRect (source rect = destination size, wrapping via REPEAT). Run from anywhere:

    python games/football/tools/make_textures.py

Outputs PNGs under games/football/assets/textures/.
"""
import os
import random
from PIL import Image

random.seed(64)  # deterministic output
OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "assets", "textures")
os.makedirs(OUT, exist_ok=True)


def clamp(v):
    return max(0, min(255, int(v)))


def save(img, name):
    path = os.path.join(OUT, name)
    img.save(path)
    print("wrote", path)


def noisy_base(w, h, base, amp):
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        for x in range(w):
            n = random.randint(-amp, amp)
            px[x, y] = (clamp(base[0] + n), clamp(base[1] + n), clamp(base[2] + n))
    return img, px


def field(w=128, h=128):
    """Green turf with faint yard lines every 32px (tiles cleanly)."""
    img, px = noisy_base(w, h, (28, 76, 40), 10)
    for y in range(h):
        # alternating mowing bands
        if (y // 16) % 2 == 0:
            for x in range(w):
                r, g, b = px[x, y]
                px[x, y] = (clamp(r + 6), clamp(g + 10), clamp(b + 6))
    for y in range(0, h, 32):  # yard lines
        for x in range(w):
            for yy in (y, (y + 1) % h):
                r, g, b = px[x, yy]
                px[x, yy] = (clamp(r * 0.4 + 150), clamp(g * 0.4 + 160), clamp(b * 0.4 + 150))
    save(img, "field.png")


def panel(w=64, h=64):
    """Dark plastic/metal panel with vertical brushed streaks + fine noise."""
    img, px = noisy_base(w, h, (46, 52, 46), 6)
    for x in range(w):
        streak = random.randint(-8, 8)
        for y in range(h):
            r, g, b = px[x, y]
            px[x, y] = (clamp(r + streak), clamp(g + streak), clamp(b + streak))
    save(img, "panel.png")


def scoreboard(w=64, h=64):
    """Very dark board with green 'LED' speckles for the HUD bar."""
    img, px = noisy_base(w, h, (16, 20, 18), 4)
    for _ in range((w * h) // 22):
        x, y = random.randint(0, w - 1), random.randint(0, h - 1)
        px[x, y] = (40, clamp(140 + random.randint(-30, 60)), 60)
    save(img, "scoreboard.png")


def button(w=64, h=48, base=(64, 100, 72)):
    """Plastic key: vertical bevel (light top, dark bottom) + noise. Tiles horizontally."""
    img = Image.new("RGB", (w, h))
    px = img.load()
    for y in range(h):
        t = y / (h - 1)
        shade = int(26 * (1.0 - t)) - int(22 * t)  # +light top, -dark bottom
        for x in range(w):
            n = random.randint(-5, 5)
            px[x, y] = (clamp(base[0] + shade + n), clamp(base[1] + shade + n), clamp(base[2] + shade + n))
    save(img, "button.png")


def wood(w=64, h=64):
    """Warm wood-ish plate for shop/consumable flavour."""
    img, px = noisy_base(w, h, (92, 62, 40), 8)
    for y in range(h):
        grain = random.randint(-6, 10)
        for x in range(w):
            r, g, b = px[x, y]
            px[x, y] = (clamp(r + grain), clamp(g + grain * 0.6), clamp(b + grain * 0.3))
    save(img, "wood.png")


def menu_bg(w=128, h=128):
    """Dark N64-era menu backdrop: navy field, diagonal chevron hatch + speckle (tiles at 128)."""
    img, px = noisy_base(w, h, (20, 24, 46), 8)
    for y in range(h):
        for x in range(w):
            d = (x + y) % 32
            r, g, b = px[x, y]
            if d < 3:  # bright diagonal rib
                px[x, y] = (clamp(r + 22), clamp(g + 26), clamp(b + 40))
            elif d < 6:  # shadow next to the rib
                px[x, y] = (clamp(r - 8), clamp(g - 8), clamp(b - 6))
    for _ in range((w * h) // 40):  # faint star speckle
        x, y = random.randint(0, w - 1), random.randint(0, h - 1)
        px[x, y] = (clamp(90 + random.randint(-20, 40)), clamp(100 + random.randint(-20, 40)), clamp(150 + random.randint(-20, 60)))
    save(img, "menu_bg.png")


if __name__ == "__main__":
    field()
    panel()
    scoreboard()
    button()
    wood()
    menu_bg()
    print("done")
