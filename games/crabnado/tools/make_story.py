#!/usr/bin/env python3
"""Writes the story comic shown between the title and the arena into assets/textures/crabnado/story/: the page
(page.png, 1280x720, the newsprint backdrop with the masthead) and its three panels (panel_1.png .. panel_3.png,
392x540 each). The story scene draws them 1:1, so these sizes are the on-screen sizes.

Needs Pillow (pip install pillow). Run from anywhere: python3 games/crabnado/tools/make_story.py
The look is a golden-age comic: thick black ink over flat four-colour fills, Ben-Day dots, colour printed a hair out of
register, and caption boxes and balloons lettered in the UI font on yellowed newsprint. Everything is drawn at 3x and
downsampled so the ink stays smooth. The copy lives here, baked into the art.
"""
import math
import random
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter, ImageFont

SCALE = 3
PANEL_W, PANEL_H = 392, 540
PAGE_W, PAGE_H = 1280, 720
ROOT = Path(__file__).resolve().parents[3]
OUT = ROOT / "assets" / "textures" / "crabnado" / "story"
FONT_PATH = ROOT / "assets" / "fonts" / "crabnado" / "LilitaOne-Regular.ttf"

# A four-colour newsprint palette: warm paper, the process primaries a little dirty, and their two-colour mixes.
INK = (22, 18, 20, 255)
CLEAR = (0, 0, 0, 0)
PAPER = (246, 234, 204, 255)
WHITE = (255, 252, 240, 255)
YELLOW = (250, 210, 60, 255)
GOLD = (236, 168, 40, 255)
RED = (214, 46, 40, 255)
ORANGE = (238, 112, 48, 255)
CORAL = (240, 140, 110, 255)
PINK = (246, 186, 160, 255)
BLUE = (40, 90, 170, 255)
CYAN = (70, 170, 210, 255)
SKY = (150, 205, 225, 255)
NAVY = (26, 36, 80, 255)
GREEN = (60, 150, 80, 255)
LEAF = (110, 180, 70, 255)
PURPLE = (110, 50, 140, 255)
VIOLET = (178, 92, 230, 255)
LILAC = (222, 180, 255, 255)
SAND = (240, 206, 130, 255)
BROWN = (130, 82, 46, 255)
GREY = (120, 120, 140, 255)
STORM = (70, 74, 100, 255)

# The colour plate is printed this far off the ink plate, in final pixels.
MISREGISTER = (2, 1)


def s(v):
    """A final-pixel length on the 3x canvas."""
    return int(round(v * SCALE))


def pts(points):
    return [(s(x), s(y)) for x, y in points]


def font(size):
    return ImageFont.truetype(str(FONT_PATH), s(size))


class Canvas:
    """Two plates the size of a panel: `color` holds the flat fills, `ink` the black line work. `finish` prints them
    on newsprint, the colour plate slightly out of register under the ink."""

    def __init__(self, w, h, ground=PAPER):
        self.w, self.h = w, h
        self.color = Image.new("RGBA", (s(w), s(h)), ground)
        self.ink = Image.new("RGBA", (s(w), s(h)), (0, 0, 0, 0))
        self.c = ImageDraw.Draw(self.color)
        self.i = ImageDraw.Draw(self.ink)

    # ── Shapes: a fill on the colour plate and an outline on the ink plate. A fill also clears the ink under it, so
    # later shapes cover earlier line work the way they would on a hand-inked page. ──

    def poly(self, points, fill=None, width=3.0):
        p = pts(points)
        if fill:
            self.c.polygon(p, fill=fill)
            self.i.polygon(p, fill=CLEAR)
        if width:
            self.i.line(p + [p[0]], fill=INK, width=s(width), joint="curve")
            for x, y in p:
                r = s(width) / 2
                self.i.ellipse([x - r, y - r, x + r, y + r], fill=INK)

    def ellipse(self, box, fill=None, width=3.0):
        b = [s(v) for v in box]
        if fill:
            self.c.ellipse(b, fill=fill)
            self.i.ellipse(b, fill=CLEAR)
        if width:
            self.i.ellipse(b, outline=INK, width=s(width))

    def circle(self, cx, cy, r, fill=None, width=3.0):
        self.ellipse([cx - r, cy - r, cx + r, cy + r], fill, width)

    def stroke(self, points, width, color, rim=3.0):
        """A thick round-ended limb (a leg, an arm, a stalk, a trunk): `color`, `width` final pixels across, ringed
        with ink `rim` pixels wide, covering what is under it."""
        p = pts(points)
        outer, inner = s(width + rim), s(width - rim)
        self.i.line(p, fill=INK, width=outer, joint="curve")
        for x, y in (p[0], p[-1]):
            self.i.ellipse([x - outer / 2, y - outer / 2, x + outer / 2, y + outer / 2], fill=INK)
        self.i.line(p, fill=CLEAR, width=inner, joint="curve")
        for x, y in (p[0], p[-1]):
            self.i.ellipse([x - inner / 2, y - inner / 2, x + inner / 2, y + inner / 2], fill=CLEAR)
        self.c.line(p, fill=color, width=outer, joint="curve")

    def line(self, points, width=3.0, color=INK, plate="ink"):
        d = self.i if plate == "ink" else self.c
        d.line(pts(points), fill=color, width=s(width), joint="curve")

    def arc(self, box, start, end, width=3.0):
        self.i.arc([s(v) for v in box], start, end, fill=INK, width=s(width))

    def fill_ink(self, points):
        self.i.polygon(pts(points), fill=INK)

    def fill_color(self, points, color):
        self.c.polygon(pts(points), fill=color)

    def mask(self):
        return Image.new("L", self.color.size, 0)

    def paste(self, layer, mask):
        self.color.paste(layer, (0, 0), mask)

    # ── Print ──

    def finish(self, border=4.0):
        paper = Image.new("RGBA", self.color.size, PAPER)
        offset = ImageChops.offset(self.color, s(MISREGISTER[0]), s(MISREGISTER[1]))
        printed = Image.alpha_composite(paper, offset)
        printed = Image.alpha_composite(printed, self.ink)
        if border:
            d = ImageDraw.Draw(printed)
            d.rectangle([0, 0, printed.width - 1, printed.height - 1], outline=INK, width=s(border))
        small = printed.resize((self.w, self.h), Image.LANCZOS)
        return newsprint(small)


