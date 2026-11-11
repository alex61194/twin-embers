/* Ascuas 2: the lower screen's second pass of the ember design (the owner's mockup, "Ascuas 2"), as shared pieces.
 * Included by 3ds_touch_skin_impl.h after EmFillRows and EmGradFill, so everything here may use the pixel, fill, text and gradient
 * helpers, and the skin's own palettes come after it and use Em2Face.
 *
 * What it holds: the exact tokens of the mockup (palettes with a gloss band in the upper 42 percent, the section
 * colours), the note with its ember stripe and rules, text that ends in "..." where it does not fit (and a wrapper
 * for running text that does the same on its last line), and the sunk pieces: the tray, the well, the value box with
 * its two amber nubs, the switch, the segmented control, the pill, the counter and the page dots.
 *
 * Rules every piece follows: it draws only inside the rectangle it is given (outline included), nothing outside, so the
 * incremental redraws that clear a 2 px margin round it are complete; fills go column by column (EmFillRows) or as
 * short spans, nothing per pixel over a big area, no blends but the tray's light row and the note's shadow. */

/* ---- the mockup's tokens ---- */
#define EM2_SLATE UI_RGB(64, 72, 104)               /* the outline of icons and cards */
#define EM2_VALSH UI_RGB(255, 206, 188)             /* the shadow of a value in the game's red */
#define EM2_STRIPE_TOP UI_RGB(255, 176, 72)
#define EM2_STRIPE_BOT UI_RGB(232, 112, 28)
#define EM2_COUNT_INK UI_RGB(48, 80, 200)           /* a counter's blue */
#define EM2_COUNT_SH UI_RGB(200, 212, 240)
#define EM2_WELL_EDGE UI_RGB(165, 142, 122)         /* a sunk well on a cream piece */
#define EM2_WELL_EDGE_AM UI_RGB(138, 58, 12)        /* and on an amber one */
#define EM2_PAGE_A UI_RGB(74, 120, 216)             /* OPTIONS' four pages, as the dots show them */
#define EM2_PAGE_B UI_RGB(56, 168, 104)
#define EM2_PAGE_C UI_RGB(168, 112, 208)
#define EM2_PAGE_D UI_RGB(216, 90, 80)
#define EM2_DEX UI_RGB(224, 72, 58)                 /* the sections: DEX, POKEMON, BAG, CARD, SAVE / PC, OPTIONS */
#define EM2_MON UI_RGB(60, 174, 88)
#define EM2_BAG UI_RGB(60, 126, 224)
#define EM2_CARD UI_RGB(230, 168, 32)
#define EM2_SAVE UI_RGB(42, 158, 154)
#define EM2_OPT UI_RGB(124, 120, 148)

/* A colour part of the way (n of d) from `a` to `b`. */
UI_FN uint16_t Em2Lerp(uint16_t a, uint16_t b, int n, int d)
{
    if (d <= 0 || n <= 0)
        return a;
    if (n >= d)
        return b;
    return (uint16_t)((((a >> 11) * (d - n) + (b >> 11) * n) / d) << 11 | (((((a >> 5) & 63) * (d - n) + ((b >> 5) & 63) * n) / d)) << 5
                      | (((a & 31) * (d - n) + (b & 31) * n) / d));
}

/* The glass face of the mockup: rows [0, 42%) from g1 to g2, the rest from f1 to f2 (a cut between them), in a rounded
 * rectangle; one fill per column. */
UI_FN void Em2Face(int x, int y, int w, int h, int radius, uint16_t g1, uint16_t g2, uint16_t f1, uint16_t f2)
{
    uint16_t colour[H];
    int split = h * 42 / 100, r0 = y < 0 ? -y : 0, r1 = y + h > H ? H - y : h;

    if (w <= 0 || h <= 0)
        return;
    if (radius > 5)
        radius = 5;
    if (radius * 2 > h)
        radius = h / 2;
    if (split < 1)
        split = 1;
    for (int row = r0; row < r1; ++row)
        colour[row - r0] = row < split ? Em2Lerp(g1, g2, row, split - 1) : Em2Lerp(f1, f2, row - split, h - split - 1);
    EmFillRows(x, y, w, h, radius, colour, r0, r1);
}

