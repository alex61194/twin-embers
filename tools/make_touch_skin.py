#!/usr/bin/env python3
"""Builds the PokeTouch screen's skin from its artwork.

The artwork is one 320x240 picture of the whole lower screen with the SAVE screen open (docs/media/poketouch.png):
the side column (POKeDEX ... OPTIONS, the X and Y shortcuts, RUN), the save card and the dialogue under it. It is used
as painted; this script only takes out what the game changes and keeps the pieces the screen is drawn with, in
3ds_port/include/poketouch_skin.h (and the backdrop's quarters poketouch_skin_bg0.h .. bg3.h):

* the backdrop: the artwork with the save card's values and badges, the portrait, the dialogue's text and buttons
  taken out (each row filled with its own background);
* every side button as painted, and selected: SAVE's golden frame with the button's own label and icon; the Trainer
  Card's without its label (the player's name is written on it); SAVE as a plain button, from BAG's frame;
* the X and Y tiles (Y with and without the Bicycle), RUN on and off;
* YES (the focused, green button) and NO (the red one) without their labels, and the labels apart;
* the portrait, the five badges and the dotted leader of the save card;
* the lettering of the values and the button labels (the move names' letters, 10 rows and 9 rows).

Needs Pillow; it is a development tool and its output is committed. Run it again after changing the artwork.
"""
from pathlib import Path

from PIL import Image

import make_move_skin as msk

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'docs/media/poketouch.png'
OUT = ROOT / '3ds_port/include/poketouch_skin.h'
BG_PARTS = 4
KEY = (248, 0, 248)                                           # see-through in a picture (RGB565 0xF81F)

# The side column: each button's area (x0, y0, x1, y1, exclusive), the button as painted inside.
SIDE = [(241, 6, 314, 33), (241, 33, 314, 60), (241, 60, 314, 87), (241, 87, 314, 112), (241, 112, 314, 145),
        (241, 145, 314, 170)]
SIDE_SAVE, SIDE_CARD, SIDE_PLAIN_FROM = 4, 3, 2
SIDE_INK = (250, 314)                                         # where a button's label and icon are
SEL_INNER = (245, 119, 313, 138)                              # inside the golden frame
SEL_FREE = [250, 251, 289, 290, 291]
SAVE_EDGE = (156, 112, 232)                                   # SAVE's colour edge as a plain button
SAVE_LETTERS = (251, 282)                                     # its label (inside the brackets) and floppy disk
SAVE_DISK = (292, 311)
OUTLINE = (8, 12, 20)
TILE_X = (241, 170, 277, 208)
TILE_Y = (278, 170, 314, 208)
TILE_Y_ICON = [(284, 182, 312, 203), (295, 178, 312, 182)]   # the Bicycle
RUN = (241, 208, 314, 237)
RUN_PILL = (250, 215, 266, 229)                               # "ON" in its capsule
RUN_WORD = (253, 218, 264, 227)
RUN_EDGE = (240, 248)
# The save card (the SAVE screen): each value with its dotted leader and the badges, taken out.
VALUES = [  # dots from x, dots top, value top, value right (inclusive), ink
    (143, 61, 53, 184, (248, 252, 248)),                      # the player's name (left of the frame's diagonal)
    (146, 94, 86, 211, (248, 252, 248)),                      # badges
    (149, 112, 103, 211, (248, 252, 248)),                    # Pokedex
    (159, 130, 121, 211, (248, 252, 248)),                    # play time
    (134, 148, 139, 211, (248, 252, 152)),                    # money
]
HOLES = [(141, 50, 192, 65), (143, 68, 214, 98), (146, 101, 214, 116), (155, 118, 214, 134), (130, 136, 214, 152)]
# The save card fills the GBA picture's place (240x160 at (4, 4)): the artwork's card, its thick bezel trimmed, widened by
# repeating a flat column of each row and made 5 rows taller by repeating a flat row of the gap between title and panel.
CARD_SRC = (9, 7, 223, 162)                                   # x0, y0, x1, y1 (exclusive) of the artwork's card
CARD_DST = (4, 4, 244, 164)
CARD_INS_ROWS = (40, 5)                                       # repeat this row 5 times
def card_ins(y):
    """The columns repeated on row y (where the row is flat) and how many times each."""
    return [(40, 13), (190, 13)] if y < 42 else [(170, 26)]
