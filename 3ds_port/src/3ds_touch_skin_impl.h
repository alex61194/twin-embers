/* The PokeTouch screen's artwork (poketouch_skin.h, made by tools/make_touch_skin.py): the pictures are drawn as
 * painted; the game's data goes on them as lettering in the artwork's own style. Included by 3ds_poketouch.c after
 * its Px / Fill. */
#include "poketouch_skin.h"

enum { TS_NORMAL, TS_BRIGHT, TS_DIM, TS_DARK };

static uint16_t TsGet(int x, int y)
{
    return sCanvas[(size_t)x * H + (H - 1 - (size_t)y)];
}

static void TsBgRect(int x, int y, int w, int h)
{
    for (int cy = y < 0 ? 0 : y; cy < y + h && cy < H; ++cy)
        for (int cx = x < 0 ? 0 : x; cx < x + w && cx < W; ++cx)
            Px(cx, cy, TS_BG(cx, cy));
}

static void TsBackdrop(void)
{
    TsBgRect(0, 0, W, H);
}

static uint16_t TsTone(uint16_t c, int mode)
{
    unsigned r = c >> 11, g = (c >> 5) & 63, b = c & 31;

    switch (mode) {
    case TS_BRIGHT:
        r = r * 5 / 4 + 3; g = g * 5 / 4 + 6; b = b * 5 / 4 + 3;
        return (uint16_t)((r > 31 ? 31 : r) << 11 | (g > 63 ? 63 : g) << 5 | (b > 31 ? 31 : b));
    case TS_DIM: {                                            /* desaturated and darkened: a spent button */
        unsigned l = (r * 2 * 77 + g * 150 + b * 2 * 29) >> 8;      /* 0..63 */

        l = (l * 3 + (r * 2 + b * 2 + g) / 3 * 2) / 8;
        return (uint16_t)((l >> 1) << 11 | l << 5 | (l >> 1));
    }
    case TS_DARK:
        return (uint16_t)((r * 5 / 8) << 11 | (g * 5 / 8) << 5 | (b * 5 / 8));
    default:
        return c;
    }
}

static void TsPicDraw(const TsPic *p, int mode)
{
    for (int v = 0; v < p->h; ++v)
        for (int u = 0; u < p->w; ++u) {
            uint16_t c = p->px[v * p->w + u];

            if (c != TS_KEY)
                Px(p->x + u, p->y + v, TsTone(c, mode));
        }
}

static CtrPokeTouchRect TsRect(const TsPic *p)
{
    CtrPokeTouchRect r = {p->x, p->y, p->w, p->h};
    return r;
}

static void TsBlend(int x, int y, const uint8_t col[3], int a)
{
    uint16_t c;
    int r, g, b;

    if ((unsigned)x >= W || (unsigned)y >= H || a <= 0)
        return;
    c = TsGet(x, y);
    r = (c >> 11) << 3; g = ((c >> 5) & 63) << 2; b = (c & 31) << 3;
    r += (col[0] - r) * a / 15; g += (col[1] - g) * a / 15; b += (col[2] - b) * a / 15;
    Px(x, y, RGB(r < 0 ? 0 : r > 255 ? 255 : r, g < 0 ? 0 : g > 255 ? 255 : g, b < 0 ? 0 : b > 255 ? 255 : b));
}

/* A lettering glyph of the string: its index in the tables, advancing `s`; -1 for a space or what is not drawn. */
static int TsGlyphOf(const char **s, int ten)
{
    const char *chars = ten ? kTsTenChars : kTsNineChars;
    const uint16_t *latin = ten ? kTsTenLatin : kTsNineLatin;
    int nChars = (int)strlen(chars), nLatin = ten ? (int)(sizeof(kTsTenLatin) / sizeof(kTsTenLatin[0]))
                                                  : (int)(sizeof(kTsNineLatin) / sizeof(kTsNineLatin[0]));
    unsigned char c = (unsigned char)**s;
    unsigned code = c;
    const char *p;

    ++*s;
    if (c == 0xc3 && (unsigned char)**s == 0xa9) { ++*s; code = 0xe9; }
    else if (c == 0xc2 && (unsigned char)**s == 0xa5) { ++*s; code = 0xa5; }
    if (code < 128) {
        p = code ? strchr(chars, (int)code) : NULL;
        return p ? (int)(p - chars) : -1;
    }
    for (int i = 0; i < nLatin; ++i)
        if (latin[i] == code)
            return nChars + i;
    return -1;
}

static int TsLetterW(int g, int ten)
{
    return g < 0 ? 3 : ten ? kTsTenWidth[g] : kTsNineWidth[g];
}

static int TsLettersWidth(const char *s, int ten)
{
    int w = 0;

    while (*s) {
        w += TsLetterW(TsGlyphOf(&s, ten), ten) + 1;
    }
    return w ? w - 1 : 0;
}

/* The string in the artwork's lettering, white with its dark outline, from `x`; what would pass `limit` is left out. */
static void TsLetters(const char *s, int x, int y, int ten, const uint8_t ink[3], int limit)
{
    static const uint8_t outline[3] = {8, 12, 20};
    int rows = ten ? 10 : 9;

    for (int pass = 0; pass < 2; ++pass) {
        const char *t = s;
        int px = x;

        while (*t) {
            int g = TsGlyphOf(&t, ten), w = TsLetterW(g, ten);

            if (px + w > limit)
                break;
            for (int r = 0; g >= 0 && r < rows; ++r)
                for (int c = 0; c < w; ++c) {
                    int cov = ten ? kTsTenRows[g][r][c] : kTsNineRows[g][r][c];

                    if (!cov)
                        continue;
                    if (pass == 0) {
                        for (int dy = -1; dy <= 1; ++dy)
                            for (int dx = -1; dx <= 1; ++dx)
                                TsBlend(px + c + dx, y + r + dy, outline, 15);
                    } else {
                        TsBlend(px + c, y + r, ink, cov);
                    }
                }
            px += w + 1;
        }
    }
}

static const uint8_t kTsWhite[3] = {248, 252, 248};

static void TsMaskDraw(const TsMask *m, int dx, int dy)
{
    static const uint8_t outline[3] = {4, 16, 12};

    for (int pass = 0; pass < 2; ++pass)
        for (int v = 0; v < m->h; ++v)
            for (int u = 0; u < m->w; ++u) {
                int cov = m->cover[v * m->w + u];

                if (!cov)
                    continue;
                if (pass == 0) {
                    for (int oy = -1; oy <= 1; ++oy)
                        for (int ox = -1; ox <= 1; ++ox)
                            TsBlend(m->x + dx + u + ox, m->y + dy + v + oy, outline, 10);
                } else {
                    TsBlend(m->x + dx + u, m->y + dy + v, kTsWhite, cov);
                }
            }
}
