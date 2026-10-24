#!/usr/bin/env python3
"""Writes Crabnado's molt upgrade icons as 96x96 PNGs into assets/textures/crabnado/upgrades/, plus the small
arrow (arrow.png) the molt cards put between a stat's old and new value, the 22x22 run-stats panel icons
(time, kills) into assets/textures/crabnado/hud/, and the 48x48 weapon slot icons into
assets/textures/crabnado/weapons/.

Needs Pillow (pip install pillow). Run from anywhere: python3 games/crabnado/tools/make_icons.py
Each icon is a round sea-blue badge with the title's navy outline and a glyph in the UI's coral and cream. Icons are
drawn at 4x and downsampled so the edges stay smooth. The file names match the upgrade ids in
assets/data/crabnado/upgrades.json.
"""
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw

SIZE = 96
SCALE = 4
S = SIZE * SCALE
TEXTURES = Path(__file__).resolve().parents[3] / "assets" / "textures" / "crabnado"
OUT = TEXTURES / "upgrades"
HUD_OUT = TEXTURES / "hud"
WEAPONS_OUT = TEXTURES / "weapons"

NAVY = (18, 40, 70, 255)
SEA = (46, 150, 180, 255)
SEA_DARK = (30, 110, 140, 255)
CORAL = (224, 94, 58, 255)
CREAM = (255, 244, 220, 255)
GOLD = (255, 200, 70, 255)
OUTLINE = 4 * SCALE // 2


def px(v):
    """Converts a 96-px design coordinate to the supersampled canvas."""
    return v * SCALE


def badge():
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    d.ellipse([px(3), px(3), px(93), px(93)], fill=SEA, outline=NAVY, width=px(4))
    return img, d


def circle(d, cx, cy, r, fill, width=OUTLINE):
    d.ellipse([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], fill=fill, outline=NAVY, width=width)


def claw(d, cx, cy, r, facing=-45.0, background=SEA):
    """A pincer opening toward `facing` degrees (0 = right, -90 = up): an arm behind a round claw, with a V notch
    cut from the front so the two tips read as fingers. The notch is filled with `background`."""
    a = math.radians(facing)
    ux, uy = math.cos(a), math.sin(a)

    def at(fwd, side):
        """A point `fwd` along the facing and `side` across it, in design pixels."""
        return px(cx + ux * fwd - uy * side), px(cy + uy * fwd + ux * side)

    arm = [at(-r * 0.4, -r * 0.32), at(-r * 1.6, -r * 0.3), at(-r * 1.6, r * 0.3), at(-r * 0.4, r * 0.32)]
    d.polygon(arm, fill=CORAL, outline=NAVY, width=OUTLINE)
    circle(d, cx, cy, r, CORAL)
    # The notch starts ahead of the centre (so it isn't a pie slice) and widens past the rim.
    notch = [at(r * 0.05, 0), at(r * 1.05, -r * 0.38), at(r * 1.05, r * 0.38)]
    d.polygon(notch, fill=background)
    rim = math.sqrt(r * r - (r * 0.36) ** 2)
    d.line([at(rim, -r * 0.36), notch[0], at(rim, r * 0.36)], fill=NAVY, width=OUTLINE, joint="curve")


