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


def shoe(s=44):
    """A running shoe in profile: red upper with a white swoosh, laces, a white sole; speed lines behind."""
    f = Flat(s); u = s / 48
    P = lambda pts: [(x * u, y * u) for x, y in pts]
    f.part((228, 56, 48), lambda d: d.polygon(P([(10, 33), (11, 16), (15, 12), (21, 13), (23, 19), (31, 22), (40, 25),
                                                 (45, 30), (45, 33)]), fill=255), shadow=3, sh=(168, 32, 36))
    f.part((248, 248, 252), lambda d: d.rounded_rectangle((8 * u, 31 * u, 46 * u, 38 * u), 3 * u, fill=255), shadow=2,
           sh=(176, 180, 200))
    f.part((248, 248, 252), lambda d: d.polygon(P([(16, 28), (30, 24), (40, 28), (22, 30)]), fill=255), shadow=0)
    f.part((228, 56, 48), lambda d: d.rectangle((8 * u, 35 * u, 46 * u, 36 * u), fill=255), shadow=0, line=False)
    img = f.done()
    d = ImageDraw.Draw(img)
    for k in range(3):                                   # laces
        x = (19 + k * 4) * u
        d.line((x, (17 + k * 1.5) * u, x + 3 * u, (14 + k * 1.5) * u), fill=(255, 255, 255, 255), width=1)
    for y, x0, l in ((18, 0, 7), (24, 2, 6), (30, 0, 6)):    # speed lines
        d.line((x0 * u, y * u, (x0 + l) * u, y * u), fill=(255, 200, 96, 255), width=2)
    return img


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
