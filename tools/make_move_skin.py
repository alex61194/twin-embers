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
    'C': ['.##', '#..', '#..', '#..', '.##'], 'D': ['##.', '#.#', '#.#', '#.#', '##.'], 'E': ['###', '#..', '##.', '#..', '###'],
    'G': ['.##', '#..', '#.#', '#.#', '.##'], 'H': ['#.#', '#.#', '###', '#.#', '#.#'], 'I': ['###', '.#.', '.#.', '.#.', '###'],
    'J': ['..#', '..#', '..#', '#.#', '.#.'], 'M': ['#.#', '###', '###', '#.#', '#.#'], 'O': ['.#.', '#.#', '#.#', '#.#', '.#.'],
    'Q': ['.#.', '#.#', '#.#', '###', '.##'], 'T': ['###', '.#.', '.#.', '.#.', '.#.'], 'U': ['#.#', '#.#', '#.#', '#.#', '###'],
    'V': ['#.#', '#.#', '#.#', '#.#', '.#.'], 'W': ['#.#', '#.#', '###', '###', '#.#'], 'X': ['#.#', '#.#', '.#.', '#.#', '#.#'],
    'Y': ['#.#', '#.#', '.#.', '.#.', '.#.'], '.': ['...', '...', '...', '...', '.#.'], '/': ['..#', '..#', '.#.', '#..', '#..'],
    '-': ['...', '...', '###', '...', '...'],
}



def title_glyph(rows):
    """The move names' lettering a touch smaller: 8 rows, one of each glyph's repeated middle rows left out."""
    for r in (4, 5, 3, 6, 2):
        if rows[r] == rows[r - 1] or rows[r] == rows[r + 1]:
            return rows[:r] + rows[r + 1:]
    return rows[:4] + rows[5:]


def title_narrow(rows):
    """And a column narrower (glyphs 7 wide): the inner column whose loss changes the glyph least (no stroke cut to one
    pixel, no stroke lost)."""
    def runs(row):
        out, n = [], 0
        for ch in row + ' ':
            if ch == '#':
                n += 1
            else:
                if n:
                    out.append(n)
                n = 0
        return out
    if len(rows[0]) < 7:
        return rows
    best = None
    for c in range(1, len(rows[0]) - 1):
        cut = [r[:c] + r[c + 1:] for r in rows]
        cost = 0
        for a, b in zip(rows, cut):
            ra, rb = runs(a), runs(b)
            cost += 5 * (len(ra) != len(rb)) + sum(x == 1 for x in rb) - sum(x == 1 for x in ra)
            cost += 2 * (a[c] != ' ' and a[c] != a[c - 1] and a[c] != a[c + 1])
        if best is None or cost < best[0]:
            best = (cost, cut)
    return best[1]


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
    # The icon's place takes the tab's own colour on its row: clear of the tab's diagonal (u = v + 95) and its glow, and
    # where the diagonal comes too close, the row above's.
    u0, v0, u1, v1 = ICON_MASK
    for v in range(v0, v1):
        ud = v + 102
        tab = field[v][ud] if ud < u0 else field[v - 1][u0]
        for u in range(max(u0, v + 99), u1):                 # right of the diagonal: it stays as painted
            field[v][u] = tab
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


def sheet_cover(card, box, bg=None):
    """A picture on a sheet card within `box` (cell coordinates): per pixel a cover over the background `bg` (the
    darker half's middle unless given) and the colour that gives the pixel back over it."""
    px = card.load()
    u0, v0, u1, v1 = box
    if bg is None:
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


# The owner's cards are drawn as painted; only what a battle changes is taken out, row by row, with the card's own
# background there (frame coordinates, 133x60): the name, the capsule's word, the matchup, the PP bar and count.
DLX_TITLE = (4, 13, 26)                  # u0, v0, v1; each row up to u = v + 92 (the tab's diagonal) or 106
DLX_TITLE_FREE = range(3, 8)
DLX_MATCH = (88, 27, 130, 44)
DLX_MATCH_FREE = range(62, 86)
DLX_BAR = (3, 44, 130, 56)
DLX_BAR_FREE = range(73, 78)
DLX_BAR_U0, DLX_BAR_U1 = 3, 80                  # the PP bar's columns (it ends before the PP count)
DLX_BAR_V0, DLX_BAR_ROWS, DLX_BAR_X0, DLX_BAR_X1 = 47, 7, 8, 71   # one bar for every type: rows, ends on row 50
DLX_PILL_ROWS = range(32, 41)
DLX_SEG_V0, DLX_SEG_H, DLX_SEG_CELL, DLX_SEG_ROW = 45, 10, 7, 50   # the PP bar's rows; cells measured on row 50


