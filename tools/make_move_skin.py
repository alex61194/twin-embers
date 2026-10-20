#!/usr/bin/env python3
"""Builds the move-selection screen's skin from its artwork.

The artwork is two 320x240 pictures of the whole lower screen (docs/media/move-screen.png and
move-screen-pressed.png): the screen as it rests and with CANCEL (and a card) pressed. This script turns them into
the tables the battle UI draws with, 3ds_port/include/battle_move_skin.h and the backdrop's four quarters
battle_move_skin_bg0.h .. bg3.h:

* the backdrop: the artwork with what changes during a battle (the balls, the footer's text, the cards) taken out;
* the move card: the four cards of the artwork share one shape in four colours, so each pixel is solved as
  base + weight * card colour, and any type is drawn with the same finish;
* the type capsule, the same way from its four colours;
* the PP segment (lit and spent), the matchup marks, the balls, the HP bar, the status capsule, the STAB tag and
  the pressed CANCEL, as small pictures;
* the display lettering of the move names, drawn after the artwork's own.

Needs Pillow; it is a development tool and its output is committed. Run it again after changing the artwork.
"""
from pathlib import Path

from PIL import Image

ROOT = Path(__file__).resolve().parents[1]
ART = ROOT / 'docs/media'
OUT = ROOT / '3ds_port/include/battle_move_skin.h'
BG_PARTS = 4                                                 # the backdrop in quarters (each header under 256 KiB)

CARD_W, CARD_H = 139, 66
CARD_SRC = [(20, 57), (161, 58), (20, 124), (161, 124)]     # where each card's cell is in the artwork
CARD_POS = [(20, 57), (161, 57), (20, 124), (161, 124)]     # where the game draws them (one grid)
ZONE = (14, 53, 306, 193)                                    # the cards' area of the backdrop
# What changes on a card, relative to its cell (u0, v0, u1, v1, exclusive): the name, the capsule, the matchup, the
# PP bar and count; the type icon sits in the tab.
CARD_MASKS = [(6, 16, 100, 30), (5, 29, 60, 49), (80, 29, 135, 47), (6, 47, 76, 60), (81, 47, 134, 60)]
ICON_MASK = (118, 4, 135, 23)
PILL = (5, 29, 55, 20)                                       # u, v, w, h of the capsule's picture
PILL_TEXT = (15, 32, 54, 46)
PILL_MID = 31                                                # the capsule's middle column
# The PP bar: ten slanted cells; on row v (from SEG_V) cell i starts at u = SEG_U0 - 1 + floor(i * 19 / 3) - v / 2.
SEG_U0, SEG_PITCH3, SEG_V, SEG_H, SEG_CELL = 14, 19, 49, 10, 7
STRIP = (21, 12, 301, 51)
STRIP_FREE = [x for a, b in ((21, 26), (64, 73), (154, 166), (200, 212), (246, 258), (292, 300)) for x in range(a, b)]
SLOT_X = [28, 74, 120, 166, 212, 258]                        # each slot's HP bar starts here
FOOT = (18, 199, 216, 228)
FOOT_FREE = list(range(18, 21)) + list(range(212, 216))
STAB = (157, 197, 217, 213)
CANCEL = (217, 190, 310, 237)
BALL = (6, 17, 23, 22)                                       # a ball's picture, relative to its slot: u, v, w, h
BALL_LEAD = (-2, 12, 39, 30)                                 # the lead's, with its golden bloom
TRACK = (-1, 41, 36, 7)                                      # the HP bar with its frame
PAR = (99, 12, 21, 13)
ZONE_FILL = (24, 4, 0)
# The type sheet (docs/media/move-cards.png): eighteen card frames at their size (133x60), three to a row, in this
# order of types.
SHEET_TYPES = [10, 4, 11, 12, 13, 15, 2, 5, 1, 3, 6, 8, 14, 7, 17, 16, 0, 9]
SHEET_W, SHEET_H = 133, 60
SHEET_NEUTRAL = 16                                           # the Body Slam card shows the neutral mark
ART_TYPES = [10, 4, 11, 12]                                  # the types of the screen artwork's four cards
BODY_SAMPLE = (60, 36, 76, 45)                               # a stretch of every card's own background (cell)

