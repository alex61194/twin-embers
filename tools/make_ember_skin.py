#!/usr/bin/env python3
"""Builds the lower screen's ember skin: 3ds_port/include/ember_skin.h.

Everything here is drawn by this script (no game pixels): the backdrop of the PokeTouch, battle and Summary screens
(a dark red gradient, faint diagonal stripes, a fire's glow at the foot and glowing embers) and the six 16x16 icons of
the START column (POKeDEX, POKeMON, BAG, the Trainer Card, SAVE, OPTIONS), flat and cel-shaded with a dark outline.
The BAG is the port's own battle Bag (battle_ui_art.h) reduced to 16x16.

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
K = (48, 40, 56)



def outline(im):
    px = im.load()
    w, h = im.size
    edge = []
    for y in range(h):
        for x in range(w):
            if px[x, y][3] == 0 and any(0 <= x + dx < w and 0 <= y + dy < h and px[x + dx, y + dy][3] and
                                        px[x + dx, y + dy][:3] != K for dx, dy in ((1, 0), (-1, 0), (0, 1), (0, -1))):
                edge.append((x, y))
    for x, y in edge:
        px[x, y] = K + (255,)
    return im


def pokeball():
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); px = im.load()
    cx = cy = 7.5
    for y in range(16):
        for x in range(16):
            dx, dy = x - cx, y - cy
            d = math.hypot(dx, dy)
            if d > 6.9:
                continue
            lit = -dx - dy                     # light from the upper left
            if abs(dy) < 1.0:
                c = K
            elif dy < 0:
                c = (255, 150, 128) if lit > 6.5 else (232, 56, 48) if lit > -3 else (176, 32, 40)
            else:
                c = (255, 255, 255) if lit > 1 else (216, 216, 228) if lit > -6 else (168, 168, 188)
            px[x, y] = c + (255,)
    for y in range(16):
        for x in range(16):
            d = math.hypot(x - cx, y - cy)
            if d <= 1.9:
                px[x, y] = ((255, 255, 255) if x + y < 15 else (216, 216, 228)) + (255,)
            elif d <= 2.95:
                px[x, y] = K + (255,)
    im = outline(im)
    px = im.load()
    for y in (7, 8):                           # the band runs out to the rim
        px[0, y] = px[15, y] = K + (255,)
    return im


def gear():
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); px = im.load()
    cx = cy = 7.5
    for y in range(16):
        for x in range(16):
            dx, dy = x - cx, y - cy
            d = math.hypot(dx, dy)
            a = math.atan2(dy, dx)
            f = (a / (2 * math.pi / 8)) % 1
            tooth = 0.3 < f < 0.8
            r = 7.5 if tooth else 5.0
            if d > r or d < 2.3:
                continue
            lit = -dx - dy
            c = (232, 232, 244) if lit > 4 else (176, 180, 200) if lit > -4 else (120, 124, 148)
            px[x, y] = c + (255,)
    im = outline(im)
    px = im.load()
    for y in range(16):                      # the hole's rim
        for x in range(16):
            d = math.hypot(x - cx, y - cy)
            if d < 2.3 and px[x, y][3] == 0 and d > 1.3:
                px[x, y] = K + (255,)
    return im


def floppy():
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); px = im.load()
    for y in range(1, 15):
        for x in range(1, 15):
            if x >= 13 and y <= 2:             # the clipped corner
                continue
            c = (64, 112, 216) if (x + y) < 12 else (48, 84, 184) if (x + y) < 22 else (32, 60, 140)
            px[x, y] = c + (255,)
    for y in range(1, 6):                      # the metal shutter
        for x in range(4, 11):
            px[x, y] = ((216, 220, 232) if x < 9 else (160, 164, 184)) + (255,)
    for y in range(2, 5):
        px[8, y] = (64, 112, 216, 255)
    for y in range(8, 14):                     # the label
        for x in range(3, 13):
            px[x, y] = ((255, 255, 255) if y < 12 else (220, 220, 232)) + (255,)
    for x in range(4, 12):
        px[x, 10] = (232, 64, 48, 255)         # a red line on it, FireRed's
    return outline(im)


def pokedex():
    """FireRed's Pokedex: a red case, the big blue lens, three small lights, a screen below."""
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); px = im.load()
    for y in range(1, 15):
        for x in range(2, 14):
            if (x in (2, 13)) and (y in (1, 14)):
                continue
            lit = -(x - 7.5) - (y - 7.5)
            c = (255, 112, 96) if lit > 8 else (224, 48, 48) if lit > -6 else (168, 28, 36)
            px[x, y] = c + (255,)
    for x in range(2, 14):                      # the hinge
        px[x, 8] = (136, 20, 28, 255)
    for y in range(1, 8):                       # the lens, in its white rim
        for x in range(2, 10):
            d = math.hypot(x - 5.5, y - 4)
            if d <= 2.0:
                px[x, y] = ((176, 228, 255) if x <= 5 and y <= 3 else (64, 144, 240) if d < 1.6
                            else (32, 80, 184)) + (255,)
            elif d <= 2.9:
                px[x, y] = (255, 255, 255, 255)
    for x, col in ((10, (248, 72, 72)), (11, (248, 216, 64)), (12, (88, 216, 96))):
        px[x, 2] = col + (255,)
    for y in range(10, 14):                     # the screen
        for x in range(4, 12):
            px[x, y] = ((168, 232, 200) if y == 10 or x == 4 else (96, 192, 152)) + (255,)
    return outline(im)