def dlx_off(v):
    """The PP cells lean right as they go up: half a pixel a row from row 50."""
    return (DLX_SEG_ROW - v) // 2


def dlx_seg_starts(src):
    """The card's PP cells as it paints them: the lit ones found as bright stretches of the bar (leaning undone),
    their pitch, and the spent ones after them up to where the bar ends. Returns the cells' starts on row 50 plus one
    past the last, how many are lit, and how many there are."""
    prof = [sum(lum(src[min(132, u + dlx_off(v)), v]) for v in range(47, 53)) / 6 for u in range(0, 82)]
    body = sorted(prof[74:79])[2]
    top = max(prof[3:79])
    lit, inr = [], False
    for u in range(3, 80):
        b = prof[u] > body + 0.5 * (top - body)
        if b and not inr:
            start, inr = u, True
        if (not b or u == 79) and inr:
            if u - start >= 3:
                lit.append(start)
            inr = False
    pitch = (lit[-1] - lit[0]) / (len(lit) - 1) if len(lit) >= 2 else 19 / 3
    end = max(u for u in range(3, 76) if prof[u] > body + 12)
    count = len(lit) + max(0, round((end + 1 - (lit[-1] + pitch)) / pitch))
    starts = lit + [round(lit[-1] + (k - len(lit) + 1) * pitch) for k in range(len(lit), count + 1)]
    return starts, len(lit), count


def dlx_cell(src, start, bg_cols):
    """A PP cell from its start on row 50: 10 rows of 7 pixels, leaning."""
    return [[src[start - 1 + dlx_off(DLX_SEG_V0 + v) + k, DLX_SEG_V0 + v] for k in range(DLX_SEG_CELL)]
            for v in range(DLX_SEG_H)]