def card_x(x, y):
    return x - CARD_SRC[0] + CARD_DST[0] + sum(n for at, n in card_ins(y) if x >= at)
def card_y(y):
    return y - CARD_SRC[1] + CARD_DST[1] + (CARD_INS_ROWS[1] if y > CARD_INS_ROWS[0] else 0)
DIAGONAL = 136                                                # the name stops short of the frame: x < y + DIAGONAL
BADGES = [(147, 69, 161, 85), (161, 69, 175, 85), (175, 69, 189, 85), (189, 69, 202, 85), (202, 69, 214, 85)]
DOTS = (146, 94, 203, 96)                                     # the leader as painted (2x2 dots, period 3)
PORTRAIT = (23, 52, 79, 132)
PORTRAIT_FREE = [28, 29]
# The side column is narrower: columns taken out of each picture (x in the artwork), where nothing is drawn.
SIDE_CUT = {244, 245, 246, 247, 252}
SAVE_SEL_CUT = {250, 251, 252, 288, 289}                      # the selected SAVE keeps its brackets and its frame
RUN_CUT = {244, 245, 268, 269, 270}
TILE_X_CUT = {270, 271}
TILE_Y_CUT = {281, 282}
# The dialogue box grows to the picture above it and the column on its right: rows and columns of it repeated.
BOX_TOP, BOX_TOP_NEW = (178, 184), 168                        # its top rows (kept as the new top), and where it now starts
BOX_RIGHT, BOX_RIGHT_NEW, BOX_END = (222, 229), 237, 244                    # its right border columns, and where they now are
# The dialogue: its text and buttons; YES and NO and their labels.
DIALOG_TEXT = (14, 183, 168, 232)
DIALOG_BUTTONS = (168, 179, 226, 232)
DIALOG_FREE = [14, 15, 16, 17]
DIALOG_EDGE = (226, 231)                                      # its right edge, under YES's glow: from row EDGE_ROW
EDGE_ROW = 226
YES = (168, 179, 230, 208)
NO = (174, 208, 225, 232)
YES_LABEL = (187, 186, 211, 201)
NO_LABEL = (190, 211, 208, 226)
INTERIOR = (32, 0, 0)
SHIFT_FROM, SHIFT = 235, 11                                   # the side column moves 11 right: the GBA picture ends at x = 243, its rim at 245


def lum(c):
    return msk.lum(c)


def dist(a, b):
    return sum(abs(a[i] - b[i]) for i in range(3))


def median(cols):
    return tuple(int(v) for v in msk.median(list(cols)))


def mode(cols):
    seen = {}
    for c in cols:
        seen[c] = seen.get(c, 0) + 1
    return max(seen, key=seen.get)


def fill_rows(px, box, free=None, right=None, lo=12, hi=60):
    """Each row of `box` takes its background: the median of its pixels in the background's brightness (of its `free`
    columns, or of the whole row), else the row above's."""
    x0, y0, x1, y1 = box
    last = None
    for y in range(y0, y1):
        cols = [px[x, y] for x in (free or range(x0, x1))]
        bg = [c for c in cols if lo <= lum(c) < hi]
        c = last = median(bg) if bg else (last or median(cols))
        for x in range(x0, min(x1, right(y)) if right else x1):
            px[x, y] = c


def crop(img, box):
    return img.crop(box).copy()


def body_colour(art, y, x0=248, x1=314):
    return mode(art.load()[x, y] for x in range(x0, x1))


def overlay(art, box, colour_of_row, threshold=60, grow=1):
    """What lies on a flat body inside `box`: the pixels away from the row's body colour and `grow` round them (their
    outline); the rest KEY."""
    px = art.load()
    x0, y0, x1, y1 = box
    ink = {(x, y) for y in range(y0, y1) for x in range(x0, x1) if dist(px[x, y], colour_of_row(y)) > threshold}
    keep = {(x + dx, y + dy) for x, y in ink for dy in range(-grow, grow + 1) for dx in range(-grow, grow + 1)
            if x0 <= x + dx < x1 and y0 <= y + dy < y1}
    out = Image.new('RGB', (x1 - x0, y1 - y0), KEY)
    op = out.load()
    for x, y in keep:
        op[x - x0, y - y0] = px[x, y]
    return out