# The move names' lettering: 9 rows, two-pixel stems; '#' ink, '+' half, '.' a quarter.
TITLE_FONT = {
    'A': ['.+###+.', '+#####+', '##+.+##', '##   ##', '#######', '#######', '##   ##', '##   ##', '##   ##'],
    'B': ['######+', '#######', '##   ##', '##++##+', '######.', '##   ##', '##   ##', '#######', '######+'],
    'C': ['.+####+', '+######', '##+   .', '##     ', '##     ', '##     ', '##+   .', '+######', '.+####+'],
    'D': ['#####+.', '######+', '##  +##', '##   ##', '##   ##', '##   ##', '##  +##', '######+', '#####+.'],
    'E': ['#######', '#######', '##     ', '##     ', '######', '######', '##     ', '#######', '#######'],
    'F': ['#######', '#######', '##     ', '##     ', '######', '######', '##     ', '##     ', '##     '],
    'G': ['.+####+', '+######', '##+    ', '##     ', '## ####', '##   ##', '##+  ##', '+######', '.+####+'],
    'H': ['##   ##', '##   ##', '##   ##', '#######', '#######', '##   ##', '##   ##', '##   ##', '##   ##'],
    'I': ['####', '####', '.##.', '.##.', '.##.', '.##.', '.##.', '####', '####'],
    'J': ['  #####', '  #####', '     ##', '     ##', '     ##', '##   ##', '##+ +##', '+#####+', '.+###+.'],
    'K': ['##  +##', '## +##+', '##+##+ ', '####+  ', '###+   ', '####+  ', '##+##+ ', '## +##+', '##  +##'],
    'L': ['##     ', '##     ', '##     ', '##     ', '##     ', '##     ', '##     ', '#######', '#######'],
    'M': ['##   ##', '###+###', '#######', '##+#+##', '## + ##', '##   ##', '##   ##', '##   ##', '##   ##'],
    'N': ['##   ##', '###  ##', '####+##', '##+####', '## +###', '##  ###', '##   ##', '##   ##', '##   ##'],
    'O': ['.+###+.', '+#####+', '##+ +##', '##   ##', '##   ##', '##   ##', '##+ +##', '+#####+', '.+###+.'],
    'P': ['######+', '#######', '##   ##', '##   ##', '#######', '######+', '##     ', '##     ', '##     '],
    'Q': ['.+###+.', '+#####+', '##+ +##', '##   ##', '##   ##', '## + ##', '##+####', '+#####+', '.+###+#'],
    'R': ['######+', '#######', '##   ##', '##   ##', '######+', '######.', '##  ## ', '##  +##', '##   ##'],
    'S': ['.+####+', '+######', '##+    ', '+####+.', '.+####+', '     ##', '    +##', '######+', '+####+.'],
    'T': ['#######', '#######', '  ##   ', '  ##   ', '  ##   ', '  ##   ', '  ##   ', '  ##   ', '  ##   '],
    'U': ['##   ##', '##   ##', '##   ##', '##   ##', '##   ##', '##   ##', '##+ +##', '+#####+', '.+###+.'],
    'V': ['##   ##', '##   ##', '##   ##', '##   ##', '+#+ +#+', '.## ##.', ' +#+#+ ', ' .###. ', '  +#+  '],
    'W': ['##   ##', '##   ##', '##   ##', '## + ##', '##+#+##', '#######', '###+###', '##+ +##', '#+   +#'],
    'X': ['##   ##', '+#+ +#+', '.##+##.', ' +###+ ', '  ###  ', ' +###+ ', '.##+##.', '+#+ +#+', '##   ##'],
    'Y': ['##   ##', '+#+ +#+', '.##+##.', ' +###+ ', ' .###. ', '  ##   ', '  ##   ', '  ##   ', '  ##   '],
    'Z': ['#######', '#######', '    +##', '   +##+', '  +##+ ', ' +##+  ', '+##+   ', '#######', '#######'],
    '0': ['.+###+.', '+#####+', '##+ +##', '##  ###', '## + ##', '###  ##', '##+ +##', '+#####+', '.+###+.'],
    '1': [' +##', '+###', '####', ' .##', '  ##', '  ##', '  ##', '  ##', '  ##'],
    '2': ['.+###+.', '+#####+', '##+ +##', '    +##', '  .+##+', ' +##+. ', '+##+   ', '#######', '#######'],
    '3': ['######+', '#######', '    +##', '  ####+', '  #####', '    +##', '    +##', '#######', '######+'],
    '4': ['   +## ', '  +### ', ' +#### ', '+##+## ', '##+ ## ', '#######', '#######', '    ## ', '    ## '],
    '5': ['#######', '#######', '##     ', '######+', '#######', '    +##', '##  +##', '#######', '+####+.'],
    '6': ['.+####+', '+######', '##+    ', '######+', '#######', '##   ##', '##+ +##', '+#####+', '.+###+.'],
    '7': ['#######', '#######', '    +##', '   +##+', '   ##+ ', '  +##  ', '  ##+  ', '  ##   ', '  ##   '],
    '8': ['.+###+.', '+#####+', '##   ##', '+#####+', '+#####+', '##   ##', '##+ +##', '+#####+', '.+###+.'],
    '9': ['.+###+.', '+#####+', '##+ +##', '##   ##', '+######', '.+#####', '    +##', '######+', '+####+.'],
    '-': ['     ', '     ', '     ', '     ', '#####', '#####', '     ', '     ', '     '],
    "'": ['##', '##', '#+', '  ', '  ', '  ', '  ', '  ', '  '],
    '.': ['  ', '  ', '  ', '  ', '  ', '  ', '  ', '##', '##'],
    ' ': ['    '] * 9,
}