def deluxe_cards(neon_by_type):
    """Each type's card from the sheet as painted (RGB, 133x60) with what changes taken out, its capsule's middle, and
    its PP bar whole: every cell lit (cells the card paints spent take its last lit cell, moved), as painted, plus per
    pixel how far it is lit over the bar's background (to paint it in another colour) and the lit colour."""
    im = Image.open(ART / 'move-cards.png').convert('RGB')
    cards, pills, bars = {}, {}, {}

    def row_bg(px, v, cols):
        return median(px[u, v] for u in cols)

    for i, t in enumerate(SHEET_TYPES):
        x, y = SHEET_W * (i % 3), SHEET_H * (i // 3)
        card = im.crop((x, y, x + SHEET_W, y + SHEET_H))
        c = card.copy()
        px, src = c.load(), card.load()
        u0, v0, v1 = DLX_TITLE
        for v in range(v0, v1):
            bg = row_bg(src, v, DLX_TITLE_FREE)
            for u in range(u0, min(106, v + 92)):
                px[u, v] = bg
        mu0, mv0, mu1, mv1 = DLX_MATCH
        for v in range(mv0, mv1):
            bg = row_bg(src, v, DLX_MATCH_FREE)
            for u in range(mu0, mu1):
                px[u, v] = bg
        body = row_bg(src, 36, DLX_MATCH_FREE)
        far = lambda u: sum(abs(src[u, 36][ch] - body[ch]) for ch in range(3)) > 60
        pl = next(u for u in range(4, 80) if far(u))
        pr = next(u for u in range(80, 4, -1) if far(u))
        pills[t] = (pl + pr + 1) // 2
        # each row of the word: the capsule's own pixels there (not white, not the word's dark outline: near the
        # capsule's overall brightness), or between the rows round it
        allv = [src[u, v] for v in DLX_PILL_ROWS for u in range(pl + 2, pr - 1) if max(src[u, v]) < 200]
        ref = lum(sorted(allv, key=lum)[len(allv) * 2 // 3])
        fills = {}
        for v in DLX_PILL_ROWS:
            vals = [src[u, v] for u in range(pl + 2, pr - 1)
                    if max(src[u, v]) < 200 and abs(lum(src[u, v]) - ref) < 0.3 * ref + 10]
            if len(vals) >= 4:
                fills[v] = median(vals)
        rows = sorted(fills)
        for v in DLX_PILL_ROWS:
            if v not in fills:
                a = max((r for r in rows if r < v), default=None)
                b = min((r for r in rows if r > v), default=None)
                fills[v] = fills[a if b is None else b if a is None else a] if a is None or b is None else tuple(
                    fills[a][ch] + (fills[b][ch] - fills[a][ch]) * (v - a) / (b - a) for ch in range(3))
            for u in range(pl + 3, pr - 2):
                px[u, v] = tuple(int(x) for x in fills[v])
        # the PP bar, one piece: the card's own extent (leaning like its cells), its lit colour row by row and its
        # empty colour row by row (its spent cells', or the background lit a little with the frame's colour)
        bgs = [row_bg(src, DLX_SEG_V0 + v, DLX_BAR_FREE) for v in range(DLX_SEG_H)]
        span = max(1.0, max(lum(src[u, DLX_SEG_V0 + v]) for v in range(DLX_SEG_H) for u in range(3, 78)) - lum(bgs[DLX_SEG_H // 2]))
        lit_rows, empty_rows, x0, x1 = {}, {}, 999, 0
        starts, lit_n, count = dlx_seg_starts(src)
        lit_end = starts[lit_n] - 1 if lit_n < len(starts) - 1 else 999
        for v in range(DLX_SEG_H):
            vv = DLX_SEG_V0 + v
            off = dlx_off(vv)
            on = [src[u, vv] for u in range(3, 78) if (lum(src[u, vv]) - lum(bgs[v])) / span > 0.55 and u - off < lit_end]
            dim = [src[u, vv] for u in range(3, 78) if 8 < lum(src[u, vv]) - lum(bgs[v]) and u - off >= lit_end]
            if len(on) >= 4:
                lit_rows[v] = median(on)
                us = [u - off for u in range(3, 78) if (lum(src[u, vv]) - lum(bgs[v])) / span > 0.2]
                x0, x1 = min(x0, min(us)), max(x1, max(us))
            if len(dim) >= 3:
                empty_rows[v] = median(dim)
        x1 = min(74, max(x1, starts[count] - 2))            # it ends where the card's last cell ends, before its PP count
        rows = sorted(lit_rows)
        top, bot = rows[0], rows[-1]
        frame = neon_by_type[t]
        lit = [lit_rows.get(v, lit_rows[top] if v < top else lit_rows[bot]) for v in range(top, bot + 1)]
        empty = [empty_rows.get(v) or tuple(bgs[v][ch] * 0.75 + frame[ch] * 0.18 for ch in range(3)) for v in range(top, bot + 1)]
        if t == 11:                                           # the sheet's Water bar is painted low: its own colour
            cyan = (72, 216, 248)
            ref = lum(lit[len(lit) // 2])
            lit = [tuple(clamp(cyan[ch] * lum(c) / max(1.0, ref)) for ch in range(3)) for c in lit]
        resample = lambda rows: [rows[min(len(rows) - 1, k * len(rows) // DLX_BAR_ROWS)] for k in range(DLX_BAR_ROWS)]
        bars[t] = (resample(lit), resample(empty))
        # the bar's place on the card: its background
        bu0, bv0, bu1, bv1 = DLX_BAR
        for v in range(bv0, bv1):
            bg = row_bg(src, v, DLX_BAR_FREE)
            for u in range(bu0, bu1):
                px[u, v] = bg
        cards[t] = c
    return cards, pills, bars


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
        sp = sheet[k].load()                                  # the tab's own colour: right of its diagonal, below the frame
        tab = median(sp[u, v] for v in range(6, 22) for u in range(v + 104, 134))
        icon = sheet_cover(sheet[k], ICON_MASK, tab)
        for v, row in enumerate(icon):                       # the tab's diagonal (u = v + 97) is the card's, not the icon's
            for u in range(len(row)):
                if ICON_MASK[0] + u <= ICON_MASK[1] + v + 100:
                    row[u] = (0.0, row[u][1])
        type_icon[t] = icon
    for k, t in enumerate(ART_TYPES):                         # the screen artwork's own four, sharper
        type_neon[t], type_pill[t] = neon[k], pill[k]
        type_icon[t] = icons[k]
    type_seg[11] = (72, 216, 248)                            # the sheet's Water card is at a fifth of its PP (orange)
    neutral = sheet_neutral(sheet[SHEET_NEUTRAL])
    dlx, dlx_pill, dlx_bars = deluxe_cards(type_neon)
    hp_green = [median(bpx[x, y] for x in range(32, 57)) for y in range(42, 47)]
    hp_yellow = [median(bpx[x, y] for x in range(76, 88)) for y in range(42, 47)]

    out = ['/* Generated by tools/make_move_skin.py from docs/media/move-screen.png and move-screen-pressed.png (the',
           ' * move-selection screen\'s artwork). Do not edit: change the artwork and run the script again. */',
           '#ifndef BATTLE_MOVE_SKIN_H', '#define BATTLE_MOVE_SKIN_H', '', '#include <stdint.h>', '']
    out.append('#define MSK_CARD_W %d\n#define MSK_CARD_H %d' % (CARD_W, CARD_H))
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
    out.append('/* The matchup marks: x2 star, x1/2 diamond, x0 cross, x4 star; cover 0..255 and colour. */')
    for i, (ink, alpha, gap, top) in enumerate(marks):
        h, w = len(alpha), len(alpha[0])
        out.append('#define MSK_MARK%d_W %d\n#define MSK_MARK%d_H %d\n#define MSK_MARK%d_GAP %d\n#define MSK_MARK%d_V %d'
                   % (i, w, i, h, i, gap, i, top))
        out.append('static const uint8_t kMskMark%dInk[3] = {%d, %d, %d};' % ((i,) + ink))
        out.append(c_rows('kMskMark%d[%d][%d]' % (i, h, w), 'uint8_t', alpha, lambda a: '%d' % round(a * 255), 20))
    ink, alpha, gap, top = neutral
    out.append('/* The neutral mark (a dash before "x1"). */')
    out.append('#define MSK_MARK4_W %d\n#define MSK_MARK4_H %d\n#define MSK_MARK4_GAP %d\n#define MSK_MARK4_V %d'
               % (len(alpha[0]), len(alpha), gap, top))
    out.append('static const uint8_t kMskMark4Ink[3] = {%d, %d, %d};' % ink)
    out.append(c_rows('kMskMark4[%d][%d]' % (len(alpha), len(alpha[0])), 'uint8_t', alpha, lambda a: '%d' % round(a * 255), 20))
    out.append('/* Per type, the frame\'s colour (the glow round the owner\'s card, an empty slot\'s frame). */')
    out.append('static const uint8_t kMskTypeNeon[18][3] = {%s};' % ', '.join('{%d, %d, %d}' % tuple(int(x) for x in c) for c in type_neon))
    out.append('/* The owner\'s eighteen cards as painted (frame size, RGB565), what a battle changes taken out; by type. */')
    out.append('#define MSK_DLX_W %d\n#define MSK_DLX_H %d' % (SHEET_W, SHEET_H))
    for part in range(0, 18, 3):
        name = 'battle_move_skin_cards%d.h' % (part // 3)
        out.append('#include "%s"' % name)
        body = ''.join(c_rows('kMskDlx%d[MSK_DLX_H][MSK_DLX_W]' % t, 'uint16_t', crop(dlx[t], 0, 0, SHEET_W, SHEET_H),
                              lambda c: '0x%04X' % rgb565(c), 20) for t in range(part, min(18, part + 3)))
        (OUT.parent / name).write_text('/* Generated by tools/make_move_skin.py: the owner\'s move cards, types %d-%d. '
                                       'Do not edit. */\n%s' % (part, min(18, part + 3) - 1, body))
    out.append('static const uint16_t (*const kMskDlx[18])[MSK_DLX_W] = {%s};' % ', '.join('kMskDlx%d' % t for t in range(18)))
    out.append('static const uint8_t kMskDlxPillMid[18] = {%s};' % ', '.join(str(dlx_pill[t]) for t in range(18)))
    out.append('/* The PP bar, one piece and the same for every type: its first row, its rows, its ends on row MSK_DLX_SEG_ROW (it')
    out.append(' * leans half a pixel a row like the cards\' cells); per type, row by row, its lit and its empty colour from the card. */')
    out.append('#define MSK_DLX_SEG_ROW %d\n#define MSK_DLX_BAR_V0 %d\n#define MSK_DLX_BAR_ROWS %d\n#define MSK_DLX_BAR_X0 %d\n#define MSK_DLX_BAR_X1 %d'
               % (DLX_SEG_ROW, DLX_BAR_V0, DLX_BAR_ROWS, DLX_BAR_X0, DLX_BAR_X1))
    out.append('static const struct { uint8_t lit[MSK_DLX_BAR_ROWS][3], empty[MSK_DLX_BAR_ROWS][3]; } kMskDlxBar[18] = {')
    for t in range(18):
        lit, empty = dlx_bars[t]
        out.append('    {{%s}, {%s}},' % (', '.join('{%d, %d, %d}' % tuple(int(x) for x in c) for c in lit),
                                        ', '.join('{%d, %d, %d}' % tuple(int(x) for x in c) for c in empty)))
    out.append('};')
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
    out.append('/* The move names\' lettering: 8 rows, at most 6 wide, cover 0..15 per pixel. */')
    title = {c: title_narrow(title_glyph([(r + ' ' * 7)[:len(TITLE_FONT[c][0])] for r in TITLE_FONT[c]])) for c in TITLE_FONT}
    out.append('static const char kMskTitleChars[] = "%s";' % chars.replace('\\', '\\\\').replace('"', '\\"'))
    out.append('static const uint8_t kMskTitleWidth[%d] = {%s};' % (len(chars), ', '.join(str(len(title[c][0])) for c in chars)))
    out.append('static const uint8_t kMskTitleRows[%d][8][7] = {' % len(chars))
    for c in chars:
        rows = [(r + ' ' * 7)[:7] for r in title[c]]
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
