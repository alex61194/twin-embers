#!/usr/bin/env python3
"""Builds the battle menu's icons: 3ds_port/include/battle_flat_icons.h.

FIGHT (an impact burst), POKeMON (a Poke Ball) and RUN (a running shoe), drawn here (no game pixels) flat and
cel-shaded like FireRed's own sprites: flat colours, one shadow band on the lower right of each part, a few shines,
and the slate outline of the game's bag sprite. BAG is the game's own bag, read from the user's pack at run time.
Each icon is RGB565 with 0 for a clear pixel.

Needs Pillow; it is a development tool and its output is committed. Run it again after changing the art.
"""
import math
from pathlib import Path

from PIL import Image, ImageChops, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '3ds_port/include/battle_flat_icons.h'
LINE = (64, 72, 104)


def dark(c, k=0.72):
    return tuple(round(v * k) for v in c)


class Flat:
    def __init__(self, w, h=None):
        self.w, self.h = w, h or w
        self.img = Image.new('RGBA', (self.w, self.h), (0, 0, 0, 0))
        self.union = Image.new('L', (self.w, self.h), 0)

    def mask(self, draw):
        m = Image.new('L', (self.w, self.h), 0)
        draw(ImageDraw.Draw(m))
        return m.point(lambda v: 255 if v > 127 else 0)

    def part(self, base, draw, shadow=3, sh=None, line=True):
        m = self.mask(draw)
        shifted = ImageChops.offset(m, -shadow, -shadow) if shadow else m
        band = ImageChops.subtract(m, shifted)
        self.img.paste(base + (255,), (0, 0), m)
        if shadow:
            self.img.paste((sh or dark(base)) + (255,), (0, 0), band)
        if line:                                        # this part's edge against what lies under it
            edge = ImageChops.subtract(m, m.filter(ImageFilter.MinFilter(3)))
            self.img.paste(LINE + (255,), (0, 0), ImageChops.multiply(edge, self.union))
        self.union = ImageChops.lighter(self.union, m)
        return m

    def spot(self, col, draw):
        self.img.paste(col + (255,), (0, 0), self.mask(draw))

    def done(self):
        ring = ImageChops.subtract(self.union.filter(ImageFilter.MaxFilter(3)), self.union)
        out = Image.new('RGBA', (self.w, self.h), (0, 0, 0, 0))
        out.paste(LINE + (255,), (0, 0), ring)
        out.alpha_composite(self.img)
        return out


def star(cx, cy, r1, r2, n, rot=0):
    return [(cx + (r1 if k % 2 == 0 else r2) * math.cos(rot + k * math.pi / n),
             cy + (r1 if k % 2 == 0 else r2) * math.sin(rot + k * math.pi / n)) for k in range(2 * n)]


def fight(s=50):
    """An impact: a red burst, an orange one inside, a yellow flash, a white heart."""
    f = Flat(s); c = s / 2 - .5
    f.part((224, 64, 40), lambda d: d.polygon(star(c, c, s * .48, s * .30, 8, -.2), fill=255), shadow=3)
    f.part((252, 148, 40), lambda d: d.polygon(star(c, c, s * .36, s * .22, 8, .2), fill=255), shadow=2, line=False)
    f.part((255, 220, 88), lambda d: d.polygon(star(c, c, s * .24, s * .15, 8, -.2), fill=255), shadow=2, line=False)
    f.spot((255, 252, 230), lambda d: d.ellipse((c - s * .09, c - s * .09, c + s * .07, c + s * .07), fill=255))
    return f.done()


def pokeball(s=36):
    f = Flat(s); c = s / 2 - .5; r = s / 2 - 2.5
    f.part((248, 248, 252), lambda d: d.ellipse((c - r, c - r, c + r, c + r), fill=255), shadow=4, sh=(196, 200, 216))
    f.part((232, 56, 48), lambda d: d.chord((c - r, c - r, c + r, c + r), 180, 360, fill=255), shadow=4, sh=(176, 32, 40))
    f.spot((255, 140, 120), lambda d: d.ellipse((c - r * .62, c - r * .78, c - r * .18, c - r * .45), fill=255))
    f.spot((255, 255, 255), lambda d: d.ellipse((c - r * .55, c - r * .72, c - r * .35, c - r * .55), fill=255))
    f.part(LINE, lambda d: d.rectangle((c - r, c - 1.5, c + r, c + 1.5), fill=255), shadow=0, line=False)
    f.part(LINE, lambda d: d.ellipse((c - 7, c - 7, c + 7, c + 7), fill=255), shadow=0, line=False)
    f.part((248, 248, 252), lambda d: d.ellipse((c - 4, c - 4, c + 4, c + 4), fill=255), shadow=2, sh=(200, 204, 220),
           line=False)
    return f.done()


def bez(*pts, n=12):
    """Points along the Bezier curve through `pts` (its control points), for smooth outlines on the pixel grid."""
    out = []
    for i in range(n + 1):
        t = i / n
        p = list(pts)
        while len(p) > 1:
            p = [(a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t) for a, b in zip(p, p[1:])]
        out.append(p[0])
    return out