# The tiny lettering of the status capsule and the HP percentage: 3x5.
TINY_FONT = {
    '0': ['.#.', '#.#', '#.#', '#.#', '.#.'], '1': ['.#.', '##.', '.#.', '.#.', '###'],
    '2': ['##.', '..#', '.#.', '#..', '###'], '3': ['##.', '..#', '.#.', '..#', '##.'],
    '4': ['#.#', '#.#', '###', '..#', '..#'], '5': ['###', '#..', '##.', '..#', '##.'],
    '6': ['.##', '#..', '###', '#.#', '###'], '7': ['###', '..#', '.#.', '.#.', '.#.'],
    '8': ['###', '#.#', '###', '#.#', '###'], '9': ['###', '#.#', '###', '..#', '##.'],
    '%': ['#.#', '..#', '.#.', '#..', '#.#'], 'P': ['##.', '#.#', '##.', '#..', '#..'],
    'A': ['.#.', '#.#', '###', '#.#', '#.#'], 'R': ['##.', '#.#', '##.', '#.#', '#.#'],
    'S': ['.##', '#..', '.#.', '..#', '##.'], 'N': ['#.#', '###', '###', '#.#', '#.#'],
    'L': ['#..', '#..', '#..', '#..', '###'], 'F': ['###', '#..', '##.', '#..', '#..'],
    'Z': ['###', '..#', '.#.', '#..', '###'], 'B': ['##.', '#.#', '##.', '#.#', '##.'],
    'K': ['#.#', '#.#', '##.', '#.#', '#.#'],
}


def lum(c):
    return 0.3 * c[0] + 0.59 * c[1] + 0.11 * c[2]