/* A vertical gradient with a middle stop: c0 down to c1 over the first `stop` rows, c1 down to c2 over the rest. */
UI_FN void Em2Fill3(int x, int y, int w, int h, int radius, uint16_t c0, uint16_t c1, int stop, uint16_t c2)
{
    uint16_t colour[H];
    int r0 = y < 0 ? -y : 0, r1 = y + h > H ? H - y : h;

    if (w <= 0 || h <= 0)
        return;
    if (radius > 5)
        radius = 5;
    if (radius * 2 > h)
        radius = h / 2;
    if (stop < 1)
        stop = 1;
    for (int row = r0; row < r1; ++row)
        colour[row - r0] = row < stop ? Em2Lerp(c0, c1, row, stop) : Em2Lerp(c1, c2, row - stop, h - stop - 1);
    EmFillRows(x, y, w, h, radius, colour, r0, r1);
}

/* A disc of diameter d with its top-left at (x, y): the pixels whose centre is inside. */
UI_FN void Em2Disc(int x, int y, int d, uint16_t c)
{
    for (int row = 0; row < d; ++row) {
        int dy = 2 * row + 1 - d, left = d;

        for (int col = 0; col < d; ++col) {
            int dx = 2 * col + 1 - d;

            if (dx * dx + dy * dy <= d * d) {
                left = col;
                break;
            }
        }
        if (left < d)
            Fill(x + left, y + row, d - 2 * left, 1, c);
    }
}

/* ---- text that is cut with "..." ---- */

/* `s` into `out` (`cap` bytes) so that it is at most `maxW` wide: whole, or its start, no spaces before the dots, and
 * three dots. Returns 1 when it was cut. Nothing is dropped in the middle of a character. */
UI_FN int Em2Fit(const char *s, int maxW, char *out, size_t cap)
{
    const char *p = s, *keep = s;
    const char *dot = ".";
    int dotW = Glyph(&dot)->width * 3, acc = 0, whole = UiTextWidth(s, 1);
    size_t n;

    if (cap == 0)
        return 0;
    if (whole <= maxW) {
        n = strlen(s);
        if (n >= cap) {                     /* too long for the buffer: cut it the same way */
            maxW = whole - 1;
        } else {
            memcpy(out, s, n + 1);
            return 0;
        }
    }
    while (*p) {
        const char *q = p;
        int gw = Glyph(&q)->width;

        if (acc + gw + dotW > maxW)
            break;
        acc += gw;
        p = q;
        keep = p;
    }
    n = (size_t)(keep - s);
    while (n > 0 && s[n - 1] == ' ')
        --n;
    if (n + 4 > cap)
        n = cap > 4 ? cap - 4 : 0;
    while (n > 0 && ((unsigned char)s[n] & 0xc0) == 0x80)    /* back to the start of a character */
        --n;
    memcpy(out, s, n);
    memcpy(out + n, "...", 4);
    return 1;
}

/* A line of text from x, in no more than the room up to `limit`: cut with "..." where it does not fit. */
UI_FN void Em2Text(const char *s, int x, int top, uint16_t fg, uint16_t shadow, int limit)
{
    char buf[100];

    Em2Fit(s, limit - x, buf, sizeof(buf));
    UiText(buf, x, top, fg, shadow, 1, limit);
}

/* `a`, a space and `b` into `out` (`cap` bytes), cut where it does not fit; no space when `a` is empty. */
UI_FN void Em2Join(char *out, size_t cap, const char *a, const char *b)
{
    size_t n = 0;

    for (; *a && n + 1 < cap; ++a)
        out[n++] = *a;
    if (n && *b && n + 1 < cap)
        out[n++] = ' ';
    for (; *b && n + 1 < cap; ++b)
        out[n++] = *b;
    out[n] = 0;
}