def card():
    """The Trainer Card: a blue card with the trainer's photo and two lines."""
    im = Image.new('RGBA', (16, 16), (0, 0, 0, 0)); px = im.load()
    for y in range(3, 14):
        for x in range(1, 15):
            if (x in (1, 14)) and (y in (3, 13)):
                continue
            c = (120, 176, 248) if x + y < 12 else (72, 136, 232) if x + y < 22 else (48, 96, 192)
            px[x, y] = c + (255,)
    for x in range(1, 15):                      # the title band
        for y in (3, 4):
            if not ((x in (1, 14)) and y == 3):
                px[x, y] = (232, 64, 48, 255)
    for y in range(6, 12):                      # the photo
        for x in range(3, 7):
            px[x, y] = ((255, 236, 200) if y < 9 else (232, 64, 48)) + (255,)
    for x in range(8, 13):                      # the lines
        px[x, 7] = px[x, 10] = (255, 255, 255, 255)
    return outline(im)


def bag():
    """The port's battle Bag (battle_ui_art.h), reduced to 16x16 by area."""
    src = ART.read_text()
    w = int(re.search(r'#define SBARTBAG_W (\d+)', src).group(1))
    h = int(re.search(r'#define SBARTBAG_H (\d+)', src).group(1))
    px = [int(v, 0) for v in re.findall(r'0x[0-9A-Fa-f]+|\d+',
                                        re.search(r'sBArtBagPx\[[^\]]*\] = \{(.*?)\};', src, re.S).group(1))]
    body = re.search(r'sBArtBagPal\[[^\]]*\] = \{(.*?)\};', src, re.S).group(1)
    pal = [None] + [tuple(int(v) for v in t) for t in re.findall(r'UI_RGB\(\s*(\d+),\s*(\d+),\s*(\d+)\)', body)]
    im = Image.new('RGBA', (w, h), (0, 0, 0, 0))
    o = im.load()
    for i, v in enumerate(px[:w * h]):
        if v:
            o[i % w, i // w] = pal[v] + (255,)
    im = im.crop(im.getbbox())
    k = 16 / max(im.size)
    im = im.resize((max(1, round(im.width * k)), max(1, round(im.height * k))), Image.LANCZOS)
    out = Image.new('RGBA', (16, 16), (0, 0, 0, 0))
    out.alpha_composite(im, ((16 - im.width) // 2, (16 - im.height) // 2))
    return out


ICONS = [('DEX', pokedex), ('BALL', pokeball), ('BAG', bag), ('CARD', card), ('SAVE', floppy), ('GEAR', gear)]


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
    out.append(f'#define EMBER_ICON 16')
    out.append(f'static const uint16_t kEmberIcon[{len(ICONS)}][256] = {{')
    alphas = []
    for name, f in ICONS:
        im = f().convert('RGBA')
        q = im.load()
        out.append('    {  /* %s */' % name)
        out.append(rows(['0x%04X' % rgb565(q[x, y]) for y in range(16) for x in range(16)]) + ',')
        out.append('    },')
        alphas.append((name, [str(q[x, y][3]) for y in range(16) for x in range(16)]))
    out.append('};')
    out.append(f'static const uint8_t kEmberIconA[{len(ICONS)}][256] = {{')
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
