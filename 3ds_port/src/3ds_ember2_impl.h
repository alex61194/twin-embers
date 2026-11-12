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

/* ---- LLAMA: the owner's "Llama" mockup (llama2_mockup.html), as shared pieces, at the end of this file ----
 * Screens call these; they may not be edited by a screen. (x, y, w, h) is the OUTER rectangle, its outline included (the mockup's CSS box
 * is it shrunk by 1 where the CSS draws a 1 px box-shadow outline: .c .win .ty .sw .back); .hpb .rib .tray and the bars are inside their box.
 * Each piece paints inside its rectangle plus what is said here (never more than 2 px); those marked (blends) tint what is already under
 * them, so draw the surface under them first. "mockup" names the CSS class; colours are the mockup's.
 *
 * Palettes (3ds_touch_skin_impl.h, EmPal, used with EmButton / Em2Banner; all have the outline LL_EDGE #2a2238):
 *   kEmFlame (.fl)  kEmSlate (.sl)  kEmPartyBlue (.pb)  kEmPartyRed (.pr)   new; kEmCream kEmBlue kEmGold kEmGreen kEmPurple now equal .cr .bl .go .gr .pu
 *   EM_DARK/EM_DARK_SH/EM_DIM are the mockup's .t / its shadow / .d (#484848, #d0d0c8, #807468).  kEmAmber stays for screens not converted yet.
 *   LlPick(chosen, idle) -> kEmFlame when chosen, else idle.
 * Window and notes:
 *   LlWindow(x,y,w,h)        .win: FireRed's text window (slate outline, white, 2 px sky blue, 1 px light blue, paper) + 2 rows of shadow below.
 *   LlWindowPlain(x,y,w,h)   the same without the shadow (a window over a piece, not the backdrop).
 *   LlRule(x,y,w)            .rule: a 1 px #d8e4ec hairline.
 * HP:
 *   LlHpBox(x,y,w,h)         .hpb: the cream HP box, slate outline, left edge slanted 9 px. The caller writes name / level / numbers on it.
 *   LlHpBar(x,y,w,cur,max)   .bar: 9 tall slate capsule, orange HP/PS/KP label (CTR_TR), track, green / yellow / red fill (>1/2, >1/5, else).
 *   LlMiniText(s,x,y,c)      the bar's 4x5 H P S K letters (others are blanks); returns the width.
 * Ribbons, badges, arrows:
 *   LlRibbon(x,y,text,top,bottom)  .rib: the slanted ribbon, 13 tall, textW + 18 wide, slant 7; returns the width. LL_RIB_TOP/BOT = red, LL_BLUE_TOP/BOT.
 *   LlRibbonBox(x,y,w,h,slant,corners,top,bottom,text)  any size (key tab X/Y: h 11, slant 7, corners LL_TL; "1o": h 11, LL_BL); text cut with "...".
 *   LlTypeBadge(x,y,text,colour)   .ty: rectangular type badge, 13 tall with its outline, textW + 12 wide; returns the width.
 *   LlCursor(x,y,colour)     .cur: FireRed's right-pointing triangle 6 x 10 (+ shadow 1 px right/below); LL_CURSOR red, or EM_WHITE on a chosen row.
 *   LlArrow(x,y,w,h,dir,c)   a CSS-style triangle (dir 0 up, 1 down, 2 left, 3 right) for the bag's up/down and BACK's left arrows.
 * The ball tray (battle):
 *   LlBallTray(x,y,w,h)      .tray: the slanted slate tray (196 x 25).
 *   LlLeadFrame(x,y,w,h)     .tray .on: the gold frame of the lead, 22 x 23 (blends).
 *   LlTraySlot(x,y)          the empty slot, a 14 px ringed disc (blends).
 *   LlTrayBar(x,y,w,cur,max) .tray .m: a ball's mini HP bar, w (18) x 4.
 * Selection and back:
 *   LlSelectRing(x,y,w,h)    .c.sel: 1 px #ffe08a then 1 px #b84a10 round the card's outer rectangle (2 px outside it); no soft glow (it would leave the 2 px).
 *   LlBackDisc(x,y)          .back: the round back arrow, outer 24 x 24.
 * Small pieces of the other screens:
 *   LlWell(x,y,w,h)          .well: the sunk light box (+ a light row at y + h) (blends).      LlSubPill(x,y,w,h)  .sub: the caption pill on action cards (blends).
 *   LlPlate(x,y,d,edge,band,bandW)  .plate: the round plate of a Pokemon with its rings (bandW 0: one ring; else edge, bandW px band, edge).
 *   LlNub(x,y,w,h,dir,pressed)      .nub: the flame arrow button (15 x 14);  LlFlameFace(x,y,w,h,radius,pressed)  its face, also .seg.
 *   LlSegments(x,y,w,h,names,count,chosen,hot)  the segmented control with a flame chosen place.
 *   LlScrollRail(x,y,w,h,thumbY,thumbH)  the bag's rail and its flame thumb.
 * Building blocks: LlSlab / LlGradSlab (a w x h shape with slanted sides, rounded corners and a colour a row), LlGrad, LlRows, LlColumn, LlCut, LlIsqrt.
 * (Sizes of the mockup: the FIGHT card is 304 x 73 with LlHpBox at (82, 32) 216 x 36 inside it; the tray is at (62, 5) 196 x 25.) */

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
/* Always inlined: with a constant d (255 in most calls) the divisions become multiplications (ARMv6k has no hardware divide). */
static inline __attribute__((always_inline, unused)) uint16_t Em2Lerp(uint16_t a, uint16_t b, int n, int d)
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
UI_FN const uint16_t *Em2FaceColours(int h, uint16_t g1, uint16_t g2, uint16_t f1, uint16_t f2)
{
    /* The colours of a face depend only on its palette and height, and a screen draws a dozen of the same: the last few are kept. */
    enum { KEPT = 6 };
    static struct { uint16_t g1, g2, f1, f2, h; uint16_t colour[H]; } cache[KEPT];
    static unsigned next;
    uint16_t *colour = 0;
    int split = h * 42 / 100;

    if (h <= 0 || h > H)
        return 0;
    if (split < 1)
        split = 1;
    for (unsigned i = 0; i < KEPT; ++i)
        if (cache[i].h == h && cache[i].g1 == g1 && cache[i].g2 == g2 && cache[i].f1 == f1 && cache[i].f2 == f2)
            colour = cache[i].colour;
    if (!colour) {
        colour = cache[next].colour;
        cache[next].g1 = g1, cache[next].g2 = g2, cache[next].f1 = f1, cache[next].f2 = f2, cache[next].h = (uint16_t)h;
        next = (next + 1) % KEPT;
        for (int row = 0; row < h; ++row)
            colour[row] = row < split ? Em2Lerp(g1, g2, row, split - 1) : Em2Lerp(f1, f2, row - split, h - split - 1);
    }
    return colour;
}