/* Running text in lines `pitch` apart, `width` wide, at most `maxLines`: it breaks at spaces and, if there is more than the
 * lines hold, the last line ends in "...". */
UI_FN void Em2Wrap(const char *text, int x, int y, int width, int maxLines, int pitch, uint16_t colour, uint16_t shadow)
{
    char word[40], line[100], joined[100], rest[100];
    int lines = 0;
    const char *p = text;

    line[0] = 0;
    while (lines < maxLines) {
        const char *start;
        int w = 0;

        while (*p == ' ' || *p == '\n')
            ++p;
        if (!*p)
            break;
        start = p;
        while (*p && *p != ' ' && *p != '\n' && w < (int)sizeof(word) - 1)
            word[w++] = *p++;
        word[w] = 0;
        Em2Join(joined, sizeof(joined), line, word);
        if (line[0] && UiTextWidth(joined, 1) > width) {
            if (lines + 1 >= maxLines) {                       /* the last line and more to say: the rest, cut */
                Em2Join(rest, sizeof(rest), line, start);
                Em2Text(rest, x, y + lines * pitch, colour, shadow, x + width);
                return;
            }
            Em2Text(line, x, y + lines * pitch, colour, shadow, x + width);
            ++lines;
            Em2Join(line, sizeof(line), "", word);
        } else
            memcpy(line, joined, sizeof(line));
    }
    if (line[0] && lines < maxLines)
        Em2Text(line, x, y + lines * pitch, colour, shadow, x + width);
}

/* ---- the note ---- */

/* The note card of the mockup: slate outline, warm paper from light to a little darker, the light along its top, two
 * rows of a darker paper along the bottom, and the ember stripe on its left. (x, y, w, h) holds the outline. */
UI_FN void Em2Note(int x, int y, int w, int h)
{
    if (w < 12 || h < 14)
        return;
    UiRoundFill(x, y, w, h, 5, EM2_SLATE);
    Em2Fill3(x + 1, y + 1, w - 2, h - 2, 4, UI_RGB(255, 253, 248), UI_RGB(251, 245, 234), (h - 2) * 55 / 100, UI_RGB(242, 232, 214));
    Fill(x + 4, y + 1, w - 8, 1, EM_WHITE);
    Fill(x + 3, y + h - 3, w - 6, 2, UI_RGB(232, 220, 198));
    UiRoundFill(x + 4, y + 6, 4, h - 12, 2, EM2_STRIPE_TOP);
    Em2Fill3(x + 4, y + 6, 4, h - 12, 2, EM2_STRIPE_TOP, EM2_STRIPE_BOT, (h - 12) / 2, EM2_STRIPE_BOT);
    Fill(x + 8, y + 7, 1, h - 14, EM_WHITE);
}

/* The shadow under a note that sits on the backdrop: three rows under it (the three pixels the mockup's shadow shows), in
 * the backdrop's own colour darkened, drawn before the note. Only where there is room under it. */
UI_FN void Em2NoteShadow(int x, int y, int w, int h)
{
    static const uint8_t alpha[3] = {150, 105, 50};

    for (int i = 0; i < 3; ++i)
        for (int cx = x + 2; cx < x + w - 2; ++cx)
            EmBlend(cx, y + h + i, UI_RGB(14, 0, 0), alpha[i]);
}

/* A hairline of the note: a rule with a white row under it. */
UI_FN void Em2Rule(int x, int y, int w)
{
    Fill(x, y, w, 1, UI_RGB(226, 212, 188));
    Fill(x, y + 1, w, 1, EM_WHITE);
}

/* ---- the sunk pieces ---- */

/* The tray: a sunk dark area the pieces of a screen stand in. `x, y, w, h` hold its outline; the light row under it is one
 * more row (y + h). */
typedef struct { int16_t x, y, w, h; uint8_t radius; } Em2Tray;
#define EM2_TRAY_EDGE UI_RGB(20, 3, 4)