def median(cols):
    cols = list(cols)
    return tuple(sorted(c[ch] for c in cols)[len(cols) // 2] for ch in range(3))


def clamp(v, lo=0, hi=255):
    return lo if v < lo else hi if v > hi else v


def rgb565(c):
    r, g, b = (int(round(clamp(v))) for v in c)
    return (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3)


def masked(u, v, boxes):
    return any(u0 <= u < u1 and v0 <= v < v1 for u0, v0, u1, v1 in boxes)


def fill_rows(field, w, h, boxes, hold_left=()):
    """Masked pixels take the value interpolated between the nearest kept ones on their row ('hold_left': copied
    from the nearest kept one on the left)."""
    for v in range(h):
        u = 0
        while u < w:
            if not masked(u, v, boxes) and not masked(u, v, hold_left):
                u += 1
                continue
            s = u
            while u < w and (masked(u, v, boxes) or masked(u, v, hold_left)):
                u += 1
            left = field[v][s - 1] if s > 0 else field[v][u]
            right = field[v][u] if u < w else left
            for k in range(s, u):
                t = (k - s + 1) / (u - s + 1)
                if masked(k, v, hold_left):
                    field[v][k] = left
                else:
                    field[v][k] = tuple(left[i] + (right[i] - left[i]) * t for i in range(len(left)))


def solve(samples, regs):
    """Least squares over the samples' channels: c = a + sum_j w_j * reg_j, a per channel, w_j shared."""
    n = len(samples)
    mc = [sum(s[ch] for s in samples) / n for ch in range(3)]
    mr = [[sum(r[k][ch] for k in range(n)) / n for ch in range(3)] for r in regs]
    m = len(regs)
    ata = [[0.0] * m for _ in range(m)]
    atb = [0.0] * m
    for k in range(n):
        for ch in range(3):
            x = [regs[j][k][ch] - mr[j][ch] for j in range(m)]
            y = samples[k][ch] - mc[ch]
            for i in range(m):
                atb[i] += x[i] * y
                for j in range(m):
                    ata[i][j] += x[i] * x[j]
    if m == 1:
        w = [atb[0] / ata[0][0]]
    else:
        det = ata[0][0] * ata[1][1] - ata[0][1] * ata[1][0]
        w = [(atb[0] * ata[1][1] - atb[1] * ata[0][1]) / det, (ata[0][0] * atb[1] - ata[1][0] * atb[0]) / det]
    a = [mc[ch] - sum(w[j] * mr[j][ch] for j in range(m)) for ch in range(3)]
    return tuple(a) + tuple(w)


def card_colours(img):
    px = img.load()
    neon = [median(px[x + u, y + v] for v in range(15, 50) for u in (3, 4)) for x, y in CARD_SRC]
    pill = []
    for x, y in CARD_SRC:
        cols = [px[x + u, y + v] for v in range(33, 45) for u in range(9, 51)]
        cols = [c for c in cols if max(c) - min(c) > 60 and max(c) > 90]
        pill.append(median(cols))
    return neon, pill


def body_rows(field, w, h, boxes, lo=6, hi=118):
    """The card's own colour on each row: the median of its kept pixels between `lo` and `hi` (the tab, top right,
    left out); rows with none take it from the rows round them."""
    rows = []
    for v in range(h):
        vals = [field[v][u] for u in range(lo, hi) if not masked(u, v, boxes) and not (v < 27 and u >= 96)]
        rows.append(tuple(sorted(x[i] for x in vals)[len(vals) // 2] for i in range(len(vals[0]))) if len(vals) >= 5 else None)
    for v in range(h):
        if rows[v] is None:
            a = next((k for k in range(v, -1, -1) if rows[k] is not None), None)
            b = next((k for k in range(v, h) if rows[k] is not None), None)
            if a is None or b is None:
                rows[v] = rows[a if a is not None else b]
            else:
                t = (v - a) / (b - a) if b != a else 0
                rows[v] = tuple(rows[a][i] + (rows[b][i] - rows[a][i]) * t for i in range(len(rows[a])))
    return rows


def build_card(img, neon):
    px = img.load()
    field = [[solve([px[x + u, y + v] for x, y in CARD_SRC], [neon]) for u in range(CARD_W)] for v in range(CARD_H)]
    body = body_rows(field, CARD_W, CARD_H, CARD_MASKS + [ICON_MASK])
    for v in range(CARD_H):
        for u in range(CARD_W):
            if masked(u, v, CARD_MASKS):
                field[v][u] = body[v]
    fill_rows(field, CARD_W, CARD_H, [], hold_left=[ICON_MASK])
    return field


def build_pill(img, neon, pill, card):
    px = img.load()
    u0, v0, w, h = PILL
    field = [[solve([px[x + u0 + u, y + v0 + v] for x, y in CARD_SRC], [neon, pill]) for u in range(w)]
             for v in range(h)]
    t = (PILL_TEXT[0] - u0, PILL_TEXT[1] - v0, PILL_TEXT[2] - u0, PILL_TEXT[3] - v0)
    fill_rows(field, w, h, [], hold_left=[t])
    mid = PILL_MID - u0                                       # a capsule: its right end mirrors its left
    for v in range(h):
        for u in range(mid + 1, w):
            if 2 * mid - u >= 2:
                field[v][u] = field[v][2 * mid - u]
            else:                                             # past its end, the card
                c = card[v0 + v][u0 + u]
                field[v][u] = (c[0], c[1], c[2], c[3], 0.0)
    return field


def card_pixel(field, u, v, neon):
    a = field[v][u]
    return tuple(a[ch] + a[3] * neon[ch] for ch in range(3))


def seg_start(i, v):
    return SEG_U0 - 1 + (i * SEG_PITCH3) // 3 - v // 2


def build_segments(img, neon, ink):
    """A lit cell (the second, lit on all four cards: base, card colour and PP colour weights) and a spent one (the
    ninth, spent on the Fire, Water and Grass cards: base and card colour weight)."""
    px = img.load()
    lit, spent = [], []
    for v in range(SEG_H):
        lrow, srow = [], []
        for k in range(SEG_CELL):
            u = seg_start(1, v) + k
            lrow.append(solve([px[x + u, y + SEG_V + v] for x, y in CARD_SRC], [neon, ink]))
            u = seg_start(8, v) + k
            cards = [0, 2, 3]
            srow.append(solve([px[CARD_SRC[c][0] + u, CARD_SRC[c][1] + SEG_V + v] for c in cards], [[neon[c] for c in cards]]))
        lit.append(lrow)
        spent.append(srow)
    return lit, spent


def cover(img, k, card, neon, box, ink=None):
    """A picture on card k within `box` (u0, v0, u1, v1) as cover over the card: per pixel its colour and how far it
    is from the card's own toward `ink` (the picture's strongest colour unless given)."""
    px = img.load()
    x, y = CARD_SRC[k]
    u0, v0, u1, v1 = box
    if ink is None:
        ink = max((px[x + u, y + v] for u in range(u0, u1) for v in range(v0, v1)),
                  key=lambda c: (max(c) - min(c)) + lum(c) / 2)
    rows = []
    for v in range(v0, v1):
        row = []
        for u in range(u0, u1):
            c = px[x + u, y + v]
            body = card_pixel(card, u, v, neon[k])
            d = [ink[i] - body[i] for i in range(3)]
            dd = sum(e * e for e in d) or 1
            al = clamp(sum((c[i] - body[i]) * d[i] for i in range(3)) / dd, 0, 1)
            row.append((al, c))
        rows.append(row)
    return ink, rows


def build_marks(img, card, neon):
    """The matchup marks (x2 star, x1/2 diamond, x0 cross, x4 star): the first shape right of the capsule."""
    marks = []
    for k in range(4):
        ink, rows = cover(img, k, card, neon, (80, 29, 135, 47))
        cols = [max(rows[v][u][0] for v in range(len(rows))) for u in range(55)]
        u = 0
        while cols[u] < 0.3:
            u += 1
        s0 = u
        while u < 55 and cols[u] >= 0.25:
            u += 1
        s1 = u
        gap = 0
        while u + gap < 55 and cols[u + gap] < 0.25:
            gap += 1
        vs = [v for v in range(len(rows)) if max(rows[v][x][0] for x in range(s0, s1)) >= 0.25]
        top, bot = vs[0], vs[-1] + 1
        alpha = [[rows[v][x][0] for x in range(max(0, s0 - 1), min(55, s1 + 1))] for v in range(top, bot)]
        marks.append((ink, alpha, gap, 29 + top))
    return marks


def build_icons(img, card, neon):
    """The four type icons in the tabs (Fire, Ground, Water, Grass): per pixel a cover from its distance to the
    tab's own colour, and the colour that, laid over the tab by that cover, gives the artwork's pixel back."""
    px = img.load()
    icons = []
    for k, (x, y) in enumerate(CARD_SRC):
        u0, v0, u1, v1 = ICON_MASK
        rows = []
        for v in range(v0, v1):
            row = []
            for u in range(u0, u1):
                c = px[x + u, y + v]
                body = card_pixel(card, u, v, neon[k])
                dist = sum((c[i] - body[i]) ** 2 for i in range(3)) ** 0.5
                al = clamp(dist / 70.0, 0, 1) if dist > 12 else 0.0
                col = tuple(clamp(body[i] + (c[i] - body[i]) / al) for i in range(3)) if al else c
                row.append((al, col))
            rows.append(row)
        icons.append(rows)
    return icons


def sheet_cards():
    """The type sheet's cards, each placed as a card cell (its frame at 3, 3)."""
    im = Image.open(ART / 'move-cards.png').convert('RGB')
    assert im.size == (SHEET_W * 3, SHEET_H * 6)
    cards = []
    for i in range(len(SHEET_TYPES)):
        x, y = SHEET_W * (i % 3), SHEET_H * (i // 3)
        cell = Image.new('RGB', (CARD_W, CARD_H))
        cell.paste(im.crop((x, y, x + SHEET_W, y + SHEET_H)), (3, 3))
        cards.append(cell)
    return cards


def sheet_colours(card):
    """A sheet card's frame, capsule and lit PP colours."""
    px = card.load()
    neon = median(px[u, v] for v in range(15, 50) for u in (3, 4))
    pill = [px[u, v] for v in range(34, 44) for u in range(9, 52)]
    pill = [c for c in pill if min(c) < 190 and lum(c) > 40]
    lit = [px[u, v] for v in range(51, 57) for u in range(10, 24)]
    lit = sorted(lit, key=lum)[len(lit) // 2:]
    return neon, median(pill), median(lit)


def sheet_cover(card, box):
    """A picture on a sheet card within `box` (cell coordinates): per pixel a cover over the card's own background
    there (the darker half's middle) and the colour that gives the pixel back over it."""
    px = card.load()
    u0, v0, u1, v1 = box
    vals = sorted((px[u, v] for u in range(u0, u1) for v in range(v0, v1)), key=lum)
    bg = vals[len(vals) // 4]
    rows = []
    for v in range(v0, v1):
        row = []
        for u in range(u0, u1):
            c = px[u, v]
            dist = sum((c[i] - bg[i]) ** 2 for i in range(3)) ** 0.5
            al = clamp(dist / 70.0, 0, 1) if dist > 18 else 0.0
            row.append((al, tuple(clamp(bg[i] + (c[i] - bg[i]) / al) for i in range(3)) if al else c))
        rows.append(row)
    return rows


def sheet_neutral(card):
    """The neutral mark: the sheet's Body Slam card draws a six-pixel dash (white, its lower row at half) at the middle
    height of "x1" (cell row 39), four pixels before it."""
    px = card.load()
    ink = max((px[u, 39] for u in range(100, 112)), key=lum)
    return ink, [[1.0] * 6, [0.45] * 6], 4, 39


def inpaint_rows(px, box, sample_x):
    """Each row of `box` takes the colour its free columns (`sample_x`) have there."""
    x0, y0, x1, y1 = box
    for y in range(y0, y1):
        c = median(px[x, y] for x in sample_x)
        for x in range(x0, x1):
            px[x, y] = c


def crop(img, x, y, w, h):
    px = img.load()
    return [[px[x + u, y + v] for u in range(w)] for v in range(h)]


def build_backdrop(img):
    bg = img.copy()
    px = bg.load()
    inpaint_rows(px, STRIP, STRIP_FREE)
    inpaint_rows(px, FOOT, FOOT_FREE)
    x0, y0, x1, y1 = ZONE
    for y in range(y0, y1):
        for x in range(x0, x1):
            px[x, y] = ZONE_FILL
    return bg


def c_rows(name, ctype, rows, fmt, per_line=16):
    out = []
    for row in rows:
        vals = [fmt(v) for v in row]
        out.append('{' + ',\n     '.join(', '.join(vals[i:i + per_line]) for i in range(0, len(vals), per_line)) + '}')
    return 'static const %s %s = {\n    %s\n};\n' % (ctype, name, ',\n    '.join(out))


def main():
    base = Image.open(ART / 'move-screen.png').convert('RGB')
    pressed = Image.open(ART / 'move-screen-pressed.png').convert('RGB')
    assert base.size == (320, 240) and pressed.size == (320, 240)
    neon, pill = card_colours(base)
    card = build_card(base, neon)
    pillf = build_pill(base, neon, pill, card)
    bpx = base.load()
    seg_ink = [median(bpx[x + seg_start(0, v) + k, y + SEG_V + v] for v in range(2, 7) for k in range(2, 5))
               for x, y in CARD_SRC]
    lit, spent = build_segments(base, neon, seg_ink)
    marks = build_marks(base, card, neon)
    icons = build_icons(base, card, neon)
    bg = build_backdrop(base)
    sheet = sheet_cards()
    type_neon, type_pill, type_seg, type_icon = [None] * 18, [None] * 18, [None] * 18, [None] * 18
    for k, t in enumerate(SHEET_TYPES):
        type_neon[t], type_pill[t], type_seg[t] = sheet_colours(sheet[k])
        type_icon[t] = sheet_cover(sheet[k], ICON_MASK)
    for k, t in enumerate(ART_TYPES):                         # the screen artwork's own four, sharper
        type_neon[t], type_pill[t] = neon[k], pill[k]
        type_icon[t] = icons[k]
    type_body = [(0, 0, 0)] * 18                             # how much lighter the sheet paints each card's background
    for k, t in enumerate(SHEET_TYPES):
        if t in ART_TYPES:
            continue
        sp = sheet[k].load()
        u0, v0, u1, v1 = BODY_SAMPLE
        want = median(sp[u, v] for u in range(u0, u1) for v in range(v0, v1))
        have = median(card_pixel(card, u, v, type_neon[t]) for u in range(u0, u1) for v in range(v0, v1))
        type_body[t] = tuple(round(want[i] - have[i]) for i in range(3))
    type_seg[11] = (72, 216, 248)                            # the sheet's Water card is at a fifth of its PP (orange)
    neutral = sheet_neutral(sheet[SHEET_NEUTRAL])
    hp_green = [median(bpx[x, y] for x in range(32, 57)) for y in range(42, 47)]
    hp_yellow = [median(bpx[x, y] for x in range(76, 88)) for y in range(42, 47)]

    out = ['/* Generated by tools/make_move_skin.py from docs/media/move-screen.png and move-screen-pressed.png (the',
           ' * move-selection screen\'s artwork). Do not edit: change the artwork and run the script again. */',
           '#ifndef BATTLE_MOVE_SKIN_H', '#define BATTLE_MOVE_SKIN_H', '', '#include <stdint.h>', '']
    out.append('#define MSK_CARD_W %d\n#define MSK_CARD_H %d' % (CARD_W, CARD_H))
    out.append('#define MSK_PILL_U %d\n#define MSK_PILL_V %d\n#define MSK_PILL_W %d\n#define MSK_PILL_H %d' % PILL)
    out.append('#define MSK_SEG_U0 %d\n#define MSK_SEG_PITCH3 %d\n#define MSK_SEG_V %d\n#define MSK_SEG_H %d\n#define MSK_SEG_CELL %d'
               % (SEG_U0, SEG_PITCH3, SEG_V, SEG_H, SEG_CELL))
    out.append('#define MSK_BALL_U %d\n#define MSK_BALL_V %d\n#define MSK_BALL_W %d\n#define MSK_BALL_H %d' % BALL)
    out.append('#define MSK_LEAD_U %d\n#define MSK_LEAD_V %d\n#define MSK_LEAD_W %d\n#define MSK_LEAD_H %d' % BALL_LEAD)
    out.append('#define MSK_TRACK_U %d\n#define MSK_TRACK_V %d\n#define MSK_TRACK_W %d\n#define MSK_TRACK_H %d' % TRACK)
    out.append('#define MSK_PAR_X %d\n#define MSK_PAR_Y %d\n#define MSK_PAR_W %d\n#define MSK_PAR_H %d' % PAR)
    out.append('#define MSK_STAB_X %d\n#define MSK_STAB_Y %d\n#define MSK_STAB_W %d\n#define MSK_STAB_H %d'
               % (STAB[0], STAB[1], STAB[2] - STAB[0], STAB[3] - STAB[1]))
    out.append('#define MSK_CANCEL_X %d\n#define MSK_CANCEL_Y %d\n#define MSK_CANCEL_W %d\n#define MSK_CANCEL_H %d'
               % (CANCEL[0], CANCEL[1], CANCEL[2] - CANCEL[0], CANCEL[3] - CANCEL[1]))
    out.append('#define MSK_STRIP_X0 %d\n#define MSK_STRIP_Y0 %d\n#define MSK_STRIP_X1 %d\n#define MSK_STRIP_Y1 %d' % STRIP)
    out.append('#define MSK_FOOT_X0 %d\n#define MSK_FOOT_Y0 %d\n#define MSK_FOOT_X1 %d\n#define MSK_FOOT_Y1 %d' % FOOT)
    out.append('static const int16_t kMskCardPos[4][2] = {%s};' % ', '.join('{%d, %d}' % p for p in CARD_POS))
    out.append('static const int16_t kMskSlotX[6] = {%s};' % ', '.join(map(str, SLOT_X)))
    out.append('/* The artwork\'s four cards: Fire, Ground, Water, Grass (frame, capsule and PP colours). */')
    out.append('static const uint8_t kMskArtNeon[4][3] = {%s};' % ', '.join('{%d, %d, %d}' % c for c in neon))
    out.append('static const uint8_t kMskArtPill[4][3] = {%s};' % ', '.join('{%d, %d, %d}' % c for c in pill))
    out.append('static const uint8_t kMskArtSeg[4][3] = {%s};' % ', '.join('{%d, %d, %d}' % c for c in seg_ink))
    out.append('/* Per type (FireRed\'s order): the frame, capsule and lit PP colours, from the type sheet. */')
    for name, table in (('kMskTypeNeon', type_neon), ('kMskTypePill', type_pill), ('kMskTypeSeg', type_seg)):
        out.append('static const uint8_t %s[18][3] = {%s};' % (name, ', '.join('{%d, %d, %d}' % c for c in table)))
    out.append('/* Per type: what the sheet adds to the card\'s own background, inside the frame. */')
    out.append('static const int8_t kMskTypeBody[18][3] = {%s};' % ', '.join('{%d, %d, %d}' % c for c in type_body))
    out.append('#define MSK_BODY_U0 5\n#define MSK_BODY_V0 5\n#define MSK_BODY_U1 134\n#define MSK_BODY_V1 61')
    out.append('static const uint8_t kMskHpGreen[5][3] = {%s};' % ', '.join('{%d, %d, %d}' % c for c in hp_green))
    out.append('static const uint8_t kMskHpYellow[5][3] = {%s};' % ', '.join('{%d, %d, %d}' % c for c in hp_yellow))
    out.append('')
    rows_per = 240 // BG_PARTS
    out.append('/* The backdrop, RGB565, in quarters of %d rows. */' % rows_per)
    for part in range(BG_PARTS):
        out.append('#include "battle_move_skin_bg%d.h"' % part)
        body = c_rows('kMskBg%d[%d][320]' % (part, rows_per), 'uint16_t', crop(bg, 0, part * rows_per, 320, rows_per),
                      lambda c: '0x%04X' % rgb565(c), 20)
        (OUT.parent / ('battle_move_skin_bg%d.h' % part)).write_text(
            '/* Generated by tools/make_move_skin.py: the move screen\'s backdrop, rows %d-%d. Do not edit. */\n%s'
            % (part * rows_per, part * rows_per + rows_per - 1, body))
    out.append('static const uint16_t (*const kMskBg[%d])[320] = {%s};' % (BG_PARTS, ', '.join('kMskBg%d' % i for i in range(BG_PARTS))))
    out.append('#define MSK_BG(x, y) (kMskBg[(y) / %d][(y) %% %d][x])' % (rows_per, rows_per))
    out.append('/* The card: per pixel the base colour and the weight of the card colour (x256). */')
    out.append('#include "battle_move_skin_card.h"')
    (OUT.parent / 'battle_move_skin_card.h').write_text(
        '/* Generated by tools/make_move_skin.py: the move card, base colour and weight of the card colour. Do not edit. */\n'
        + c_rows('kMskCard[MSK_CARD_H][MSK_CARD_W][4]', 'int16_t', card,
                 lambda a: '{%d,%d,%d,%d}' % (round(a[0]), round(a[1]), round(a[2]), round(a[3] * 256)), 8))
    out.append('/* The type capsule: base, weight of the card colour and of the capsule colour (x256). */')
    out.append(c_rows('kMskPill[MSK_PILL_H][MSK_PILL_W][5]', 'int16_t', pillf,
                      lambda a: '{%d, %d, %d, %d, %d}' % (round(a[0]), round(a[1]), round(a[2]), round(a[3] * 256), round(a[4] * 256)), 5))
    out.append('/* A PP cell, lit: base, weight of the card colour and of the PP colour (x256); spent: base and card colour. */')
    out.append(c_rows('kMskSegLit[MSK_SEG_H][MSK_SEG_CELL][5]', 'int16_t', lit,
                      lambda a: '{%d, %d, %d, %d, %d}' % (round(a[0]), round(a[1]), round(a[2]), round(a[3] * 256), round(a[4] * 256)), 5))
    out.append(c_rows('kMskSegSpent[MSK_SEG_H][MSK_SEG_CELL][4]', 'int16_t', spent,
                      lambda a: '{%d, %d, %d, %d}' % (round(a[0]), round(a[1]), round(a[2]), round(a[3] * 256)), 6))
    out.append('/* The matchup marks: x2 star, x1/2 diamond, x0 cross, x4 star; cover 0..255 and colour. */')
    for i, (ink, alpha, gap, top) in enumerate(marks):
        h, w = len(alpha), len(alpha[0])
        out.append('#define MSK_MARK%d_W %d\n#define MSK_MARK%d_H %d\n#define MSK_MARK%d_GAP %d\n#define MSK_MARK%d_V %d'
                   % (i, w, i, h, i, gap, i, top))
        out.append('static const uint8_t kMskMark%dInk[3] = {%d, %d, %d};' % ((i,) + ink))
        out.append(c_rows('kMskMark%d[%d][%d]' % (i, h, w), 'uint8_t', alpha, lambda a: '%d' % round(a * 255), 20))
    out.append('/* The type icons in the tab, by type: RGB565 and cover 0..255. */')
    out.append('#define MSK_ICON_U %d\n#define MSK_ICON_V %d\n#define MSK_ICON_W %d\n#define MSK_ICON_H %d'
               % (ICON_MASK[0], ICON_MASK[1], ICON_MASK[2] - ICON_MASK[0], ICON_MASK[3] - ICON_MASK[1]))
    out.append('static const uint16_t kMskIcon[18][MSK_ICON_H][MSK_ICON_W][2] = {')
    for rows in type_icon:
        out.append('    {%s},' % ', '.join('{%s}' % ', '.join('{0x%04X, %d}' % (rgb565(c), round(al * 255)) for al, c in r) for r in rows))
    out.append('};')
    ink, alpha, gap, top = neutral
    out.append('/* The neutral mark (a dash before "x1"). */')
    out.append('#define MSK_MARK4_W %d\n#define MSK_MARK4_H %d\n#define MSK_MARK4_GAP %d\n#define MSK_MARK4_V %d'
               % (len(alpha[0]), len(alpha), gap, top))
    out.append('static const uint8_t kMskMark4Ink[3] = {%d, %d, %d};' % ink)
    out.append(c_rows('kMskMark4[%d][%d]' % (len(alpha), len(alpha[0])), 'uint8_t', alpha, lambda a: '%d' % round(a * 255), 20))
    shots = [('kMskBall', crop(base, SLOT_X[2] + BALL[0], BALL[1], BALL[2], BALL[3])),
             ('kMskBallFaint', crop(base, SLOT_X[3] + BALL[0], BALL[1], BALL[2], BALL[3])),
             ('kMskBallLead', crop(base, SLOT_X[0] + BALL_LEAD[0], BALL_LEAD[1], BALL_LEAD[2], BALL_LEAD[3])),
             ('kMskTrack', crop(base, SLOT_X[3] + TRACK[0], TRACK[1], TRACK[2], TRACK[3])),
             ('kMskPar', crop(base, *PAR)),
             ('kMskStab', crop(base, STAB[0], STAB[1], STAB[2] - STAB[0], STAB[3] - STAB[1])),
             ('kMskCancelDown', crop(pressed, CANCEL[0], CANCEL[1], CANCEL[2] - CANCEL[0], CANCEL[3] - CANCEL[1]))]
    for name, rows in shots:
        out.append(c_rows('%s[%d][%d]' % (name, len(rows), len(rows[0])), 'uint16_t', rows, lambda c: '0x%04X' % rgb565(c)))
    chars = ''.join(sorted(TITLE_FONT))
    level = {' ': 0, '.': 4, '+': 9, '#': 15}
    out.append('/* The move names\' lettering: 9 rows, cover 0..15 per pixel. */')
    out.append('static const char kMskTitleChars[] = "%s";' % chars.replace('\\', '\\\\').replace('"', '\\"'))
    out.append('static const uint8_t kMskTitleWidth[%d] = {%s};' % (len(chars), ', '.join(str(len(TITLE_FONT[c][0])) for c in chars)))
    out.append('static const uint8_t kMskTitleRows[%d][9][7] = {' % len(chars))
    for c in chars:
        rows = [(r + ' ' * 7)[:7] for r in TITLE_FONT[c]]
        out.append('    {%s},' % ', '.join('{%s}' % ', '.join(str(level[ch]) for ch in r) for r in rows))
    out.append('};')
    tiny = ''.join(sorted(TINY_FONT))
    out.append('/* The tiny lettering (status capsule, HP percentage): 3x5, a bit per pixel. */')
    out.append('static const char kMskTinyChars[] = "%s";' % tiny)
    out.append('static const uint8_t kMskTinyRows[%d][5] = {%s};' % (len(tiny), ', '.join(
        '{%s}' % ', '.join(str(int(r.replace('.', '0').replace('#', '1'), 2)) for r in TINY_FONT[c]) for c in tiny)))
    out.append('')
    out.append('#endif')
    OUT.write_text('\n'.join(out) + '\n')
    print('wrote', OUT.relative_to(ROOT), 'neon', neon, 'pill', pill, 'seg', seg_ink)


if __name__ == '__main__':
    main()
