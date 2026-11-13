#!/usr/bin/env python3
"""Builds the lower screen's ember skin: 3ds_port/include/ember_skin.h.

Everything here is drawn by this script (no game pixels): the backdrop of the PokeTouch, battle and Summary screens
(a dark red gradient, faint diagonal stripes, a fire's glow at the foot and glowing embers) and the six 16x16 icons of
the START column (POKeDEX, POKeMON, BAG, the Trainer Card, SAVE, OPTIONS), flat and cel-shaded with a slate outline, drawn pixel by pixel.

Needs Pillow; it is a development tool and its output is committed. Run it again after changing the art.
"""
import math
import random
import re
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / '3ds_port/include/ember_skin.h'
ART = ROOT / '3ds_port/include/battle_ui_art.h'
W, H = 320, 240
BG_PARTS = 4
EMBER_T, EMBER_B = (132, 26, 22), (34, 6, 8)



# The START column's icons, drawn pixel by pixel (ICON x ICON): a slate outline (64, 72, 104), flat tones, the light from the
# upper left. In the pictures below '.' is clear, 'O' the outline and any other letter a colour of that picture's own table.
O = (64, 72, 104)
ICON = 16

DEX = ("""
..OOOOOOOOOOOO..
.OrrrrrrrrrrrRO.
OrrWWWWrRcRyRgDO
OrWwbBBWRRRRRRDO
OrWbBBBWRRRRRRDO
OrWBBBNWRRRRRRDO
OrrWWWWrRRRRRRDO
OrRRRRRRRRRRRRDO
OnnnnnnnnnnnnnnO
OrRRRRRRRRRRRRDO
OrROOOOOOOOOORDO
OrROGGGGGGGGORDO
OrROGHHHHHHHORDO
OrROGHHHHHIIORDO
OrROOOOOOOOOORDO
..OOOOOOOOOOOO..
""", {'r': (255, 128, 110), 'R': (230, 56, 48), 'D': (174, 32, 44), 'n': (126, 20, 34), 'W': (255, 255, 255),
      'w': (214, 240, 255), 'b': (120, 196, 255), 'B': (56, 136, 240), 'N': (30, 84, 190), 'c': (120, 230, 255),
      'y': (255, 226, 80), 'g': (104, 226, 120), 'G': (200, 246, 218), 'H': (112, 206, 162), 'I': (70, 160, 128)})

BAG = ("""
.....OOOOOO.....
....OttttttO....
....OtOOOOTO....
...OOtO..OTOO...
..OffffffffffO..
.OffFFFFFFFFFEO.
.OFFFFFyyFFFFEO.
.OOOOOOyYOOOOOO.
.OtttTTTTTTTTuO.
.OtTOOOOOOOOTuO.
.OtTOppppppOTuO.
.OtTOPPPPPPOTuO.
.OtTOOOOOOOOTuO.
.OTTTTTTTuuuuuO.
..OTTTTuuuuuuO..
...OOOOOOOOOO...
""", {'f': (255, 124, 104), 'F': (226, 60, 52), 'E': (170, 34, 44), 't': (250, 204, 136), 'T': (218, 154, 86),
      'u': (164, 104, 60), 'p': (255, 220, 158), 'P': (236, 178, 108), 'y': (255, 240, 150), 'Y': (240, 190, 60)})

CARD = ("""
................
................
.OOOOOOOOOOOOOO.
OrrrrrrrrrrrrRRO
ORRRRRRRRRRRRRDO
ObbbbbbbbbbBBBBO
ObOOOOOObBBBBBNO
ObOhhhhObWWWWBNO
ObOskksObBBBBBNO
ObOssssObWWWWBNO
ObOqQQqObBBBBBNO
OBOOOOOOBBByyBNO
OBBBBBBBBBNNyyNO
.OOOOOOOOOOOOOO.
................
................
""", {'r': (255, 124, 104), 'R': (228, 60, 52), 'D': (172, 36, 46), 'b': (156, 204, 255), 'B': (92, 150, 240),
      'N': (56, 106, 204), 'h': (112, 72, 54), 's': (255, 228, 194), 'k': O, 'q': (255, 124, 104), 'Q': (228, 60, 52),
      'W': (255, 255, 255), 'y': (255, 224, 84)})