/* The colour of each row of a tray (h entries): outline, a shade for the first two inner rows, the dark body, and the
 * light line on the last inner row. */
UI_FN void Em2TrayRows(const Em2Tray *t, uint16_t *rows)
{
    int ih = t->h - 2;
    uint16_t top = UI_RGB(62, 14, 14), mid = UI_RGB(42, 8, 9), bot = UI_RGB(48, 10, 10);

    rows[0] = rows[t->h - 1] = EM2_TRAY_EDGE;
    for (int i = 0; i < ih; ++i) {
        int stop = ih * 70 / 100;
        uint16_t c = i < stop ? Em2Lerp(top, mid, i, stop) : Em2Lerp(mid, bot, i - stop, ih - stop - 1);

        if (i < 2)
            c = Em2Lerp(c, UI_RGB(0, 0, 0), i == 0 ? 115 : 70, 255);   /* the shade under the top edge */
        else if (i == ih - 1)
            c = Em2Lerp(c, UI_RGB(255, 150, 100), 41, 255);          /* and the glint on the last */
        rows[1 + i] = c;
    }
}

/* Is (dx, dy) inside a w x h rounded shape? */
UI_FN int Em2In(int dx, int dy, int w, int h, int r)
{
    int cut = 0;

    if (dx < 0 || dy < 0 || dx >= w || dy >= h)
        return 0;
    if (r > 5)
        r = 5;
    if (dy < 4)
        cut = sUiCut[r][dy];
    else if (h - 1 - dy < 4)
        cut = sUiCut[r][h - 1 - dy];
    return dx >= cut && dx < w - cut;
}

/* The part of the tray inside the clip rectangle, over whatever is there (the caller put the backdrop back). */
UI_FN void Em2TrayPaint(const Em2Tray *t, int cx0, int cy0, int cx1, int cy1)
{
    uint16_t rows[H + 4];
    int r = t->radius > 5 ? 5 : t->radius;

    if (t->h < 4 || t->h > H + 3)
        return;
    Em2TrayRows(t, rows);
    if (cx0 < t->x)
        cx0 = t->x;
    if (cy0 < t->y)
        cy0 = t->y;
    if (cx1 > t->x + t->w)
        cx1 = t->x + t->w;
    if (cy1 > t->y + t->h + 1)
        cy1 = t->y + t->h + 1;
    for (int cx = cx0; cx < cx1; ++cx) {
        int dx = cx - t->x, near = dx < 5 || dx >= t->w - 5;

        for (int cy = cy0; cy < cy1; ++cy) {
            int dy = cy - t->y;

            if (dy >= t->h) {                                  /* the light row under the outline */
                if (Em2In(dx, dy - 1, t->w, t->h, r) && !Em2In(dx, dy, t->w, t->h, r))
                    EmBlend(cx, cy, UI_RGB(255, 150, 100), 31);
                continue;
            }
            if (!near) {
                Px(cx, cy, rows[dy]);                           /* the middle of the tray: its row's colour */
                continue;
            }
            if (!Em2In(dx, dy, t->w, t->h, r))
                continue;                                      /* the corner's cut: the backdrop shows */
            Px(cx, cy, Em2In(dx - 1, dy - 1, t->w - 2, t->h - 2, r ? r - 1 : 0) ? rows[dy] : EM2_TRAY_EDGE);
        }
    }
}

/* The backdrop back over a rectangle that may cross a tray, then the tray's own pixels there. */
UI_FN void Em2TrayRestore(const Em2Tray *t, int x, int y, int w, int h)
{
    TsBgRect(x, y, w, h);
    Em2TrayPaint(t, x, y, x + w, y + h);
}

/* The tray with its backdrop: the whole of it, for a screen's frame. */
UI_FN void Em2TrayDraw(const Em2Tray *t)
{
    Em2TrayRestore(t, t->x, t->y, t->w, t->h + 1);
}

/* The well: a light sunk box (the value of a row, a counter), a warm shade at its top, and a light line under it. The
 * rectangle holds its outline; `amber` is a well on a chosen (amber) piece. */