def star(d, cx, cy, r, fill=GOLD):
    pts = []
    for i in range(8):
        a = math.radians(-90 + i * 45)
        rr = r if i % 2 == 0 else r * 0.38
        pts.append((px(cx + math.cos(a) * rr), px(cy + math.sin(a) * rr)))
    d.polygon(pts, fill=fill, outline=NAVY, width=OUTLINE // 2)


def arrow_up(d, cx, top, h, w, fill=CREAM):
    head = h * 0.45
    pts = [
        (cx, top), (cx + w, top + head), (cx + w * 0.4, top + head), (cx + w * 0.4, top + h),
        (cx - w * 0.4, top + h), (cx - w * 0.4, top + head), (cx - w, top + head),
    ]
    d.polygon([(px(x), px(y)) for x, y in pts], fill=fill, outline=NAVY, width=OUTLINE)


def speed_lines(d, x0, ys, length):
    for y in ys:
        d.line([px(x0), px(y), px(x0 + length), px(y)], fill=CREAM, width=px(4))


def bubble(d, cx, cy, r):
    d.ellipse([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], fill=(190, 235, 245, 255), outline=NAVY, width=OUTLINE)
    hr = r * 0.3
    d.ellipse([px(cx - r * 0.45 - hr), px(cy - r * 0.45 - hr), px(cx - r * 0.45 + hr), px(cy - r * 0.45 + hr)], fill=CREAM)


def wave(img, d, crest_scale=1.0):
    """A curling wave: a cream crest curling over a band of water."""
    r = 22 * crest_scale
    cx, cy = 50, 60 - r * 0.55
    d.ellipse([px(cx - r), px(cy - r), px(cx + r), px(cy + r)], fill=CREAM, outline=NAVY, width=OUTLINE)
    ir = r * 0.62
    ix, iy = cx + r * 0.28, cy + r * 0.22
    d.ellipse([px(ix - ir), px(iy - ir), px(ix + ir), px(iy + ir)], fill=SEA, outline=NAVY, width=OUTLINE)
    pts = [(8, 92)]
    for i in range(0, 81):
        x = 8 + i
        pts.append((x, 64 + math.sin(x / 9.0) * 3))
    pts.append((88, 92))
    clip = Image.new("L", (S, S), 0)
    ImageDraw.Draw(clip).ellipse([px(6), px(6), px(90), px(90)], fill=255)
    band = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    bd = ImageDraw.Draw(band)
    bd.polygon([(px(x), px(y)) for x, y in pts], fill=SEA_DARK, outline=NAVY, width=OUTLINE)
    # Clipped to the badge so the band's corners don't poke past its rim.
    img.alpha_composite(Image.merge("RGBA", (*band.split()[:3], ImageChops.multiply(band.getchannel("A"), clip))))


def crab(d, cx, cy, r):
    for side in (-1, 1):
        for k in range(3):
            x = cx + side * (r * 0.6 + k * r * 0.18)
            d.line([px(x), px(cy + r * 0.2), px(x + side * r * 0.35), px(cy + r * 0.85)], fill=NAVY, width=px(3))
        circle(d, cx + side * r * 1.05, cy - r * 0.55, r * 0.38, CORAL)
    d.ellipse([px(cx - r), px(cy - r * 0.6), px(cx + r), px(cy + r * 0.6)], fill=CORAL, outline=NAVY, width=OUTLINE)
    for side in (-1, 1):
        circle(d, cx + side * r * 0.35, cy - r * 0.62, r * 0.16, CREAM, OUTLINE // 2)


def long_reach(_img, d):
    for rr in (40, 33):
        d.arc([px(48 - rr), px(48 - rr), px(48 + rr), px(48 + rr)], 200, 340, fill=CREAM, width=px(3))
        d.arc([px(48 - rr), px(48 - rr), px(48 + rr), px(48 + rr)], 20, 160, fill=CREAM, width=px(3))
    claw(d, 48, 50, 20)


def sharper_claws(_img, d):
    claw(d, 44, 54, 22)
    star(d, 68, 28, 15)


def deep_roots(_img, d):
    pts = [(54, 12), (28, 52), (46, 52), (38, 84), (68, 40), (50, 40), (60, 12)]
    d.polygon([(px(x), px(y)) for x, y in pts], fill=GOLD, outline=NAVY, width=OUTLINE)


def scuttle(_img, d):
    speed_lines(d, 10, (38, 50, 62), 18)
    crab(d, 56, 52, 20)


def thick_shell(_img, d):
    # A hermit shell (spiral cone) with a heart badge.
    d.ellipse([px(20), px(24), px(70), px(74)], fill=CREAM, outline=NAVY, width=OUTLINE)
    for r in (17, 10, 4):
        d.arc([px(45 - r), px(49 - r), px(45 + r), px(49 + r)], 180, 450, fill=SEA_DARK, width=px(3))
    hx, hy, hr = 66, 66, 10
    d.ellipse([px(hx - hr * 1.8), px(hy - hr * 1.2), px(hx), px(hy + hr * 0.6)], fill=CORAL)
    d.ellipse([px(hx), px(hy - hr * 1.2), px(hx + hr * 1.8), px(hy + hr * 0.6)], fill=CORAL)
    d.polygon([(px(hx - hr * 1.75), px(hy - hr * 0.1)), (px(hx + hr * 1.75), px(hy - hr * 0.1)), (px(hx), px(hy + hr * 1.7))], fill=CORAL)
    d.line([px(hx - 6), px(hy - 2), px(hx + 6), px(hy - 2)], fill=CREAM, width=px(3))
    d.line([px(hx), px(hy - 8), px(hx), px(hy + 4)], fill=CREAM, width=px(3))


def tide_pull(_img, d):
    # A horseshoe magnet pulling a shell.
    box = [px(14), px(16), px(62), px(64)]
    d.arc(box, 90, 270, fill=NAVY, width=px(18))
    d.arc([box[0] + px(2), box[1] + px(2), box[2] - px(2), box[3] - px(2)], 90, 270, fill=CORAL, width=px(14))
    for y in (16, 50):
        d.rectangle([px(38), px(y + 1), px(54), px(y + 13)], fill=CREAM, outline=NAVY, width=OUTLINE)
    d.ellipse([px(62), px(32), px(84), px(54)], fill=CREAM, outline=NAVY, width=OUTLINE)
    d.arc([px(67), px(37), px(79), px(49)], 180, 450, fill=SEA_DARK, width=px(3))


def bubble_burst(_img, d):
    bubble(d, 36, 58, 17)
    bubble(d, 62, 62, 13)
    bubble(d, 50, 32, 12)
    d.line([px(70), px(24), px(70), px(44)], fill=CREAM, width=px(5))
    d.line([px(60), px(34), px(80), px(34)], fill=CREAM, width=px(5))


def quick_bubbles(_img, d):
    speed_lines(d, 12, (36, 48, 60), 22)
    bubble(d, 58, 48, 22)


def riptide(img, d):
    wave(img, d, 1.0)


def stronger_riptide(img, d):
    wave(img, d, 1.3)
    arrow_up(d, 76, 14, 26, 10, GOLD)


ICONS = {
    "long_reach": long_reach,
    "sharper_claws": sharper_claws,
    "deep_roots": deep_roots,
    "scuttle": scuttle,
    "thick_shell": thick_shell,
    "tide_pull": tide_pull,
    "bubble_burst": bubble_burst,
    "quick_bubbles": quick_bubbles,
    "riptide": riptide,
    "stronger_riptide": stronger_riptide,
}


ARROW_SIZE = (20, 14)


def arrow():
    """A navy right-pointing arrow for the card stat lines, sized to sit between 19 px text."""
    w, h = ARROW_SIZE[0] * SCALE, ARROW_SIZE[1] * SCALE
    img = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    shaft = h * 0.34
    head = w * 0.45
    d.rectangle([0, (h - shaft) / 2, w - head + 2, (h + shaft) / 2], fill=NAVY)
    d.polygon([(w - head, 0), (w, h / 2), (w - head, h)], fill=NAVY)
    return img.resize(ARROW_SIZE, Image.LANCZOS)


HUD_SIZE = 22
# Outline width on the 96-px design grid; thick, so it survives the shrink to 22 px.
HUD_OUTLINE = px(9)


def hud_time(d):
    d.ellipse([px(8), px(8), px(88), px(88)], fill=CREAM, outline=NAVY, width=HUD_OUTLINE)
    d.line([px(48), px(48), px(48), px(24)], fill=NAVY, width=px(10))
    d.line([px(48), px(48), px(66), px(58)], fill=NAVY, width=px(10))


def hud_kills(d):
    # A crab: claws up either side of a round body.
    for side in (-1, 1):
        d.ellipse([px(48 + side * 30 - 14), px(14), px(48 + side * 30 + 14), px(42)], fill=CORAL, outline=NAVY, width=HUD_OUTLINE)
    d.ellipse([px(12), px(34), px(84), px(86)], fill=CORAL, outline=NAVY, width=HUD_OUTLINE)
    for side in (-1, 1):
        d.ellipse([px(48 + side * 15 - 7), px(46), px(48 + side * 15 + 7), px(60)], fill=NAVY)


HUD_ICONS = {"time": hud_time, "kills": hud_kills}


WEAPON_SIZE = 48


def weapon_claw_snap(_img, d):
    claw(d, 50, 50, 26)


def weapon_bubbles(_img, d):
    bubble(d, 38, 56, 20)
    bubble(d, 62, 36, 15)
    bubble(d, 66, 66, 10)


# The upgrade badges, drawn smaller: the same look in the HUD as on the molt cards.
WEAPON_ICONS = {"claw_snap": weapon_claw_snap, "bubbles": weapon_bubbles, "riptide": lambda img, d: wave(img, d, 1.0)}


def main():
    WEAPONS_OUT.mkdir(parents=True, exist_ok=True)
    for name, draw in WEAPON_ICONS.items():
        img, d = badge()
        draw(img, d)
        img.resize((WEAPON_SIZE, WEAPON_SIZE), Image.LANCZOS).save(WEAPONS_OUT / f"{name}.png")
        print(f"wrote {WEAPONS_OUT / name}.png")
    HUD_OUT.mkdir(parents=True, exist_ok=True)
    for name, draw in HUD_ICONS.items():
        img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
        draw(ImageDraw.Draw(img))
        img.resize((HUD_SIZE, HUD_SIZE), Image.LANCZOS).save(HUD_OUT / f"{name}.png")
        print(f"wrote {HUD_OUT / name}.png")
    OUT.mkdir(parents=True, exist_ok=True)
    arrow().save(OUT / "arrow.png")
    print(f"wrote {OUT / 'arrow.png'}")
    for name, draw in ICONS.items():
        img, d = badge()
        draw(img, d)
        img.resize((SIZE, SIZE), Image.LANCZOS).save(OUT / f"{name}.png")
        print(f"wrote {OUT / name}.png")


if __name__ == "__main__":
    main()