SAVE = ("""
.OOOOOOOOOOOOO..
ObbOwwwwwwgOBBO.
ObbOwwwwBwgOBBBO
ObbOwwwwBwgOBBNO
ObBOwwwwwwgOBBNO
ObBOOOOOOOOOBBNO
ObBBBBBBBBBBBBNO
OBBOOOOOOOOOOBNO
OBBOWWWWWWWWOBNO
OBBOWrrrrrrWOBNO
OBBOWWWWWWWWONNO
OBBOWggggggWONNO
OBBOWWWWWWWsONNO
OBNOssssssssONNO
OOOOOOOOOOOOOOOO
................
""", {'b': (134, 180, 255), 'B': (72, 122, 226), 'N': (44, 84, 180), 'w': (238, 242, 252), 'g': (172, 180, 206),
      'W': (255, 255, 255), 's': (222, 226, 240), 'r': (232, 64, 48)})

GEAR_MASK = """
......XXXX......
..XX..XXXX..XX..
.XXXXXXXXXXXXXX.
.XXXXXXXXXXXXXX.
..XXXXXXXXXXXX..
.XXXXXXXXXXXXXX.
XXXXXXX..XXXXXXX
XXXXXX....XXXXXX
XXXXXX....XXXXXX
XXXXXXX..XXXXXXX
.XXXXXXXXXXXXXX.
..XXXXXXXXXXXX..
.XXXXXXXXXXXXXX.
.XXXXXXXXXXXXXX.
..XX..XXXX..XX..
......XXXX......
"""


def art_rows(art):
    r = art.strip('\n').split('\n')
    assert len(r) == ICON and all(len(l) == ICON for l in r), [(i, len(l)) for i, l in enumerate(r) if len(l) != ICON]
    return r


def from_art(art, pal):
    im = Image.new('RGBA', (ICON, ICON), (0, 0, 0, 0))
    px = im.load()
    for y, l in enumerate(art_rows(art)):
        for x, ch in enumerate(l):
            if ch == 'O':
                px[x, y] = O + (255,)
            elif ch != '.':
                px[x, y] = pal[ch] + (255,)
    return im


def ball():
    """A circle: the red top, the white bottom, the slate band, and a round button (a plus-shaped white 4x4 in a slate ring)."""
    im = Image.new('RGBA', (ICON, ICON), (0, 0, 0, 0))
    px = im.load()
    c = (ICON - 1) / 2
    for y in range(ICON):
        for x in range(ICON):
            dx, dy = x - c, y - c
            d = math.hypot(dx, dy)
            if d > 7.95:
                continue
            lit = -(dx + dy)
            if d > 6.9 or abs(dy) < 1.0:
                col = O
            elif dy < 0:
                col = (255, 132, 112) if lit > 6.2 else (232, 56, 48) if lit > -3.2 else (176, 32, 44)
            else:
                col = (255, 255, 255) if lit > 1.0 else (226, 230, 242) if lit > -6.0 else (176, 182, 206)
            if d <= 1.7:
                col = (255, 255, 255) if dx + dy < 0.5 else (226, 230, 242)
            elif d <= 3.4:
                col = O
            px[x, y] = col + (255,)
    for x, y in ((3, 4), (4, 3)):
        px[x, y] = (255, 236, 228, 255)
    return im