UI_FN void Em2Well(int x, int y, int w, int h, int amber)
{
    UiRoundFill(x, y, w, h, 5, amber ? EM2_WELL_EDGE_AM : EM2_WELL_EDGE);
    Em2Fill3(x + 1, y + 1, w - 2, h - 2, 4, UI_RGB(230, 216, 194), UI_RGB(246, 238, 224), 3, UI_RGB(255, 250, 241));
}

/* One of the value box's little buttons: amber, 15 x h with its outline, a white arrow on it with its shadow. */
UI_FN void Em2Nub(int x, int y, int w, int h, int dir, int pressed)
{
    uint16_t edge = EM2_WELL_EDGE_AM, ink = EM_WHITE;

    UiRoundFill(x, y, w, h, 3, edge);
    if (pressed)
        EmGradFill(x + 1, y + 2, w - 2, h - 3, 2, UI_RGB(246, 154, 56), UI_RGB(232, 118, 30));
    else {
        Em2Face(x + 1, y + 1, w - 2, h - 2, 2, UI_RGB(255, 220, 144), UI_RGB(255, 194, 96), UI_RGB(246, 154, 56), UI_RGB(232, 118, 30));
        Fill(x + 3, y + 1, w - 6, 1, UI_RGB(255, 242, 200));
    }
    for (int pass = 0; pass < 2; ++pass) {
        int ox = x + w / 2 - 2 + (pass ? 0 : 1), oy = y + h / 2 + (pass ? 0 : 1);

        for (int k = 0; k < 4; ++k) {
            int half = k < 3 ? k + 1 : 3;              /* 1, 2, 3, 3 tall on each side of the middle */
            int col = dir < 0 ? ox + k : ox + 3 - k;

            Fill(col, oy - half, 1, half * 2, pass ? ink : edge);
        }
    }
}

/* The value box: a well with its value in the game's red between two nubs (PREV on the left, NEXT on the right); the nub
 * under a held touch (zone 1 left, 2 right) sinks. */
UI_FN void Em2ValueBox(int x, int y, int w, int h, const char *value, int zone)
{
    int nub = 15, nh = h - 2, tw;

    Em2Well(x, y, w, h, 0);
    Em2Nub(x + 1, y + 1, nub, nh, -1, zone == 1);
    Em2Nub(x + w - 1 - nub, y + 1, nub, nh, 1, zone == 2);
    tw = UiTextWidth(value, 1);
    if (tw > w - 2 * nub - 6) {
        Em2Text(value, x + nub + 3, y + (h - UI_TEXT_H) / 2, EM_VALUE, EM2_VALSH, x + w - nub - 3);
        return;
    }
    UiText(value, x + (w - tw) / 2, y + (h - UI_TEXT_H) / 2, EM_VALUE, EM2_VALSH, 1, x + w - nub);
}

/* The switch: 28 x 12 with its outline, green with the knob on the right when on, grey with the knob on the left when off.
 * No arrows. */
UI_FN void Em2Switch(int x, int y, int on)
{
    int kx = on ? x + 16 : x;

    UiRoundFill(x, y, 28, 12, 5, EM2_SLATE);
    if (on)
        Em2Face(x + 1, y + 1, 26, 10, 5, UI_RGB(166, 238, 174), UI_RGB(92, 200, 116), UI_RGB(60, 176, 92), UI_RGB(42, 148, 74));
    else
        Em2Face(x + 1, y + 1, 26, 10, 5, UI_RGB(216, 212, 220), UI_RGB(216, 212, 220), UI_RGB(170, 166, 178), UI_RGB(170, 166, 178));
    Fill(x + 4, y + 1, 20, 1, on ? UI_RGB(52, 150, 82) : UI_RGB(148, 144, 156));
    Em2Disc(kx, y, 12, EM2_SLATE);
    Em2Disc(kx + 1, y + 1, 10, UI_RGB(214, 216, 226));
    Em2Disc(kx + 1, y + 1, 9, UI_RGB(238, 240, 246));
    Em2Disc(kx + 3, y + 2, 4, EM_WHITE);
}