def newsprint(img):
    """Yellows and grains the print like cheap pulp paper."""
    noise = Image.effect_noise(img.size, 18).convert("RGBA")
    grain = Image.blend(Image.new("RGBA", img.size, (128, 128, 128, 255)), noise, 0.25)
    out = ImageChops.overlay(img, grain)
    tint = Image.new("RGBA", img.size, (250, 230, 190, 255))
    out = ImageChops.multiply(out, Image.blend(Image.new("RGBA", img.size, (255, 255, 255, 255)), tint, 0.35))
    out.putalpha(255)
    return out


# ── Ben-Day dots and fills ──


def dots(size, color, spacing, radius, ground=None, fade=None, angle_offset=True):
    """A layer of Ben-Day dots `spacing` apart. `radius` is a dot's radius, or with `fade` a function of the dot's
    y fraction returning its radius, for the graded skies golden-age printers faked with dot size."""
    layer = Image.new("RGBA", size, ground or (0, 0, 0, 0))
    d = ImageDraw.Draw(layer)
    sp = s(spacing)
    w, h = size
    for row, y in enumerate(range(-sp, h + sp, sp)):
        shift = sp // 2 if angle_offset and row % 2 else 0
        r = s(fade(y / h)) if fade else s(radius)
        if r <= 0:
            continue
        for x in range(-sp + shift, w + sp, sp):
            d.ellipse([x - r, y - r, x + r, y + r], fill=color)
    return layer


def dot_fill(cv, mask, base, dot_color, spacing=6.0, radius=1.6, fade=None):
    """Floods `mask` on the colour plate with `base` overprinted by dots."""
    layer = Image.new("RGBA", cv.color.size, base)
    layer = Image.alpha_composite(layer, dots(cv.color.size, dot_color, spacing, radius, fade=fade))
    cv.paste(layer, mask)


def rect_mask(cv, box):
    m = cv.mask()
    ImageDraw.Draw(m).rectangle([s(v) for v in box], fill=255)
    return m


def poly_mask(cv, points):
    m = cv.mask()
    ImageDraw.Draw(m).polygon(pts(points), fill=255)
    return m


def sunburst(cv, cx, cy, colors, rays=24, reach=900, spin=0.0):
    """Alternating wedges radiating from (cx, cy): the heroic splash background."""
    for k in range(rays):
        a0 = spin + k * 2 * math.pi / rays
        a1 = spin + (k + 1) * 2 * math.pi / rays
        p = [(cx, cy), (cx + math.cos(a0) * reach, cy + math.sin(a0) * reach),
             (cx + math.cos(a1) * reach, cy + math.sin(a1) * reach)]
        cv.fill_color(p, colors[k % len(colors)])


def speed_lines(cv, cx, cy, count, r0, r1, width=1.6, seed=1):
    rng = random.Random(seed)
    for _ in range(count):
        a = rng.uniform(0, 2 * math.pi)
        a0, a1 = rng.uniform(r0, r0 * 1.25), rng.uniform(r1 * 0.85, r1)
        cv.line([(cx + math.cos(a) * a0, cy + math.sin(a) * a0), (cx + math.cos(a) * a1, cy + math.sin(a) * a1)],
                width=width)


# ── Lettering ──


def wrap(text, fnt, width):
    lines, line = [], ""
    for word in text.split():
        trial = f"{line} {word}".strip()
        if fnt.getlength(trial) <= width or not line:
            line = trial
        else:
            lines.append(line)
            line = word
    lines.append(line)
    return lines


def text_block(text, size, width, emphasis=()):
    """Lays out upper-case `text` wrapped to `width` final pixels. Returns (lines, line height, block height, font).
    Words in `emphasis` are drawn in the bold-red style a letterer used for shouted words."""
    fnt = font(size)
    lines = wrap(text.upper(), fnt, s(width))
    lh = s(size * 1.12)
    return lines, lh, lh * len(lines), fnt


def letter(cv, x, y, lines, lh, fnt, width, emphasis=(), align="center"):
    """Letters `lines` from (x, y) (3x pixels) inside a box `width` final pixels wide, on the ink plate. Words in
    `emphasis` are printed red on the colour plate with an ink outline."""
    for n, line in enumerate(lines):
        lw = fnt.getlength(line)
        cx = x + (s(width) - lw) / 2 if align == "center" else x
        ty = y + n * lh
        for word in line.split(" "):
            plain = word.strip(".,!?'\"—-")
            ww = fnt.getlength(word)
            if plain in emphasis:
                cv.i.text((cx, ty), word, font=fnt, fill=RED, stroke_width=max(1, s(0.6)), stroke_fill=INK)
            else:
                cv.i.text((cx, ty), word, font=fnt, fill=INK)
            cx += ww + fnt.getlength(" ")


def caption(cv, x, y, w, text, size=15.0, emphasis=(), color=YELLOW):
    """A narrator's box: lettering on a flat yellow rectangle, ink-ruled. Returns its bottom in final pixels."""
    pad = 7
    lines, lh, bh, fnt = text_block(text, size, w - pad * 2)
    h = bh / SCALE + pad * 2 - size * 0.12
    cv.poly([(x, y), (x + w, y), (x + w, y + h), (x, y + h)], fill=color, width=2.5)
    letter(cv, s(x + pad), s(y + pad) - s(size * 0.1), lines, lh, fnt, w - pad * 2, emphasis)
    return y + h


