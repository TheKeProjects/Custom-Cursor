# Draws the app's logo: Clicky, a cursor arrow with a face, in the classic look and one per season.
# Run by hand when the art changes (needs Pillow): python Tools/GenerateLogos.py [Christmas ...]
#
# Like the Inazuma Eleven VR Save Editor's Iconos folder, for every look it writes
#   Iconos\IconHD<look>.png   1024 x 1024, the full drawing
#   Iconos\icon<look>.png     32 x 32
#   Iconos\icon<look>.ico     every Windows size (ICON_SIZES)
#   Assets\Icons\icon<look>.ico   the exe / window icon (App.rc)
#   Assets\Logo\logo<look>.png    256 x 256, what the app draws (title bar, side bar, loading screen)
#
# The sizes aren't one picture shrunk: each range has its own drawing, as an icon designer would do it.
#   hd    (64 and up) the face, the gloss, the sparkles and the season's accessories in detail
#   mid   (30 - 48)   a bigger face and a thicker outline, no gloss or sparkles
#   tiny  (16 - 24)   the silhouette with a heavy outline and the season's colour; two dots for eyes at 24
#
# The .ico is written by hand: 32-bit BMP entries (with their AND mask) up to 128 and a PNG one at 256,
# which every Windows version, rc.exe and Explorer read.
import io
import math
import os
import struct
import sys
from PIL import Image, ImageChops, ImageDraw

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
# Every size Windows asks for: 100 % (16, 32, 48, 256), the 125 / 150 / 175 / 200 % scales of the
# small and large icons (20, 24, 28, 30, 36, 40, 60, 64, 72, 80, 96) and the tile / jumbo views (128)
ICON_SIZES = [16, 20, 24, 28, 30, 32, 36, 40, 48, 60, 64, 72, 80, 96, 128, 256]
INK = (11, 22, 44, 255)
WHITE = (255, 255, 255, 255)

# The arrow's skeleton in a 100 x 100 box (rounded by the offset it's drawn with)
ARROW = [(20, 12), (20, 82), (38, 68), (48, 92), (63, 85), (53, 62), (78, 60)]

BLUE = [(136, 221, 255), (68, 153, 255), (45, 95, 205)]
LOOKS = {
    "": dict(body=BLUE, rim=(235, 250, 255)),
    " Halloween": dict(body=[(176, 150, 112), (146, 120, 86), (112, 90, 62)], rim=(250, 240, 216)),
    " Summer": dict(body=[(127, 240, 224), (46, 196, 200), (14, 136, 167)], rim=(235, 255, 250)),
    " Christmas": dict(body=BLUE, rim=(235, 250, 255)),
    " LGBTQ+": dict(body=None, rim=(255, 255, 255)),
}
RAINBOW = [(228, 3, 3), (255, 140, 0), (255, 237, 0), (0, 128, 38), (0, 77, 255), (117, 7, 135)]


# ── Paths ───────────────────────────────────────────────────────────────────────────────────────

def cubic(p0, p1, p2, p3, n=28):
    """Points along a cubic Bézier (without its first point)."""
    out = []
    for i in range(1, n + 1):
        t = i / n
        u = 1 - t
        out.append((u * u * u * p0[0] + 3 * u * u * t * p1[0] + 3 * u * t * t * p2[0] + t * t * t * p3[0],
                    u * u * u * p0[1] + 3 * u * u * t * p1[1] + 3 * u * t * t * p2[1] + t * t * t * p3[1]))
    return out


def path(start, *segments):
    """start, then segments: a point (a line) or a (c1, c2, end) tuple (a curve)."""
    pts = [start]
    for seg in segments:
        if len(seg) == 3 and isinstance(seg[0], tuple):
            pts += cubic(pts[-1], *seg)
        else:
            pts.append(seg)
    return pts