UI_FN void Em2Face(int x, int y, int w, int h, int radius, uint16_t g1, uint16_t g2, uint16_t f1, uint16_t f2)
{
    const uint16_t *colour = Em2FaceColours(h, g1, g2, f1, f2);
    int split = h * 42 / 100, r0 = y < 0 ? -y : 0, r1 = y + h > H ? H - y : h;

    if (w <= 0 || h <= 0)
        return;
    if (radius > 5)
        radius = 5;
    if (radius * 2 > h)
        radius = h / 2;
    if (split < 1)
        split = 1;
    if (!colour) {                          /* taller than the screen (not used): only the rows on it, not kept */
        uint16_t part[H];

        for (int row = r0; row < r1; ++row)
            part[row - r0] = row < split ? Em2Lerp(g1, g2, row, split - 1) : Em2Lerp(f1, f2, row - split, h - split - 1);
        EmFillRows(x, y, w, h, radius, part, r0, r1);
        return;
    }
    EmFillRows(x, y, w, h, radius, colour + r0, r0, r1);   /* (EmFillRows reads colour[row - r0] for rows r0..r1) */
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
UI_FN unsigned LlIsqrt(unsigned n);

UI_FN void Em2Disc(int x, int y, int d, uint16_t c)
{
    /* A column at a time (the canvas' order): the pixels of column col whose centre is inside are rows [(d - s) / 2, (d + s - 1) / 2], s
     * the integer root of d * d - dx * dx. */
    for (int col = 0; col < d; ++col) {
        int dx = 2 * col + 1 - d, s = (int)LlIsqrt((unsigned)(d * d - dx * dx)), lo = (d - s) / 2, hi = (d + s - 1) / 2;

        if (hi >= lo)
            Fill(x + col, y + lo, 1, hi - lo + 1, c);
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
    {
        uint16_t paper[H];
        int ih = h - 2, stop = ih * 55 / 100;

        if (h > H)
            return;
        if (stop < 1)
            stop = 1;
        for (int row = 0; row < ih; ++row)      /* Em2Fill3's rows */
            paper[row] = row < stop ? Em2Lerp(UI_RGB(255, 253, 248), UI_RGB(251, 245, 234), row, stop) : Em2Lerp(UI_RGB(251, 245, 234), UI_RGB(242, 232, 214), row - stop, ih - stop - 1);
        EmFramedRows(x, y, w, h, 5, 4, EM2_SLATE, paper);   /* the outline and the paper, each pixel once */
    }
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
    int x0 = x + 2 < 0 ? 0 : x + 2, x1 = x + w - 2 > W ? W : x + w - 2;

    for (int cx = x0; cx < x1; ++cx)
        for (int i = 0; i < 3; ++i)
            if ((unsigned)(y + h + i) < H) {            /* EmBlend of the dark red over what is there, a column at a time */
                uint16_t *p = &sCanvas[(size_t)cx * H + (H - 1 - (size_t)(y + h + i))];

                *p = UiMix(*p, UI_RGB(14, 0, 0), alpha[i] * 8 / 255);
            }
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

/* A page dot: 5 x 5 with a dark ring in its place, 7 x 7 with a slate ring (9 x 9, so the pill keeps air above and below it) when it is the page shown.
 * (x, y) is the top-left of the dot's own box. */
UI_FN void Em2Dot(int x, int y, uint16_t colour, int on)
{
    if (on) {
        Em2Disc(x - 1, y - 1, 9, EM2_SLATE);
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
    int x0 = x + 3 < 0 ? 0 : x + 3, x1 = x + w - 3 > W ? W : x + w - 3;

    if (!sTsBgReady)
        TsBgPrepare();
    for (int cx = x0; cx < x1; ++cx)
        for (int i = 0; i < 2; ++i)
            if ((unsigned)(y + h + i) < H) {
                size_t at = (size_t)cx * H + (H - 1 - (size_t)(y + h + i));

                sCanvas[at] = UiMix(sTsBg[at], UI_RGB(14, 0, 0), i == 0 ? 5 : 3);     /* sTsBg is EMBER_BG in the canvas' order */
            }
}

/* A banner: a raised band with a short word at its left (the title of a window), in a palette's face. */
UI_FN void Em2Banner(int x, int y, int w, int h, const EmPal *p, const char *text)
{
    UiRoundFill(x, y, w, h, 5, p->edge);
    Em2Face(x + 1, y + 1, w - 2, h - 2, 4, p->g1, p->top, p->f1, p->bottom);
    Fill(x + 4, y + 1, w - 8, 1, p->hl);
    Em2Text(text, x + 8, y + (h - UI_TEXT_H) / 2, p->ink, p->inkSh, x + w - 4);
}

/* ======================================================================================================================================
 * Llama: the pieces of the owner's "Llama" mockup (llama2_mockup.html), in the order of the list at the top of this file.
 *
 * Conventions: (x, y, w, h) is the OUTER rectangle of a piece, its outline included (Em2 pieces do the same); where the mockup draws
 * an outline outside its CSS box (a 1 px box-shadow: .c, .win, .ty, .sw, .back) the CSS box is that rectangle shrunk by 1 on every side.
 * .hpb, .rib, .tray and the bars are clipped inside their box, so their rectangle is the box as written. A piece paints only inside
 * its rectangle, apart from what its comment says it adds (at most 2 px), and never reads the canvas unless it says so: the pieces
 * that blend over what is there (LlLeadFrame, LlTraySlot, LlSubPill, LlWell's light line) want the surface under them drawn first.
 * Fills go column by column (the canvas' order). Nothing here allocates; nothing is per pixel over more than a card's worth.
 * ==================================================================================================================================== */
#define LL_SLATE EM2_SLATE                          /* the outline of the window, the HP box and the bar (#404868) */
#define LL_SLATE_DK UI_RGB(32, 38, 63)              /* the tray's outline and the mini bars' track (#20263f) */
#define LL_RULE UI_RGB(216, 228, 236)               /* .rule (#d8e4ec) */
#define LL_RING_IN UI_RGB(255, 224, 138)            /* .c.sel: the ring's inner px (#ffe08a) */
#define LL_RING_OUT UI_RGB(184, 74, 16)             /* and its outer px (#b84a10) */
#define LL_CURSOR UI_RGB(232, 56, 32)               /* .cur (#e83820), its shadow (#50100c) */
#define LL_CURSOR_SH UI_RGB(80, 16, 12)
#define LL_RIB_TOP UI_RGB(248, 96, 72)              /* .rib: red from #f86048 to #d02c20 */
#define LL_RIB_BOT UI_RGB(208, 44, 32)
#define LL_BLUE_TOP UI_RGB(92, 154, 244)            /* the blue ribbon of a key tab: #5c9af4 to #2e68d2 */
#define LL_BLUE_BOT UI_RGB(46, 104, 210)

#define LL_TL 1                                     /* corners LlSlab / LlRibbonBox round (radius 5) */
#define LL_BL 2
#define LL_TR 4
#define LL_BR 8

/* The integer square root. */
UI_FN unsigned LlIsqrt(unsigned n)
{
    unsigned r = 0, b = 1u << 30;

    while (b > n)
        b >>= 2;
    for (; b; b >>= 2) {
        if (n >= r + b) {
            n -= r + b;
            r = (r >> 1) + b;
        } else
            r >>= 1;
    }
    return r;
}

/* Rows 0..h-1 of `out` from c0 to c1 (a straight gradient, one colour a row). */
UI_FN void LlRows(uint16_t *out, int h, uint16_t c0, uint16_t c1)
{
    for (int r = 0; r < h; ++r)
        out[r] = Em2Lerp(c0, c1, r, h > 1 ? h - 1 : 1);
}

/* A rounded rectangle with a straight gradient c0 to c1 down its rows (the glass without the cut: windows, bars). */
UI_FN void LlGrad(int x, int y, int w, int h, int radius, uint16_t c0, uint16_t c1)
{
    uint16_t colour[H];
    int r0 = y < 0 ? -y : 0, r1 = y + h > H ? H - y : h;

    if (w <= 0 || h <= 0)
        return;
    if (radius > 5)
        radius = 5;
    if (radius * 2 > h)
        radius = h / 2;
    for (int row = r0; row < r1; ++row)
        colour[row - r0] = Em2Lerp(c0, c1, row, h > 1 ? h - 1 : 1);
    EmFillRows(x, y, w, h, radius, colour, r0, r1);
}

/* The rows [r0, r1) of the column cx painted from `colour` (colour[r] for row r of a shape whose top is at y). */
UI_FN void LlColumn(int cx, int y, int r0, int r1, const uint16_t *colour)
{
    uint16_t *p;

    if ((unsigned)cx >= W)
        return;
    if (y + r0 < 0)
        r0 = -y;
    if (y + r1 > H)
        r1 = H - y;
    if (r0 >= r1)
        return;
    p = sCanvas + (size_t)cx * H + (H - 1 - (size_t)(y + r0));
    for (int r = r0; r < r1; ++r)
        *p-- = colour[r];
}

/* The first row that column dx has inside a left edge that slants `s` px (its top `s` px in from the left, its bottom at the left:
 * polygon(s 0, ..., 0 100%)): the rows whose centre is right of the edge. */
UI_FN int LlSlantLo(int h, int s, int dx)
{
    int num = h * (2 * s - 2 * dx - 1) - s;

    return num <= 0 ? 0 : (num + 2 * s - 1) / (2 * s);
}

/* The number of rows from the top that column dx has inside a right edge that slants `s` px (its top at the right edge w, its bottom
 * `s` px in: polygon(..., 100% 0, calc(100% - s) 100%)). */
UI_FN int LlSlantHi(int w, int h, int s, int dx)
{
    int t = h * (2 * w - 2 * dx - 1) - s, n;

    if (t < 0)
        return 0;
    n = t / (2 * s) + 1;
    return n > h ? h : n;
}

/* The corner cut (sUiCut[5]) of row `row` of a shape h tall. */
UI_FN int LlCut(int row, int h)
{
    if (row < 4)
        return sUiCut[5][row];
    if (h - 1 - row < 4)
        return sUiCut[5][h - 1 - row];
    return 0;
}

/* A slab: a w x h shape whose left edge slants `sl` px and right edge `sr` px (0: straight), the `corners` rounded (radius 5), row r
 * painted colour[r] (h entries). The mockup's clip-path shapes: .hpb (sl 9), .rib (sr 7), .tray (both 9). One pass, column by column. */
UI_FN void LlSlab(int x, int y, int w, int h, int sl, int sr, int corners, const uint16_t *colour)
{
    if (w <= 0 || h <= 0 || h > H)
        return;
    for (int dx = 0; dx < w; ++dx) {
        int lo = sl > 0 && dx < sl ? LlSlantLo(h, sl, dx) : 0, hi = sr > 0 && dx >= w - sr ? LlSlantHi(w, h, sr, dx) : h;
        int fromRight = w - 1 - dx;

        if (corners & LL_TL)
            while (lo < 4 && lo < hi && sUiCut[5][lo] > dx)
                ++lo;
        if (corners & LL_TR)
            while (lo < 4 && lo < hi && sUiCut[5][lo] > fromRight)
                ++lo;
        if (corners & LL_BL)
            while (hi > lo && h - hi < 4 && sUiCut[5][h - hi] > dx)
                --hi;
        if (corners & LL_BR)
            while (hi > lo && h - hi < 4 && sUiCut[5][h - hi] > fromRight)
                --hi;
        LlColumn(x + dx, y, lo, hi, colour);
    }
}

/* A gradient slab: LlSlab with the rows from c0 to c1. */
UI_FN void LlGradSlab(int x, int y, int w, int h, int sl, int sr, int corners, uint16_t c0, uint16_t c1)
{
    uint16_t colour[H];

    if (h > H)
        return;
    LlRows(colour, h, c0, c1);
    LlSlab(x, y, w, h, sl, sr, corners, colour);
}

/* 2. LlWindow: FireRed's text window (.win). The outer rectangle holds the slate outline (the mockup's box is it shrunk by 1), then
 * white, two px of sky blue, a px of light blue and the paper (white to #f8f8f0); corners radius 5. LlWindow adds the shadow the mockup
 * throws under it (two rows, from the backdrop's own pixels: only where the backdrop shows under it); LlWindowPlain is without it. */
UI_FN void LlWindowPlain(int x, int y, int w, int h)
{
    /* Five nested rounded shapes (slate, white, sky blue, light blue, the paper's gradient), the inner ones over the outer: each pixel is
     * painted once, a column at a time. A column is its rows' colours top to bottom, built from where each shape starts and ends in it; the
     * columns the corners do not reach (all but the six at each end) are one image. */
    static const uint8_t inset[5] = {0, 1, 2, 4, 5}, radius[5] = {5, 5, 4, 3, 2};
    const uint16_t flat[4] = {LL_SLATE, EM_WHITE, UI_RGB(122, 180, 216), UI_RGB(200, 228, 240)};
    uint16_t grad[H], img[H];                       /* img in the canvas' order: row r of the window is img[h - 1 - r] */
    int imgFor = -1;

    if (w < 14 || h < 14 || h > H)
        return;
    for (int r = 0; r < h - 10; ++r)
        grad[r] = Em2Lerp(EM_WHITE, UI_RGB(248, 248, 240), r, h - 10 > 1 ? h - 11 : 1);
    for (int c = 0; c < w; ++c) {
        int cx = x + c, t[5], b[5], lo, hi, built = c >= 6 && c < w - 6;

        if ((unsigned)cx >= W)
            continue;
        for (int k = 0; k < 5; ++k) {
            int lx = c - inset[k], lw = w - 2 * inset[k], edge = lx < lw - 1 - lx ? lx : lw - 1 - lx, n = 0;

            if (lx < 0 || lx >= lw) {               /* this shape does not reach the column: the one outside it fills it */
                t[k] = b[k] = k ? t[k - 1] : 0;
                continue;
            }
            while (n < 4 && sUiCut[radius[k]][n] > edge)
                ++n;
            t[k] = inset[k] + n;
            b[k] = h - inset[k] - n;
        }
        if (!(built && imgFor == 1)) {
            for (int k = 0; k < 4; ++k) {
                for (int r = t[k]; r < t[k + 1]; ++r)
                    img[h - 1 - r] = flat[k];
                for (int r = b[k + 1]; r < b[k]; ++r)
                    img[h - 1 - r] = flat[k];
            }
            for (int r = t[4]; r < b[4]; ++r)
                img[h - 1 - r] = grad[r - inset[4]];
            imgFor = built ? 1 : -1;
        }
        lo = t[0] < -y ? -y : t[0];
        hi = b[0] > H - y ? H - y : b[0];
        if (hi > lo)
            memcpy(sCanvas + (size_t)cx * H + (H - h - y) + (h - hi), img + (h - hi), (size_t)(hi - lo) * sizeof(uint16_t));
    }
}

UI_FN void LlWindow(int x, int y, int w, int h)
{
    LlWindowPlain(x, y, w, h);
    Em2CardShadow(x, y, w, h);
}

/* .rule: the hairline inside a window, a px of #d8e4ec. */
UI_FN void LlRule(int x, int y, int w)
{
    Fill(x, y, w, 1, LL_RULE);
}

/* 3. LlHpBox: FireRed's cream HP box (.hpb), the box as written: a slate outline, the left edge slanted 9 px (its top 9 px in), a cream
 * gradient inside (#fffff0, #f8f8d8 at 60 percent, #e8e8c0). The caller writes the name, the level and the numbers on it. */
UI_FN void LlHpBox(int x, int y, int w, int h)
{
    uint16_t colour[H];

    if (w < 24 || h < 6 || h > H)
        return;
    for (int r = 0; r < h; ++r)
        colour[r] = LL_SLATE;
    LlSlab(x, y, w, h, 9, 0, 0, colour);
    for (int r = 0, ih = h - 2, stop = ih * 60 / 100; r < ih; ++r)
        colour[r] = r < stop ? Em2Lerp(UI_RGB(255, 255, 240), UI_RGB(248, 248, 216), r, stop) : Em2Lerp(UI_RGB(248, 248, 216), UI_RGB(232, 232, 192), r - stop, ih - stop - 1);
    LlSlab(x + 1, y + 1, w - 2, h - 2, 9, 0, 0, colour);
}

/* The HP bar's label, in 4 x 5 letters (FireRed draws its own small "HP" too): H, P, S or K, the others a space. Returns the width. */
UI_FN int LlMiniText(const char *s, int x, int y, uint16_t c)
{
    static const uint8_t glyph[4][5] = {{9, 9, 15, 9, 9}, {14, 9, 14, 8, 8}, {7, 8, 6, 1, 14}, {9, 10, 12, 10, 9}};   /* H P S K, a row a nibble */
    int w = 0;

    for (; *s; ++s, w += 5) {
        int g = *s == 'H' ? 0 : *s == 'P' ? 1 : *s == 'S' ? 2 : *s == 'K' ? 3 : -1;

        for (int row = 0; g >= 0 && row < 5; ++row)
            for (int col = 0; col < 4; ++col)
                if (glyph[g][row] >> (3 - col) & 1)
                    Px(x + w + col, y + row, c);
    }
    return w ? w - 1 : 0;
}

/* LlHpBar: the capsule (.bar), 9 tall and w wide: slate, the orange label (HP in English, PS in Spanish, KP in German) and the track
 * (#586080) whose fill is green over half the HP, yellow over a fifth, red below: a light band over its upper 40 percent. */
UI_FN void LlHpBar(int x, int y, int w, unsigned cur, unsigned max)
{
    int tx = x + 15, tw = w - 17, ty = y + 2, fw;
    uint16_t light, dark;

    if (w < 24)
        return;
    UiRoundFill(x, y, w, 9, 4, LL_SLATE);
    LlMiniText(CTR_TR("HP", "PS", "KP"), x + 3, y + 2, UI_RGB(248, 176, 32));
    UiRoundFill(tx, ty, tw, 5, 2, UI_RGB(88, 96, 128));
    if (!cur || !max)
        return;
    fw = (int)((unsigned long)(cur > max ? max : cur) * (unsigned)tw / max);
    if (fw < 1)
        fw = 1;
    if (cur * 2 > max) {
        light = UI_RGB(168, 248, 184);
        dark = UI_RGB(48, 200, 72);
    } else if (cur * 5 > max) {
        light = UI_RGB(248, 232, 144);
        dark = UI_RGB(232, 184, 32);
    } else {
        light = UI_RGB(248, 168, 160);
        dark = UI_RGB(224, 56, 40);
    }
    /* The track's rounded ends clip the fill: its first and last column lose their top and bottom pixel. */
    for (int col = 0; col < fw; ++col) {
        int end = col == 0 || col == tw - 1;

        Fill(tx + col, ty + end, 1, 2 - end, light);
        Fill(tx + col, ty + 2, 1, 3 - end, dark);
    }
}

/* 4. LlRibbonBox: a slanted ribbon (.rib) of any size: the right edge slanted `slant` px, `corners` rounded (LL_TL / LL_BL), a gradient
 * from `top` to `bottom`, the text white at x + 6 with the bottom colour darkened as its shadow, cut with "..." where it would run
 * into the slant. The mockup's standard ribbon is 13 tall, slant 7, as wide as its text + 18 (6 before, 12 after); a key tab or the
 * "1o" of the party is 11 tall (its width is UiTextWidth of the text + 18). The game's ordinal sign (U+00BA, the party's "1" + ordinal) is a
 * superscript that rises 2 px above the capitals' line, so it reaches 2 px above an 11 px ribbon: the piece owns those 2 px. */
UI_FN void LlRibbonBox(int x, int y, int w, int h, int slant, int corners, uint16_t top, uint16_t bottom, const char *text)
{
    if (w < 8 || h < 6)
        return;
    LlGradSlab(x, y, w, h, 0, slant, corners, top, bottom);
    Em2Text(text, x + 6, y + (h - UI_TEXT_H) / 2, EM_WHITE, Em2Lerp(bottom, UI_RGB(0, 0, 0), 100, 255), x + w - slant - 1);
}

/* The standard ribbon (LL_RIB_TOP / LL_RIB_BOT are the mockup's red) at (x, y); returns its width. */
UI_FN int LlRibbon(int x, int y, const char *text, uint16_t top, uint16_t bottom)
{
    int w = UiTextWidth(text, 1) + 18;

    LlRibbonBox(x, y, w, 13, 7, 0, top, bottom, text);
    return w;
}

/* LlTypeBadge: a rectangular badge (.ty): `colour` with a light first row, a dark outline made from the colour, the word in white with its
 * shadow. 13 tall with its outline (the mockup's box is 11), textWidth + 12 wide; returns the width. */
UI_FN int LlTypeBadge(int x, int y, const char *text, uint16_t colour)
{
    int w = UiTextWidth(text, 1) + 12;
    uint16_t edge = Em2Lerp(colour, UI_RGB(30, 20, 40), 160, 255);

    UiRoundFill(x, y, w, 13, 4, edge);
    UiRoundFill(x + 1, y + 1, w - 2, 11, 3, colour);
    Fill(x + 3, y + 1, w - 6, 1, Em2Lerp(colour, EM_WHITE, 115, 255));
    UiText(text, x + 6, y + 2, EM_WHITE, Em2Lerp(colour, UI_RGB(0, 0, 0), 115, 255), 1, x + w - 1);
    return w;
}

/* A triangle: (x, y, w, h) its box, `dir` where it points (0 up, 1 down, 2 left, 3 right); its width across is even. Each step along it
 * is two px more across than the last (rounded), like the mockup's CSS triangles. */
UI_FN void LlArrow(int x, int y, int w, int h, int dir, uint16_t c)
{
    int across = dir < 2 ? w : h, along = dir < 2 ? h : w;

    for (int j = 0; j < along; ++j) {                       /* j: the distance from the tip */
        int n = 2 * ((across * (j + 1) + along) / (2 * along)), k = dir == 0 || dir == 2 ? j : along - 1 - j;

        if (n < 2)
            n = 2;
        if (n > across)
            n = across;
        if (dir < 2)
            Fill(x + (across - n) / 2, y + k, n, 1, c);
        else
            Fill(x + k, y + (across - n) / 2, 1, n, c);
    }
}

/* LlCursor: FireRed's red triangle (.cur), 6 x 10 at (x, y) pointing right, with the dark red shadow one px to the right and below it
 * (7 x 11 in all); `colour` is LL_CURSOR, or white on a chosen row. */
UI_FN void LlCursor(int x, int y, uint16_t colour)
{
    LlArrow(x + 1, y, 6, 10, 3, LL_CURSOR_SH);
    LlArrow(x, y + 1, 6, 10, 3, LL_CURSOR_SH);
    LlArrow(x, y, 6, 10, 3, colour);
}

/* 5. LlBallTray: the slanted tray of the party's balls (.tray), the box as written (the mockup's is 196 x 25): both ends slanted 9 px
 * (the top shifted right), the #20263f outline, a gradient inside from #56608c to #343c62. The balls, bars and the lead's frame are
 * drawn over it. */
UI_FN void LlBallTray(int x, int y, int w, int h)
{
    uint16_t colour[H];

    if (w < 30 || h < 8 || h > H)
        return;
    for (int r = 0; r < h; ++r)
        colour[r] = LL_SLATE_DK;
    LlSlab(x, y, w, h, 9, 9, 0, colour);
    LlRows(colour, h - 2, UI_RGB(86, 96, 140), UI_RGB(52, 60, 98));
    LlSlab(x + 1, y + 1, w - 2, h - 2, 9, 9, 0, colour);
}

/* LlLeadFrame: the frame of the lead (.tray .on): a rounded box (radius 4) tinted gold over the tray under it (22 percent of
 * #ffc85a) with a ring of #ffd870 inside its edge. Blends over what is there: draw the tray first. (x, y, w, h) is the box (22 x 23). */
UI_FN void LlLeadFrame(int x, int y, int w, int h)
{
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx) {
            int cx = x + dx, cy = y + dy;

            if ((unsigned)cx >= W || (unsigned)cy >= H || !Em2In(dx, dy, w, h, 4))
                continue;
            if (!Em2In(dx - 1, dy - 1, w - 2, h - 2, 3))
                Px(cx, cy, UI_RGB(255, 216, 112));
            else
                Px(cx, cy, Em2Lerp(TsGet(cx, cy), UI_RGB(255, 200, 90), 56, 255));
        }
}

/* An empty slot of the tray: a 14 px disc ringed #20263f with a dark tint (#000014 at 25 percent) over the tray. Blends: draw the tray first. */
UI_FN void LlTraySlot(int x, int y)
{
    for (int dy = 0; dy < 14; ++dy)
        for (int dx = 0; dx < 14; ++dx) {
            int px = 2 * dx + 1 - 14, py = 2 * dy + 1 - 14, d2 = px * px + py * py;
            int cx = x + dx, cy = y + dy;

            if ((unsigned)cx >= W || (unsigned)cy >= H || d2 > 14 * 14)
                continue;
            Px(cx, cy, d2 > 12 * 12 ? LL_SLATE_DK : Em2Lerp(TsGet(cx, cy), UI_RGB(0, 0, 20), 64, 255));
        }
}

/* A ball's mini HP bar (.tray .m), w wide (18) and 4 tall: the #20263f track and a fill (green over half, yellow over a fifth, red
 * below; light over dark) for cur / max. */
UI_FN void LlTrayBar(int x, int y, int w, unsigned cur, unsigned max)
{
    int fw;
    uint16_t c0, c1;

    UiRoundFill(x, y, w, 4, 2, LL_SLATE_DK);
    if (!cur || !max)
        return;
    fw = (int)((unsigned long)(cur > max ? max : cur) * (unsigned)w / max);
    if (fw < 1)
        fw = 1;
    c0 = cur * 2 > max ? UI_RGB(168, 248, 184) : cur * 5 > max ? UI_RGB(248, 232, 144) : UI_RGB(248, 168, 160);
    c1 = cur * 2 > max ? UI_RGB(48, 200, 72) : cur * 5 > max ? UI_RGB(232, 184, 32) : UI_RGB(224, 56, 40);
    for (int col = 0; col < fw; ++col) {
        int end = col == 0 || col == w - 1;

        for (int r = end; r < 4 - end; ++r)
            Px(x + col, y + r, Em2Lerp(c0, c1, r, 3));
    }
}

/* 6. LlSelectRing: the ring of a chosen card (.c.sel): a px of #ffe08a and outside it a px of #b84a10, round the card's outer rectangle
 * (so 2 px beyond it), following its corners (radius 5). The mockup's soft glow would leave the 2 px a piece owns: it is left out. */
UI_FN void LlSelectRing(int x, int y, int w, int h)
{
    for (int k = 1; k <= 2; ++k) {
        uint16_t c = k == 1 ? LL_RING_IN : LL_RING_OUT;
        int ow = w + 2 * k, oh = h + 2 * k, ox = x - k, oy = y - k;

        for (int row = 0; row < oh; ++row) {
            int cut, irow = row - 1, l, r;

            if (row >= 5 && row < oh - 5)                   /* the straight sides: a column each, once */
                continue;
            cut = LlCut(row, oh);
            if (irow < 0 || irow >= oh - 2) {
                Fill(ox + cut, oy + row, ow - 2 * cut, 1, c);
                continue;
            }
            l = 1 + LlCut(irow, oh - 2);
            r = ow - l;
            Fill(ox + cut, oy + row, (l < ow - cut ? l : ow - cut) - cut, 1, c);
            if (r > l)
                Fill(ox + (r > cut ? r : cut), oy + row, ow - cut - (r > cut ? r : cut), 1, c);
        }
        Fill(ox, oy + 5, 1, oh - 10, c);
        Fill(ox + ow - 1, oy + 5, 1, oh - 10, c);
    }
}

/* A piece wears flame when it is chosen, else its own palette: the mockup's rule in one place. */
UI_FN const EmPal *LlPick(int chosen, const EmPal *idle)
{
    return chosen ? &kEmFlame : idle;
}

/* 7. LlBackDisc: the round back arrow (.back): a 22 px disc of a light radial gradient (white to #d8def4, lit from above), a #20263f
 * ring, the slate arrow (#4c5a8c) pointing left. (x, y) is the outer 24 x 24 (the mockup's box is it shrunk by 1). */
UI_FN void LlBackDisc(int x, int y)
{
    static uint16_t kept[22][22];                           /* the gradient never changes: made the first time */
    static int ready;

    Em2Disc(x, y, 24, LL_SLATE_DK);
    for (int row = 0; row < 22; ++row)
        for (int col = 0; col < 22; ++col) {
            int px = 2 * col + 1 - 22, py = 2 * row + 1 - 22;

            if (px * px + py * py > 22 * 22)
                continue;
            if (!ready) {
                /* the gradient's centre is at 50% / 35%: (11, 7.7) in tenths of a pixel; its radius reaches the far corner (18) */
                for (int r = 0; r < 22; ++r)
                    for (int c = 0; c < 22; ++c) {
                        int gx = c * 10 + 5 - 110, gy = r * 10 + 5 - 77;

                        kept[r][c] = Em2Lerp(EM_WHITE, UI_RGB(216, 222, 244), (int)LlIsqrt((unsigned)(gx * gx + gy * gy)) * 255 / 180, 255);
                    }
                ready = 1;
            }
            Px(x + 1 + col, y + 1 + row, kept[row][col]);
        }
    LlArrow(x + 7, y + 7, 7, 10, 2, UI_RGB(76, 90, 140));
}

/* ---- more of the mockup's small pieces, for the screens that use them ---- */

/* LlWell: the sunk light box (.well, the value of a row, a counter, an item's pocket): #a09480 outline, paper from #e2dcc8 to #f8f4e6
 * over 3 rows then #fffdf4, and the light row the mockup throws under it (#fff at 60 percent, over what is there: draw the piece it
 * sinks into first). (x, y, w, h) holds its outline; the light row is y + h. */
UI_FN void LlWell(int x, int y, int w, int h)
{
    UiRoundFill(x, y, w, h, 5, UI_RGB(160, 148, 128));
    Em2Fill3(x + 1, y + 1, w - 2, h - 2, 4, UI_RGB(226, 220, 200), UI_RGB(248, 244, 230), 3, UI_RGB(255, 253, 244));
    if ((unsigned)(y + h) < H)
        for (int cx = x + 2; cx < x + w - 2; ++cx)
            if ((unsigned)cx < W)
                Px(cx, y + h, Em2Lerp(TsGet(cx, y + h), EM_WHITE, 153, 255));
}

/* LlSubPill: the sunk caption under an action card's icon (.sub): a rounded box (radius 4) that darkens the card under it (#000028 at
 * 30 percent) with a darker first row and a light row under it (#fff at 30 percent). Blends: draw the card first. (x, y, w, h) is the box;
 * the light row is y + h. */
UI_FN void LlSubPill(int x, int y, int w, int h)
{
    for (int dy = 0; dy < h; ++dy)
        for (int dx = 0; dx < w; ++dx) {
            int cx = x + dx, cy = y + dy;
            uint16_t c;

            if ((unsigned)cx >= W || (unsigned)cy >= H || !Em2In(dx, dy, w, h, 4))
                continue;
            c = Em2Lerp(TsGet(cx, cy), UI_RGB(0, 0, 40), 77, 255);
            Px(cx, cy, dy == 0 ? Em2Lerp(c, UI_RGB(0, 0, 0), 77, 255) : c);
        }
    if ((unsigned)(y + h) < H)
        for (int cx = x + 2; cx < x + w - 2; ++cx)
            if ((unsigned)cx < W)
                Px(cx, y + h, Em2Lerp(TsGet(cx, y + h), EM_WHITE, 77, 255));
}

/* The colour of the plate's pixel (col, row) when it is inside the disc of diameter d (returns 0 when it is not). */
UI_FN int LlPlateColour(int d, int col, int row, uint16_t *out)
{
    int px = 2 * col + 1 - d, py = 2 * row + 1 - d, below = 2 * row + 3 - d;
    int gx, gy, t;
    uint16_t c;

    if (px * px + py * py > d * d)
        return 0;
    gx = col * 10 + 5 - 5 * d;                              /* the light's centre: 50% across, 35% down; tenths of a pixel */
    gy = row * 10 + 5 - (7 * d + 1) / 2;
    t = (int)LlIsqrt((unsigned)(gx * gx + gy * gy)) * 256 / (82 * d / 10);
    c = t < 179 ? Em2Lerp(UI_RGB(255, 253, 244), UI_RGB(248, 236, 200), t, 179) : Em2Lerp(UI_RGB(248, 236, 200), UI_RGB(232, 212, 156), t - 179, 77);
    if (px * px + below * below > d * d)                    /* the inset shade: what the disc shifted up two px would not cover */
        c = Em2Lerp(c, UI_RGB(120, 90, 40), 46, 255);
    *out = c;
    return 1;
}

/* LlPlate: the round plate a Pokemon sits on (.plate), `d` across at (x, y) (its CSS box): cream with a radial light from above (#fffdf4,
 * #f8ecc8 at 70 percent, #e8d49c), a warm shade along its bottom, and its rings: with bandW 0 one px of `edge` round it (the mockup's
 * #404868); with bandW > 0 a px of `edge`, bandW px of `band`, a px of `edge` (the FIGHT card's #8a1610 / #ffd890 x2, the party's
 * #184a86 / #e8f8ff x1). The rings reach 1 or 2 + bandW px outside the box. */
UI_FN void LlPlate(int x, int y, int d, uint16_t edge, uint16_t band, int bandW)
{
    int e = bandW > 0 ? 2 + bandW : 1;

    Em2Disc(x - e, y - e, d + 2 * e, edge);
    if (bandW > 0) {
        Em2Disc(x - e + 1, y - e + 1, d + 2 * e - 2, band);
        Em2Disc(x - 1, y - 1, d + 2, edge);
    }
    /* The disc's colours depend only on its diameter and a screen draws several of one size, so the last few sizes are kept (in the
     * canvas' order: column by column, bottom up) and a plate is a copy a column. A larger plate is drawn a pixel at a time. */
    enum { KEPT = 3, MAXD = 66 };
    static struct { int d; uint16_t colour[MAXD * MAXD]; } cache[KEPT];
    static unsigned next;
    uint16_t *kept = 0;

    if (d < 1)
        return;
    if (d > MAXD) {
        for (int row = 0; row < d; ++row)
            for (int col = 0; col < d; ++col) {
                uint16_t c;

                if (LlPlateColour(d, col, row, &c))
                    Px(x + col, y + row, c);
            }
        return;
    }
    for (int i = 0; i < KEPT; ++i)
        if (cache[i].d == d)
            kept = cache[i].colour;
    if (!kept) {
        kept = cache[next].colour;
        cache[next].d = d;
        next = (next + 1) % KEPT;
        for (int col = 0; col < d; ++col)
            for (int row = 0; row < d; ++row)
                LlPlateColour(d, col, row, &kept[col * d + (d - 1 - row)]);
    }
    for (int col = 0; col < d; ++col) {
        int dx = 2 * col + 1 - d, s = (int)LlIsqrt((unsigned)(d * d - dx * dx)), lo = (d - s) / 2, hi = (d + s - 1) / 2;

        if (x + col < 0 || x + col >= W)
            continue;
        if (y + lo < 0)
            lo = -y;
        if (y + hi >= H)
            hi = H - 1 - y;
        if (hi >= lo)
            memcpy(sCanvas + (size_t)(x + col) * H + (H - 1 - (size_t)(y + hi)), kept + col * d + (d - 1 - hi), (size_t)(hi - lo + 1) * sizeof(uint16_t));
    }
}

/* The flame face of a small raised piece (.nub, .seg): #8a1610 outline, the glass from #ffc45a over #ff9a38 to #f8642a over #d8301e, a light
 * row (#ffe9a8) along the top; pressed it is the lower band only. (x, y, w, h) holds the outline. */
UI_FN void LlFlameFace(int x, int y, int w, int h, int radius, int pressed)
{
    UiRoundFill(x, y, w, h, radius, kEmFlame.lip);
    if (pressed) {
        EmGradFill(x + 1, y + 2, w - 2, h - 3, radius - 1, kEmFlame.f1, kEmFlame.bottom);
        return;
    }
    Em2Face(x + 1, y + 1, w - 2, h - 2, radius - 1, kEmFlame.g1, kEmFlame.top, kEmFlame.f1, kEmFlame.bottom);
    Fill(x + 3, y + 1, w - 6, 1, kEmFlame.hl);
}

/* LlNub: a value box's arrow button (.nub): the flame face (15 x 14 with its outline in the mockup) with a white arrow (4 x 6) pointing
 * `dir` (-1 left, 1 right); pressed it sinks a pixel. */
UI_FN void LlNub(int x, int y, int w, int h, int dir, int pressed)
{
    LlFlameFace(x, y, w, h, 3, pressed);
    LlArrow(x + (w - 4) / 2 + (dir > 0), y + (h - 6) / 2 + pressed, 4, 6, dir < 0 ? 2 : 3, EM_WHITE);
}

/* LlSegments: the segmented control (.seg) in the well whose inside is (x, y, w, h): `count` equal places, the chosen one a flame face
 * with white ink (#7a1410 shadow), the others the dim ink; the one under a held touch (hot) a warm tint. A hairline between the others. */
UI_FN void LlSegments(int x, int y, int w, int h, const char *const *names, int count, int chosen, int hot)
{
    for (int k = 0; k < count; ++k) {
        int sx = x + k * w / count, sw = x + (k + 1) * w / count - sx;
        int tx = sx + (sw - UiTextWidth(names[k], 1)) / 2, ty = y + (h - UI_TEXT_H) / 2;

        if (k == chosen) {
            LlFlameFace(sx, y, sw, h, 3, 0);
            Em2Text(names[k], tx < sx + 1 ? sx + 1 : tx, ty, EM_WHITE, kEmFlame.inkSh, sx + sw - 1);
            continue;
        }
        if (hot == k)
            UiRoundFill(sx + 1, y + 1, sw - 2, h - 2, 3, UI_RGB(255, 214, 160));
        if (k > 0 && k - 1 != chosen)
            Fill(sx, y + 2, 1, h - 4, UI_RGB(220, 202, 176));
        Em2Text(names[k], tx < sx + 1 ? sx + 1 : tx, ty, EM_DIM, EM_DARK_SH, sx + sw - 1);
    }
}

/* LlScrollRail: the list's rail with its thumb (the bag's): a #14182c outline round a dark gradient (#20263f to #343c62 across), and the
 * thumb, a flame capsule (#ffc45a to #f8642a across, #8a1610 outline) that starts `thumbY` rows down and is `thumbH` tall, inset a px at
 * each side. (x, y, w, h) holds the rail (10 x 68 with its outline in the mockup). */
UI_FN void LlScrollRail(int x, int y, int w, int h, int thumbY, int thumbH)
{
    if (w < 6 || h < 12)
        return;
    for (int part = 0; part < 2; ++part) {
        int px = x + (part ? 1 : 0), py = y + (part ? thumbY : 0), pw = w - (part ? 2 : 0), ph = part ? thumbH : h;
        int r = pw / 2 < 5 ? pw / 2 : 5;
        uint16_t edge = part ? kEmFlame.lip : UI_RGB(20, 24, 44);
        uint16_t grad[32];                  /* the gradient across: one colour a column, not three divisions a pixel */
        const uint16_t c0 = part ? UI_RGB(255, 196, 90) : UI_RGB(32, 38, 63), c1 = part ? UI_RGB(248, 100, 42) : UI_RGB(52, 60, 98);

        if (part && thumbH < 6)
            break;
        for (int dx = 0; dx < pw && dx < 32; ++dx)
            grad[dx] = Em2Lerp(c0, c1, dx - 1, pw - 3);
        for (int dy = 0; dy < ph; ++dy)
            for (int dx = 0; dx < pw; ++dx) {
                if (!Em2In(dx, dy, pw, ph, r))
                    continue;
                if (!Em2In(dx - 1, dy - 1, pw - 2, ph - 2, r > 0 ? r - 1 : 0))
                    Px(px + dx, py + dy, edge);
                else
                    Px(px + dx, py + dy, dx < 32 ? grad[dx] : Em2Lerp(c0, c1, dx - 1, pw - 3));
            }
    }
}
