#!/usr/bin/env python3
"""Builds the lower screen's ember skin: 3ds_port/include/ember_skin.h.

Everything here is drawn by this script (no game pixels): the backdrop of the PokeTouch, battle and Summary screens
(a dark red gradient, faint diagonal stripes, a fire's glow at the foot and glowing embers) and the six 16x16 icons of
the START column (POKeDEX, POKeMON, BAG, the Trainer Card, SAVE, OPTIONS), flat and cel-shaded, each with an outline in the dark tone of its own colour, drawn pixel by pixel.

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



# The START column's icons, drawn pixel by pixel (ICON x ICON): flat, cel-shaded sprites in hue-shifted ramps (lights lean
# warm, shadows cool), the light from the upper left, and an outline in the dark tone of the colour it borders (tint_outline)
# instead of one slate for all. In the pictures below '.' is clear, 'O' the outline and any other letter a colour of that
# picture's own table.
O = (64, 72, 104)
ICON = 16
RED = ((255, 156, 108), (232, 64, 56), (168, 32, 72), (112, 24, 64))
YEL = ((255, 244, 160), (248, 192, 72), (212, 120, 56))
BLU = ((152, 224, 255), (80, 136, 236), (60, 80, 188))
GRY = ((250, 250, 255), (206, 212, 236), (148, 148, 192), (116, 116, 164))
OUTLINE_MIX = ((24, 10, 36), 0.78)      # the touched colours' average, pulled this far towards this dark plum
VIVID = 0.18                            # and every colour's distance from its grey is made this much larger

DEX = ("""
..OOOOOOOOOOOO..
.OrrrrrrrrrrrRO.
OrrWWWWrRRRRRRDO
OrWwbBBWRcRyRgDO
OrWbBBNWRcRyRgDO
OrWBBNNWRRRRRRDO
OrrWWWWrRRRRRRDO
OnnnnnnnnnnnnnnO
OrRRRRRRRRRRRRDO
OrRnnnnnnnnnnRDO
OrRnGGGGGGHHnRDO
OrRnGGGGHHHHnRDO
OrRnGGHHHHHHnRDO
OrRnnnnnnnnnnRDO
.ORRRRRRRRRDDDO.
..OOOOOOOOOOOO..
""", {'r': RED[0], 'R': RED[1], 'D': RED[2], 'n': RED[3], 'W': (255, 255, 255),
      'w': (214, 240, 255), 'b': BLU[0], 'B': BLU[1], 'N': BLU[2], 'c': (120, 230, 255),
      'y': (255, 226, 80), 'g': (104, 226, 120), 'G': (216, 250, 232), 'H': (128, 214, 176)})

# The bag is the game's hiker's backpack, drawn small from its left half and mirrored (the right side is a shade darker): two
# ears, a flap with a grey clasp, a seam, side pockets, a front pocket, a dark base and an orange tab on the right pocket.
BAG_HALF = ["........",
            "..OOO...",
            ".OyyYO..",
            ".OyyYOOO",
            ".OyYuOyy",
            "..Odddyy",
            "..OyyyyG",
            "OOOuyyGy",
            "OyyuuuuY",
            "OyyuyddY",
            "OyYuydyy",
            "OyYuydyy",
            "Oyuuyddd",
            "OOOubbbb",
            "..OOOOOO",
            "........"]
BAG_SHADE = {'y': 'Y', 'Y': 'u'}
BAG_PAL = {'y': YEL[0], 'Y': YEL[1], 'u': (222, 156, 46), 'd': (166, 104, 44), 'b': (112, 96, 128), 'G': (176, 182, 214), 'a': (240, 96, 40)}
BAG = ('\n'.join(l + ''.join(BAG_SHADE.get(c, c) for c in reversed(l)) for l in BAG_HALF) + '\n', BAG_PAL)
BAG_TAB = ((14, 9), (14, 10))             # the orange tab; the pocket's left twin stays light


CARD = ("""
................
................
.OOOOOOOOOOOOOO.
ObbbbbbbbbbbbbBO
ObOOOOOObbbbbBNO
ObOhhhhObWWWWBNO
ObOhsshObBBBBBNO
ObOssssObWWWBBNO
ObOrRRrObBBBBBNO
ObOrRRrObWWWWBNO
OBOOOOOOBBBBBNNO
OBBBBBBBBBNNNNNO
OyyyyyyyyyyyyYYO
.OOOOOOOOOOOOOO.
................
................
""", {'b': BLU[0], 'B': BLU[1], 'N': BLU[2], 'h': (112, 72, 54), 's': (255, 228, 194),
      'r': RED[0], 'R': RED[1], 'W': (255, 255, 255), 'y': YEL[0], 'Y': YEL[1]})

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
""", {'b': BLU[0], 'B': BLU[1], 'N': BLU[2], 'w': (238, 242, 252), 'g': GRY[2],
      'W': (255, 255, 255), 's': GRY[1], 'r': RED[1]})

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
    """A circle: the red top, the white bottom, a slate band, and a round button (a white disc in a slate ring)."""
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
                col = RED[0] if lit > 6.2 else RED[1] if lit > -3.2 else RED[2]
            else:
                col = GRY[0] if lit > 1.0 else GRY[1] if lit > -6.0 else GRY[2]
            if d <= 1.7:
                col = GRY[0] if dx + dy < 0.5 else GRY[1]
            elif d <= 3.4:
                col = O
            px[x, y] = col + (255,)
    for x, y in ((3, 4), (4, 3)):
        px[x, y] = (255, 240, 224, 255)
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
            k = 1 if math.hypot(x - c, y - c) < 4.3 else 0   # the hub's ring, a tone down
            col = GRY[k] if lit > 4.5 else GRY[1 + k] if lit > -4.5 else GRY[2 + k]
            px[x, y] = col + (255,)
    return im