def shoe(k=0.76):
    """A running shoe in profile, toe to the right: the heel high at the back, the collar's dip and the tongue, the
    instep sloping down to a white toe cap, a white side stripe, a white midsole that springs up at the toe and a slate
    outsole with treads; three speed lines behind it. Drawn on a 48x32 grid and scaled by `k` so that it sits inside
    the RUN piece's disc."""
    RED, RED_SH, RED_HI = (230, 58, 50), (170, 32, 38), (255, 128, 108)
    WH, WH_SH = (250, 250, 252), (192, 198, 216)
    SOLE, TREAD, LINING, GOLD = (110, 116, 140), (80, 86, 110), (96, 28, 34), (255, 204, 72)

    def P(pts):
        return [(x * k, y * k) for x, y in pts]

    def B(*pts):
        return P(bez(*pts))

    f = Flat(round(48 * k) + 2, round(32 * k) + 2)
    upper = (P([(5, 25)]) + B((5, 25), (3, 15), (5, 8), (9, 7)) + B((9, 7), (13, 7), (15, 11), (18, 10))
             + B((18, 10), (19, 6), (23, 5), (25, 8)) + B((25, 8), (30, 13), (38, 15), (43, 19))
             + B((43, 19), (46, 21), (46, 25), (44, 25)))
    f.part(RED, lambda d: d.polygon(upper, fill=255), shadow=2, sh=RED_SH)
    f.part(LINING, lambda d: d.polygon(B((8, 9), (12, 8), (15, 11), (16, 12)) + P([(12, 13), (8, 12)]), fill=255), shadow=0)
    f.part(RED_SH, lambda d: d.polygon(P([(5, 25)]) + B((4, 17), (5, 11), (7, 9)) + P([(9, 13), (9, 25)]), fill=255), shadow=0)
    f.part(WH, lambda d: d.polygon(B((36, 25), (36, 19), (40, 17), (43, 19)) + B((43, 19), (46, 21), (46, 25), (44, 25)),
                                   fill=255), shadow=1, sh=WH_SH)
    f.part(WH, lambda d: d.polygon(B((13, 23), (20, 17), (28, 15), (35, 17)) + B((35, 17), (28, 18.5), (21, 21.5), (17, 24.5)),
                                   fill=255), shadow=0)
    f.part(WH, lambda d: d.polygon(P([(3, 24), (44, 24)]) + B((44, 24), (47, 24), (47, 28), (44, 29)) + P([(4, 29), (3, 27)]),
                                   fill=255), shadow=1, sh=WH_SH)
    f.part(SOLE, lambda d: d.polygon(P([(4, 29), (43, 29), (41, 32), (5, 32)]), fill=255), shadow=0)
    img = f.done()
    d = ImageDraw.Draw(img)
    px = img.load()
    sole_y = round(31 * k)
    for x in range(round(8 * k), round(40 * k), 3):      # treads
        if px[x, sole_y][3]:
            px[x, sole_y] = TREAD + (255,)
    for i in range(3):                                   # laces
        x, y = round((22 + i * 4) * k), round((9 + i * 1.6) * k)
        d.line((x, y + 1, x + 1, y), fill=WH + (255,))
    d.point((round(6 * k), round(11 * k)), fill=RED_HI + (255,))   # shines
    d.line((round(27 * k), round(11 * k), round(32 * k), round(13.5 * k)), fill=RED_HI + (255,))
    m = 6
    out = Image.new('RGBA', (img.width + m, img.height), (0, 0, 0, 0))
    out.alpha_composite(img, (m, 0))
    d = ImageDraw.Draw(out)
    for y, l in ((9, 4), (13, 5), (18, 3)):              # speed lines
        d.line((m - l, y, m - 1, y), fill=GOLD + (255,))
        d.point((m - l - 1, y), fill=(255, 236, 160, 255))
    return out


def rgb565(r, g, b):
    v = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)
    return v or 0x0821                                   # 0 means clear


def emit(name, im):
    im = im.crop(im.getbbox())
    w, h = im.size
    px = im.load()
    rows = []
    for y in range(h):
        rows.append('    ' + ', '.join('0x%04X' % (rgb565(*px[x, y][:3]) if px[x, y][3] >= 128 else 0) for x in range(w)) + ',')
    return ['#define %s_W %d' % (name.upper(), w), '#define %s_H %d' % (name.upper(), h),
            'static const uint16_t k%s[%d] = {' % (name, w * h)] + rows + ['};']


def main():
    out = ['/* Generated by tools/make_battle_icons.py: the battle menu\'s flat icons, RGB565, 0 clear. */',
           '#ifndef BATTLE_FLAT_ICONS_H', '#define BATTLE_FLAT_ICONS_H', '#include <stdint.h>']
    for name, im in (('FlatFight', fight()), ('FlatBall', pokeball()), ('FlatShoe', shoe())):
        out += emit(name, im)
    out.append('#endif')
    OUT.write_text('\n'.join(out) + '\n')
    print('wrote', OUT)


if __name__ == '__main__':
    main()