/* The segmented control, in the well whose inside is (x, y, w, h): `count` equal places, the chosen one a small amber
 * face with white ink, the others the dim ink with a hairline between them (none beside the chosen one); the one under a
 * held touch (hot) a warm tint. Returns nothing; the places are what OPTIONS hit-tests. */
UI_FN void Em2Segments(int x, int y, int w, int h, const char *const *names, int count, int chosen, int hot)
{
    for (int k = 0; k < count; ++k) {
        int sx = x + k * w / count, sw = x + (k + 1) * w / count - sx;
        int tx = sx + (sw - UiTextWidth(names[k], 1)) / 2, ty = y + (h - UI_TEXT_H) / 2;

        if (k == chosen) {
            UiRoundFill(sx, y, sw, h, 4, EM2_WELL_EDGE_AM);
            Em2Face(sx + 1, y + 1, sw - 2, h - 2, 3, UI_RGB(255, 224, 160), UI_RGB(255, 196, 100), UI_RGB(248, 160, 64), UI_RGB(236, 122, 36));
            Fill(sx + 3, y + 1, sw - 6, 1, UI_RGB(255, 244, 204));
            Em2Text(names[k], tx < sx + 1 ? sx + 1 : tx, ty, EM_WHITE, UI_RGB(122, 44, 8), sx + sw - 1);
            continue;
        }
        if (hot == k)
            UiRoundFill(sx + 1, y + 1, sw - 2, h - 2, 3, UI_RGB(255, 214, 160));
        if (k > 0 && k - 1 != chosen)
            Fill(sx, y + 2, 1, h - 4, UI_RGB(220, 202, 176));
        Em2Text(names[k], tx < sx + 1 ? sx + 1 : tx, ty, EM_DIM, EM_DARK_SH, sx + sw - 1);
    }
}

/* A pill: a rounded tag with a gradient from `top` to `bottom`, the light along its top and a short white word with the
 * outline's colour as its shadow (a type, an effectiveness, a state, an action). (x, y) is its top-left with the outline;
 * it is 15 tall and as wide as the word needs; returns its width. */
UI_FN int Em2Pill(int x, int y, const char *text, uint16_t top, uint16_t bottom, uint16_t edge)
{
    int w = UiTextWidth(text, 1) + 14;

    UiRoundFill(x, y, w, 15, 5, edge);
    EmGradFill(x + 1, y + 1, w - 2, 13, 4, top, bottom);
    Fill(x + 4, y + 1, w - 8, 1, Em2Lerp(top, EM_WHITE, 90, 255));
    UiText(text, x + 7, y + 3, EM_WHITE, edge, 1, x + w);
    return w;
}

/* A counter: a small sunk well with its number in the game's blue. (x, y) top-left, 15 tall; returns its width. */
UI_FN int Em2Count(int x, int y, const char *text)
{
    int w = UiTextWidth(text, 1) + 12;

    UiRoundFill(x, y, w, 15, 5, UI_RGB(184, 162, 142));
    Em2Fill3(x + 1, y + 1, w - 2, 13, 4, UI_RGB(232, 220, 200), UI_RGB(251, 245, 234), 4, UI_RGB(255, 250, 242));
    UiText(text, x + 6, y + 3, EM2_COUNT_INK, EM2_COUNT_SH, 1, x + w);
    return w;
}

/* A page dot: 5 x 5 with a dark ring in its place, 7 x 7 with a white ring and a slate one when it is the page shown.
 * (x, y) is the top-left of the dot's own box. */
UI_FN void Em2Dot(int x, int y, uint16_t colour, int on)
{
    if (on) {
        Em2Disc(x - 2, y - 2, 11, EM2_SLATE);
        Em2Disc(x - 1, y - 1, 9, EM_WHITE);
        Em2Disc(x, y, 7, colour);
        return;
    }
    Em2Disc(x - 1, y - 1, 7, UI_RGB(120, 112, 124));
    Em2Disc(x, y, 5, colour);
}