def paste_key(dst, src, x, y):
    dp, sp = dst.load(), src.load()
    for v in range(src.size[1]):
        for u in range(src.size[0]):
            if sp[u, v] != KEY and 0 <= x + u < dst.size[0] and 0 <= y + v < dst.size[1]:
                dp[x + u, y + v] = sp[u, v]


def golden(art, h):
    """SAVE's golden frame, empty, `h` rows tall (rows from its middle go first)."""
    x0, y0, x1, y1 = SIDE[SIDE_SAVE]
    frame = crop(art, SIDE[SIDE_SAVE])
    fp = frame.load()
    for y in range(SEL_INNER[1], SEL_INNER[3]):
        c = mode(fp[x - x0, y - y0] for x in SEL_FREE)
        for x in range(SEL_INNER[0], SEL_INNER[2]):
            fp[x - x0, y - y0] = c
    rows = [[fp[u, v] for u in range(frame.size[0])] for v in range(frame.size[1])]
    while len(rows) > h:
        del rows[len(rows) // 2]
    out = Image.new('RGB', (frame.size[0], h))
    op = out.load()
    for v, row in enumerate(rows):
        for u, c in enumerate(row):
            op[u, v] = c
    return out


def side_buttons(art, clean):
    """Per button its area as (normal, selected); the Trainer Card's without the label; and where that label goes."""
    px = art.load()
    out = []
    for k, (x0, y0, x1, y1) in enumerate(SIDE):
        h = y1 - y0
        if k == SIDE_SAVE:
            sel = crop(art, SIDE[k])
            # The plain SAVE: BAG's button with SAVE's colour edge, centred in SAVE's area, SAVE's label and disk on it.
            sx0, sy0, sx1, sy1 = SIDE[SIDE_PLAIN_FROM]
            btn = crop(art, SIDE[SIDE_PLAIN_FROM])
            bp = btn.load()
            for v in range(sy1 - sy0):
                body = body_colour(art, sy0 + v)
                for u in range(sx1 - sx0):
                    x = sx0 + u
                    if SIDE_INK[0] <= x < SIDE_INK[1] - 1 and dist(bp[u, v], body) > 0 and 3 <= v < sy1 - sy0 - 4:
                        bp[u, v] = body
                    elif RUN_EDGE[0] <= x < 252 and bp[u, v][1] > bp[u, v][0] + 12:
                        l = lum(bp[u, v]) / 190
                        bp[u, v] = tuple(min(255, int(SAVE_EDGE[i] * l)) for i in range(3))
            plain = crop(clean, SIDE[k])
            top = (h - btn.size[1]) // 2
            plain.paste(btn, (0, top))
            pp = plain.load()
            for v in range(h):                                # the rows round it: BAG's own, not SAVE's golden glow
                if v < top or v >= top + btn.size[1]:
                    for u in range(plain.size[0]):
                        pp[u, v] = bp[u, 0 if v < top else btn.size[1] - 1]
            # SAVE's letters (white, outlined dark as on the other buttons) and its floppy disk, from the painted one.
            frame = golden(art, h)
            fpx = frame.load()
            mid_sel = (SEL_INNER[1] + SEL_INNER[3]) // 2
            dy = top + (sy1 - sy0) // 2 - 1 - (mid_sel - y0)
            pl = plain.load()
            letters = {(x, y) for y in range(SEL_INNER[1], SEL_INNER[3]) for x in range(*SAVE_LETTERS) if lum(px[x, y]) > 170}
            for x, y in letters:
                for ny in range(y - 1, y + 2):
                    for nx in range(x - 1, x + 2):
                        if (nx, ny) not in letters:
                            pl[nx - x0, ny - y0 + dy] = OUTLINE
            for x, y in letters:
                pl[x - x0, y - y0 + dy] = px[x, y]
            for y in range(SEL_INNER[1], SEL_INNER[3]):
                for x in range(*SAVE_DISK):
                    if dist(px[x, y], fpx[SEL_FREE[0] - x0, y - y0]) > 90:
                        pl[x - x0, y - y0 + dy] = px[x, y]
            out.append((plain, sel))
            continue
        normal = crop(art, SIDE[k])
        ink = overlay(art, (SIDE_INK[0], y0 + 3, SIDE_INK[1] - 1, y1 - 4), lambda y: body_colour(art, y))
        if k == SIDE_CARD:                                    # the name is written: no label on either
            ip, npx = ink.load(), normal.load()
            for v in range(ink.size[1]):
                for u in range(0, 46):
                    ip[u, v] = KEY
            for v in range(3, h - 4):
                body = body_colour(art, y0 + v)
                for x in range(SIDE_INK[0], 296):
                    npx[x - x0, v] = body
        frame = golden(art, h)
        paste_key(frame, ink, SIDE_INK[0] - x0, 3)
        out.append((normal, frame))
    return out


def label_box(art, box):
    """Where the white lettering inside `box` is (x0, y0, x1, y1 inclusive)."""
    px = art.load()
    pts = [(x, y) for y in range(box[1], box[3]) for x in range(box[0], box[2]) if min(px[x, y]) > 200]
    return min(p[0] for p in pts), min(p[1] for p in pts), max(p[0] for p in pts), max(p[1] for p in pts)


def build_backdrop(art):
    clean = art.copy()
    cp = clean.load()
    for k, box in enumerate(HOLES):
        fill_rows(cp, box, right=(lambda y: y + DIAGONAL) if k == 0 else None)
    fill_rows(cp, PORTRAIT, PORTRAIT_FREE, lo=0, hi=256)
    base = [[cp[x, y] for x in range(320)] for y in range(240)]
    fill_rows(cp, DIALOG_TEXT, lo=8, hi=20)
    fill_rows(cp, DIALOG_BUTTONS, DIALOG_FREE, lo=8, hi=20)
    for y in range(DIALOG_BUTTONS[1], 183):                   # the dialogue's top edge, under YES's glow
        for x in range(DIALOG_BUTTONS[0], DIALOG_EDGE[1]):
            cp[x, y] = cp[150, y]
    for y in range(DIALOG_BUTTONS[1], EDGE_ROW):              # and its right edge
        if y >= 183:
            for x in range(*DIALOG_EDGE):
                cp[x, y] = cp[x, EDGE_ROW]
    for x in range(319, SHIFT_FROM + SHIFT - 1, -1):          # the side column's panel, to the right (the GBA picture
        for y in range(240):                                  # ends at x = 243)
            cp[x, y] = cp[x - SHIFT, y]
    for y in range(240):                                       # the old buttons' left edges left behind: the panel's margin
        for x in range(239, SHIFT_FROM + SHIFT):
            cp[x, y] = cp[238, y]
    for y in range(108, 150):                                  # SAVE's golden glow beside its button: the column's own
        for x in range(233, 258):
            cp[x, y] = cp[x, 106 if y < 129 else 151]
    # The dialogue box: taller (to the picture's rim) and wider (to the column).
    t0, t1 = BOX_TOP
    top = [[cp[x, y] for x in range(4, BOX_END)] for y in range(t0, t1)]
    for y in range(BOX_TOP_NEW, 233 + 1):
        src = [cp[x, 200] for x in range(4, BOX_END)] if y < 226 else [cp[x, y] for x in range(4, BOX_END)]
        if y < BOX_TOP_NEW + (t1 - t0):
            src = top[y - BOX_TOP_NEW]
        for x in range(4, BOX_END):
            cp[x, y] = src[x - 4]
    r0, r1 = BOX_RIGHT
    for y in range(BOX_TOP_NEW, 234):
        border = [cp[x, y] for x in range(r0, r1)]
        fill = cp[r0 - 12, y]
        for x in range(r0, BOX_RIGHT_NEW):
            cp[x, y] = fill
        for i, c in enumerate(border):
            cp[BOX_RIGHT_NEW + i, y] = c
    # The save card, widened and made taller, in the picture's place.
    sx0, sy0, sx1, sy1 = CARD_SRC
    dx0, dy0, dx1, dy1 = CARD_DST
    for sy in range(sy0, sy1):
        rows = [sy] + ([sy] * CARD_INS_ROWS[1] if sy == CARD_INS_ROWS[0] else [])
        line = []
        for x in range(sx0, sx1):
            line.append(base[sy][x])
            line += [base[sy][x]] * sum(n for at, n in card_ins(sy) if at == x)
        for r, row_y in enumerate(rows):
            ny = card_y(sy) - (CARD_INS_ROWS[1] if sy == CARD_INS_ROWS[0] else 0) + r
            for i, c in enumerate(line):
                cp[dx0 + i, ny] = c
    for y in range(dy1, BOX_TOP_NEW):                         # between the card and the dialogue: the card's last row
        for x in range(dx0, dx1):
            cp[x, y] = cp[x, dy1 - 1]
    return clean


def ink_mask(art, box, colour_of_row):
    """The white lettering inside `box` as cover 0..15: how far each pixel is from the body towards white."""
    px = art.load()
    rows = []
    for y in range(box[1], box[3]):
        body = lum(colour_of_row(y))
        rows.append([max(0, min(15, round(15 * (lum(px[x, y]) - body) / (250 - body)))) if lum(px[x, y]) > body + 40 else 0
                     for x in range(box[0], box[2])])
    return rows


def strip_label(art, box, label, colour_of_row):
    out = crop(art, box)
    op = out.load()
    for y in range(label[1], label[3]):
        c = colour_of_row(y)
        for x in range(label[0], label[2]):
            op[x - box[0], y - box[1]] = c
    return out


def keyed(img, near, limit=40):
    """`img` with the pixels close to `near` (the dialogue's inside) made KEY."""
    out = img.copy()
    op = out.load()
    for v in range(out.size[1]):
        for u in range(out.size[0]):
            if dist(op[u, v], near) <= limit:
                op[u, v] = KEY
    return out


# The lettering: the move names' letters (make_move_skin.TITLE_FONT, 9 rows, two-pixel stems) a column narrower, and
# for the values one row taller. The characters it lacks, in its style.
EXTRA = {
    ':': ['  ', '  ', '##', '##', '  ', '  ', '##', '##', '  '],
    '/': ['    ##', '   +##', '   ##+', '  +##.', '  ##+ ', ' +##. ', ' ##+  ', '+##.  ', '##+   '],
    '?': ['.+###+.', '+#####+', '##+ +##', '    +##', '   +##+', '  ##+  ', '  ##   ', '       ', '  ##   '],
    '!': ['##', '##', '##', '##', '##', '##', '  ', '##', '##'],
    '¥': ['##   ##', '+#+ +#+', '.##+##.', '#######', '  ##   ', '#######', '  ##   ', '  ##   ', '  ##   '],
    'é': ['  +##  ', ' +#+   ', '.+###+.', '+#####+', '##   ##', '#######', '##     ', '+######', '.+####+'],
}


def lettering():
    font = dict(msk.TITLE_FONT)
    font.update(EXTRA)
    nine, ten = {}, {}
    for c, rows in font.items():
        width = max(len(r) for r in rows)
        rows = [(r + ' ' * width)[:width] for r in rows]
        narrow = msk.title_narrow(rows)
        nine[c] = narrow
        ten[c] = narrow[:4] + [narrow[4]] + narrow[4:]
    return nine, ten


def rgb565(c):
    v = msk.rgb565(c)
    return 0xF81E if v == 0xF81F and c != KEY else v


def emit(clean, pieces, positions, masks, card_label, nine, ten):
    out = ['/* Generated by tools/make_touch_skin.py from docs/media/poketouch.png (the PokeTouch screen\'s artwork). Do not',
           ' * edit: change the artwork and run the script again. */',
           '#ifndef POKETOUCH_SKIN_H', '#define POKETOUCH_SKIN_H', '', '#include <stdint.h>', '',
           '#define TS_KEY 0xF81F   /* a picture\'s see-through pixels */']
    rows_per = 240 // BG_PARTS
    out.append('/* The backdrop, RGB565, in quarters of %d rows. */' % rows_per)
    for part in range(BG_PARTS):
        out.append('#include "poketouch_skin_bg%d.h"' % part)
        body = msk.c_rows('kTsBg%d[%d][320]' % (part, rows_per), 'uint16_t', msk.crop(clean, 0, part * rows_per, 320, rows_per),
                          lambda c: '0x%04X' % rgb565(c), 20)
        (OUT.parent / ('poketouch_skin_bg%d.h' % part)).write_text(
            '/* Generated by tools/make_touch_skin.py: the PokeTouch backdrop, rows %d-%d. Do not edit. */\n%s'
            % (part * rows_per, part * rows_per + rows_per - 1, body))
    out.append('static const uint16_t (*const kTsBg[%d])[320] = {%s};' % (BG_PARTS, ', '.join('kTsBg%d' % i for i in range(BG_PARTS))))
    out.append('#define TS_BG(x, y) (kTsBg[(y) / %d][(y) %% %d][x])' % (rows_per, rows_per))
    out.append('/* The pictures (RGB565, TS_KEY see-through) and where the artwork has them. */')
    out.append('#include "poketouch_skin_parts0.h"')
    out.append('#include "poketouch_skin_parts1.h"')
    parts = []
    out.append('typedef struct { int16_t x, y, w, h; const uint16_t *px; } TsPic;')
    for name, im in pieces:
        w, h = im.size
        parts.append(msk.c_rows('%s[%d][%d]' % (name, h, w), 'uint16_t', msk.crop(im, 0, 0, w, h), lambda c: '0x%04X' % rgb565(c), 20))
        x, y = positions[name]
        out.append('static const TsPic %sPic = {%d, %d, %d, %d, &%s[0][0]};' % (name, x, y, w, h, name))
    half = len(parts) // 2
    for n, chunk in enumerate((parts[:half], parts[half:])):
        (OUT.parent / ('poketouch_skin_parts%d.h' % n)).write_text(
            '/* Generated by tools/make_touch_skin.py: the PokeTouch screen\'s pictures, part %d. Do not edit. */\n%s\n'
            % (n, '\n'.join(chunk)))
    out.append('/* The YES and NO lettering as painted: cover 0..15, drawn white with a dark outline on either button. */')
    out.append('typedef struct { int16_t x, y, w, h; const uint8_t *cover; } TsMask;')
    for name, rows, (x, y) in masks:
        out.append(msk.c_rows('%s[%d][%d]' % (name, len(rows), len(rows[0])), 'uint8_t', rows, str, 24))
        out.append('static const TsMask %sPic = {%d, %d, %d, %d, &%s[0][0]};' % (name, x, y, len(rows[0]), len(rows), name))
    out.append('static const TsPic *const kTsSide[6] = {%s};' % ', '.join('&kTsSide%dPic' % k for k in range(6)))
    out.append('static const TsPic *const kTsSideSel[6] = {%s};' % ', '.join('&kTsSideSel%dPic' % k for k in range(6)))
    out.append('static const TsPic *const kTsBadge[%d] = {%s};' % (len(BADGES), ', '.join('&kTsBadge%dPic' % i for i in range(len(BADGES)))))
    out.append('/* The Trainer Card\'s label (the player\'s name): its left column and first row. */')
    out.append('#define TS_CARD_LABEL_X %d\n#define TS_CARD_LABEL_Y %d' % card_label)
    out.append('/* The save card\'s values: their dotted leader (from x, top row), the value\'s top row, its right column, ink;')
    out.append(' * and the area each one is drawn in (x0, y0, x1, y1, exclusive). The name stops short of x = y + TS_DIAGONAL. */')
    out.append('static const struct { int16_t dotsX, dotsY, top, right; uint8_t ink[3]; int16_t area[4]; } kTsValue[%d] = {' % len(VALUES))
    for (dx, dy, top, right, ink), hole in zip(VALUES, HOLES):
        mapped = (card_x(dx, dy), card_y(dy), card_y(top), card_x(right, top))
        area = (card_x(hole[0], hole[1]), card_y(hole[1]), card_x(hole[2], hole[1]), card_y(hole[3]))
        out.append('    {%d, %d, %d, %d, {%d, %d, %d}, {%d, %d, %d, %d}},' % (mapped + ink + area))
    out.append('};')
    out.append('#define TS_DIAGONAL %d' % (DIAGONAL + card_x(0, 100) - card_y(100)))
    level = {' ': 0, '.': 4, '+': 9, '#': 15}
    for name, font, rows in (('Ten', ten, 10), ('Nine', nine, 9)):
        chars = ''.join(sorted(c for c in font if ord(c) < 128))
        latin = [c for c in sorted(font) if ord(c) >= 128]
        out.append('/* The lettering, %d rows, up to 7 wide, cover 0..15 per pixel: "%s", then %s. */'
                   % (rows, chars.replace('\\', '\\\\'), ', '.join('U+%04X' % ord(c) for c in latin)))
        allc = list(chars) + latin
        out.append('static const char kTs%sChars[] = "%s";' % (name, chars.replace('\\', '\\\\').replace('"', '\\"')))
        out.append('static const uint16_t kTs%sLatin[%d] = {%s};' % (name, len(latin), ', '.join('0x%04X' % ord(c) for c in latin)))
        out.append('static const uint8_t kTs%sWidth[%d] = {%s};' % (name, len(allc), ', '.join(str(len(font[c][0])) for c in allc)))
        out.append('static const uint8_t kTs%sRows[%d][%d][7] = {' % (name, len(allc), rows))
        for c in allc:
            rr = [(r + ' ' * 7)[:7] for r in font[c]]
            out.append('    {%s},' % ', '.join('{%s}' % ', '.join(str(level[ch]) for ch in r) for r in rr))
        out.append('};')
    out.append('')
    out.append('#endif')
    OUT.write_text('\n'.join(out) + '\n')


def main():
    art = Image.open(ART).convert('RGB')
    assert art.size == (320, 240)
    px = art.load()
    clean = build_backdrop(art)
    side = side_buttons(art, clean)
    tile_y_empty = crop(art, TILE_Y)
    tp = tile_y_empty.load()
    for bx0, by0, bx1, by1 in TILE_Y_ICON:
        for y in range(by0, by1):
            c = px[TILE_Y_ICON[0][0] - 1, max(y, TILE_Y_ICON[0][1])]
            for x in range(bx0, bx1):
                tp[x - TILE_Y[0], y - TILE_Y[1]] = c
    run_off = crop(art, RUN)
    rp = run_off.load()
    for y in range(RUN[1], RUN[3]):
        for x in range(RUN[0], RUN[2]):
            in_pill = RUN_PILL[0] <= x < RUN_PILL[2] and RUN_PILL[1] <= y < RUN_PILL[3]
            r, g, b = rp[x - RUN[0], y - RUN[1]]
            if (in_pill or RUN_EDGE[0] <= x < RUN_EDGE[1]) and g > r + 16:
                l = int(lum((r, g, b)) * 0.75)
                rp[x - RUN[0], y - RUN[1]] = (l, l, min(255, l + 8))
    # "OFF" in the grey capsule: its "ON" taken out, the tiny letters of the battle screen, centred.
    pill_body = mode(rp[x - RUN[0], y - RUN[1]] for x in range(*RUN_WORD[::2]) for y in range(*RUN_WORD[1::2]))
    for y in range(RUN_WORD[1], RUN_WORD[3]):
        for x in range(RUN_WORD[0], RUN_WORD[2]):
            rp[x - RUN[0], y - RUN[1]] = pill_body
    word = [msk.TINY_FONT[c] for c in '0FF']
    wx = (RUN_PILL[0] + RUN_PILL[2]) // 2 - (len(word) * 4 - 1) // 2 - RUN[0]
    wy = (RUN_PILL[1] + RUN_PILL[3]) // 2 - 3 - RUN[1]
    for i, glyph in enumerate(word):
        for v, row in enumerate(glyph):
            for u, ch in enumerate(row):
                if ch == '#':
                    rp[wx + i * 4 + u, wy + v] = (200, 204, 208)
    yes_row = lambda y: px[YES_LABEL[0] - 1, y]
    no_row = lambda y: px[NO_LABEL[0] - 1, y]
    yes = keyed(strip_label(art, YES, YES_LABEL, yes_row), INTERIOR)
    no = keyed(strip_label(art, NO, NO_LABEL, no_row), INTERIOR)
    masks = [('kTsYesMask', ink_mask(art, YES_LABEL, yes_row), YES_LABEL[:2]),
             ('kTsNoMask', ink_mask(art, NO_LABEL, no_row), NO_LABEL[:2])]
    portrait = crop(art, PORTRAIT)
    pieces, positions = [], {}

    def add(name, im, at):
        cuts = (SAVE_SEL_CUT if name == 'kTsSideSel4' else RUN_CUT if name.startswith('kTsRun') else TILE_X_CUT if name == 'kTsTileX' else TILE_Y_CUT
                if name.startswith('kTsTileY') else SIDE_CUT if name.startswith('kTsSide') else set())
        if cuts:
            keep = [u for u in range(im.size[0]) if at[0] + u not in cuts]
            out = Image.new('RGB', (len(keep), im.size[1]))
            for n, u in enumerate(keep):
                for v in range(im.size[1]):
                    out.putpixel((n, v), im.getpixel((u, v)))
            im = out
        pieces.append((name, im))
        x = at[0] + SHIFT - (len(TILE_X_CUT) + 1 if name.startswith('kTsTileY') else 0)
        if name.startswith('kTsBadge'):                        # the badges stay in a row: from the first one's place
            at = (card_x(BADGES[0][0], at[1]) + at[0] - BADGES[0][0], card_y(at[1]))
        elif name in ('kTsDots', 'kTsPortrait'):
            at = (card_x(at[0], at[1]), card_y(at[1]))
        positions[name] = (x, at[1]) if name[3:7] in ('Side', 'Tile', 'RunO') else at
    for k, (normal, sel) in enumerate(side):
        add('kTsSide%d' % k, normal, SIDE[k][:2])
        add('kTsSideSel%d' % k, sel, SIDE[k][:2])
    add('kTsTileX', crop(art, TILE_X), TILE_X[:2])
    add('kTsTileY', crop(art, TILE_Y), TILE_Y[:2])
    add('kTsTileYEmpty', tile_y_empty, TILE_Y[:2])
    add('kTsRunOn', crop(art, RUN), RUN[:2])
    add('kTsRunOff', run_off, RUN[:2])
    add('kTsYes', yes, YES[:2])
    add('kTsNo', no, NO[:2])
    add('kTsDots', crop(art, DOTS), DOTS[:2])
    add('kTsPortrait', portrait, PORTRAIT[:2])
    for i, b in enumerate(BADGES):
        add('kTsBadge%d' % i, crop(art, b), b[:2])
    lx, ly, _, _ = label_box(art, (SIDE_INK[0], SIDE[SIDE_CARD][1], 296, SIDE[SIDE_CARD][3]))
    lx = lx - SIDE[0][0] - sum(1 for c in SIDE_CUT if c < lx) + SIDE[0][0] + SHIFT              # where the label is on the screen
    nine, ten = lettering()
    return art, clean, pieces, positions, masks, (lx, ly), nine, ten


if __name__ == '__main__':
    import sys
    art, clean, pieces, positions, masks, card_label, nine, ten = main()
    if len(sys.argv) > 1:                                     # a look at the pieces, for checking
        out = Path(sys.argv[1])
        clean.save(out / 'clean.png')
        sheet = Image.new('RGB', (420, 420), (40, 90, 40))
        x = y = rowh = 0
        for name, im in pieces:
            if x + im.size[0] > 420:
                x, y, rowh = 0, y + rowh + 3, 0
            sheet.paste(im, (x, y))
            x += im.size[0] + 3
            rowh = max(rowh, im.size[1])
        sheet.save(out / 'pieces.png')
    else:
        side = None
        emit(clean, pieces, positions, masks, card_label, nine, ten)
        print('wrote', OUT.relative_to(ROOT), 'card label at', card_label)