def bag():
    im = from_art(*BAG)
    for xy in BAG_TAB:
        im.putpixel(xy, BAG_PAL['a'] + (255,))
    for xy in ((1, 9), (1, 10)):
        im.putpixel(xy, YEL[0] + (255,))
    return im


def finish(im):
    """The outline in its own colour, then the colours made a little livelier."""
    im = tint_outline(im)
    px = im.load()
    for y in range(ICON):
        for x in range(ICON):
            r, g, b, a = px[x, y]
            if a and (r, g, b) != O:
                m = (r + g + b) / 3
                px[x, y] = tuple(max(0, min(255, round(m + (c - m) * (1 + VIVID)))) for c in (r, g, b)) + (255,)
    return im


def tint_outline(im):
    """Each outline pixel takes the average of the colours it touches, pulled towards a dark plum: a red icon gets a dark red
    rim, a blue one a dark blue rim. An outline pixel that touches no colour keeps the slate."""
    src = im.load()
    out = im.copy()
    dst = out.load()
    mix, k = OUTLINE_MIX
    for y in range(ICON):
        for x in range(ICON):
            if src[x, y][3] == 0 or src[x, y][:3] != O:
                continue
            near = [src[x + a, y + b][:3] for a in (-1, 0, 1) for b in (-1, 0, 1)
                    if 0 <= x + a < ICON and 0 <= y + b < ICON and src[x + a, y + b][3] and src[x + a, y + b][:3] != O]
            if near:
                avg = [sum(c[i] for c in near) // len(near) for i in range(3)]
                dst[x, y] = tuple(round(avg[i] + (mix[i] - avg[i]) * k) for i in range(3)) + (255,)
    return out


ICONS = [('DEX', lambda: finish(from_art(*DEX))), ('BALL', lambda: finish(ball())), ('BAG', lambda: finish(bag())),
         ('CARD', lambda: finish(from_art(*CARD))), ('SAVE', lambda: finish(from_art(*SAVE))), ('GEAR', lambda: finish(gear()))]


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