/* The frame round the GBA picture or the HOME map (x, y, w, h the picture): three rings outside it, a light slate edge, a copper line and a dark
 * one, like the mockup's bezel; nothing outside it (its drop shadow would be under the bar's rectangle). */
UI_FN void Em2Bezel(int x, int y, int w, int h)
{
    UiRoundFill(x - 3, y - 3, w + 6, h + 6, 5, UI_RGB(60, 46, 54));
    UiRoundFill(x - 2, y - 2, w + 4, h + 4, 4, UI_RGB(184, 96, 42));
    UiRoundFill(x - 1, y - 1, w + 2, h + 2, 3, UI_RGB(42, 28, 34));
}

/* A key's tab: the button's letter on a small sunk blue pill, centred on cx at the top of a tile. */
UI_FN void Em2KeyTab(int cx, int y, const char *letter)
{
    UiRoundFill(cx - 10, y, 20, 11, 5, UI_RGB(14, 36, 96));
    UiRoundFill(cx - 9, y + 1, 18, 9, 4, UI_RGB(58, 110, 224));
    Fill(cx - 6, y + 1, 12, 1, UI_RGB(34, 76, 168));
    UiText(letter, cx - UiTextWidth(letter, 1) / 2, y + 1, EM_WHITE, UI_RGB(14, 36, 96), 1, cx + 9);
}

/* The tint of the well an icon of the column sits in, by section (0 DEX, 1 POKEMON, 2 BAG, 3 CARD, 4 SAVE, 5 OPTIONS); chosen, the amber one. */
UI_FN void Em2Tint(int section, int chosen, uint16_t *top, uint16_t *bottom)
{
    if (chosen) {
        *top = UI_RGB(255, 242, 204);
        *bottom = UI_RGB(255, 216, 144);
    } else if (section == 0) {
        *top = UI_RGB(251, 224, 218);
        *bottom = UI_RGB(244, 200, 190);
    } else if (section == 1) {
        *top = UI_RGB(224, 246, 226);
        *bottom = UI_RGB(196, 232, 200);
    } else if (section == 2) {
        *top = UI_RGB(224, 236, 255);
        *bottom = UI_RGB(198, 216, 250);
    } else if (section == 3) {
        *top = UI_RGB(255, 242, 204);
        *bottom = UI_RGB(248, 222, 160);
    } else if (section == 4) {
        *top = UI_RGB(216, 244, 242);
        *bottom = UI_RGB(188, 228, 224);
    } else {
        *top = UI_RGB(236, 235, 242);
        *bottom = UI_RGB(216, 214, 226);
    }
}

/* The shadow under a raised piece that sits on the backdrop: the two rows right under its lip, the backdrop's own pixels darkened (computed
 * from the backdrop, not blended onto the canvas, so drawing it again changes nothing). Only the 2 px a piece owns. */
UI_FN void Em2CardShadow(int x, int y, int w, int h)
{
    for (int i = 0; i < 2; ++i)
        for (int cx = x + 3; cx < x + w - 3; ++cx)
            if ((unsigned)cx < W && (unsigned)(y + h + i) < H)
                Px(cx, y + h + i, UiMix(EMBER_BG(cx, y + h + i), UI_RGB(14, 0, 0), i == 0 ? 5 : 3));
}

/* A banner: a raised band with a short word at its left (the title of a window), in a palette's face. */
UI_FN void Em2Banner(int x, int y, int w, int h, const EmPal *p, const char *text)
{
    UiRoundFill(x, y, w, h, 5, p->edge);
    Em2Face(x + 1, y + 1, w - 2, h - 2, 4, p->g1, p->top, p->f1, p->bottom);
    Fill(x + 4, y + 1, w - 8, 1, p->hl);
    Em2Text(text, x + 8, y + (h - UI_TEXT_H) / 2, p->ink, p->inkSh, x + w - 4);
}