def balloon(cv, cx, cy, w, text, tail, size=15.0, emphasis=(), shout=False):
    """A speech balloon centred on (cx, cy), `w` final pixels wide, its tail pointing at `tail`. A `shout` balloon has
    a jagged burst edge. Returns the balloon's (top, bottom)."""
    lines, lh, bh, fnt = text_block(text, size, w * 0.74)
    h = bh / SCALE + size * 1.6
    rx, ry = w / 2, h / 2
    # The tail: a curved wedge from the balloon's edge toward the speaker.
    ang = math.atan2(tail[1] - cy, tail[0] - cx)
    base = 0.22
    b0 = (cx + math.cos(ang - base) * rx * 0.8, cy + math.sin(ang - base) * ry * 0.8)
    b1 = (cx + math.cos(ang + base) * rx * 0.8, cy + math.sin(ang + base) * ry * 0.8)
    tail_poly = [b0, tail, b1]
    if shout:
        spikes = 26
        burst = []
        for k in range(spikes * 2):
            a = k * math.pi / spikes
            out = 1.12 if k % 2 == 0 else 0.94
            burst.append((cx + math.cos(a) * rx * out, cy + math.sin(a) * ry * out))
        cv.poly(burst, fill=WHITE, width=2.5)
        cv.poly(tail_poly, fill=WHITE, width=2.5)
        cv.fill_color(burst, WHITE)
        cv.i.polygon(pts([(cx + math.cos(k * math.pi / spikes) * rx * (1.12 if k % 2 == 0 else 0.94) * 0.97,
                           cy + math.sin(k * math.pi / spikes) * ry * (1.12 if k % 2 == 0 else 0.94) * 0.97)
                          for k in range(spikes * 2)]), fill=(0, 0, 0, 0))
    else:
        cv.poly(tail_poly, fill=WHITE, width=2.5)
        cv.ellipse([cx - rx, cy - ry, cx + rx, cy + ry], fill=WHITE, width=2.5)
    # Paint out the ink where the tail meets the balloon so they read as one shape.
    hole = Image.new("L", cv.ink.size, 0)
    hd = ImageDraw.Draw(hole)
    if shout:
        hd.polygon(pts([(cx + math.cos(k * math.pi / 26) * rx * (1.12 if k % 2 == 0 else 0.94) * 0.96,
                         cy + math.sin(k * math.pi / 26) * ry * (1.12 if k % 2 == 0 else 0.94) * 0.96)
                        for k in range(52)]), fill=255)
    else:
        e = 1.3
        hd.ellipse([s(cx - rx + e), s(cy - ry + e), s(cx + rx - e), s(cy + ry - e)], fill=255)
    hd.polygon(pts([(cx + (b0[0] - cx) * 0.9, cy + (b0[1] - cy) * 0.9), (cx, cy),
                    (cx + (b1[0] - cx) * 0.9, cy + (b1[1] - cy) * 0.9)]), fill=255)
    cv.ink.paste((0, 0, 0, 0), (0, 0), hole)
    cv.color.paste(WHITE, (0, 0), hole)
    letter(cv, s(cx - w * 0.37), s(cy) - bh / 2 - s(size * 0.1), lines, lh, fnt, w * 0.74, emphasis)
    return cy - ry, cy + ry