def gear():
    m = art_rows(GEAR_MASK)

    def on(x, y):
        return 0 <= x < ICON and 0 <= y < ICON and m[y][x] == 'X'

    im = Image.new('RGBA', (ICON, ICON), (0, 0, 0, 0))
    px = im.load()
    c = (ICON - 1) / 2
    for y in range(ICON):
        for x in range(ICON):
            if not on(x, y):
                continue
            if not all(on(x + dx, y + dy) for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                px[x, y] = O + (255,)                       # the rim, outside and round the hole
                continue
            lit = -((x - c) + (y - c))
            col = (246, 248, 255) if lit > 4.5 else (206, 212, 230) if lit > -4.5 else (152, 160, 190)
            if max(abs(x - c), abs(y - c)) < 4.1:           # the hub's ring, a tone down
                col = (206, 212, 230) if lit > 4.5 else (152, 160, 190) if lit > -4.5 else (122, 130, 162)
            px[x, y] = col + (255,)
    return im


ICONS = [('DEX', lambda: from_art(*DEX)), ('BALL', ball), ('BAG', lambda: from_art(*BAG)), ('CARD', lambda: from_art(*CARD)),
         ('SAVE', lambda: from_art(*SAVE)), ('GEAR', gear)]


def grad(w, h, top, bot):
    im = Image.new('RGBA', (w, h))
    for y in range(h):
        t = y / max(1, h - 1)
        im.paste(tuple(round(top[i] + (bot[i] - top[i]) * t) for i in range(3)) + (255,), (0, y, w, y + 1))
    return im


def embers(w, h, seed):
    """Glowing embers drifting up, and the heat of a fire at the foot of the screen."""
    rnd = random.Random(seed)
    glow = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    d = ImageDraw.Draw(glow)
    d.ellipse((-w // 2, h - 40, w + w // 2, h + 60), fill=(255, 110, 30, 100))
    sparks = []
    for _ in range(w * h // 420):
        x, y = rnd.randrange(w), rnd.randrange(h)
        y = int(h - (h - y) * rnd.random() ** 0.5)
        r = rnd.choice((1, 1, 1, 2))
        col = rnd.choice(((255, 200, 80), (255, 150, 40), (255, 96, 32)))
        d.ellipse((x - 3 * r, y - 3 * r, x + 3 * r, y + 3 * r), fill=col + (110,))
        sparks.append((x, y, r, col))
    glow = glow.filter(ImageFilter.GaussianBlur(3))
    d = ImageDraw.Draw(glow)
    for x, y, r, col in sparks:
        d.rectangle((x, y, x + r - 1, y + r - 1), fill=(255, 236, 170, 255) if r == 1 else col + (255,))
    return glow


def backdrop():
    g = grad(W, H, EMBER_T, EMBER_B)
    px = g.load()
    for y in range(H):
        for x in range(W):
            if (x + y) % 8 < 2:
                r, gg, b, a = px[x, y]
                px[x, y] = (max(0, r - 14), max(0, gg - 4), max(0, b - 4), a)
    g.alpha_composite(embers(W, H, 0))
    return g.convert('RGB')


def rgb565(c):
    r, g, b = c[:3]
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def rows(values, per=16):
    return ',\n'.join('    ' + ', '.join(values[i:i + per]) for i in range(0, len(values), per))


def main():
    bg = backdrop()
    p = bg.load()
    head = ['/* Generated by tools/make_ember_skin.py (port-authored art, no game pixels). Do not edit: change the script',
            ' * and run it again. */']
    for part in range(BG_PARTS):
        rows_per = H // BG_PARTS
        name = OUT.with_name('ember_skin_bg%d.h' % part)
        lines = head + [f'static const uint16_t kEmberBg{part}[{rows_per}][{W}] = {{']
        for y in range(part * rows_per, (part + 1) * rows_per):
            lines.append('    {' + ', '.join('0x%04X' % rgb565(p[x, y]) for x in range(W)) + '},')
        lines.append('};')
        name.write_text('\n'.join(lines) + '\n')
    out = head + ['#ifndef EMBER_SKIN_H', '#define EMBER_SKIN_H', '', '#include <stdint.h>', '',
                  '/* The ember backdrop, RGB565, in quarters of 60 rows. */']
    out += ['#include "ember_skin_bg%d.h"' % part for part in range(BG_PARTS)]
    out.append('static const uint16_t (*const kEmberBg[%d])[%d] = {%s};' % (
        BG_PARTS, W, ', '.join('kEmberBg%d' % part for part in range(BG_PARTS))))
    out.append('#define EMBER_BG(x, y) (kEmberBg[(y) / %d][(y) %% %d][x])' % (H // BG_PARTS, H // BG_PARTS))
    out.append('')
    out.append('/* The START column\'s icons, 16x16: RGB565 and coverage (0 clear, 255 solid), in the order of PT_POKEDEX ..')
    out.append(' * PT_OPTIONS. */')
    out.append(f'#define EMBER_ICON {ICON}')
    out.append(f'static const uint16_t kEmberIcon[{len(ICONS)}][{ICON * ICON}] = {{')
    alphas = []
    for name, f in ICONS:
        im = f().convert('RGBA')
        q = im.load()
        out.append('    {  /* %s */' % name)
        out.append(rows(['0x%04X' % rgb565(q[x, y]) for y in range(ICON) for x in range(ICON)]) + ',')
        out.append('    },')
        alphas.append((name, [str(q[x, y][3]) for y in range(ICON) for x in range(ICON)]))
    out.append('};')
    out.append(f'static const uint8_t kEmberIconA[{len(ICONS)}][{ICON * ICON}] = {{')
    for name, a in alphas:
        out.append('    {  /* %s */' % name)
        out.append(rows(a) + ',')
        out.append('    },')
    out.append('};')
    out.append('')
    out.append('#endif')
    OUT.write_text('\n'.join(out) + '\n')
    print('wrote', OUT)


if __name__ == '__main__':
    main()