def ellipse_pts(cx, cy, rx, ry, rot=0.0, start=0.0, end=360.0, n=72):
    r = math.radians(rot)
    out = []
    for i in range(n + 1):
        a = math.radians(start + (end - start) * i / n)
        x, y = rx * math.cos(a), ry * math.sin(a)
        out.append((cx + x * math.cos(r) - y * math.sin(r), cy + x * math.sin(r) + y * math.cos(r)))
    return out


def lerp(a, b, t):
    return (a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t)


# ── Canvas ──────────────────────────────────────────────────────────────────────────────────────

class Canvas:
    def __init__(self, size):
        self.size = size
        self.k = size / 100.0
        self.image = Image.new("RGBA", (size, size), (0, 0, 0, 0))

    def blank(self):
        return Image.new("L", (self.size, self.size), 0)

    def mask(self, points, grow=0.0):
        """The polygon grown by `grow` units with round corners."""
        m = self.blank()
        d = ImageDraw.Draw(m)
        pts = [(x * self.k, y * self.k) for x, y in points]
        d.polygon(pts, fill=255)
        if grow > 0:
            r = grow * self.k
            for i, a in enumerate(pts):
                b = pts[(i + 1) % len(pts)]
                d.line([a, b], fill=255, width=max(1, int(r * 2)))
                d.ellipse([a[0] - r, a[1] - r, a[0] + r, a[1] + r], fill=255)
        return m

    def ellipse_mask(self, cx, cy, rx, ry):
        m = self.blank()
        ImageDraw.Draw(m).ellipse([(cx - rx) * self.k, (cy - ry) * self.k, (cx + rx) * self.k, (cy + ry) * self.k], fill=255)
        return m

    def circles_mask(self, circles, grow=0.0):
        m = self.blank()
        d = ImageDraw.Draw(m)
        for cx, cy, r in circles:
            r += grow
            d.ellipse([(cx - r) * self.k, (cy - r) * self.k, (cx + r) * self.k, (cy + r) * self.k], fill=255)
        return m

    def stroke_mask(self, points, width):
        """A polyline with round joins and caps."""
        m = self.blank()
        d = ImageDraw.Draw(m)
        pts = [(x * self.k, y * self.k) for x, y in points]
        w = max(1, int(width * self.k))
        d.line(pts, fill=255, width=w, joint="curve")
        r = width * self.k / 2
        for x, y in (pts[0], pts[-1]):
            d.ellipse([x - r, y - r, x + r, y + r], fill=255)
        return m

    def fill(self, mask, color):
        layer = Image.new("RGBA", (self.size, self.size), color)
        self.image.paste(layer, (0, 0), mask)

    def paint(self, mask, layer):
        self.image.paste(layer, (0, 0), mask)

    def glaze(self, mask, color, alpha):
        """Blends a colour over what's there, inside `mask`."""
        m = mask.point(lambda v: v * alpha // 255)
        over = Image.new("RGBA", (self.size, self.size), color)
        self.image = Image.composite(over, self.image, m)

    def inked(self, mask_at, color, outline):
        """mask_at(grow) -> mask; draws the ink outline then the colour."""
        if outline:
            self.fill(mask_at(outline), INK)
        self.fill(mask_at(0.0), color)

    def shape(self, points, color, grow=1.5, outline=2.2):
        """A rounded shape with the ink outline."""
        self.inked(lambda g: self.mask(points, grow + g), color, outline)

    def circle(self, cx, cy, r, color, outline=2.2):
        self.inked(lambda g: self.ellipse_mask(cx, cy, r + g, r + g), color, outline)

    def stroke(self, points, color, width):
        self.fill(self.stroke_mask(points, width), color)


def clip(a, b):
    return ImageChops.multiply(a, b)


def gradient(size, stops):
    """Top-left to bottom-right through the stops."""
    n = 64
    im = Image.new("RGB", (n, n))
    px = im.load()
    for y in range(n):
        for x in range(n):
            t = (x + y) / (2.0 * (n - 1)) * (len(stops) - 1)
            i = min(int(t), len(stops) - 2)
            f = t - i
            px[x, y] = tuple(int(stops[i][c] + (stops[i + 1][c] - stops[i][c]) * f) for c in range(3))
    return im.resize((size, size), Image.BICUBIC).convert("RGBA")


def rainbow(size):
    """Diagonal bands of the pride flag."""
    n = 400
    im = Image.new("RGBA", (n, n))
    px = im.load()
    for y in range(n):
        for x in range(n):
            t = (x * 0.45 + y) / (n * 1.45)
            px[x, y] = RAINBOW[min(int(t * 1.25 * len(RAINBOW)), len(RAINBOW) - 1)] + (255,)
    return im.resize((size, size), Image.NEAREST)


# ── Small pieces ────────────────────────────────────────────────────────────────────────────────

def star(c, cx, cy, r, color, outline=1.4):
    """A four-point sparkle with curved sides."""
    t = r * 0.18
    pts = path((cx, cy - r), ((cx + t, cy - t), (cx + t, cy - t), (cx + r, cy)),
               ((cx + t, cy + t), (cx + t, cy + t), (cx, cy + r)),
               ((cx - t, cy + t), (cx - t, cy + t), (cx - r, cy)),
               ((cx - t, cy - t), (cx - t, cy - t), (cx, cy - r)))
    c.inked(lambda g: c.mask(pts, g), color, outline)


def heart(c, cx, cy, s, color, outline=1.6):
    pts = path((cx, cy + s * 0.9),
               ((cx - s * 1.3, cy + s * 0.1), (cx - s * 1.1, cy - s * 1.1), (cx, cy - s * 0.45)),
               ((cx + s * 1.1, cy - s * 1.1), (cx + s * 1.3, cy + s * 0.1), (cx, cy + s * 0.9)))
    c.inked(lambda g: c.mask(pts, g), color, outline)
    c.glaze(c.ellipse_mask(cx - s * 0.45, cy - s * 0.35, s * 0.22, s * 0.16), WHITE, 200)


def snowflake(c, cx, cy, r, color, width=1.6, outline=1.3):
    arms = []
    for i in range(6):
        a = math.radians(90 + i * 60)
        tip = (cx + r * math.cos(a), cy - r * math.sin(a))
        arms.append([(cx, cy), tip])
        mid = lerp((cx, cy), tip, 0.58)
        for side in (-1, 1):
            b = a + side * math.radians(45)
            arms.append([mid, (mid[0] + r * 0.36 * math.cos(b), mid[1] - r * 0.36 * math.sin(b))])
    if outline:
        m = c.blank()
        for arm in arms:
            m = ImageChops.lighter(m, c.stroke_mask(arm, width + outline * 2))
        c.fill(m, INK)
    for arm in arms:
        c.stroke(arm, color, width)


# ── Clicky ──────────────────────────────────────────────────────────────────────────────────────

def draw_body(c, look, tier):
    style = LOOKS[look]
    outline, rim = {"hd": (3.0, 2.2), "mid": (5.0, 2.6), "tiny": (8.0, 0.0)}[tier]
    round_by = 3.0
    c.fill(c.mask(ARROW, round_by + rim + outline), INK)
    if rim:
        c.fill(c.mask(ARROW, round_by + rim), style["rim"] + (255,))
    inner = c.mask(ARROW, round_by)
    c.paint(inner, rainbow(c.size) if style["body"] is None else gradient(c.size, style["body"]))
    if tier == "hd":
        # A curved gloss down the left edge and the shade along the bottom, kept inside the body
        gloss = clip(inner, c.mask(path((12, 6), ((30, 14), (44, 30), (46, 36)), ((34, 40), (26, 54), (24, 74)), (12, 80))))
        c.glaze(gloss, WHITE, 64)
        c.glaze(clip(inner, c.mask(path((18, 8), (25, 13), ((24, 30), (23, 46), (23, 66)), (18, 70)))), WHITE, 70)
        shade = clip(inner, c.mask(path((16, 78), ((30, 66), (40, 62), (56, 61)), (82, 57), (82, 100), (16, 100))))
        c.glaze(shade, INK, 40)
    return inner


def draw_face(c, tier, eyes=True, shades=False, cheeks=True):
    if tier == "tiny":
        if eyes and c.tiny_eyes:
            c.circle(30, 47, 5.5, INK, 0)
            c.circle(46, 47, 5.5, INK, 0)
        return
    big = 1.2 if tier == "mid" else 1.0
    left, right, y = 30.5, 46.5, 46.0
    if not shades:
        for x in (left, right):
            c.fill(c.ellipse_mask(x, y, 7.2 * big, 8.6 * big), INK)
            c.fill(c.ellipse_mask(x, y, 5.8 * big, 7.2 * big), WHITE)
            c.fill(c.ellipse_mask(x + 0.6, y + 0.8, 3.9 * big, 5.1 * big), (20, 30, 60, 255))
            c.fill(c.ellipse_mask(x + 1.8, y - 1.8, 1.5 * big, 1.8 * big), WHITE)
            if tier == "hd":
                c.fill(c.ellipse_mask(x - 1.2, y + 2.6, 0.8, 0.9), (200, 225, 255, 255))
    if tier == "hd" and cheeks:
        for x in (left - 5.0, right + 5.0):
            c.glaze(c.ellipse_mask(x, y + 8.5, 4.2, 2.4), (255, 110, 150, 255), 150)
    c.stroke(path((32.5, 54.5), ((35, 58.5), (42, 58.5), (44.5, 54.5))), INK, 2.2 if tier == "hd" else 3.0)


def draw_sparkles(c):
    star(c, 74, 28, 9.0, (255, 236, 150, 255))
    star(c, 87, 45, 4.6, WHITE)


# ── Seasons ─────────────────────────────────────────────────────────────────────────────────────

def christmas(c, inner, tier):
    """Santa's hat pulled over the arrow's tip: a soft cone flopping to the right, a fluffy brim and
    a pompom hanging off its end; snowflakes around."""
    draw_face(c, tier)
    outline = {"hd": 2.2, "mid": 3.4, "tiny": 5.0}[tier]
    red, dark_red = (222, 36, 52, 255), (150, 14, 34, 255)

    # The cone: up from the brim, over the top and down the right, its tip drooping
    cone = path((8, 32),
                ((7, 18), (16, 4), (33, 3)),
                ((48, 2), (60, 9), (67, 22)),
                (63, 24),
                ((58, 16), (51, 14), (46, 19)),
                ((47, 23), (48, 26), (48, 28)))
    c.shape(cone, red, 0.8, outline)
    cone_mask = c.mask(cone, 0.8)
    if tier != "tiny":
        # The fold behind the droop and a soft light on the left
        c.glaze(clip(cone_mask, c.mask(path((36, 22), ((44, 8), (58, 8), (70, 22)), (70, 30), (36, 30)))), dark_red, 150)
        c.glaze(clip(cone_mask, c.mask(path((5, 32), ((6, 18), (14, 7), (26, 5)), ((18, 12), (14, 22), (14, 32))))), (255, 140, 140, 255), 90)

    # The brim: a band of fur, a row of bumps along a slight tilt
    a, b = (5, 35), (51, 27.5)
    if tier == "tiny":
        band = [(a[0], a[1] - 4.5), (b[0], b[1] - 4.5), (b[0], b[1] + 4.5), (a[0], a[1] + 4.5)]
        c.shape(band, WHITE, 1.5, outline)
    else:
        bumps = 9 if tier == "hd" else 6
        r = 5.0 if tier == "hd" else 5.4
        fur = []
        for i in range(bumps):
            p = lerp(a, b, i / (bumps - 1))
            wobble = 0.5 if i % 2 else -0.4
            fur.append((p[0], p[1] - 1.4 + wobble, r * (0.92 if i % 2 else 1.0)))
            fur.append((p[0], p[1] + 1.4 - wobble, r * (0.92 if i % 2 == 0 else 1.0)))
        c.inked(lambda g: c.circles_mask(fur, g), WHITE, outline)
        if tier == "hd":
            fur_mask = c.circles_mask(fur)
            low = c.mask([(0, a[1] + 1.5), (60, b[1] + 1.5), (60, 50), (0, 50)])
            c.glaze(clip(fur_mask, low), (190, 215, 240, 255), 160)
            for i in range(1, bumps - 1, 2):
                p = lerp(a, b, i / (bumps - 1))
                c.glaze(c.ellipse_mask(p[0] - 1.2, p[1] - 3.0, 1.6, 1.0), WHITE, 255)

    # The pompom
    px, py = 66.5, 25.5
    if tier == "tiny":
        c.circle(px, py, 7.0, WHITE, outline)
    else:
        pr = 6.4 if tier == "hd" else 7.4
        puffs = [(px + pr * 0.55 * math.cos(math.radians(a)), py + pr * 0.55 * math.sin(math.radians(a)), pr * 0.62) for a in range(0, 360, 45)]
        puffs.append((px, py, pr * 0.8))
        c.inked(lambda g: c.circles_mask(puffs, g), WHITE, outline)
        if tier == "hd":
            c.glaze(clip(c.circles_mask(puffs), c.ellipse_mask(px + 2.5, py + 3.2, pr, pr * 0.8)), (190, 215, 240, 255), 170)
            c.glaze(c.ellipse_mask(px - 2.2, py - 2.4, 1.8, 1.3), WHITE, 255)

    if tier == "hd":
        snowflake(c, 84, 46, 8.0, (220, 240, 255, 255), 1.7, 1.2)
        snowflake(c, 93, 31, 3.8, WHITE, 1.2, 1.0)


def bat(c, cx, cy, s):
    """A little bat: rounded body, pointy ears, scalloped wings."""
    wing = path((cx - 2 * s, cy - 1 * s),
                ((cx - 5 * s, cy - 5 * s), (cx - 10 * s, cy - 6 * s), (cx - 13 * s, cy - 3 * s)),
                ((cx - 12 * s, cy + 0 * s), (cx - 11 * s, cy + 2 * s), (cx - 9.5 * s, cy + 3 * s)),
                ((cx - 9 * s, cy + 0.5 * s), (cx - 7.5 * s, cy + 0.5 * s), (cx - 6.5 * s, cy + 2.5 * s)),
                ((cx - 5.5 * s, cy + 0.5 * s), (cx - 4 * s, cy + 0.5 * s), (cx - 2 * s, cy + 3 * s)))
    other = [(2 * cx - x, y) for x, y in wing]
    body = ellipse_pts(cx, cy + 0.5 * s, 3.2 * s, 4.2 * s)
    ears = [(cx - 2.6 * s, cy - 2 * s), (cx - 2.2 * s, cy - 6.2 * s), (cx - 0.6 * s, cy - 3.6 * s),
            (cx + 0.6 * s, cy - 3.6 * s), (cx + 2.2 * s, cy - 6.2 * s), (cx + 2.6 * s, cy - 2 * s)]
    parts = [wing, other, body, ears]
    m_ink = c.blank()
    for p in parts:
        m_ink = ImageChops.lighter(m_ink, c.mask(p, 0.6 + 1.5))
    c.fill(m_ink, INK)
    purple = (132, 72, 196, 255)
    for p in parts:
        c.fill(c.mask(p, 0.6), purple)
    c.glaze(c.mask(body, 0.0), (160, 100, 224, 255), 255)
    for x in (cx - 1.3 * s, cx + 1.3 * s):
        c.fill(c.ellipse_mask(x, cy, 0.85 * s, 1.0 * s), (255, 190, 70, 255))


def pumpkin(c, cx, cy, r):
    orange, deep = (255, 140, 30, 255), (220, 92, 10, 255)
    lobes = [(cx - r * 0.5, cy, r * 0.62, r * 0.82), (cx + r * 0.5, cy, r * 0.62, r * 0.82), (cx, cy, r * 0.62, r * 0.88)]

    def lobes_mask(g):
        m = c.blank()
        for x, y, rx, ry in lobes:
            m = ImageChops.lighter(m, c.ellipse_mask(x, y, rx + g, ry + g))
        return m
    stem = [(cx - 1.2, cy - r * 0.7), (cx - 0.6, cy - r * 1.15), (cx + 2.4, cy - r * 1.3), (cx + 1.4, cy - r * 0.7)]
    c.shape(stem, (96, 140, 50, 255), 0.6, 1.6)
    c.inked(lobes_mask, orange, 1.8)
    for x, y, rx, ry in lobes[:2]:
        c.stroke(ellipse_pts(cx, y, abs(x - cx) * 1.15, ry * 0.95, 0, -60 if x > cx else 120, 60 if x > cx else 240, 24), deep, 0.9)
    c.glaze(c.ellipse_mask(cx - r * 0.55, cy - r * 0.35, r * 0.2, r * 0.13), (255, 220, 160, 255), 220)
    # The jack-o'-lantern face
    glow = (60, 24, 8, 255)
    for side in (-1, 1):
        ex = cx + side * r * 0.36
        c.fill(c.mask([(ex - r * 0.17, cy - r * 0.03), (ex, cy - r * 0.32), (ex + r * 0.17, cy - r * 0.03)], 0.3), glow)
    mouth = [(cx - r * 0.5, cy + r * 0.2), (cx - r * 0.25, cy + r * 0.3), (cx - r * 0.12, cy + r * 0.2), (cx, cy + r * 0.32),
             (cx + r * 0.12, cy + r * 0.2), (cx + r * 0.25, cy + r * 0.3), (cx + r * 0.5, cy + r * 0.2), (cx, cy + r * 0.56)]
    c.fill(c.mask(mouth, 0.3), glow)


def halloween(c, inner, tier):
    """A mummy: bandages over the body, the eyes peeking through, a bat and a pumpkin keeping it company."""
    if tier == "tiny":
        for y in (22, 66):
            band = clip(inner, c.mask([(0, y), (100, y + 8), (100, y + 15), (0, y + 7)]))
            c.fill(band, (250, 240, 216, 255))
        draw_face(c, tier)
        return
    bands = [(10, 9, 7), (24, 9, -6), (60, 9, 6), (71, 9, -5), (83, 9, 6)] if tier == "hd" else [(16, 12, 6), (62, 12, 6), (78, 12, -5)]
    for y, h, tilt in bands:
        pts = [(0, y - tilt), (100, y + tilt), (100, y + tilt + h), (0, y - tilt + h)]
        c.fill(clip(inner, c.mask(pts)), (250, 240, 216, 255))
        low = [(0, y - tilt + h - 1.6), (100, y + tilt + h - 1.6), (100, y + tilt + h), (0, y - tilt + h)]
        c.fill(clip(inner, c.mask(low)), (112, 88, 58, 255))
    draw_face(c, tier)
    if tier == "hd":
        bat(c, 84, 20, 1.15)
        pumpkin(c, 82, 84, 11)


def summer(c, inner, tier):
    """Sunglasses and a straw sun hat with a pink ribbon; a little sun at the top."""
    draw_face(c, tier, shades=True)
    pink = (246, 104, 150, 255)
    if tier == "tiny":
        c.fill(c.mask([(20, 42), (56, 42), (54, 54), (22, 54)]), INK)
    else:
        lens = 7.6 if tier == "hd" else 9.0
        for x in (30.0, 47.5):
            c.fill(c.ellipse_mask(x, 45.5, lens + 1.8, lens * 0.82 + 1.8), pink)
            c.fill(c.ellipse_mask(x, 45.5, lens, lens * 0.82), (24, 26, 44, 255))
            if tier == "hd":
                c.glaze(clip(c.ellipse_mask(x, 45.5, lens, lens * 0.82), c.mask([(x - 9, 38), (x + 2, 38), (x - 6, 50), (x - 9, 50)])), (120, 200, 240, 255), 150)
        c.stroke([(36.5, 44.5), (41, 44.5)], pink, 2.2)

    outline = {"hd": 2.2, "mid": 3.4, "tiny": 5.0}[tier]
    straw, straw_dark = (250, 214, 128, 255), (214, 160, 72, 255)
    tilt = -9
    hx, hy, hr = 25.5, 22.5, 16.5   # the dome, over the arrow's tip
    brim = ellipse_pts(hx + 3, hy + 4.5, 30, 8.5, tilt)
    dome = ellipse_pts(hx, hy, hr, hr, tilt, 180, 360) + ellipse_pts(hx, hy, hr, 4.5, tilt, 0, 180)
    c.shape(brim, straw, 0.6, outline)
    if tier != "tiny":
        c.glaze(clip(c.mask(brim, 0.6), c.mask(ellipse_pts(30, 30.5, 29, 6.5, tilt))), straw_dark, 120)
    c.shape(dome, straw, 0.6, outline)
    if tier != "tiny":
        # The ribbon round the dome and its bow
        band = ellipse_pts(hx, hy, hr + 2, 5.5, tilt, 180, 360) + ellipse_pts(hx, hy - 0.7, hr + 2, 1.0, tilt, 0, 180)
        ribbon = clip(c.mask(dome, 0.6), c.mask(band))
        c.fill(ribbon, pink)
        c.glaze(clip(c.mask(dome, 0.6), c.mask(path((hx - 15, hy), ((hx - 14, hy - 9), (hx - 8, hy - 14), (hx - 1, hy - 15)), ((hx - 6, hy - 10), (hx - 9, hy - 5), (hx - 10, hy))))), WHITE, 90)
    if tier == "hd":
        # The straw's weave, kept inside the dome above the ribbon
        weave = c.blank()
        for i in range(-4, 5):
            x = hx + i * 4.2
            weave = ImageChops.lighter(weave, c.stroke_mask([(x - 1.0, hy - 20), (x + 1.4, hy - 2)], 0.6))
        c.fill(clip(weave, clip(c.mask(dome), ImageChops.invert(c.mask(band, 0.4)))), straw_dark)
        # The bow on the ribbon: two loops and a knot
        bx, by = hx + 11.5, hy - 4.5
        c.shape([(bx, by), (bx + 5.5, by - 3.6), (bx + 6, by + 2.8)], pink, 1.0, 1.4)
        c.shape([(bx, by), (bx - 3.4, by - 4.6), (bx - 4.4, by + 1.6)], pink, 1.0, 1.4)
        c.circle(bx, by, 1.9, (226, 72, 124, 255), 1.4)
        # The sun
        sun = (82, 40)
        rays = []
        for i in range(10):
            a = math.radians(i * 36)
            ra, rb = 8.6, 12.0
            rays.append([(sun[0] + ra * math.cos(a), sun[1] + ra * math.sin(a)), (sun[0] + rb * math.cos(a), sun[1] + rb * math.sin(a))])
        m = c.blank()
        for ray in rays:
            m = ImageChops.lighter(m, c.stroke_mask(ray, 2.4 + 2.4))
        c.fill(m, INK)
        for ray in rays:
            c.stroke(ray, (255, 196, 40, 255), 2.4)
        c.circle(*sun, 6.6, (255, 214, 64, 255), 1.6)
        c.glaze(c.ellipse_mask(sun[0] - 2, sun[1] - 2.2, 2.2, 1.6), WHITE, 200)


def lgbtq(c, inner, tier):
    draw_face(c, tier)
    if tier == "hd":
        star(c, 74, 28, 9.0, (255, 236, 150, 255))
        heart(c, 87, 46, 5.2, (255, 92, 150, 255), 1.4)


def draw(look, tier, final_size):
    # Enough resolution to shrink cleanly; the small drawings don't need the 1024 one's
    work = max(1024, final_size * 8) if tier == "hd" else final_size * 16
    c = Canvas(work)
    c.tiny_eyes = final_size >= 24
    inner = draw_body(c, look, tier)
    if look == " Halloween":
        halloween(c, inner, tier)
    elif look == " Christmas":
        christmas(c, inner, tier)
    elif look == " Summer":
        summer(c, inner, tier)
    elif look == " LGBTQ+":
        lgbtq(c, inner, tier)
    else:
        draw_face(c, tier)
        if tier == "hd":
            draw_sparkles(c)
    return fit(c.image, final_size)


def fit(image, size):
    """Crops to the drawing and centres it, filling the square (a hair of margin at the larger sizes)."""
    box = image.getbbox()
    cropped = image.crop(box)
    side = int(max(cropped.size) * (1.0 if size <= 32 else 1.03))
    square = Image.new("RGBA", (side, side), (0, 0, 0, 0))
    square.paste(cropped, ((side - cropped.width) // 2, (side - cropped.height) // 2))
    return square.resize((size, size), Image.LANCZOS)


def tier_of(size):
    return "tiny" if size <= 24 else "mid" if size <= 48 else "hd"


# ── .ico ────────────────────────────────────────────────────────────────────────────────────────

def bmp_entry(image):
    """A 32-bit DIB (bottom-up BGRA, then the 1-bit AND mask) as an .ico stores it."""
    w, h = image.size
    header = struct.pack("<IiiHHIIiiII", 40, w, h * 2, 1, 32, 0, 0, 0, 0, 0, 0)
    pixels = image.convert("RGBA").load()
    xor = bytearray()
    for y in range(h - 1, -1, -1):
        for x in range(w):
            r, g, b, a = pixels[x, y]
            xor += bytes((b, g, r, a))
    stride = ((w + 31) // 32) * 4
    andm = bytearray()
    for y in range(h - 1, -1, -1):
        row = bytearray(stride)
        for x in range(w):
            if pixels[x, y][3] == 0:
                row[x // 8] |= 0x80 >> (x % 8)
        andm += row
    return header + bytes(xor) + bytes(andm)


def png_entry(image):
    buf = io.BytesIO()
    image.save(buf, format="PNG", optimize=True)
    return buf.getvalue()


def save_ico(path, images):
    entries = [(im.size[0], png_entry(im) if im.size[0] >= 256 else bmp_entry(im)) for im in images]
    out = bytearray(struct.pack("<HHH", 0, 1, len(entries)))
    offset = 6 + 16 * len(entries)
    for size, data in entries:
        dim = 0 if size >= 256 else size
        out += struct.pack("<BBBBHHII", dim, dim, 0, 0, 1, 32, len(data), offset)
        offset += len(data)
    for _, data in entries:
        out += data
    with open(path, "wb") as f:
        f.write(out)


def main():
    icons = os.path.join(ROOT, "Iconos")
    res_icons = os.path.join(ROOT, "Assets", "Icons")
    logos = os.path.join(ROOT, "Assets", "Logo")
    for folder in (icons, res_icons, logos):
        os.makedirs(folder, exist_ok=True)
    only = sys.argv[1:]   # e.g. "Halloween": just that look
    for look in LOOKS:
        if only and look.strip() not in only:
            continue
        images = [draw(look, tier_of(s), s) for s in ICON_SIZES]
        for path_ in (os.path.join(icons, f"icon{look}.ico"), os.path.join(res_icons, f"icon{look}.ico")):
            save_ico(path_, images)
        draw(look, "hd", 1024).save(os.path.join(icons, f"IconHD{look}.png"))
        images[ICON_SIZES.index(32)].save(os.path.join(icons, f"icon{look}.png"))
        draw(look, "hd", 256).save(os.path.join(logos, f"logo{look}.png"))
        print("drew", look or "classic")


if __name__ == "__main__":
    sys.exit(main())