def sound_effect(cv, x, y, text, size, fill=YELLOW, outline=RED, angle=-8):
    """A big lettered sound effect: fill, a thick coloured outline, then an ink rim, rotated `angle` degrees."""
    fnt = font(size)
    pad = s(size * 0.5)
    w = int(fnt.getlength(text)) + pad * 2
    h = s(size * 1.5) + pad * 2
    color = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ink = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    ImageDraw.Draw(ink).text((pad, pad), text, font=fnt, fill=INK, stroke_width=s(5), stroke_fill=INK)
    ImageDraw.Draw(color).text((pad, pad), text, font=fnt, fill=fill, stroke_width=s(3), stroke_fill=outline)
    ink = ink.rotate(angle, resample=Image.BICUBIC, expand=True)
    color = color.rotate(angle, resample=Image.BICUBIC, expand=True)
    at = (s(x) - ink.width // 2, s(y) - ink.height // 2)
    cv.ink.alpha_composite(ink, at)
    cv.ink.alpha_composite(color, at)


# ── Characters ──


def eye(cv, x, y, r, look=(0.0, 0.0), angry=0.0, red=False, lid_side=1):
    """An eye on the end of a stalk: a white ball, a pupil looking along `look`, and a slanted brow when `angry`."""
    cv.circle(x, y, r, fill=WHITE, width=2.5)
    pr = r * 0.48
    cv.circle(x + look[0] * r * 0.4, y + look[1] * r * 0.4, pr, fill=RED if red else INK, width=0 if not red else 1.5)
    if not red:
        cv.i.ellipse([s(x + look[0] * r * 0.4 - pr), s(y + look[1] * r * 0.4 - pr),
                      s(x + look[0] * r * 0.4 + pr), s(y + look[1] * r * 0.4 + pr)], fill=INK)
        cv.circle(x + look[0] * r * 0.4 - pr * 0.35, y + look[1] * r * 0.4 - pr * 0.35, pr * 0.3, fill=WHITE, width=0)
        cv.i.ellipse([s(x + look[0] * r * 0.4 - pr * 0.65), s(y + look[1] * r * 0.4 - pr * 0.65),
                      s(x + look[0] * r * 0.4 - pr * 0.05), s(y + look[1] * r * 0.4 - pr * 0.05)], fill=WHITE)
    if angry:
        # A brow: a heavy wedge slanting down toward the face's centre.
        dy = angry * r * 0.9
        cv.fill_ink([(x - r * 1.2, y - r * 1.05 - dy * lid_side), (x + r * 1.2, y - r * 1.05 + dy * lid_side),
                     (x + r * 1.2, y - r * 0.55 + dy * lid_side), (x - r * 1.2, y - r * 0.55 - dy * lid_side)])


def pincer(cv, x, y, size, angle, color, open_=0.5, width=3.0, shade=None):
    """A crab claw whose palm sits at (x, y) and whose fingers point along `angle` (radians), opened by `open_`."""
    ux, uy = math.cos(angle), math.sin(angle)
    vx, vy = -uy, ux

    def at(f, side):
        return (x + ux * f * size + vx * side * size, y + uy * f * size + vy * side * size)

    palm = [at(-0.55, -0.45), at(0.15, -0.62), at(0.55, -0.48), at(0.62, 0.0), at(0.55, 0.48), at(0.0, 0.6),
            at(-0.6, 0.42), at(-0.75, 0.0)]
    gap = open_ * 0.35
    top = [at(0.4, -0.5), at(0.9, -0.55 - gap), at(1.35, -0.32 - gap * 1.6), at(1.15, -0.12 - gap * 0.6),
           at(0.55, -0.05)]
    bottom = [at(0.45, 0.42), at(1.0, 0.36 + gap * 0.4), at(1.22, 0.12 + gap * 0.8), at(0.95, 0.04 + gap * 0.3),
              at(0.55, 0.08)]
    cv.poly(bottom, fill=color, width=width)
    cv.poly(top, fill=color, width=width)
    cv.poly(palm, fill=color, width=width)
    # Knuckle bumps along the top finger for that chunky inked-crab look.
    for f in (0.75, 1.0):
        bx, by = at(f, -0.48 - gap * (0.4 + f * 0.6))
        cv.circle(bx, by, size * 0.06, fill=None, width=width * 0.6)
    if shade:
        sh = [at(-0.5, 0.1), at(0.4, 0.2), at(0.45, 0.45), at(-0.4, 0.4)]
        cv.fill_color(sh, shade)


def legs(cv, x, y, spread, size, color, side, count=3, width=3.0, lift=0.0):
    """Jointed walking legs on one `side` (-1 left, +1 right), from the body edge at (x, y)."""
    for k in range(count):
        oy = y + (k - (count - 1) / 2) * size * 0.32
        knee = (x + side * size * 0.55, oy - size * (0.35 + lift) + k * size * 0.05)
        foot = (x + side * size * (0.85 + k * 0.08), oy + size * 0.35 - lift * size * 0.2)
        cv.stroke([(x, oy), knee, foot], size * 0.13, color, rim=width)


def hermit_king(cv, x, y, scale=1.0, claw_angle=-1.9, look=(0.3, -0.3), mouth="shout"):
    """The hero: a red hermit crab in a great coral spiral shell, a gold crown between his eye stalks, his big claw
    raised. (x, y) is the middle of his body; `scale` is relative to a crab about 230 px tall."""
    k = scale

    def p(dx, dy):
        return x + dx * k, y + dy * k

    # The shell sits behind and to the left, a spiral of whorls.
    sx, sy = p(-48, 18)
    cv.ellipse([sx - 78 * k, sy - 70 * k, sx + 78 * k, sy + 70 * k], fill=CORAL, width=3.5)
    m = cv.mask()
    ImageDraw.Draw(m).ellipse([s(sx - 78 * k), s(sy - 70 * k), s(sx + 78 * k), s(sy + 70 * k)], fill=255)
    lower = cv.mask()
    ImageDraw.Draw(lower).rectangle([0, s(sy + 5 * k), cv.color.width, cv.color.height], fill=255)
    dot_fill(cv, ImageChops.multiply(m, lower), CORAL, RED, spacing=5.5, radius=1.3)
    for n in range(4):
        r = 70 - n * 17
        cv.arc([sx - r * k * 1.05 - n * 6 * k, sy - r * k + n * 2 * k, sx + r * k * 1.05 - n * 6 * k,
                sy + r * k + n * 2 * k], 140 + n * 25, 400 + n * 25, width=3)
    cv.circle(sx - 24 * k, sy + 4 * k, 9 * k, fill=PINK, width=2.5)
    # Ridges on the shell's lip.
    for a in range(-40, 50, 18):
        ra = math.radians(a)
        cv.line([(sx + math.cos(ra) * 60 * k, sy + math.sin(ra) * 55 * k),
                 (sx + math.cos(ra) * 78 * k, sy + math.sin(ra) * 70 * k)], width=2.5)

    # Back legs, then the body peeking from the shell mouth.
    legs(cv, *p(8, 52), 1, 70 * k, RED, 1, count=3)
    legs(cv, *p(-20, 60), 1, 60 * k, RED, -1, count=2)
    body = [p(-10, -30), p(30, -42), p(62, -20), p(70, 20), p(48, 56), p(0, 62), p(-22, 30)]
    cv.poly(body, fill=RED, width=3.5)
    m = poly_mask(cv, [p(10, 10), p(70, 20), p(48, 56), p(0, 62), p(-22, 30)])
    dot_fill(cv, m, RED, (150, 24, 30, 255), spacing=5, radius=1.4)
    # Segment lines on the body.
    for n in range(3):
        cv.arc([x + (-5 + n * 4) * k, y + (-20 + n * 22) * k, x + (66 - n * 2) * k, y + (10 + n * 22) * k], 200, 330,
               width=2.2)

    # The small claw, low and to the front.
    pincer(cv, *p(70, 38), 26 * k, 0.4, ORANGE, open_=0.3, shade=RED)
    # Eye stalks and eyes.
    for ex, tilt in ((18, -6), (48, 6)):
        top = p(ex + tilt, -98)
        cv.stroke([p(ex, -36), top], 9 * k, RED)
        eye(cv, top[0], top[1], 13 * k, look=look, angry=0.25, lid_side=1 if ex < 30 else -1)
    # The crown, between and behind the stalks: a gold band with three points and red jewels.
    cx, cy = p(33, -52)
    crown = [(cx - 22 * k, cy + 10 * k), (cx - 24 * k, cy - 14 * k), (cx - 12 * k, cy - 2 * k), (cx, cy - 22 * k),
             (cx + 12 * k, cy - 2 * k), (cx + 24 * k, cy - 14 * k), (cx + 22 * k, cy + 10 * k)]
    cv.poly(crown, fill=YELLOW, width=3)
    cv.fill_color([(cx - 22 * k, cy + 4 * k), (cx + 22 * k, cy + 4 * k), (cx + 22 * k, cy + 10 * k),
                   (cx - 22 * k, cy + 10 * k)], GOLD)
    for jx, jy in ((cx, cy - 22 * k), (cx - 24 * k, cy - 14 * k), (cx + 24 * k, cy - 14 * k)):
        cv.circle(jx, jy, 4 * k, fill=RED, width=2)
    cv.circle(cx, cy + 3 * k, 4.5 * k, fill=CYAN, width=2)
    # The mouth: an open, shouting heroic grin.
    mx, my = p(46, -8)
    if mouth == "shout":
        cv.poly([(mx - 14 * k, my - 4 * k), (mx + 12 * k, my - 6 * k), (mx + 6 * k, my + 12 * k),
                 (mx - 8 * k, my + 11 * k)], fill=(120, 20, 30, 255), width=2.5)
        cv.fill_color([(mx - 12 * k, my - 3 * k), (mx + 10 * k, my - 5 * k), (mx + 9 * k, my), (mx - 11 * k, my + 1 * k)],
                      WHITE)
    else:
        cv.arc([mx - 14 * k, my - 14 * k, mx + 12 * k, my + 6 * k], 20, 160, width=3)
    # The big claw, raised high, arm then pincer.
    ax, ay = p(-14, -12)
    hx, hy = ax + math.cos(claw_angle) * 70 * k, ay + math.sin(claw_angle) * 70 * k
    cv.stroke([(ax, ay), ((ax + hx) / 2 - 10 * k, (ay + hy) / 2), (hx, hy)], 18 * k, RED)
    pincer(cv, hx, hy, 46 * k, claw_angle, ORANGE, open_=0.9, shade=RED)


def warlord(cv, x, y, scale=1.0, claws_up=False, look=(-0.4, 0.2), sneer=True):
    """The villain, Warlord Snapjaw: a wide spiked purple crab under a pirate bicorne, one eye patched, one claw
    far too big. (x, y) is the carapace's middle; `scale` is relative to a crab about 260 px wide."""
    k = scale

    def p(dx, dy):
        return x + dx * k, y + dy * k

    legs(cv, *p(-70, 30), 1, 80 * k, PURPLE, -1, count=4, lift=0.2)
    legs(cv, *p(70, 30), 1, 80 * k, PURPLE, 1, count=4, lift=0.2)
    # Carapace: a wide shield with spikes along its brow.
    shell = []
    for n in range(19):
        a = math.pi + n * math.pi / 18
        r = 1.0 if n % 2 == 0 else 1.13
        if n in (0, 18):
            r = 1.0
        shell.append(p(math.cos(a) * 112 * r, math.sin(a) * 62 * r + 10))
    shell += [p(100, 34), p(64, 60), p(0, 68), p(-64, 60), p(-100, 34)]
    cv.poly(shell, fill=PURPLE, width=3.5)
    m = poly_mask(cv, shell)
    lower = cv.mask()
    ImageDraw.Draw(lower).ellipse([s(x - 140 * k), s(y + 8 * k), s(x + 140 * k), s(y + 120 * k)], fill=255)
    dot_fill(cv, ImageChops.multiply(m, lower), PURPLE, NAVY, spacing=5, radius=1.5)
    # Battle scars and a carapace ridge.
    cv.line([p(-60, -20), p(-30, 10)], width=2.5)
    cv.line([p(-52, -2), p(-40, -10)], width=2)
    cv.arc([x - 80 * k, y - 30 * k, x + 80 * k, y + 50 * k], 200, 340, width=2.5)
    # Glaring eyes on short stalks: one red, one under a patch.
    for ex, patched in ((-28, False), (28, True)):
        top = p(ex, -66)
        cv.stroke([p(ex * 0.8, -36), top], 9 * k, PURPLE)
        if patched:
            cv.circle(top[0], top[1], 13 * k, fill=INK, width=2.5)
            cv.line([(top[0] - 18 * k, top[1] - 18 * k), (top[0] + 22 * k, top[1] + 10 * k)], width=3)
        else:
            cv.circle(top[0], top[1], 13 * k, fill=YELLOW, width=2.5)
            cv.circle(top[0] + look[0] * 5 * k, top[1] + look[1] * 5 * k, 6 * k, fill=RED, width=0)
            cv.i.ellipse([s(top[0] + look[0] * 5 * k - 2.2 * k), s(top[1] + look[1] * 5 * k - 5 * k),
                          s(top[0] + look[0] * 5 * k + 2.2 * k), s(top[1] + look[1] * 5 * k + 5 * k)], fill=INK)
            cv.fill_ink([(top[0] - 17 * k, top[1] - 20 * k), (top[0] + 17 * k, top[1] - 8 * k),
                         (top[0] + 17 * k, top[1] - 2 * k), (top[0] - 17 * k, top[1] - 12 * k)])
    # The bicorne: a black two-pointed admiral's hat with a gold skull-and-claw badge, perched between the stalks.
    hx, hy = p(0, -96)
    hat = [(hx - 70 * k, hy + 12 * k), (hx - 40 * k, hy - 12 * k), (hx - 14 * k, hy - 34 * k), (hx + 14 * k, hy - 34 * k),
           (hx + 40 * k, hy - 12 * k), (hx + 70 * k, hy + 12 * k), (hx + 30 * k, hy + 6 * k), (hx - 30 * k, hy + 6 * k)]
    cv.poly(hat, fill=(40, 34, 50, 255), width=3)
    cv.line([(hx - 64 * k, hy + 9 * k), (hx - 30 * k, hy + 2 * k), (hx + 30 * k, hy + 2 * k), (hx + 64 * k, hy + 9 * k)],
            width=4 * k, color=GOLD, plate="color")
    cv.circle(hx, hy - 12 * k, 10 * k, fill=WHITE, width=2.5)
    cv.circle(hx - 3.5 * k, hy - 14 * k, 2.4 * k, fill=INK, width=0)
    cv.i.ellipse([s(hx - 6 * k), s(hy - 16.5 * k), s(hx - 1 * k), s(hy - 11.5 * k)], fill=INK)
    cv.i.ellipse([s(hx + 1 * k), s(hy - 16.5 * k), s(hx + 6 * k), s(hy - 11.5 * k)], fill=INK)
    cv.line([(hx - 12 * k, hy - 1 * k), (hx + 12 * k, hy - 1 * k)], width=2)
    # A jagged grin full of teeth.
    mx, my = p(0, 22)
    mouth = [(mx - 40 * k, my - 6 * k), (mx + 40 * k, my - 10 * k), (mx + 30 * k, my + 16 * k), (mx - 30 * k, my + 18 * k)]
    cv.poly(mouth, fill=(90, 10, 30, 255), width=3)
    for n in range(7):
        tx = mx - 34 * k + n * 11 * k
        cv.poly([(tx, my - 7 * k + n * -0.5 * k), (tx + 10 * k, my - 8 * k - n * 0.5 * k), (tx + 5 * k, my + 4 * k)],
                fill=WHITE, width=1.5)
        cv.poly([(tx + 3 * k, my + 16 * k), (tx + 12 * k, my + 15 * k), (tx + 7.5 * k, my + 7 * k)], fill=WHITE,
                width=1.5)
    # Claws: the right one enormous.
    if claws_up:
        big = (p(118, -34), -1.15, 70 * k)
        small = (p(-110, -40), -2.2, 40 * k)
    else:
        big = (p(150, -4), -0.6, 72 * k)
        small = (p(-122, 6), math.pi + 0.5, 40 * k)
    for (cxp, cyp), ang, size in (small, big):
        base = p(90 if cxp > x else -90, 20)
        cv.stroke([base, ((base[0] + cxp) / 2, (base[1] + cyp) / 2 + 12 * k), (cxp, cyp)], size * 0.4, PURPLE)
        pincer(cv, cxp, cyp, size, ang, VIOLET, open_=0.8, shade=PURPLE)
        for spike in (0.2, 0.45):
            sx2, sy2 = cxp + math.cos(ang - 1.9) * size * 0.55, cyp + math.sin(ang - 1.9) * size * 0.55
            cv.poly([(sx2, sy2), (sx2 + math.cos(ang - 1.6) * size * spike, sy2 + math.sin(ang - 1.6) * size * spike),
                     (sx2 + math.cos(ang) * size * 0.15, sy2 + math.sin(ang) * size * 0.15)], fill=LILAC, width=2)


def minion(cv, x, y, r, color=RED, facing=1, eyes=True):
    """A small invading crab: a dome, two claws up, two beady eyes."""
    cv.line([(x - r * 1.0, y), (x - r * 1.5, y - r * 0.9)], width=r * 0.3 + 2)
    cv.line([(x + r * 1.0, y), (x + r * 1.5, y - r * 0.9)], width=r * 0.3 + 2)
    for side in (-1, 1):
        cx = x + side * r * 1.6
        cv.circle(cx, y - r * 1.05, r * 0.42, fill=color, width=2)
        cv.line([(cx, y - r * 1.45), (cx + side * r * 0.1, y - r * 0.95)], width=2)
    cv.ellipse([x - r * 1.1, y - r * 0.7, x + r * 1.1, y + r * 0.55], fill=color, width=2.5)
    if eyes:
        for side in (-0.35, 0.35):
            ex = x + side * r
            cv.line([(ex, y - r * 0.5), (ex, y - r * 1.0)], width=2)
            cv.circle(ex, y - r * 1.1, r * 0.24, fill=WHITE, width=1.5)
            cv.circle(ex + facing * r * 0.06, y - r * 1.08, r * 0.1, fill=INK, width=0)
            cv.i.ellipse([s(ex + facing * r * 0.06 - r * 0.1), s(y - r * 1.18), s(ex + facing * r * 0.06 + r * 0.1),
                          s(y - r * 0.98)], fill=INK)


def waves(cv, top, bottom, base=BLUE, crest=CYAN, rows=4, seed=3, amp=8.0):
    """Rows of scalloped ocean waves from `top` to `bottom`, each row a band of dots with an inked crest."""
    rng = random.Random(seed)
    band = (bottom - top) / rows
    m = rect_mask(cv, [0, top, cv.w, bottom])
    dot_fill(cv, m, base, NAVY, spacing=6, radius=1.0, fade=lambda f: 0.6 + 1.6 * f)
    for n in range(rows):
        y = top + n * band + band * 0.4
        x = -rng.uniform(0, 40)
        width = 46 + n * 14
        while x < cv.w + width:
            cv.arc([x, y - amp - n * 2, x + width, y + amp + n * 2], 180, 360, width=2.2 + n * 0.4)
            cv.c.chord([s(x), s(y - amp - n * 2), s(x + width), s(y + amp + n * 2)], 180, 360, fill=crest)
            x += width


def tornado(cv, cx, top, bottom, width, color=STORM, flecks=True, seed=5):
    """The crabnado: a funnel of stacked, swaying ellipses with crabs whirled up in it."""
    rng = random.Random(seed)
    rows = 11
    for n in range(rows):
        f = n / (rows - 1)
        y = top + (bottom - top) * f
        w = width * (1.0 - f * 0.78)
        sway = math.sin(f * 4.0) * width * 0.12
        cv.ellipse([cx + sway - w / 2, y - w * 0.12 - 6, cx + sway + w / 2, y + w * 0.12 + 6],
                   fill=GREY if n % 2 else color, width=2.5)
        cv.arc([cx + sway - w * 0.4, y - w * 0.06, cx + sway + w * 0.4, y + w * 0.08], 10, 170, width=1.8)
    if flecks:
        for _ in range(7):
            f = rng.uniform(0.05, 0.8)
            y = top + (bottom - top) * f
            w = width * (1.0 - f * 0.78)
            minion(cv, cx + rng.uniform(-w * 0.45, w * 0.45), y, 6 + rng.uniform(0, 3), color=RED,
                   facing=rng.choice((-1, 1)))


def palm(cv, x, y, h, lean=0.15):
    trunk = []
    for n in range(7):
        f = n / 6
        trunk.append((x + math.sin(f * 1.4) * h * lean, y - f * h))
    cv.stroke(trunk, 12, BROWN)
    for n in range(1, 6):
        a, b = trunk[n - 1], trunk[n]
        cv.line([(a[0] - 6, a[1] - 3), (a[0] + 6, a[1] + 1)], width=1.8)
    tx, ty = trunk[-1]
    for a in (-2.8, -2.2, -1.5, -0.9, -0.3, 0.3):
        ex, ey = tx + math.cos(a) * h * 0.55, ty + math.sin(a) * h * 0.3 + h * 0.12
        mid = (tx + math.cos(a) * h * 0.3, ty + math.sin(a) * h * 0.3 - 8)
        cv.poly([(tx, ty), (mid[0], mid[1] - 8), (ex, ey), (mid[0], mid[1] + 8)], fill=GREEN, width=2.5)
        cv.line([(tx, ty), mid, (ex, ey)], width=1.6)
    for dx in (-6, 6):
        cv.circle(tx + dx, ty + 8, 6, fill=BROWN, width=2)


def stone_pillar(cv, x, base, w, h, orb=True):
    """A barrier rune pillar: a dark standing stone banded with a glowing rune and topped with a floating orb."""
    cv.poly([(x - w / 2, base), (x - w * 0.42, base - h), (x + w * 0.42, base - h), (x + w / 2, base)], fill=STORM,
            width=3)
    band = base - h * 0.55
    cv.poly([(x - w * 0.47, band - 7), (x + w * 0.47, band - 7), (x + w * 0.47, band + 7), (x - w * 0.47, band + 7)],
            fill=VIOLET, width=2)
    cv.line([(x - 4, band - 4), (x + 4, band + 4)], width=1.6)
    cv.line([(x + 4, band - 4), (x - 4, band + 4)], width=1.6)
    if orb:
        cv.circle(x, base - h - 16, w * 0.32, fill=LILAC, width=2.5)
        cv.circle(x - w * 0.1, base - h - 19, w * 0.08, fill=WHITE, width=0)


# ── Panels ──


def panel_challenge():
    """Panel 1: Warlord Snapjaw, on a sea stack before his horde and his crabnado, throws down the challenge."""
    cv = Canvas(PANEL_W, PANEL_H)
    w, h = PANEL_W, PANEL_H
    # A sickly storm sky graded with dots, the crabnado on the horizon.
    dot_fill(cv, rect_mask(cv, [0, 0, w, 330]), (190, 200, 120, 255), (90, 120, 90, 255), spacing=7,
             fade=lambda f: 2.6 - 2.2 * f)
    for n, (yy, ww) in enumerate(((150, 160), (190, 210), (230, 150))):
        cv.ellipse([w - ww - 20 + n * 30, yy, w + 60, yy + 38], fill=(160, 170, 110, 255), width=0)
    tornado(cv, 300, 120, 330, 150, seed=11)
    # Lightning.
    cv.poly([(70, 120), (98, 168), (84, 170), (110, 226), (66, 172), (82, 170)], fill=YELLOW, width=2.5)
    # The sea, with the horde bobbing in it.
    waves(cv, 320, h, base=BLUE, crest=CYAN, rows=4, seed=4)
    rng = random.Random(9)
    for _ in range(26):
        mx, my = rng.uniform(10, w - 10), rng.uniform(340, 400)
        minion(cv, mx, my, 5 + (my - 340) * 0.09, color=RED, facing=-1)
    # The sea stack he stands on.
    rock = [(150, h), (176, 432), (214, 404), (300, 396), (360, 424), (392, 470), (392, h)]
    cv.poly(rock, fill=BROWN, width=3.5)
    m = poly_mask(cv, rock)
    dot_fill(cv, m, BROWN, (80, 50, 30, 255), spacing=5, radius=1.5)
    cv.line([(220, 430), (240, 470), (232, 520)], width=2)
    warlord(cv, 250, 362, scale=0.68, claws_up=False, look=(-0.6, 0.2))
    bottom = caption(cv, 10, 10, 300, "Hermit Isle, at the turn of the tide...")
    caption(cv, 130, bottom - 6, 252, "...the Red Tide came crawling out of the deep!", emphasis=("RED", "TIDE"))
    balloon(cv, 132, 230, 236, "Hear me, shell-dweller! I, Warlord Snapjaw, claim your island! Come out and fight...",
            tail=(205, 300), size=14.5, emphasis=("SNAPJAW",), shout=True)
    balloon(cv, 100, 470, 170, "...or hide in that shell forever!", tail=(190, 420), size=14)
    return cv.finish()


def panel_vow():
    """Panel 2: the Hermit King wades out from his island, claw high, and swears he will win."""
    cv = Canvas(PANEL_W, PANEL_H)
    w, h = PANEL_W, PANEL_H
    sunburst(cv, 200, 300, [YELLOW, (250, 180, 60, 255)], rays=22, reach=700, spin=0.08)
    m = rect_mask(cv, [0, 0, w, h])
    burst = Image.new("RGBA", cv.color.size, (0, 0, 0, 0))
    burst = Image.alpha_composite(burst, dots(cv.color.size, (240, 140, 50, 140), 8, 1.4))
    cv.paste(burst, ImageChops.multiply(m, dots(cv.color.size, (255,), 8, 1.4).split()[3]))
    speed_lines(cv, 200, 300, 34, 190, 420, width=1.4, seed=2)
    # Hermit Isle behind him: sand, a palm, and a crown-shaped flag over his shell keep.
    island = [(0, 420), (40, 380), (130, 360), (220, 372), (280, 400), (300, 440), (0, 440)]
    cv.poly(island, fill=SAND, width=3)
    dot_fill(cv, poly_mask(cv, island), SAND, (200, 150, 80, 255), spacing=5, radius=1.0)
    palm(cv, 60, 380, 120, lean=0.25)
    # A heraldic pennant on a pole: the Hermit King's banner.
    cv.line([(250, 382), (250, 268)], width=4)
    cv.poly([(252, 270), (310, 282), (252, 300)], fill=RED, width=2.5)
    cv.circle(270, 285, 5, fill=YELLOW, width=1.6)
    waves(cv, 430, h, base=BLUE, crest=CYAN, rows=3, seed=7, amp=7)
    hermit_king(cv, 215, 410, scale=0.88, claw_angle=-2.05, look=(0.5, -0.4))
    # Splash at his feet as he wades in.
    # A crown of spray, then droplets flung up and out.
    for dx, r in ((-78, 13), (-56, 17), (-30, 15), (30, 15), (58, 17), (82, 12)):
        cv.ellipse([200 + dx - r, 478 - r * 1.3, 200 + dx + r, 478 + r * 0.6], fill=WHITE, width=2.2)
    for dx, dy, r in ((-98, -18, 5), (-70, -40, 6), (74, -44, 6), (104, -20, 5), (-40, -58, 4), (46, -62, 4)):
        x0, y0 = 200 + dx, 478 + dy
        cv.poly([(x0, y0 - r * 2.2), (x0 + r, y0), (x0, y0 + r), (x0 - r, y0)], fill=WHITE, width=1.8)
    caption(cv, 10, 10, 372, "But the King of Hermit Isle had never once hidden from a fight!")
    balloon(cv, 196, 132, 300,
            "Hide? Never! This shell has weathered a hundred storms, Snapjaw! I'll send your Red Tide "
            "scuttling back to the deep. Victory will be mine!",
            tail=(240, 238), size=14.5, emphasis=("NEVER!", "VICTORY", "MINE!"))
    sound_effect(cv, 88, 500, "SPLOOSH!", 30, fill=WHITE, outline=BLUE, angle=6)
    return cv.finish()


def panel_barrier():
    """Panel 3: Snapjaw raises his claws and summons the magic barrier around the tide pools, the crabnado roaring."""
    cv = Canvas(PANEL_W, PANEL_H)
    w, h = PANEL_W, PANEL_H
    # A night-violet sky, graded dots darkening upward.
    dot_fill(cv, rect_mask(cv, [0, 0, w, h]), (60, 40, 110, 255), (20, 14, 50, 255), spacing=7,
             fade=lambda f: 2.8 - 2.4 * f)
    sunburst(cv, 200, 190, [(150, 70, 200, 120), (0, 0, 0, 0)], rays=30, reach=600)
    tornado(cv, 70, 70, 300, 110, color=STORM, seed=21)
    # The barrier dome: a great glowing arc of light across the panel.
    for n, col in enumerate((LILAC, VIOLET, (140, 70, 200, 255))):
        r = 220 - n * 10
        cv.c.arc([s(200 - r * 1.15), s(400 - r), s(200 + r * 1.15), s(400 + r)], 180, 360, fill=col, width=s(9 - n * 2))
    cv.arc([200 - 230 * 1.15, 400 - 230, 200 + 230 * 1.15, 400 + 230], 180, 360, width=2)
    cv.arc([200 - 192 * 1.15, 400 - 192, 200 + 192 * 1.15, 400 + 192], 180, 360, width=1.6)
    # Runes floating on the dome.
    for a in (200, 225, 250, 290, 315, 340):
        ra = math.radians(a)
        rx, ry = 200 + math.cos(ra) * 211 * 1.15, 400 + math.sin(ra) * 211
        cv.circle(rx, ry, 7, fill=WHITE, width=2)
        cv.line([(rx - 3, ry - 3), (rx + 3, ry + 3)], width=1.4)
    # The tide pool arena below, ringed by rune pillars.
    waves(cv, 400, h, base=(40, 70, 140, 255), crest=(90, 140, 200, 255), rows=3, seed=12, amp=6)
    pool = [(40, 470), (110, 440), (290, 440), (360, 470), (290, 505), (110, 505)]
    cv.poly(pool, fill=CYAN, width=3)
    for x, base, ph in ((22, 470, 90), (370, 470, 90), (100, 440, 70), (300, 440, 70)):
        stone_pillar(cv, x, base, 26, ph)
    # Snapjaw at the centre, claws high, magic crackling from them.
    for (bx, by) in ((100, 220), (300, 150)):
        zig = [(bx, by)]
        for n in range(5):
            zig.append((bx + (-1) ** n * 12 + (200 - bx) * 0.0, by - 20 - n * 22))
        cv.line(zig, width=7, color=WHITE, plate="color")
        cv.line(zig, width=2.5)
    warlord(cv, 205, 330, scale=0.72, claws_up=True, look=(0.0, -0.5))
    sound_effect(cv, 200, 64, "KRA-KOOOM!", 40, fill=YELLOW, outline=RED, angle=-6)
    balloon(cv, 196, 178, 300,
            "By the Deep, I seal this ring! No crab leaves until one of us is shell-less... "
            "and my Crabnado will rain claws upon you!",
            tail=(220, 255), size=14, emphasis=("SHELL-LESS", "CRABNADO"), shout=True)
    caption(cv, 10, h - 46, 372, "And so began the Battle for Hermit Isle!", size=16, emphasis=("BATTLE",))
    return cv.finish()


def page():
    """The page behind the panels: halftone pulp with the masthead along the top."""
    cv = Canvas(PAGE_W, PAGE_H, ground=(242, 226, 190, 255))
    dot_fill(cv, rect_mask(cv, [0, 0, PAGE_W, PAGE_H]), (242, 226, 190, 255), (226, 196, 150, 255), spacing=9,
             radius=1.2)
    # The masthead: a red ribbon with the title, a price badge and an issue badge.
    ribbon = [(250, 18), (1030, 18), (1050, 44), (1030, 72), (250, 72), (230, 44)]
    cv.poly(ribbon, fill=RED, width=3.5)
    fnt = font(40)
    title = "THE BATTLE FOR HERMIT ISLE!"
    tw = fnt.getlength(title)
    cv.i.text((s(640) - tw / 2, s(21)), title, font=fnt, fill=YELLOW, stroke_width=s(2.4), stroke_fill=INK)
    # The cover price, a must for a golden-age book.
    cv.circle(150, 46, 34, fill=YELLOW, width=3)
    f2 = font(30)
    cv.i.text((s(150) - f2.getlength("10¢") / 2, s(28)), "10¢", font=f2, fill=INK)
    cv.circle(1130, 46, 38, fill=CYAN, width=3)
    f3 = font(12)
    for n, word in enumerate(("CRABNADO", "No. 1")):
        cv.i.text((s(1130) - f3.getlength(word) / 2, s(28 + n * 17)), word, font=f3, fill=INK)
    return cv.finish(border=0)


def main():
    OUT.mkdir(parents=True, exist_ok=True)
    page().save(OUT / "page.png")
    for n, make in enumerate((panel_challenge, panel_vow, panel_barrier), start=1):
        make().save(OUT / f"panel_{n}.png")
    print(f"Wrote the story page and 3 panels to {OUT}")


if __name__ == "__main__":
    main()
