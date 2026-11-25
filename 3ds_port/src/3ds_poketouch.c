/* PokeTouch: the bottom screen, drawn and hit-tested.
 *
 * Layout (320x240, see 3ds_poketouch.h): the frame with the 240x160 viewport
 * (the GBA picture is drawn there 1:1 by the GPU, 3ds_video.c), the
 * contextual bar under it, the side column POKeDEX / POKeMON / BAG / the
 * player's name (Trainer Card) / SAVE / OPTIONS and under it the X and Y
 * shortcuts and the Running Shoes (the running mode), icons only. With no
 * menu open the viewport is HOME: FireRed's Region Map art, drawn passively
 * with the player on it (no Map button: the map is always there).
 *
 * Look: the Llama mockup's (3ds_ember2_impl.h): FireRed's text window for the note, the map's frame, the OPTIONS card and the SAVE card;
 * cream pieces that wear flame (with a ring) when chosen; slate for BACK; the region's red ribbon on the map.
 *
 * Art: FireRed's own item icons for the shortcuts (read from the game, so
 * they show whatever is configured), FireRed's latin_normal font for the
 * bar, a 5x7 bitmap font for buttons, and pack-backed item icons for
 * six menu entries (original shoe fallback). Drawn only when the view
 * changes.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "3ds_poketouch.h"
#include "3ds_bottom.h"
#include "3ds_data.h"
#include "poketouch_assets.h"
#include "twin_font.h"
#include "3ds_ui_theme.h"
#include "3ds_lang.h"
#include "battle_flat_icons.h"

/* Game bridge (src/ctr_poketouch.c): an item's 24x24 icon decompressed from
 * FireRed's own icon table. */
extern int CtrPokeTouch_ItemIcon(unsigned item, unsigned char *tiles, unsigned short *palette);

#define W CTR_BOTTOM_WIDTH
#define H CTR_BOTTOM_HEIGHT
#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
/* A GBA colour (BGR555) as the canvas's RGB565, green's low bit clear: FireRed's palettes as its icons and pictures
 * are drawn here. */
static inline uint16_t GbaColor(uint16_t c)
{
    return RGB((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
}


/* Native menu icons: the six themed item graphics already reconstructed from
 * the user's FireRed ROM into twinembers.pak. No game pixels or palettes are
 * compiled into the port. Running Shoes keeps the port-authored shoe symbol:
 * the supported ROM has no corresponding item-icon entry. */
static const char *const sMenuItemArt[6] = {
    "fame_checker", "poke_ball", "berry_pouch",
    "card_key", "coin_case", "teachy_tv"
};

/* Bounded GBA LZ77 (0x10) decoder for the original item 4bpp/palette files.
 * Input is already size/CRC-verified by CtrData_Load. Still validate every
 * token and backreference before writing into the small local buffer. */
static int PokeTouch_Lz77(const uint8_t *src, uint32_t len, uint8_t *out, uint32_t cap)
{
    uint32_t used = 4, written = 0, size;
    if (!src || len < 4 || src[0] != 0x10)
        return 0;
    size = (uint32_t)src[1] | ((uint32_t)src[2] << 8) | ((uint32_t)src[3] << 16);
    if (!size || size > cap)
        return 0;
    while (written < size) {
        uint8_t flags;
        if (used >= len)
            return 0;
        flags = src[used++];
        for (int bit = 7; bit >= 0 && written < size; --bit) {
            if (flags & (1u << bit)) {
                uint8_t a, b;
                uint32_t count, back;
                if (len - used < 2)
                    return 0;
                a = src[used++];
                b = src[used++];
                count = (a >> 4) + 3u;
                back = (((uint32_t)a & 15u) << 8 | b) + 1u;
                if (back > written || count > size - written)
                    return 0;
                while (count--) {
                    out[written] = out[written - back];
                    ++written;
                }
            } else {
                if (used >= len)
                    return 0;
                out[written++] = src[used++];
            }
        }
    }
    return (int)size;
}

static int PokeTouch_LoadLz(const char *path, uint8_t *out, uint32_t cap)
{
    uint32_t len = 0;
    uint8_t *packed = CtrData_Load(path, &len);
    int unpacked = 0;
    if (packed) {
        if (len > 0 && len <= 4096)
            unpacked = PokeTouch_Lz77(packed, len, out, cap);
        free(packed);
    }
    return unpacked;
}

static int CtrPokeTouch_PackMenuIcon(unsigned icon, uint8_t pixels[12 * 12], uint16_t pal[16])
{
    char path[96];
    uint8_t tiles[512], colors[32], scaled[12 * 12];
    uint16_t converted[16];
    int pitch, n, visible = 0;
    if (icon >= 6)
        return 0;

    n = snprintf(path, sizeof(path), "graphics/items/icons/%s.4bpp.lz", sMenuItemArt[icon]);
    if (n < 0 || (size_t)n >= sizeof(path))
        return 0;
    n = PokeTouch_LoadLz(path, tiles, sizeof(tiles));
    /* FireRed icons are 24x24 (3x3 tiles), with some packed into 32x32.
     * Decode the upper 24x24 exactly in GBA 4bpp tile order. */
    if (n != 9 * 32 && n != 16 * 32)
        return 0;
    pitch = n == 9 * 32 ? 3 : 4;

    n = snprintf(path, sizeof(path), "graphics/items/icon_palettes/%s.gbapal.lz", sMenuItemArt[icon]);
    if (n < 0 || (size_t)n >= sizeof(path) ||
        PokeTouch_LoadLz(path, colors, sizeof(colors)) != 32)
        return 0;
    converted[0] = 0;
    for (int i = 1; i < 16; ++i) {
        uint16_t c = (uint16_t)colors[i * 2] | (uint16_t)(colors[i * 2 + 1] << 8);
        /* Replicate the high green bit into RGB565's sixth bit (white stays
         * white); transparency is an index, never a colour-key test. */
        unsigned g = (c >> 5) & 31;
        converted[i] = (uint16_t)((c & 31) << 11 | (g << 1 | g >> 4) << 5 | (c >> 10 & 31));
    }
    for (int y = 0; y < 12; ++y)
        for (int x = 0; x < 12; ++x) {
            /* A fixed 2x2 coverage sample preserves one-pixel outlines that
             * top-left-only sampling could discard. Prefer the first opaque
             * pixel, with no averaging, blur or invented palette colours. */
            uint8_t v = 0;
            for (int oy = 0; oy < 2 && !v; ++oy)
                for (int ox = 0; ox < 2 && !v; ++ox) {
                    int sx = x * 2 + ox, sy = y * 2 + oy;
                    int tile = (sy / 8 * pitch + sx / 8) * 32 + sy % 8 * 4 + sx % 8 / 2;
                    uint8_t packed = tiles[tile];
                    v = (sx & 1) ? packed >> 4 : packed & 15;
                }
            scaled[y * 12 + x] = v;
            visible |= v != 0;
        }
    if (!visible)
        return 0;
    memcpy(pixels, scaled, sizeof(scaled));
    memcpy(pal, converted, sizeof(converted));
    return 1;
}

/* The START column on the right of the GBA picture's frame (the picture ends at x = 244, its frame at 247): six raised
 * buttons, the X and Y shortcut tiles and RUN with its switch. */
#define CLASSIC_RECTS { \
    [PT_POKEDEX]    = {250,   4, 68, 21}, \
    [PT_POKEMON]    = {250,  29, 68, 21}, \
    [PT_BAG]        = {250,  54, 68, 21}, \
    [PT_CARD]       = {250,  79, 68, 21}, \
    [PT_SAVE]       = {250, 104, 68, 21}, \
    [PT_OPTIONS]    = {250, 129, 68, 21}, \
    [PT_REGISTERED] = {250, 158, 32, 40}, \
    [PT_BIKE]       = {286, 158, 32, 40}, \
    [PT_RUN]        = {250, 204, 68, 30}, \
}
static const CtrPokeTouchRect sRectsClassic[PT_COUNT] = CLASSIC_RECTS;
/* HOME's layout (the map in the middle, PT_HOME_X/Y): the START column split in the two side bands (Pokedex, Pokemon, Bag and the
 * Trainer Card on the left, Save, Options, X and Y on the right), RUN in the top band. 35 wide, so a button's ring and shadow stay off
 * the map's frame; 36 tall, 40 apart. */
static const CtrPokeTouchRect sRectsHome[PT_COUNT] = {
    [PT_POKEDEX]    = {  0,  40, 35, 36},
    [PT_POKEMON]    = {  0,  80, 35, 36},
    [PT_BAG]        = {  0, 120, 35, 36},
    [PT_CARD]       = {  0, 160, 35, 36},
    [PT_SAVE]       = {285,  40, 35, 36},
    [PT_OPTIONS]    = {285,  80, 35, 36},
    [PT_REGISTERED] = {285, 120, 35, 36},
    [PT_BIKE]       = {285, 160, 35, 36},
    [PT_RUN]        = {245,   2, 71, 33},
};
CtrPokeTouchRect gCtrPokeTouchRects[PT_COUNT] = CLASSIC_RECTS;
static int sLayoutHome;                 /* HOME's layout is the one on the screen (and in gCtrPokeTouchRects) */

static void SetLayout(int home)
{
    sLayoutHome = home != 0;
    memcpy(gCtrPokeTouchRects, home ? sRectsHome : sRectsClassic, sizeof(gCtrPokeTouchRects));
}


/* Areas that are not buttons: the picture's frame, and under it the bar: the note card on the left and, on its
 * right, BACK (and PAGE over it) or SAVE's buttons. */
static const CtrPokeTouchRect sMenuFrame = {0, 0, 248, 168};
static const CtrPokeTouchRect sBar = {0, 168, 248, 72};
const CtrPokeTouchRect gCtrPokeTouchBack = {172, 202, 72, 34};
const CtrPokeTouchRect gCtrPokeTouchOptPage = {172, 172, 72, 26};
#define NOTE_X 4
#define NOTE_Y 172
#define NOTE_H 64
#define NOTE_W_FULL 240
#define NOTE_W_SIDE 164

/* The palette and components are the lower screen's design system (3ds_ui_theme.h / .inc). The only colour of
 * its own is the empty viewport's. */
#define C_VIEW_EMPTY UI_PANEL_LO

static uint16_t *sCanvas;

static void Px(int x, int y, uint16_t c)
{
    if ((unsigned)x < W && (unsigned)y < H)
        sCanvas[(size_t)x * H + (H - 1 - (size_t)y)] = c;
}

/* Two pixels a store where the column allows it: the ARM11 writes a word as fast as a halfword. */
typedef uint32_t __attribute__((may_alias)) PtPair;

static void Fill(int x, int y, int w, int h, uint16_t c)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    uint32_t pair = (uint32_t)c * 0x10001u;

    for (int cx = x0; cx < x1; ++cx) {
        uint16_t *p = sCanvas + (size_t)cx * H + (H - (size_t)y1);
        int n = y1 - y0;

        if (n > 0 && ((uintptr_t)p & 2)) {
            *p++ = c;
            --n;
        }
        for (; n >= 2; n -= 2, p += 2)
            *(PtPair *)p = pair;
        if (n > 0)
            *p = c;
    }
}

/* FireRed's charmap for the two-byte UTF-8 Latin letters its European
 * releases use (U+00A1..U+00FF and Œ/œ): accents, Ñ/ñ, ¿, ¡, º, ª, ¥. */
static unsigned LatinCode(unsigned char lead, unsigned char next)
{
    static const uint8_t sC3[64] = {
        /* U+00C0 À Á Â Ã Ä Å Æ Ç */ 0x01, 0x02, 0x03, 0, 0xf1, 0, 0, 0x04,
        /* U+00C8 È É Ê Ë Ì Í Î Ï */ 0x05, 0x06, 0x07, 0x08, 0x09, 0x5a, 0x0b, 0x0c,
        /* U+00D0 Ð Ñ Ò Ó Ô Õ Ö × */ 0, 0x14, 0x0d, 0x0e, 0x0f, 0, 0xf2, 0,
        /* U+00D8 Ø Ù Ú Û Ü Ý Þ ß */ 0, 0x11, 0x12, 0x13, 0xf3, 0, 0, 0x15,
        /* U+00E0 à á â ã ä å æ ç */ 0x16, 0x17, 0, 0, 0xf4, 0, 0, 0x19,
        /* U+00E8 è é ê ë ì í î ï */ 0x1a, 0x1b, 0x1c, 0x1d, 0x1e, 0x6f, 0x20, 0x21,
        /* U+00F0 ð ñ ò ó ô õ ö ÷ */ 0, 0x29, 0x22, 0x23, 0x24, 0, 0xf5, 0,
        /* U+00F8 ø ù ú û ü ý þ ÿ */ 0, 0x26, 0x27, 0x28, 0xf6, 0, 0, 0,
    };

    if (lead == 0xc3) return next >= 0x80 ? sC3[next - 0x80] : 0;
    if (lead == 0xc5) return next == 0x92 ? 0x10 : next == 0x93 ? 0x25 : 0;   /* Œ œ */
    switch (next) {   /* lead 0xc2 */
        case 0xa1: return 0x52;   /* ¡ */
        case 0xa5: return 0xb7;   /* ¥ */
        case 0xaa: return 0x2b;   /* ª */
        case 0xba: return 0x2a;   /* º */
        case 0xbf: return 0x51;   /* ¿ */
        default: return 0;
    }
}

/* The character code of the next UTF-8 character of `*s` in FireRed's charmap (0 for what the font does not draw). */
static unsigned GlyphCode(const char **s)
{
    unsigned char c = (unsigned char)**s;
    unsigned code = 0;

    ++*s;
    if (c >= 'A' && c <= 'Z') code = 0xbb + c - 'A';
    else if (c >= 'a' && c <= 'z') code = 0xd5 + c - 'a';
    else if (c >= '0' && c <= '9') code = 0xa1 + c - '0';
    else if ((c == 0xc2 || c == 0xc3 || c == 0xc5) && (**s & 0xc0) == 0x80)
        code = LatinCode(c, (unsigned char)*(*s)++);
    else if (c == 0x01 && **s) code = (unsigned char)*(*s)++;   /* a FireRed code as it is (CtrPokeTouch name text) */
    else if (c == 0x02 && **s) code = 0x100u | (unsigned char)*(*s)++;   /* a Japanese one (a Japanese Pokemon's name) */
    else switch (c) {
        case '!': code=0xab; break; case '?': code=0xac; break;
        case '.': code=0xad; break; case '-': code=0xae; break;
        case ',': code=0xb8; break; case '/': code=0xba; break;
        case ':': code=0xf0; break; case '(': code=0x5c; break;
        case ')': code=0x5d; break; case '%': code=0x5b; break;
        case '=': code=0x35; break; case ';': code=0x36; break;   /* BUTTON MODE's "L=A", the NUZLOCKE note */
        case '\'': code=0xb4; break; case '"': code=0xb2; break;   /* FireRed's closing quotes: KING'S ROCK, 2'00" */
        default: break;
    }
    return code;
}

/* FireRed's latin_normal glyphs, text and shadow pixels apart. Capitals
 * cover glyph rows 3-11, so a line drawn at `top` has its capitals there. */
static const PtGlyph *Glyph(const char **s)
{
    unsigned code;

    TwinIconsPrepare();
    code = GlyphCode(s);
    if (!sPtFontReady[code]) {
        static uint8_t jp[64];
        const uint8_t *src = code & 0x100u ? CtrTwinJapaneseGlyph(code & 0xffu, jp) : CtrTwinFontData() + code * 64;
        PtGlyph *g = &sPtFont[code];
        g->width = code & 0x100u ? CtrTwinJapaneseFontWidth(code & 0xffu) : CtrTwinFontWidth(code);
        if (g->width > 16) g->width = 16;
        TwinGlyphDecode(src, g->width, g->rows);
        sPtFontReady[code] = true;
    }
    return &sPtFont[code];
}

/* Images: palette indices, 0 clear; disabled art is a light grey. */
static uint16_t Tone(uint16_t c, int on)
{
    if (on)
        return c;
    unsigned r = c >> 11, g = (c >> 5) & 63, b = c & 31;
    unsigned luma = (r * 2 * 77 + g * 150 + b * 2 * 29) >> 8; /* 0..63 */
    luma = 34 + luma * 3 / 8;
    return (uint16_t)((luma >> 1) << 11 | luma << 5 | (luma >> 1));
}

static void Art(const uint8_t *pixels, const uint16_t *pal, int stride, int x0, int y0, int w, int h,
                int x, int y, int on)
{
    int px0 = x < 0 ? -x : 0, px1 = x + w > W ? W - x : w, py0 = y < 0 ? -y : 0, py1 = y + h > H ? H - y : h;

    for (int px = px0; px < px1; ++px) {                       /* a column at a time, the canvas' order, clipped once */
        uint16_t *p = sCanvas + (size_t)(x + px) * H + (H - 1 - (size_t)(y + py0));

        for (int py = py0; py < py1; ++py, --p) {
            uint8_t v = pixels[(y0 + py) * stride + x0 + px];
            if (v)
                *p = Tone(pal[v], on);
        }
    }
}

#include "3ds_ui_theme_impl.h"

/* FireRed's small font (8 wide cells): the labels under HOME's buttons, the plates' captions. Without it, the normal font. */
static int SmallWidth(const char *s)
{
    int w = 0;

    if (!CtrTwinSmallFontData())
        return UiTextWidth(s, 1);
    while (*s) {
        unsigned code = GlyphCode(&s) & 0xffu, cw = CtrTwinSmallFontWidth(code);

        w += (int)(cw > 8 ? 8 : cw);
    }
    return w;
}

/* FireRed's small font has the accented letters as blank cells: the base letter and a mark above it (acute, grave, diaeresis, tilde)
 * stand in for them. Returns the base code, the mark in *mark (0 none, 1 acute, 2 grave, 3 diaeresis, 4 tilde). */
static unsigned SmallBase(unsigned code, int *mark)
{
    static const struct { uint8_t code, base, mark; } kMap[] = {
        {0x17, 0xd5, 1}, {0x1b, 0xd9, 1}, {0x6f, 0xde, 1}, {0x23, 0xe3, 1}, {0x27, 0xe6, 1},
        {0x16, 0xd5, 2}, {0x1a, 0xd9, 2}, {0x1e, 0xde, 2}, {0x22, 0xe3, 2}, {0x26, 0xe6, 2},
        {0xf4, 0xd5, 3}, {0x1d, 0xd9, 3}, {0x21, 0xde, 3}, {0xf5, 0xe3, 3}, {0xf6, 0xe6, 3},
        {0x02, 0xbb, 1}, {0x06, 0xbf, 1}, {0x5a, 0xc3, 1}, {0x0e, 0xc9, 1}, {0x12, 0xcf, 1},
        {0x01, 0xbb, 2}, {0x05, 0xbf, 2}, {0x09, 0xc3, 2}, {0x0d, 0xc9, 2}, {0x11, 0xcf, 2},
        {0xf1, 0xbb, 3}, {0x08, 0xbf, 3}, {0x0c, 0xc3, 3}, {0xf2, 0xc9, 3}, {0xf3, 0xcf, 3},
        {0x29, 0xe2, 4}, {0x14, 0xc8, 4},
    };

    *mark = 0;
    for (unsigned i = 0; i < sizeof(kMap) / sizeof(kMap[0]); ++i)
        if (kMap[i].code == code) {
            *mark = kMap[i].mark;
            return kMap[i].base;
        }
    return code;
}

static void SmallText(const char *s, int x, int cellTop, uint16_t fg, uint16_t shadow, int limit)
{
    const uint8_t *font = CtrTwinSmallFontData();

    if (!font) {
        UiText(s, x, cellTop + 3, fg, shadow, 1, limit);
        return;
    }
    while (*s) {
        unsigned code = GlyphCode(&s) & 0xffu, w = CtrTwinSmallFontWidth(code);
        uint32_t rows[16];

        if (w > 8)
            w = 8;
        if (x + (int)w > limit)
            break;
        TwinSmallGlyphDecode(font + code * 32, w, rows);
        {
            int blank = 1, mark = 0, top = 16;
            unsigned base = SmallBase(code, &mark);

            for (int row = 0; row < 16; ++row)
                if (rows[row])
                    blank = 0;
            if (mark && blank) {                       /* a blank cell: the base letter, then its mark above */
                uint32_t b[16];

                TwinSmallGlyphDecode(font + base * 32, w, b);
                memcpy(rows, b, sizeof(rows));
                for (int row = 15; row >= 0; --row)
                    if (b[row])
                        top = row;
            } else {
                mark = 0;
            }
            for (int row = 0; row < 16; ++row)
                for (unsigned col = 0; col < w; ++col) {
                    unsigned px = (rows[row] >> (2 * col)) & 3;

                    if (px)
                        Px(x + (int)col, cellTop + row, px == 1 ? fg : shadow);
                }
            if (mark && top >= 2) {
                int c = ((int)w - 1) / 2, y0 = cellTop + top - 2, y1 = cellTop + top - 1;

                if (mark == 1) { Px(x + c + 1, y0, fg); Px(x + c, y1, fg); }
                else if (mark == 2) { Px(x + c, y0, fg); Px(x + c + 1, y1, fg); }
                else if (mark == 3) { Px(x + c - 1, y0, fg); Px(x + c + 1, y0, fg); }
                else { Px(x + c - 1, y1, fg); Px(x + c, y0, fg); Px(x + c + 1, y1, fg); }
            }
        }
        x += (int)w;
    }
}
#include "3ds_icons.h"
/* The skin keeps the Ascuas pieces (EmWell, EmKeyTab, EmSwitch, EmBezel) for the screens not converted to Llama; this one draws its own. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-function"
#include "3ds_touch_skin_impl.h"
#pragma GCC diagnostic pop

#define BTN_DEPTH 3                     /* the lip under a big button (BACK, PAGE, SAVE's) */
#define COL_DEPTH 2                     /* and under the column's and the rows' */

/* A pill: `rows` (h colours) in a shape w wide and h tall whose two ends are half discs (w >= h). One span a row. */
static void LhPill(int x, int y, int w, int h, const uint16_t *rows)
{
    for (int row = 0; row < h; ++row) {
        int dy = 2 * row + 1 - h, left = h;

        for (int col = 0; col < h; ++col) {
            int dx = 2 * col + 1 - h;

            if (dx * dx + dy * dy <= h * h) {
                left = col;
                break;
            }
        }
        if (left < h)
            Fill(x + left, y + row, w - 2 * left, 1, rows[row]);
    }
}

/* The mockup's switch (.sw): 30 x 15 with its outline (its box is 28 x 13), a slate outline, a glass track (green when on, grey when off) and
 * a 15 px knob, at the right when on and at the left when off. The knob is lit from its upper left (white, going to #d6d8e2 at 11 px). */
static void LhSwitch(int x, int y, int on)
{
    uint16_t rows[15];
    int kx = on ? x + 15 : x;

    for (int r = 0; r < 15; ++r)
        rows[r] = LL_SLATE;
    LhPill(x, y, 30, 15, rows);
    for (int r = 0; r < 13; ++r)
        rows[r] = on ? (r < 6 ? Em2Lerp(UI_RGB(166, 238, 174), UI_RGB(92, 200, 116), r, 5) : Em2Lerp(UI_RGB(60, 176, 92), UI_RGB(42, 148, 74), r - 6, 6))
                     : Em2Lerp(UI_RGB(216, 212, 220), UI_RGB(170, 166, 178), r, 12);
    LhPill(x + 1, y + 1, 28, 13, rows);
    Em2Disc(kx, y, 15, LL_SLATE);
    for (int row = 0; row < 13; ++row)
        for (int col = 0; col < 13; ++col) {
            int px = 2 * col + 1 - 13, py = 2 * row + 1 - 13, gx = col * 100 + 50 - 585, gy = row * 100 + 50 - 455, d;

            if (px * px + py * py > 13 * 13)
                continue;
            d = (int)LlIsqrt((unsigned)(gx * gx + gy * gy));
            Px(kx + 1 + col, y + 1 + row, d <= 385 ? EM_WHITE : Em2Lerp(EM_WHITE, UI_RGB(214, 216, 226), d - 385, 715));
        }
}

/* The value of an OPTIONS row (the mockup's .well with .nub on both sides): "<" value ">" in a sunk well w x h, the value in the game's red; the
 * nub under a held touch (zone 1 left, 2 right) sinks. */
#define LH_VAL UI_RGB(224, 56, 40)
#define LH_VALSH UI_RGB(248, 200, 184)
static void LhValue(int x, int y, int w, int h, const char *value, int zone)
{
    int tw = UiTextWidth(value, 1);

    LlWell(x, y, w, h);
    LlNub(x + 1, y + 1, 15, h - 2, -1, zone == 1);
    LlNub(x + w - 16, y + 1, 15, h - 2, 1, zone == 2);
    if (tw > w - 36)
        Em2Text(value, x + 18, y + 3, LH_VAL, LH_VALSH, x + w - 18);
    else
        UiText(value, x + (w - tw) / 2, y + 3, LH_VAL, LH_VALSH, 1, x + w - 16);
}

/* The frame round the picture, the HOME map and the GBA picture alike (x, y, w, h the picture): FireRed's window in the 3 px there is
 * outside it: the slate outline, the sky blue and the light blue next to the picture. Nothing inside the picture's rectangle. */
static void HomeFrame(int x, int y, int w, int h)
{
    if (sStyleBall) {                                   /* Poke Ball: a white frame, 3 px */
        UiRoundFill(x - 3, y - 3, w + 6, h + 6, 5, BL_WHITE);
        return;
    }
    UiRoundFill(x - 3, y - 3, w + 6, h + 6, 5, LL_SLATE);
    UiRoundFill(x - 2, y - 2, w + 4, h + 4, 4, UI_RGB(122, 180, 216));
    UiRoundFill(x - 1, y - 1, w + 2, h + 2, 3, UI_RGB(200, 228, 240));
}

/* The rectangle with the two rows the shadow of a raised piece under it takes. */
static CtrPokeTouchRect Under(const CtrPokeTouchRect *r)
{
    CtrPokeTouchRect u = *r;

    u.h = (int16_t)(u.h + 2);
    return u;
}

/* Shortcut item icons from FireRed's icon table, decoded once per item and
 * shown as the 20x20 around their drawn part. */
enum { ICON_REGISTERED, ICON_BIKE, ICON_BOX_HAND, ICON_BOX_NOTE, ICON_MAIL, ICON_REPEL, ICON_REPEL_2, ICON_REPEL_3, ICON_COUNT };

static struct {
    unsigned item;
    int ok, x0, y0;
    uint8_t pixels[24 * 24];
    uint16_t pal[16];
} sIcons[ICON_COUNT];

static int ItemIcon(int slot, unsigned item)
{
    static unsigned char tiles[0x200];
    static unsigned short pal[16];

    if (item == 0)
        return 0;
    if (sIcons[slot].item == item)
        return sIcons[slot].ok;
    sIcons[slot].item = item;
    sIcons[slot].ok = CtrPokeTouch_ItemIcon(item, tiles, pal);
    if (!sIcons[slot].ok)
        return 0;
    int left = 24, right = 0, top = 24, bottom = 0;
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x) {
            unsigned byte = tiles[(y / 8 * 3 + x / 8) * 32 + y % 8 * 4 + x % 8 / 2];
            uint8_t v = (uint8_t)((byte >> (x & 1) * 4) & 15);
            sIcons[slot].pixels[y * 24 + x] = v;
            if (v) {
                if (x < left) left = x;
                if (x >= right) right = x + 1;
                if (y < top) top = y;
                if (y >= bottom) bottom = y + 1;
            }
        }
    int x0 = left < right ? (left + right) / 2 - 10 : 2, y0 = top < bottom ? (top + bottom) / 2 - 10 : 2;
    sIcons[slot].x0 = x0 < 0 ? 0 : x0 > 4 ? 4 : x0;
    sIcons[slot].y0 = y0 < 0 ? 0 : y0 > 4 ? 4 : y0;
    for (int i = 0; i < 16; ++i) {
        unsigned c = pal[i];
        sIcons[slot].pal[i] = GbaColor(c);
    }
    return 1;
}

static void ItemArt(int slot, unsigned item, int x, int y, int on)
{
    if (ItemIcon(slot, item))
        Art(sIcons[slot].pixels, sIcons[slot].pal, 24, sIcons[slot].x0, sIcons[slot].y0, 20, 20, x, y, on);
}

/* The HOME map: its art, its image and the worker that builds it are in 3ds_home_map_job_impl.h. */
#include "3ds_home_map_job_impl.h"

/* The worker's stack is heap memory that libctru unmaps at exit and it reads the data pack: it is stopped before the process ends (atexit,
 * registered after the data's, so it runs before it). */
void CtrPokeTouch_Shutdown(void)
{
    MapJobShutdown();
}

bool CtrPokeTouch_MapPrepare(int group, int version)
{
    return MapJobPrepare(group, version);
}

static int MapReady(const CtrPokeTouchView *v)
{
    return v->mapReady && sMap.ok && sMap.group == v->mapGroup && sMap.version == v->mapVersion && MapJobImageReady();
}

/* The view being drawn (HOME's zoom and centre), so that a rectangle can be put back from the image. */
static int sViewZoom, sViewCX, sViewCY, sViewReady;

static void HomeSetView(const CtrPokeTouchView *v)
{
    sViewZoom = v->mapZoom;
    sViewCX = v->mapCX;
    sViewCY = v->mapCY;
    sViewReady = MapReady(v);
}

#define C_MAP_WAIT UI_RGB(156, 213, 255)

/* The map's own pixels over a rectangle of the viewport (its own coordinates): the image, or the sea's blue while it is built. */
static void MapRestore(int x, int y, int w, int h)
{
    int x1 = x + w > PT_VIEW_W ? PT_VIEW_W : x + w, y1 = y + h > PT_VIEW_H ? PT_VIEW_H : y + h;

    x = x < 0 ? 0 : x;
    y = y < 0 ? 0 : y;
    if (x1 <= x || y1 <= y)
        return;
    if (sViewReady)
        HmDrawRect(sViewZoom, sViewCX, sViewCY, x, y, x1 - x, y1 - y);
    else
        Fill(PT_HOME_X + x, PT_HOME_Y + y, x1 - x, y1 - y, C_MAP_WAIT);
}

/* The centre of a cell in the viewport (its own pixels); false when the cell is off the map or the image is not built. */
static int CellView(const CtrPokeTouchView *v, int cx, int cy, int *x, int *y)
{
    int wx, wy;

    if (cx < 0 || cy < 0 || cx >= 22 || cy >= 15 || !MapReady(v))
        return 0;
    HmCellWorld(cx, cy, &wx, &wy);
    HmWorldToView(v->mapZoom, v->mapCX, v->mapCY, wx, wy, x, y);
    return 1;
}

/* A cell's centre in the image (its town's orb where there is one); false when the cell is off the map or the image is not built. */
bool CtrPokeTouch_HomeCellWorld(int cx, int cy, int *wx, int *wy)
{
    if (cx < 0 || cy < 0 || cx >= 22 || cy >= 15 || !MapJobImageReady())
        return false;
    HmCellWorld(cx, cy, wx, wy);
    return true;
}

void CtrPokeTouch_HomeClamp(int zoom, int *cx, int *cy)
{
    HmClamp(zoom, cx, cy);
}

/* The zoom's scale in 1/256 (screen pixels per image pixel). */
int CtrPokeTouch_HomeScale(int zoom)
{
    return HmScale(zoom);
}

/* A selected cell that is out of the window (16 pixels in from its edges, for the ring and the name) is brought to the centre; else nothing moves. */
void CtrPokeTouch_HomeReveal(int zoom, int *cx, int *cy, int gx, int gy)
{
    int wx, wy, vx, vy;

    if (!CtrPokeTouch_HomeCellWorld(gx, gy, &wx, &wy))
        return;
    HmWorldToView(zoom, *cx, *cy, wx, wy, &vx, &vy);
    if (vx < 16 || vy < 16 || vx >= PT_VIEW_W - 16 || vy >= PT_VIEW_H - 16) {
        *cx = wx;
        *cy = wy;
        HmClamp(zoom, cx, cy);
    }
}

/* The cell under a viewport pixel (its own coordinates): the town whose orb is within 14 pixels, else the cell the pixel is in. */
bool CtrPokeTouch_HomePick(int zoom, int cx, int cy, int vx, int vy, int *gx, int *gy)
{
    int wx, wy, s = HmScale(zoom), best = -1;
    long bestD = 14L * 14L;

    if (!MapJobImageReady())
        return false;
    HmViewToWorld(zoom, cx, cy, vx, vy, &wx, &wy);
    for (int i = 0; i < sHmOrbN; ++i) {
        long dx = (long)(sHmOrb[i][0] - wx) * s / 256, dy = (long)(sHmOrb[i][1] - wy) * s / 256;

        if (dx * dx + dy * dy <= bestD) {
            bestD = dx * dx + dy * dy;
            best = i;
        }
    }
    if (best >= 0) {
        wx = sHmOrb[best][0];
        wy = sHmOrb[best][1];
    }
    return HmWorldCell(wx, wy, gx, gy) != 0;
}

/* The map's two icons, FireRed's own 16x16 sprites (four 4bpp tiles in sprite
 * order, 0 clear): the player (Red or Leaf) and the selection cursor (two
 * frames, which blink). An icon is centred on its cell: 8x8 pixels at (32, 32)
 * + 8 per cell on the Region Map's screen, so its 16x16 is at (8 x + 28, 8 y + 28)
 * there. HOME enlarges both by the map's 5/4 (nearest neighbour): 20x20 icons
 * on 10x10 cells. The cursor is above the player. */
extern int CtrPokeTouch_MapPlayerIcon(uint8_t *tiles, uint16_t *palette);
extern int CtrPokeTouch_MapCursorIcon(uint8_t *tiles, uint16_t *palette);

#define MARKER 16
#define MAP_CELLS_X 22
#define MAP_CELLS_Y 15

static struct Icon {
    int key;                            /* what is cached (gender), -1 none */
    int ok;
    int frames;
    uint8_t pixels[2][MARKER * MARKER]; /* palette index per frame */
    uint16_t pal[16];
} sPlayer = {-1, 0, 1, {{0}}, {0}};

static void IconBuild(struct Icon *icon, int key, int (*art)(uint8_t *, uint16_t *))
{
    uint8_t tiles[8 * 32];
    uint16_t gba[16];

    icon->key = key;
    memset(tiles, 0, sizeof(tiles));
    icon->ok = art(tiles, gba);
    if (!icon->ok)
        return;
    for (int i = 0; i < 16; ++i)
        icon->pal[i] = Gba(gba[i]);
    for (int frame = 0; frame < icon->frames; ++frame)
        for (int y = 0; y < MARKER; ++y)
            for (int x = 0; x < MARKER; ++x) {
                int tile = frame * 4 + y / 8 * 2 + x / 8, tx = x % 8, ty = y % 8;
                uint8_t byte = tiles[tile * 32 + ty * 4 + tx / 2];
                icon->pixels[frame][y * MARKER + x] = (tx & 1 ? byte >> 4 : byte) & 15;
            }
}

/* An icon (16x16) centred at (cx, cy) of the viewport, clipped to it. */
static void IconDraw(const struct Icon *icon, int frame, int cx, int cy)
{
    if (!icon->ok)
        return;
    for (int py = 0; py < MARKER; ++py)
        for (int px = 0; px < MARKER; ++px) {
            int x = cx - MARKER / 2 + px, y = cy - MARKER / 2 + py;
            uint8_t index = icon->pixels[frame % icon->frames][py * MARKER + px];

            if (index && x >= 0 && y >= 0 && x < PT_VIEW_W && y < PT_VIEW_H)
                Px(PT_HOME_X + x, PT_HOME_Y + y, icon->pal[index]);
        }
}

/* The pieces over the HOME map, in the order they are drawn: the region's red ribbon at the map's corner (it carries a small arrow when there is
 * another map to go to, and a touch on it goes there), the two zoom buttons at its right edge, the player's icon and, for the destination chosen,
 * its ring (an orange ring with a dark red edge round the orb) and its name on a small window above (below, if there is no room). Every piece
 * paints inside the viewport only. */
#define LABEL_H 21
#define ZOOM_D 20

static const char *RegionName(int group)
{
    return group == PT_MAP_KANTO ? "Kanto" : group == PT_MAP_SEVII123 ? "Sevii 1-3" : group == PT_MAP_SEVII45 ? "Sevii 4-5" : "Sevii 6-7";
}

static CtrPokeTouchRect sHomeRibbon;            /* as last drawn (w 0: no other map, so nothing to touch) */

static int sHomeShown;                          /* HOME's overlay was the last thing drawn on the viewport */

bool CtrPokeTouch_HomeRibbonHit(int x, int y)
{
    const CtrPokeTouchRect *r = &sHomeRibbon;

    return sHomeShown && r->w && x >= r->x - 3 && y >= r->y - 3 && x < r->x + r->w + 3 && y < r->y + r->h + 3;
}

static CtrPokeTouchRect ZoomRect(int plus)
{
    CtrPokeTouchRect r = {PT_HOME_X + PT_VIEW_W - ZOOM_D - 6, (int16_t)(PT_HOME_Y + PT_VIEW_H / 2 - ZOOM_D - 2 + (plus ? 0 : ZOOM_D + 4)), ZOOM_D, ZOOM_D};

    return r;
}

/* +1 on the "+" button, -1 on "-", else 0. */
int CtrPokeTouch_HomeZoomHit(int x, int y)
{
    if (!sHomeShown)
        return 0;
    for (int plus = 0; plus < 2; ++plus) {
        CtrPokeTouchRect r = ZoomRect(plus);

        if (x >= r.x - 3 && y >= r.y - 3 && x < r.x + r.w + 3 && y < r.y + r.h + 3)
            return plus ? 1 : -1;
    }
    return 0;
}

/* A zoom button: a cream disc with a slate rim and a sign (greyed at its limit). */
static void ZoomButton(int plus, int enabled)
{
    CtrPokeTouchRect r = ZoomRect(plus);
    uint16_t rim = enabled ? LL_SLATE : UI_RGB(150, 158, 180), face = enabled ? UI_RGB(255, 255, 250) : UI_RGB(226, 230, 240);
    uint16_t sign = enabled ? LL_SLATE : UI_RGB(170, 176, 196), ring = UI_RGB(122, 180, 216);

    if (sStyleBall) {                                   /* Poke Ball: white, its rim and sign red; pale at its limit */
        rim = enabled ? UI_RGB(150, 22, 26) : UI_RGB(214, 170, 170);
        face = enabled ? BL_WHITE : UI_RGB(244, 234, 234);
        sign = enabled ? BL_REDTXT : UI_RGB(214, 170, 170);
        ring = enabled ? UI_RGB(244, 226, 226) : face;
    }

    for (int row = 0; row < ZOOM_D; ++row)
        for (int col = 0; col < ZOOM_D; ++col) {
            int dx = 2 * col + 1 - ZOOM_D, dy = 2 * row + 1 - ZOOM_D, d2 = dx * dx + dy * dy;

            if (d2 > ZOOM_D * ZOOM_D)
                continue;
            Px(r.x + col, r.y + row, d2 > (ZOOM_D - 2) * (ZOOM_D - 2) ? rim : d2 > (ZOOM_D - 4) * (ZOOM_D - 4) ? ring : face);
        }
    Fill(r.x + 6, r.y + ZOOM_D / 2 - 1, 8, 2, sign);
    if (plus)
        Fill(r.x + ZOOM_D / 2 - 1, r.y + 6, 2, 8, sign);
}

/* The player's icon: its 16x16 rectangle in the viewport; false when the player is off the map. */
static int PlayerRect(const CtrPokeTouchView *v, CtrPokeTouchRect *r)
{
    int x, y;

    if (!CellView(v, v->playerX, v->playerY, &x, &y))
        return 0;
    r->x = (int16_t)(PT_HOME_X + x - MARKER / 2);
    r->y = (int16_t)(PT_HOME_Y + y - MARKER / 2);
    r->w = r->h = MARKER;
    return 1;
}

/* The destination's ring: its diameter (the orb's, grown a little with the zoom). */
static int RingD(const CtrPokeTouchView *v)
{
    int s = HmScale(v->mapZoom);

    return (24 * (s < 205 ? 205 : s) / 256 + 6) & ~1;
}

/* The destination's ring: the square it is drawn in; false when nothing is chosen or its cell is off the map. */
static int RingRect(const CtrPokeTouchView *v, CtrPokeTouchRect *r)
{
    int x, y, d = RingD(v);

    if (v->cursorX < 0 || !CellView(v, v->cursorX, v->cursorY, &x, &y) || x < 0 || y < 0 || x >= PT_VIEW_W || y >= PT_VIEW_H)
        return 0;                           /* off the window: no ring, no name */
    r->x = (int16_t)(PT_HOME_X + x - d / 2);
    r->y = (int16_t)(PT_HOME_Y + y - d / 2);
    r->w = r->h = (int16_t)d;
    return 1;
}

/* The destination's name, on a window centred over its ring 3 px above it (below if that is off the map), kept inside the map. */
static int LabelRect(const CtrPokeTouchView *v, CtrPokeTouchRect *r)
{
    CtrPokeTouchRect ring;
    int w, x, y, left = PT_HOME_X + 3, right = PT_HOME_X + PT_VIEW_W - 3, top = PT_HOME_Y + 3, bottom = PT_HOME_Y + PT_VIEW_H - 3;

    if (!v->bar[0][0] || !RingRect(v, &ring))
        return 0;
    w = UiTextWidth(v->bar[0], 1) + 18;
    if (w > 150)
        w = 150;
    x = ring.x + ring.w / 2 - w / 2;
    y = ring.y - 3 - LABEL_H;
    if (y < top)
        y = ring.y + ring.h + 3;
    if (y + LABEL_H > bottom)
        y = bottom - LABEL_H;
    if (x + w > right)
        x = right - w;
    if (x < left)
        x = left;
    r->x = (int16_t)x;
    r->y = (int16_t)y;
    r->w = (int16_t)w;
    r->h = LABEL_H;
    return 1;
}

static void RingDraw(const CtrPokeTouchRect *r, int blink)
{
    int D = r->w;

    for (int row = 0; row < D; ++row)
        for (int col = 0; col < D; ++col) {
            int dx = 2 * col + 1 - D, dy = 2 * row + 1 - D, d2 = dx * dx + dy * dy, x = r->x + col, y = r->y + row;

            if (x < PT_HOME_X || y < PT_HOME_Y || x >= PT_HOME_X + PT_VIEW_W || y >= PT_HOME_Y + PT_VIEW_H || d2 > D * D)
                continue;
            if (d2 > (D - 2) * (D - 2))
                Px(x, y, UI_RGB(138, 22, 16));
            else if (d2 > (D - 6) * (D - 6))
                Px(x, y, blink ? UI_RGB(255, 200, 96) : UI_RGB(255, 154, 56));
        }
}

static void HomeOverlay(const CtrPokeTouchView *v)
{
    CtrPokeTouchRect ring, label, player;
    const char *name = RegionName(v->mapGroup);
    int multi = (v->mapOpen & (v->mapOpen - 1)) != 0, textW = UiTextWidth(name, 1), w = textW + 18 + (multi ? 9 : 0);

    if (sStyleBall) {                                   /* Poke Ball: a white ribbon, the region's name and its arrow red */
        LlRibbonBox(PT_HOME_X, PT_HOME_Y, w, 13, 7, 0, BL_WHITE, UI_RGB(244, 236, 236), "");
        UiText(name, PT_HOME_X + 6, PT_HOME_Y + 1, BL_REDTXT, BL_WHITE, 1, PT_HOME_X + w);
    } else
        LlRibbonBox(PT_HOME_X, PT_HOME_Y, w, 13, 7, 0, LL_RIB_TOP, LL_RIB_BOT, name);
    if (multi)
        LlArrow(PT_HOME_X + 6 + textW + 4, PT_HOME_Y + 3, 4, 7, 3, sStyleBall ? BL_REDTXT : EM_WHITE);
    sHomeShown = 1;
    sHomeRibbon = multi ? (CtrPokeTouchRect){PT_HOME_X, PT_HOME_Y, (int16_t)w, 13} : (CtrPokeTouchRect){0, 0, 0, 0};
    ZoomButton(1, v->mapZoom < HM_ZOOMS - 1);
    ZoomButton(0, v->mapZoom > 0);
    if (sPlayer.key != v->playerFemale)
        IconBuild(&sPlayer, v->playerFemale, CtrPokeTouch_MapPlayerIcon);
    if (PlayerRect(v, &player))
        IconDraw(&sPlayer, 0, player.x - PT_HOME_X + MARKER / 2, player.y - PT_HOME_Y + MARKER / 2);
    if (!RingRect(v, &ring))
        return;
    RingDraw(&ring, v->cursorFrame);
    if (LabelRect(v, &label)) {
        LlWindowPlain(label.x, label.y, label.w, label.h);
        Em2Text(v->bar[0], label.x + 10, label.y + 6, EM_DARK, EM_DARK_SH, label.x + label.w - 6);
    }
}

/* HOME's picture: the map (or the sea's blue while it is built), the player and the selection on it. */
static int HomeShow(const CtrPokeTouchView *v)
{
    if (!sMap.ok || sMap.group != v->mapGroup || sMap.version != v->mapVersion) {
        sHomeShown = 0;                             /* an empty panel: the old ribbon and zoom buttons must not answer touches */
        sHomeRibbon.w = 0;
        return 0;
    }
    HomeSetView(v);
    MapRestore(0, 0, PT_VIEW_W, PT_VIEW_H);
    HomeOverlay(v);
    return 1;
}

/* The OPTIONS rows (the Llama mockup's cream cards): 236x22 with their lip, 22 apart from (7, 7), so they touch. The value sits in a well at the
 * right: 120 wide inside OPT_BOX_W: "<" (28), the value and ">" (28) for the hit zones. */
#define OPT_X 42
#define OPT_Y 43
#define OPT_W 236
#define OPT_H 22
#define OPT_STEP 22
#define OPT_BOX_W 124
#define OPT_ARROW_W 28

static CtrPokeTouchRect OptRect(int row)
{
    CtrPokeTouchRect r = {OPT_X, (int16_t)(OPT_Y + row * OPT_STEP), OPT_W, OPT_H};
    return r;
}

bool CtrPokeTouch_OptionHit(int x, int y, int rows, int *row, int *zone)
{
    int boxX = OPT_X + OPT_W - OPT_BOX_W;

    if (sBallFull) {                                    /* Poke Ball: rows 20 apart from (8, 34), the value in the right half */
        int r = (y - 34) / 20;

        if (x < 8 || x >= 232 || y < 34 || r >= rows || r >= PT_OPT_MAX || (y - 34) % 20 >= 19)
            return false;
        *row = r;
        *zone = x < 128 ? PT_OPT_ZONE_LABEL : x < 180 ? PT_OPT_ZONE_PREV : PT_OPT_ZONE_NEXT;
        return true;
    }

    if (x < OPT_X || x >= OPT_X + OPT_W || y < OPT_Y)
        return false;
    int r = (y - OPT_Y) / OPT_STEP;
    if (r >= rows || r >= PT_OPT_MAX || (y - OPT_Y) % OPT_STEP >= OPT_H)
        return false;
    *row = r;
    *zone = x < boxX ? PT_OPT_ZONE_LABEL : x < boxX + OPT_ARROW_W ? PT_OPT_ZONE_PREV : PT_OPT_ZONE_NEXT;
    return true;
}

/* A row of levels: its values share the well's inside, OPT_SEG_W wide in all (they need no arrows). */
#define OPT_SEG_X(boxX) ((boxX) + 3)
#define OPT_SEG_W (OPT_BOX_W - 9)

int CtrPokeTouch_OptionSegment(int x, int count)
{
    int at = sBallFull ? x - 128 : x - OPT_SEG_X(OPT_X + OPT_W - OPT_BOX_W), k;

    if (count <= 0)
        return 0;
    k = at < 0 ? 0 : at * count / (sBallFull ? 104 : OPT_SEG_W);
    return k >= count ? count - 1 : k;
}

/* The colour of a page's left edge on its rows (the chosen row has a white one): the four pages are told apart at a glance, as the dots of the PAGE button
 * count them. An if-chain keeps it away from the audit's tables. */
static uint16_t OptPageColor(int page)
{
    if (page == 0)
        return UI_RGB(74, 120, 216);
    if (page == 1)
        return UI_RGB(56, 168, 104);
    if (page == 2)
        return UI_RGB(168, 112, 208);
    if (page == 4)
        return EM2_PAGE_E;
    return UI_RGB(216, 90, 80);
}

/* One row: a raised cream card (flame, ringed, with the white cursor, when selected) with its page's colour on the left edge and the label,
 * and on its right a sunk well with the value: "<" value ">" between two flame nubs, a switch for ON and OFF, or all the levels of a row of
 * levels, the chosen one flame. The zone under a held touch sinks or tints. The rows touch, so a row paints inside its own rectangle, 2 px
 * at the sides and, at the first row and the last, 2 above and below (the chosen row's ring goes over the rows next to it: it is drawn
 * last, and also when a neighbour was drawn). */
static void OptRow(int row, const CtrPokeTouchView *v)
{
    CtrPokeTouchRect r = OptRect(row);
    int selected = v->optSelected == row, pressed = v->optPressed == row;
    int boxX = r.x + r.w - OPT_BOX_W, zone = pressed ? v->optZone : PT_OPT_ZONE_NONE;
    const EmPal *pal = LlPick(selected, &kEmCream);
    int fy, wy, ww = OPT_BOX_W - 4;

    TsBgRect(r.x - 2, r.y, r.w + 4, r.h);
    if (row == 0)
        TsBgRect(r.x - 2, r.y - 2, r.w + 4, 2);
    if (row + 1 == v->optCount || row + 1 == PT_OPT_MAX)
        TsBgRect(r.x - 2, r.y + r.h, r.w + 4, 2);
    if (selected)
        LlSelectRing(r.x, r.y, r.w, r.h);
    fy = EmButton(r.x, r.y, r.w, r.h, pal, COL_DEPTH, zone == PT_OPT_ZONE_LABEL);
    wy = fy + 2;
    UiRoundFill(r.x + 4, fy + 4, 3, 12, 1, selected ? EM_WHITE : OptPageColor(v->optSection));
    if (selected)
        LlCursor(r.x + 10, fy + 5, EM_WHITE);
    Em2Text(v->optLabel[row], r.x + (selected ? 19 : 12), fy + 5, pal->ink, pal->inkSh, boxX - 2);
    if (v->optSegCount[row]) {
        const char *names[PT_OPT_SEGS];
        int count = v->optSegCount[row] > PT_OPT_SEGS ? PT_OPT_SEGS : v->optSegCount[row];

        for (int k = 0; k < count; ++k)
            names[k] = v->optSegName[row][k];
        LlWell(boxX, wy, ww, 16);
        LlSegments(OPT_SEG_X(boxX), wy + 1, OPT_SEG_W, 14, names, count, v->optSegValue[row],
                   zone >= PT_OPT_ZONE_SEG ? zone - PT_OPT_ZONE_SEG : -1);
        return;
    }
    if (v->optSwitch[row]) {
        /* A switch has two places and no arrows: the whole box toggles (PREV and NEXT both do), so the whole box darkens when held. */
        LlWell(boxX, wy, ww, 16);
        if (zone == PT_OPT_ZONE_PREV || zone == PT_OPT_ZONE_NEXT)
            UiRoundFill(boxX + 1, wy + 1, ww - 2, 14, 3, UI_RGB(255, 214, 160));
        LhSwitch(boxX + (ww - 30) / 2, wy, v->optSwitch[row] == 2);
        return;
    }
    LhValue(boxX, wy, ww, 16, v->optValue[row], zone == PT_OPT_ZONE_PREV ? 1 : zone == PT_OPT_ZONE_NEXT ? 2 : 0);
}

/* The keypad of the private room's code, over the rows (CtrPokeTouchPad): along the top four boxes of four digits (the next digit underlined); under
 * them the keys in four columns, 1-4, 5-8, then 9, 0, DELETE and OK, OK lit (flame) once the code is a good one; and at the foot NEW (a code made
 * at random) and CANCEL. Cream raised keys as the rows; the key under a held touch sinks. */
/* The keypad's origin: where the OPTIONS rows start in HOME's layout, or (8, 26) on the Poke Ball style's whole screen (sBallFull). */
#define PAD_OX (sBallFull ? 8 : OPT_X)
#define PAD_OY (sBallFull ? 26 : OPT_Y)
#define PAD_BOX_X (PAD_OX + 4)
#define PAD_BOX_Y (PAD_OY + 2)
#define PAD_BOX_W 54
#define PAD_BOX_H 22
#define PAD_BOX_GAP 4
#define PAD_KEY_X (PAD_OX + 4)
#define PAD_KEY_Y (PAD_OY + 30)
#define PAD_KEY_W 54
#define PAD_KEY_H 26
#define PAD_KEY_GAP 4
#define PAD_KEY_STEP 30
#define PAD_WIDE_Y (PAD_OY + 120)
#define PAD_WIDE_W 112

static CtrPokeTouchRect PadKeyRect(int key)
{
    /* the keys in reading order: 1..9, then 0, DELETE, OK (four to a row) */
    int slot = key >= 1 && key <= 9 ? key - 1 : key == 0 ? 9 : key == PT_PAD_KEY_DELETE ? 10 : 11;
    CtrPokeTouchRect r = {(int16_t)(PAD_KEY_X + (slot % 4) * (PAD_KEY_W + PAD_KEY_GAP)), (int16_t)(PAD_KEY_Y + (slot / 4) * PAD_KEY_STEP),
                          PAD_KEY_W, PAD_KEY_H};

    if (key == PT_PAD_KEY_NEW || key == PT_PAD_KEY_CANCEL) {
        r.x = (int16_t)(PAD_KEY_X + (key == PT_PAD_KEY_CANCEL ? PAD_WIDE_W + 4 : 0));
        r.y = PAD_WIDE_Y;
        r.w = PAD_WIDE_W;
    }
    return r;
}

int CtrPokeTouch_PadHit(int x, int y)
{
    for (int key = 0; key < PT_PAD_KEYS; ++key) {
        CtrPokeTouchRect r = PadKeyRect(key);

        if (x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h)
            return key;
    }
    return -1;
}

static void OptPad(const CtrPokeTouchView *v)
{
    const CtrPokeTouchPad *p = &v->optPad;

    if (!sBallFull)
        TsBgRect(OPT_X - 2, OPT_Y - 2, OPT_W + 4, PT_OPT_MAX * OPT_STEP + 4);
    for (int g = 0; g < PT_PAD_DIGITS / 4; ++g) {
        int x = PAD_BOX_X + g * (PAD_BOX_W + PAD_BOX_GAP);

        LlWell(x, PAD_BOX_Y, PAD_BOX_W, PAD_BOX_H);
        for (int k = 0; k < 4; ++k) {
            int i = g * 4 + k, dx = x + 7 + k * 11;
            char digit[2] = {i < p->len ? p->code[i] : 0, 0};

            if (digit[0])
                UiText(digit, dx, PAD_BOX_Y + 5, LH_VAL, LH_VALSH, 1, dx + 10);
            else if (i == p->len)
                UiRoundFill(dx, PAD_BOX_Y + PAD_BOX_H - 6, 8, 2, 1, LH_VAL);          /* where the next digit goes */
        }
    }
    for (int key = 0; key < PT_PAD_KEYS; ++key) {
        static const char *const digits[10] = {"0", "1", "2", "3", "4", "5", "6", "7", "8", "9"};
        CtrPokeTouchRect r = PadKeyRect(key);
        const EmPal *pal = key == PT_PAD_KEY_OK ? (p->ok ? &kEmFlame : &kEmGrey) : &kEmCream;
        const char *label = key <= 9 ? digits[key] : key == PT_PAD_KEY_DELETE ? CTR_TR("Delete", "Borrar", "L\xC3\xB6schen")
                          : key == PT_PAD_KEY_OK ? "OK" : key == PT_PAD_KEY_NEW ? CTR_TR("New code", "C\xC3\xB3" "digo nuevo", "Neuer Code")
                          : CTR_TR("Cancel", "Cancelar", "Zur\xC3\xBC" "ck");
        int fy = EmButton(r.x, r.y, r.w, r.h, pal, COL_DEPTH, p->pressed == key);

        EmLabel(label, r.x, r.w, fy, r.h - COL_DEPTH, pal);
    }
}

static void BDescribeN(const char *text, int x, int y, int width, int maxLines, uint16_t color);
static void BDescribeSh(const char *text, int x, int y, int width, int maxLines, uint16_t color, uint16_t shadow);

/* The preview under the look rows of OPTIONS (3ds_poketouch.h CtrPokeTouchLook): in the rows the page does not use, a FireRed window with
 * the look's walking picture facing down, its name and its artists. It starts 2 px under the last row (where the chosen row's ring is) and
 * leaves 2 px at the bottom, where its shadow is. */
static CtrPokeTouchRect OptLookRect(const CtrPokeTouchView *v)
{
    CtrPokeTouchRect r = {OPT_X, (int16_t)(OPT_Y + v->optCount * OPT_STEP + 2), OPT_W,
                          (int16_t)((PT_OPT_MAX - v->optCount) * OPT_STEP - 4)};

    return r;
}

static void OptLookPanel(const CtrPokeTouchView *v)
{
    const CtrPokeTouchLook *l = &v->optLook;
    CtrPokeTouchRect r = OptLookRect(v);
    int big = r.h >= 72;                     /* three rows on the page leave 62... and four 84: the picture twice as large only where it fits the paper */
    int textX = r.x + (big ? 84 : 54), right = r.x + r.w - 12;

    if (r.h < 40)
        return;
    TsBgRect(r.x, r.y, r.w, r.h + 2);
    LlWindow(r.x, r.y, r.w, r.h);
    if (l->has) {
        static uint8_t pixels[32 * 32];
        uint16_t pal[16];

        for (int py = 0; py < 32; ++py)
            for (int px = 0; px < 32; ++px)
                pixels[py * 32 + px] = (uint8_t)((l->tiles[(py / 8 * 4 + px / 8) * 32 + py % 8 * 4 + px % 8 / 2] >> (px & 1) * 4) & 15);
        for (int i = 0; i < 16; ++i)
            pal[i] = GbaColor(l->palette[i]);
        /* The first rows of a person are empty: the paper holds the other 30, at their own size or at twice it. */
        if (big)
            UiImage(pixels + 2 * 32, pal, 32, 32, 30, r.x + 14, r.y + (r.h - 60) / 2, 2, 1);
        else
            UiImage(pixels + 2 * 32, pal, 32, 32, 30, r.x + 14, r.y + (r.h - 30) / 2, 1, 1);
    }
    Em2Text(l->name, textX, r.y + (r.h >= 56 ? 11 : 8), EM_DARK, EM_DARK_SH, right);
    LlRule(textX, r.y + (r.h >= 56 ? 26 : 19), right - textX);
    {
        /* The artists behind an ART ribbon, as many lines as the window holds, up to three. */
        const char *tag = CTR_TR("Art", "Arte", "Kunst");
        int y = r.y + (r.h >= 56 ? 31 : 22), pw, lines = big ? 3 : r.h >= 56 ? 2 : 1;

        pw = LlRibbon(textX, y, tag, LL_RIB_TOP, LL_RIB_BOT);
        Em2Wrap(l->credit, textX + pw + 4, y + 3, right - textX - pw - 4, lines, UI_LINE_H, EM_DIM, EM_DARK_SH);
    }
}

/* The SAVE screen, in HOME's layout (the status plate and the clock in the top band, the columns at the sides; see MenuChrome):
 * a card in the middle with the four lines (label on the left, blue value on the right, a rule under each) and the player's own map
 * icon, enlarged by 3, in a sunk well; under it the question in a second window, YES over NO (or a lone OK) at its right; the note
 * under the floor says what is being done. */
enum { SF_NAME, SF_BADGES, SF_DEX, SF_TIME, SF_COUNT };
static const CtrPokeTouchRect sSaveYes = {204, 145, 72, 24};
static const CtrPokeTouchRect sSaveNo = {204, 172, 72, 24};
static const CtrPokeTouchRect sSaveButtons = {200, 142, 80, 58};   /* both, with the ring and the shadow */
static const char *const sSaveLabel[SF_COUNT] = {CTR_TR("Player", "Jugador", "Spieler"), CTR_TR("Badges", "Medallas", "Orden"), CTR_TR("Poké" "dex", "Poké" "dex", "Poké" "dex"), CTR_TR("Time", "Tiempo", "Zeit")};

/* The dialogue's buttons: YES over NO, or a lone OK on YES's place. */
static const CtrPokeTouchRect sBallSaveYes = {242, 78, 70, 30};   /* the Poke Ball SAVE's buttons (3ds_ball_menus_impl.h) */
static const CtrPokeTouchRect sBallSaveNo = {242, 114, 70, 30};

static CtrPokeTouchRect SaveButtonRect(int which, int state)
{
    if (sBallFull)
        return state == SAVE_CONFIRM && which == PT_SAVE_SECOND ? sBallSaveNo : sBallSaveYes;
    return state == SAVE_CONFIRM && which == PT_SAVE_SECOND ? sSaveNo : sSaveYes;
}

int CtrPokeTouch_SaveHit(int x, int y, int state)
{
    if (state == SAVE_WRITING)
        return PT_SAVE_NONE;
    for (int which = PT_SAVE_FIRST; which <= (state == SAVE_CONFIRM ? PT_SAVE_SECOND : PT_SAVE_FIRST); ++which) {
        CtrPokeTouchRect r = SaveButtonRect(which, state);

        if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h)
            return which;
    }
    return PT_SAVE_NONE;
}

/* The backdrop: the embers. */
static void Frame(void)
{
    TsBackdrop();
}

/* Where the frame goes round the GBA picture (and the HOME map). */
static int FrameShown(const CtrPokeTouchView *v)
{
    return v->viewport || (v->home && !v->options && !v->save);
}

/* The field Bag the port draws (3ds_field_bag_impl.h, included at the end). */
static void FBagFrame(const CtrPokeTouchView *v);
static void FBagNote(const CtrPokeTouchView *v, int w);
static void FBPositionTop(const CtrPokeTouchBag *b);
/* The Pokedex the port draws (3ds_field_dex_impl.h, included at the end). */
static void FDexFrame(const CtrPokeTouchView *v);
static void FDexNote(const CtrPokeTouchView *v, int w);
/* The Trainer Card the port draws (3ds_field_card_impl.h, included at the end). */
static void FCardFrame(const CtrPokeTouchView *v);
static void FCardNote(const CtrPokeTouchView *v, int w);
/* The move to forget the port draws (3ds_field_learn_impl.h, included at the end). */
static void FLearnFrame(const CtrPokeTouchView *v);
static void FLearnNote(const CtrPokeTouchView *v, int w);
/* The move relearner's list the port draws (3ds_field_relearn_impl.h, included at the end). */
static void FRelearnFrame(const CtrPokeTouchView *v);
static void FRelearnNote(const CtrPokeTouchView *v, int w);
/* The letter being read the port draws (3ds_field_mail_impl.h, included at the end). */
static void FMailFrame(const CtrPokeTouchView *v);
static void FMailNote(const CtrPokeTouchView *v, int w);
/* Easy Chat, over the whole screen (3ds_field_easychat_impl.h, included at the end). */
static void FEasyDraw(const CtrPokeTouchView *v);
static void FEasyUpdate(uint16_t *canvas, const CtrPokeTouchView *old, const CtrPokeTouchView *v, CtrDirtyList *dirty);
/* The field's choice the port draws (3ds_field_choice_impl.h, included at the end). */
static void FChoiceFrame(const CtrPokeTouchView *v);
static void FChoiceNote(const CtrPokeTouchView *v, int w);
/* The naming screen the port draws (3ds_field_naming_impl.h, included at the end). */
static void FNameFrame(const CtrPokeTouchView *v);
static void FNameNote(const CtrPokeTouchView *v, int w);
static void FNameDraw(const CtrPokeTouchView *v);
static void FNameUpdate(uint16_t *canvas, const CtrPokeTouchView *old, const CtrPokeTouchView *v, CtrDirtyList *dirty);
/* The PC box the port draws (3ds_field_box_impl.h, included at the end). */
static void FBoxFrame(const CtrPokeTouchView *v);
static void FBoxNote(const CtrPokeTouchView *v, int w);

/* The frame's area: the window frame round the GBA picture or the map, OPTIONS' rows, the save card, or the Bag, on the embers. */
static void MenuFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sMenuFrame;

    if (!(v->home && !v->viewport && !v->options && !v->save)) {
        sHomeShown = 0;                             /* HOME's ribbon and zoom buttons are not on the screen */
        sHomeRibbon.w = 0;
    }
    TsBgRect(r->x, r->y, r->w, r->h);
    if (v->dex.on && !v->viewport) {
        FDexFrame(v);
        return;
    }
    if (v->card.on && !v->viewport) {
        FCardFrame(v);
        return;
    }
    if (v->learn.on && !v->viewport) {
        FLearnFrame(v);
        return;
    }
    if (v->relearn.on && !v->viewport) {
        FRelearnFrame(v);
        return;
    }
    if (v->mail.on && !v->viewport) {
        FMailFrame(v);
        return;
    }
    if (v->choice.on && !v->viewport) {
        FChoiceFrame(v);
        return;
    }
    if (v->name.on && !v->viewport) {
        FNameFrame(v);
        return;
    }
    if (v->box.on && !v->viewport) {
        FBoxFrame(v);
        return;
    }
    if (v->bag.on && !v->viewport) {
        FBagFrame(v);
        return;
    }
    if (v->options && !v->viewport) {
        for (int pass = 0; pass < 2; ++pass)            /* the chosen row last: its ring lies on the rows next to it */
            for (int i = 0; i < v->optCount && i < PT_OPT_MAX; ++i)
                if ((pass == 1) == (v->optSelected == i))
                    OptRow(i, v);
        if (v->optLook.on)
            OptLookPanel(v);
        return;
    }
    if (!FrameShown(v))
        return;
    HomeFrame(PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H);
    /* Covered by the GPU's picture when there is one; else the HOME map. */
    if (v->viewport || !(v->home && HomeShow(v)))
        Fill(PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H, v->viewport ? 0 : C_VIEW_EMPTY);
}

/* BACK: the slate button of going back ("<" and its word at the left, as the mockup draws it); pressed it sinks, disabled it is grey. */
static void Back(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchBack;
    const EmPal *pal = v->backEnabled ? &kEmSlate : &kEmGrey;
    int fy, fh = r->h - BTN_DEPTH;

    TsBgRect(r->x, r->y, r->w, r->h + 2);
    Em2CardShadow(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, pal, BTN_DEPTH, v->backEnabled && v->backPressed);
    LlArrow(r->x + 12, fy + (fh - 10) / 2, 6, 10, 2, EM_WHITE);
    UiText(CTR_TR("Back", "Atr\xC3\xA1s", "Zur\xc3\xbc" "ck"), r->x + 24, fy + (fh - UI_TEXT_H) / 2, pal->ink, pal->inkSh, 1, r->x + r->w - 3);
}

/* The dots of the PAGE button: one for each page up to 8 (a view that does not say its pages has one for each section), and the colour of the section
 * the page is in: Settings blue, Graphics green, QoL purple, Nuzlocke red, Cheats gold. More than six dots are packed 8 px apart to fit the button. */
static int OptDotCount(const CtrPokeTouchView *v)
{
    return v->optPageCount > 8 ? 8 : (v->optPageCount > 0 ? v->optPageCount : v->optSectionCount);
}

static uint16_t OptDotColour(const CtrPokeTouchView *v, int dot)
{
    static const uint16_t sections[] = {EM2_PAGE_A, EM2_PAGE_B, EM2_PAGE_C, EM2_PAGE_D, EM2_PAGE_E};

    if (v->optPageCount > 0)
        return sections[v->optDotSection[dot] < 5 ? v->optDotSection[dot] : 3];
    return sections[dot < 5 ? dot : 3];                     /* no page list: one dot for each section, in order */
}

/* PAGE: switches between OPTIONS pages (PAGE 2/4) right above BACK: a dot for each page in that page's colour, the shown one larger with a ring. */
static void OptPageButton(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchOptPage;
    int dots = OptDotCount(v), step = dots > 6 ? 8 : 10;
    int fy, x0;
    char label[40];

    TsBgRect(r->x, r->y, r->w, r->h + 2);
    Em2CardShadow(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, COL_DEPTH, v->optPagePressed);
    x0 = r->x + (r->w - ((dots - 1) * step + 7)) / 2;
    for (int i = 0; i < dots; ++i) {
        uint16_t colour = OptDotColour(v, i);

        if (i == v->optPage)
            Em2Dot(x0 + i * step, fy + 3, colour, 1);
        else
            Em2Dot(x0 + i * step + 1, fy + 4, colour, 0);
    }
    snprintf(label, sizeof(label), "%s", v->optPageName);
    EmLabel(label, r->x, r->w, fy + 12, 11, &kEmCream);
}

/* The bar: the note card with up to three lines, 15 pixels apart and centred on it; the first line is the subject,
 * the others what is known about it (dimmer). It is narrower when BACK, PAGE or SAVE's buttons are on its right. */

/* FRAME's sample: the chosen frame (the game's own graphics/text_window/typeN, 3x3 tiles of 8x8) as FireRed draws a
 * window with it, stretched to the sample's size, with YES / NO inside. The tiles and palette are read from the
 * pack when the type changes; if they are not there the sample is left out. */
#define FRAME_W 56
#define FRAME_H 40

static int FrameLoadFile(const char *base, const char *ext, uint8_t *out, uint32_t cap, uint32_t want)
{
    char path[64];
    uint32_t len = 0;
    uint8_t *data;
    int n = 0;

    snprintf(path, sizeof(path), "graphics/text_window/%s.%s", base, ext);
    data = CtrData_Load(path, &len);
    if (data != NULL && len == want) {
        memcpy(out, data, want);
        n = (int)want;
    }
    free(data);
    if (n == (int)want)
        return n;
    snprintf(path, sizeof(path), "graphics/text_window/%s.%s.lz", base, ext);
    return PokeTouch_LoadLz(path, out, cap) == (int)want ? (int)want : 0;
}

static int FrameSampleH(int type, int x, int y, int fh)
{
    static int loaded = -1;
    static uint16_t px[24 * 24];
    static int ok;

    if (loaded != type) {
        uint8_t tiles[9 * 32], colors[32];
        char base[16];
        uint16_t pal[16];

        loaded = type;
        ok = 0;
        snprintf(base, sizeof(base), "type%d", type + 1);
        if (FrameLoadFile(base, "4bpp", tiles, sizeof(tiles), sizeof(tiles))
            && FrameLoadFile(base, "gbapal", colors, sizeof(colors), sizeof(colors))) {
            for (int i = 0; i < 16; ++i) {
                unsigned c = (unsigned)colors[i * 2] | (unsigned)colors[i * 2 + 1] << 8;

                pal[i] = GbaColor(c);
            }
            for (int ty = 0; ty < 24; ++ty)
                for (int tx = 0; tx < 24; ++tx) {
                    uint8_t b = tiles[(ty / 8 * 3 + tx / 8) * 32 + ty % 8 * 4 + tx % 8 / 2];
                    uint8_t v = (tx & 1) ? b >> 4 : b & 15;

                    px[ty * 24 + tx] = v ? pal[v] : RGB(255, 255, 255);
                }
            ok = 1;
        }
    }
    if (!ok)
        return 0;
    for (int dy = 0; dy < fh; ++dy)
        for (int dx = 0; dx < FRAME_W; ++dx) {
            int sx = dx < 8 ? dx : dx >= FRAME_W - 8 ? 16 + dx - (FRAME_W - 8) : 8 + dx % 8;
            int sy = dy < 8 ? dy : dy >= fh - 8 ? 16 + dy - (fh - 8) : 8 + dy % 8;

            Px(x + dx, y + dy, px[sy * 24 + sx]);
        }
    if (fh < FRAME_H) {                                 /* the low sample: both words on one line */
        UiText(CTR_TR("Yes", "S\xC3\xAD", "Ja"), x + 10, y + (fh - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, 1, x + FRAME_W - 6);
        UiText(CTR_TR("No", "No", "Nein"), x + 32, y + (fh - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, 1, x + FRAME_W - 6);
        return 1;
    }
    UiText(CTR_TR("Yes", "S\xC3\xAD", "Ja"), x + 14, y + 7, EM_DARK, EM_DARK_SH, 1, x + FRAME_W - 6);
    UiText(CTR_TR("No", "No", "Nein"), x + 14, y + 21, EM_DARK, EM_DARK_SH, 1, x + FRAME_W - 6);
    return 1;
}

static int FrameSample(int type, int x, int y)
{
    return FrameSampleH(type, x, y, FRAME_H);
}

/* HOME's FLY button as last drawn (w 0: not there), so a touch on it can be told. */
static CtrPokeTouchRect sFlyButton;

bool CtrPokeTouch_FlyHit(int x, int y)
{
    const CtrPokeTouchRect *r = &sFlyButton;

    return r->w && x >= r->x - 4 && y >= r->y - 4 && x < r->x + r->w + 4 && y < r->y + r->h + 4;
}

static void Bar(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sBar;
    int lines = 0;
    int hasPage = v->options && !v->viewport && v->optPageCount > 1;
    int side = v->backVisible || hasPage || v->save;
    int w = side ? NOTE_W_SIDE : NOTE_W_FULL;

    sFlyButton.w = 0;
    TsBgRect(r->x, r->y, r->w, r->h);
    for (int i = 0; i < PT_BAR_LINES; ++i)
        if (v->bar[i][0])
            lines = i + 1;
    if (!v->viewport && !v->save && (v->dex.on || v->card.on || v->learn.on || v->relearn.on || v->mail.on || v->choice.on || v->name.on || v->box.on || v->bag.on))
        Em2NoteShadow(NOTE_X, NOTE_Y, w, NOTE_H);           /* the screens that draw their own note (each puts the shadow its note throws) */
    if (v->dex.on && !v->viewport)
        FDexNote(v, w);
    else if (v->card.on && !v->viewport)
        FCardNote(v, w);
    else if (v->learn.on && !v->viewport)
        FLearnNote(v, w);
    else if (v->relearn.on && !v->viewport)
        FRelearnNote(v, w);
    else if (v->mail.on && !v->viewport)
        FMailNote(v, w);
    else if (v->choice.on && !v->viewport)
        FChoiceNote(v, w);
    else if (v->name.on && !v->viewport)
        FNameNote(v, w);
    else if (v->box.on && !v->viewport)
        FBoxNote(v, w);
    else if (v->bag.on && !v->viewport)
        FBagNote(v, w);
    else {
        /* The note is FireRed's window (the Llama mockup's): the subject at its top left, a rule under it, then the lines that say more, 14 apart,
         * dimmer; HOME's FLY is a flame button at the right of the subject. */
        int right = NOTE_X + w - 12;

        LlWindow(NOTE_X, NOTE_Y, w, NOTE_H);
        if (v->options && v->optFrameShow && FrameSample(v->optFrameType, NOTE_X + w - FRAME_W - 8,
                                                         NOTE_Y + (NOTE_H - FRAME_H) / 2 - 1))
            right -= FRAME_W + 4;
        /* HOME with a destination that can be Flown to: a FLY button beside the name says so, and the line that said the same goes. */
        const char *fly = v->home && !v->viewport && !v->options && v->flyOk ? CTR_TR("Fly", "Volar", "Fliegen") : NULL;
        const char *shown[PT_BAR_LINES] = {NULL};
        int count = 0, titleRight = right;

        for (int i = 0; i < lines; ++i)
            if (!(fly && i == 1))
                shown[count++] = v->bar[i];
        if (fly) {
            int pw = UiTextWidth(fly, 1) + 16, px = right - pw, fy;

            if (pw < 36) {
                pw = 36;
                px = right - pw;
            }
            fy = EmButton(px, NOTE_Y + 7, pw, 17, &kEmFlame, 1, 0);
            sFlyButton = (CtrPokeTouchRect){(int16_t)px, NOTE_Y + 7, (int16_t)pw, 17};
            EmLabel(fly, px, pw, fy, 16, &kEmFlame);
            titleRight = px - 4;
        }
        if (count == 1) {
            Em2Text(shown[0], NOTE_X + 13, NOTE_Y + (NOTE_H - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, titleRight);
        } else if (count > 1) {
            Em2Text(shown[0], NOTE_X + 13, NOTE_Y + 11, EM_DARK, EM_DARK_SH, titleRight);
            LlRule(NOTE_X + 13, NOTE_Y + 26, right - NOTE_X - 13);
            for (int i = 1; i < count; ++i)
                Em2Text(shown[i], NOTE_X + 13, NOTE_Y + 32 + (i - 1) * 14, EM_DIM, EM_DARK_SH, right);
        }
    }
    if (hasPage)
        OptPageButton(v);
    if (v->backVisible)
        Back(v);
}

/* An element's area: its rectangle and the ring (or the shadow) round it. */
static CtrPokeTouchRect Area(int id)
{
    return UiInset(&gCtrPokeTouchRects[id], -2);
}

/* The column's icons, by button. */
static int ColumnIcon(int id)
{
    return id;          /* kEmberIcon is in the order PT_POKEDEX .. PT_OPTIONS */
}

/* A button: cream, flame and ringed when selected (the screen shown, or the running mode on), sunk when pressed, grey when it cannot be used.
 * The column's carry their icon and label (the Trainer Card the player's name), the shortcuts their key on a blue ribbon and their item in
 * its well, RUN its switch. */
static uint16_t BallShoeColour(uint16_t c);

static void Button(int id, const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchRects[id];
    CtrPokeTouchRect area = Area(id);
    int on = v->enabled[id] || v->selected == id;
    int pressed = v->enabled[id] && v->pressed == id;
    int selected = (v->selected == id && id != PT_RUN) || (id == PT_RUN && v->enabled[id] && v->running);
    const EmPal *pal = !on ? &kEmGrey : id == PT_RUN ? &kEmCream : LlPick(selected, &kEmCream);
    int fy, fh = r->h - COL_DEPTH;


    TsBgRect(area.x, area.y, area.w, area.h);
    if (selected && id != PT_RUN && on)
        LlSelectRing(r->x, r->y, r->w, r->h);
    else
        Em2CardShadow(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, pal, COL_DEPTH, pressed);
    switch (id) {
    case PT_REGISTERED:
    case PT_BIKE: {
        const char *key = id == PT_REGISTERED ? "X" : "Y";

        if (sStyleBall) {                               /* Poke Ball: the key is red; only the tab that was blue is white, its letter red */
            LlRibbonBox(r->x + 1, fy + 1, UiTextWidth(key, 1) + 18, 11, 7, LL_TL, BL_WHITE, UI_RGB(244, 236, 236), "");
            UiText(key, r->x + 1 + (UiTextWidth(key, 1) + 18 - UiTextWidth(key, 1)) / 2 - 2, fy + 1, BL_REDTXT, BL_WHITE, 1, r->x + r->w - 3);
        } else
            LlRibbonBox(r->x + 1, fy + 1, UiTextWidth(key, 1) + 18, 11, 7, LL_TL, LL_BLUE_TOP, LL_BLUE_BOT, key);
        {
            int wellY = fy + (sLayoutHome ? 13 : 15), wellH = fh - (sLayoutHome ? 14 : 18);

            LlWell(r->x + 3, wellY, r->w - 6, wellH);
            ItemArt(id == PT_REGISTERED ? ICON_REGISTERED : ICON_BIKE, id == PT_REGISTERED ? v->registeredItem : v->bikeItem,
                    r->x + (r->w - 20) / 2, wellY + (wellH - 20) / 2, on);
        }
        break;
    }
    case PT_RUN:
        /* The battle menu's RUN shoe, small, beside the switch. */
        for (int v2 = 0; v2 < FLATSHOESMALL_H; ++v2)
            for (int u = 0; u < FLATSHOESMALL_W; ++u)
                if (kFlatShoeSmall[v2 * FLATSHOESMALL_W + u])
                    Px(r->x + 5 + u, fy + (fh - FLATSHOESMALL_H) / 2 + v2,
                       sStyleBall ? BallShoeColour(kFlatShoeSmall[v2 * FLATSHOESMALL_W + u]) : kFlatShoeSmall[v2 * FLATSHOESMALL_W + u]);
        LhSwitch(r->x + 34, fy + (fh - 15) / 2, selected);
        break;
    default: {
        char name[16] = {0};
        const char *label = id == PT_POKEDEX ? CTR_TR("Pok\xC3\xA9" "dex", "Pok\xC3\xA9" "dex", "Pok\xc3\xa9" "dex")
                          : id == PT_POKEMON ? CTR_TR("Pok\xC3\xA9mon", "Pok\xC3\xA9mon", "Pok\xc3\xa9" "mon")
                          : id == PT_BAG ? CTR_TR("Bag", "Mochila", "Beutel")
                          : id == PT_SAVE ? CTR_TR("Save", "Guardar", "Sichern") : id == PT_OPTIONS ? CTR_TR("Option", "Ajustes", "Option") : name;

        if (id == PT_CARD) {
            /* The player's name, as typed. */
            int n = 0;

            for (; n < (int)sizeof(name) - 1 && v->playerName[n]; ++n) {
                name[n] = v->playerName[n];
            }
            name[n] = 0;
        }
        if (sLayoutHome) {
            /* No label: the icon alone, centred on the key's face. */
            EmIcon(ColumnIcon(id), r->x + (r->w - EMBER_ICON) / 2, fy + (fh - EMBER_ICON) / 2, on);
            break;
        }
        EmIcon(ColumnIcon(id), r->x + 4, fy + (fh - EMBER_ICON) / 2, on);
        UiText(label, r->x + 23, fy + (fh - UI_TEXT_H) / 2, pal->ink, pal->inkSh, 1, r->x + r->w - 2);
        break;
    }
    }
}

int CtrPokeTouch_HitTest(int x, int y)
{
    if (sBallFull)
        return PT_NONE;                                 /* a whole-screen view has no side columns */
    for (int i = 0; i < PT_COUNT; ++i) {
        const CtrPokeTouchRect *r = &gCtrPokeTouchRects[i];
        if (x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h)
            return i;
    }
    return PT_NONE;
}

/* The viewport shows the GBA picture 1:1. */
bool CtrPokeTouch_ViewportToGba(int x, int y, int *gx, int *gy)
{
    x -= sLayoutHome ? PT_HOME_X : PT_VIEW_X;
    y -= sLayoutHome ? PT_HOME_Y : PT_VIEW_Y;
    if (x < 0 || y < 0 || x >= PT_VIEW_W || y >= PT_VIEW_H)
        return false;
    *gx = x;
    *gy = y;
    return true;
}

static int Changed(int id, const CtrPokeTouchView *a, const CtrPokeTouchView *b);
static void Mark(CtrDirtyList *dirty, const CtrPokeTouchRect *r);
static void MapRestoreRect(const CtrPokeTouchRect *r, CtrDirtyList *dirty);

/* ---------------------------------------------------------------- HOME's layout
 * The map in the middle (PT_HOME_X/Y, 240x160 1:1 in FireRed's window frame); round it: BACK (grey, nothing to go back from), the status
 * plate (the money and the turbo), the clock and RUN in the top band, the column split in the two side bands, the note under the map. */
static const CtrPokeTouchRect sHomeBack = {4, 2, 58, 33};
static const CtrPokeTouchRect sHomeStatus = {65, 2, 116, 33};
/* The Poke Ball style's HOME has no BACK (there is nothing to go back from there): its plate takes BACK's place too and shows the money and the turbo
 * and, while one is in force, the Repel at the same time. sHomeWide says that HOME, with this plate, is what is on the screen (for its touch). */
static const CtrPokeTouchRect sHomeStatusWide = {4, 2, 177, 33};
static int sHomeWide;
static const CtrPokeTouchRect sHomeClock = {184, 2, 58, 33};
static const CtrPokeTouchRect sHomeMapArea = {PT_HOME_X - 3, PT_HOME_Y - 3, PT_VIEW_W + 6, PT_VIEW_H + 6};
static const CtrPokeTouchRect sHomeNote = {0, 203, W, H - 203};   /* below the map's frame (its last row is 202) */
#define HN_X 4
#define HN_Y 203
#define HN_W 312
#define HN_H 34
static void NoteWindow(void);

/* Whether the view is HOME with nothing over the map: the layout with the map in the middle. */
/* A screen drawn in HOME's layout (the band on top, the columns at the sides, a floor in the middle, a note under it): the SAVE card. */
/* The Poke Ball Bag (and its cases, a mart, the item PC) takes the whole lower screen. */
static int BallBagActive(const CtrPokeTouchView *v)
{
    return sStyleBall && v->bag.on && !v->viewport && !v->options && !v->save && !v->ec.on && !v->name.on && !v->dex.on && !v->card.on
        && !v->learn.on && !v->relearn.on && !v->mail.on && !v->choice.on && !v->box.on;
}

static void BallBagScreen(const CtrPokeTouchView *v);
static void BallChoiceScreen(const CtrPokeTouchView *v);
static void BallOptionsScreen(const CtrPokeTouchView *v);

/* The Poke Ball OPTIONS (its rows of a page, the PAGE header, under the look rows the look's preview, and the keypad of the private room's code in
 * place of the rows) take the whole lower screen. */
static int BallOptionsActive(const CtrPokeTouchView *v)
{
    return sStyleBall && v->options && !v->viewport;
}

/* The Poke Ball choice of the field (not the trade's cards nor the chat's keyboard) takes the whole lower screen too. */
static int BallChoiceActive(const CtrPokeTouchView *v)
{
    return sStyleBall && v->choice.on && !v->trade.on && !v->chat.on && !v->viewport && !v->options && !v->save && !v->ec.on && !v->name.on
        && !v->dex.on && !v->card.on && !v->learn.on && !v->relearn.on && !v->mail.on;
}

/* The Poke Ball SAVE takes the whole lower screen (3ds_ball_menus_impl.h). */
static void BallSaveScreen(const CtrPokeTouchView *v);
static int BallSaveActive(const CtrPokeTouchView *v)
{
    return sStyleBall && v->save && !v->viewport && !v->options;
}

/* The Poke Ball Trainer Card takes the whole lower screen too (a tap on its card turns it over). */
static const CtrPokeTouchRect sBallCardR = {8, 8, 304, 148};
static void BallCardScreen(const CtrPokeTouchView *v);
static int BallCardActive(const CtrPokeTouchView *v)
{
    return sStyleBall && v->card.on && !v->viewport && !v->options && !v->save;
}

/* The Poke Ball Pokedex takes the whole lower screen as well (its AREA page, FireRed's own picture, is the viewport and stays as it is). */
static void BallDexScreen(const CtrPokeTouchView *v);
static int BallDexActive(const CtrPokeTouchView *v)
{
    return sStyleBall && v->dex.on && !v->viewport && !v->options && !v->save && !v->card.on;
}

/* The Poke Ball move to forget and move relearner take the whole lower screen (3ds_ball_learn_impl.h). */
static void BallLearnScreen(const CtrPokeTouchView *v);
static void BallRelearnScreen(const CtrPokeTouchView *v);
static int BallLearnActive(const CtrPokeTouchView *v)
{
    return sStyleBall && v->learn.on && !v->viewport && !v->options && !v->save && !v->relearn.on;
}

static int BallRelearnActive(const CtrPokeTouchView *v)
{
    return sStyleBall && v->relearn.on && !v->viewport && !v->options && !v->save;
}

/* The Poke Ball PC box, the trade's cards, the chat's keyboard (they keep their layout, in the middle of the screen) and the letter. */
static void BallShiftedScreen(const CtrPokeTouchView *v);
static void BallMailScreen(const CtrPokeTouchView *v);
static int BallShiftedActive(const CtrPokeTouchView *v)
{
    return sStyleBall && !v->viewport && !v->options && !v->save && !v->ec.on && !v->name.on && !v->learn.on && !v->relearn.on && !v->mail.on
        && (v->box.on || (v->choice.on && (v->trade.on || v->chat.on)));
}

static int BallMailActive(const CtrPokeTouchView *v)
{
    return sStyleBall && v->mail.on && !v->viewport && !v->options && !v->save && !v->ec.on && !v->name.on;
}

static int BallFullView(const CtrPokeTouchView *v)
{
    return BallBagActive(v) || BallChoiceActive(v) || BallOptionsActive(v) || BallSaveActive(v) || BallCardActive(v) || BallDexActive(v)
        || BallLearnActive(v) || BallRelearnActive(v) || BallShiftedActive(v) || BallMailActive(v);
}

static int NewMenu(const CtrPokeTouchView *v)
{
    return ((v->save || v->options || v->bag.on || v->dex.on || v->card.on || v->learn.on || v->relearn.on || v->mail.on || v->choice.on
             || v->box.on) && !v->viewport && !v->ec.on && !v->name.on) || (v->viewport && v->pictureHome);
}

static int IsHome(const CtrPokeTouchView *v)
{
    return v->home && !v->viewport && !v->options && !v->save && !v->ec.on && !v->name.on && !v->dex.on && !v->card.on && !v->learn.on
        && !v->relearn.on && !v->mail.on && !v->choice.on && !v->box.on && !v->bag.on;
}

/* The RUN shoe on the Poke Ball style's dark-red pill: only its red parts are yellow (its blue-grey body and white laces are as they were). */
static uint16_t BallShoeColour(uint16_t c)
{
    switch (c) {
    case 0xe1c6: return UI_RGB(255, 214, 64);           /* red: yellow */
    case 0xfc0d: return UI_RGB(255, 232, 120);
    case 0xa904: return UI_RGB(200, 150, 10);
    default: return c;
    }
}

static void HomeBack(void)
{
    const CtrPokeTouchRect *r = &sHomeBack;
    int fy, fh = r->h - BTN_DEPTH;

    TsBgRect(r->x, r->y, r->w, r->h + 2);
    Em2CardShadow(r->x, r->y, r->w, r->h);
    if (sStyleBall) {                                   /* Poke Ball: a dimmed red key (nothing to go back from), its word pale */
        UiRoundFill(r->x, r->y, r->w, r->h, 8, UI_RGB(120, 16, 20));
        UiRoundFill(r->x + 2, r->y + 2, r->w - 4, r->h - 4, 6, UI_RGB(156, 40, 44));
        fy = r->y + BTN_DEPTH / 2;
    } else
        fy = EmButton(r->x, r->y, r->w, r->h, &kEmGrey, BTN_DEPTH, 0);
    {
        const uint16_t ink = sStyleBall ? UI_RGB(226, 160, 160) : kEmGrey.ink, inkSh = sStyleBall ? UI_RGB(156, 40, 44) : kEmGrey.inkSh;
        const char *word = CTR_TR("Back", "Atr\xC3\xA1s", "Zur\xc3\xbc" "ck");
        int x = sStyleBall ? r->x + (r->w - (10 + UiTextWidth(word, 1))) / 2 : r->x + 8;    /* Poke Ball: the arrow and the word centred */

        LlArrow(x, fy + (fh - 10) / 2, 6, 10, 2, ink);
        UiText(word, x + 10, fy + (fh - UI_TEXT_H) / 2, ink, inkSh, 1, r->x + r->w - 2);
    }
}

/* The status plate: the money, and the turbo on a chip at its right. */
static void HomeStatus(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sHomeStatus;
    char money[24], turbo[8];
    unsigned m = v->homeMoney;
    int chipW = 26, chipX = r->x + r->w - chipW - 5, fy;

    TsBgRect(r->x, r->y, r->w, r->h + 2);
    LlWindow(r->x, r->y, r->w, r->h);
    if (!NewMenu(v)) {
        /* The plate opens the panel: a small chevron at its lower edge says so. */
        int cx = r->x + r->w / 2;

        for (int i = 0; i < 3; ++i)
            Fill(cx - 3 + i, r->y + r->h - 5 + i, 7 - 2 * i, 1, sStyleBall ? BL_REDTXT : UI_RGB(120, 172, 232));
    }
    if (v->homeRepelSteps) {
        /* What is in force: the Repel in use, the steps it has left and a bar of how much is left of its length. */
        static const char *const kName[3] = {CTR_TR("Repel", "Repelente", "Schutz"), CTR_TR("Super Repel", "Superrep.", "Superschutz"),
                                              CTR_TR("Max Repel", "Máx. Rep.", "Top-Schutz")};
        int kind = v->homeRepelKind > 2 ? 0 : v->homeRepelKind, barW = r->w - 36;
        int total = v->homeRepelTotal ? v->homeRepelTotal : v->homeRepelSteps, fill;

        ItemArt(ICON_REPEL + kind, v->homeRepelItem[kind], r->x + 5, r->y + 6, 1);
        SmallText(kName[kind], r->x + 28, r->y + 1, EM_DIM, EM_DARK_SH, r->x + r->w - 4);
        snprintf(money, sizeof(money), CTR_TR("%u steps", "%u pasos", "%u Schritte"), (unsigned)v->homeRepelSteps);
        Em2Text(money, r->x + 28, r->y + 13, EM_DARK, EM_DARK_SH, r->x + r->w - 4);
        fill = (int)((unsigned)barW * v->homeRepelSteps / (unsigned)(total > 0 ? total : 1));
        if (fill > barW)
            fill = barW;
        UiRoundFill(r->x + 28, r->y + 27, barW, 3, 1, sStyleBall ? UI_RGB(120, 16, 20) : LL_SLATE);
        if (fill > 0)
            Fill(r->x + 28 + 1, r->y + 28, fill > 2 ? fill - 2 : fill, 1, UI_RGB(96, 214, 120));
        return;
    }
    if (m >= 1000000)
        snprintf(money, sizeof(money), "%u.%03u.%03u", m / 1000000, m / 1000 % 1000, m % 1000);
    else if (m >= 1000)
        snprintf(money, sizeof(money), "%u.%03u", m / 1000, m % 1000);
    else
        snprintf(money, sizeof(money), "%u", m);
    SmallText(CTR_TR("Money", "Dinero", "Geld"), r->x + 7, r->y + 1, EM_DIM, EM_DARK_SH, chipX - 2);
    Em2Text(money, r->x + 7, r->y + 18, EM_DARK, EM_DARK_SH, chipX - 2);
    snprintf(turbo, sizeof(turbo), "x%u", v->homeTurbo > 1 ? (unsigned)v->homeTurbo : 1u);
    fy = EmButton(chipX, r->y + 9, chipW, 17, v->homeTurbo > 1 ? &kEmFlame : &kEmCream, 1, 0);
    EmLabel(turbo, chipX, chipW, fy, 15, v->homeTurbo > 1 ? &kEmFlame : &kEmCream);
}

/* The Poke Ball HOME's wide plate: the money on the left, the Repel in force (its icon, the steps it has left and a bar) in the middle, the turbo's chip
 * at the right; with no Repel the money has the plate to itself. A tap on it opens the panel, as on the narrow one. */
static void HomeStatusWide(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sHomeStatusWide;
    char money[24], turbo[8], steps[24];
    unsigned m = v->homeMoney;
    int chipW = 26, chipX = r->x + r->w - chipW - 5, moneyRight = v->homeRepelSteps ? r->x + 62 : chipX - 2, fy;

    TsBgRect(r->x, r->y, r->w, r->h + 2);
    LlWindow(r->x, r->y, r->w, r->h);
    {
        int cx = v->homeRepelSteps ? r->x + 32 : r->x + r->w / 2;   /* the plate opens the panel: the chevron at its lower edge, under the money when the Repel's bar is there */

        for (int i = 0; i < 3; ++i)
            Fill(cx - 3 + i, r->y + r->h - 5 + i, 7 - 2 * i, 1, BL_REDTXT);
    }
    if (m >= 1000000)
        snprintf(money, sizeof(money), "%u.%03u.%03u", m / 1000000, m / 1000 % 1000, m % 1000);
    else if (m >= 1000)
        snprintf(money, sizeof(money), "%u.%03u", m / 1000, m % 1000);
    else
        snprintf(money, sizeof(money), "%u", m);
    SmallText(CTR_TR("Money", "Dinero", "Geld"), r->x + 7, r->y + 1, EM_DIM, EM_DARK_SH, moneyRight);
    Em2Text(money, r->x + 7, r->y + 18, EM_DARK, EM_DARK_SH, moneyRight);
    if (v->homeRepelSteps) {
        static const char *const kName[3] = {CTR_TR("Repel", "Repelente", "Schutz"), CTR_TR("Super Repel", "Superrep.", "Superschutz"),
                                              CTR_TR("Max Repel", "M\xC3\xA1x. Rep.", "Top-Schutz")};
        int kind = v->homeRepelKind > 2 ? 0 : v->homeRepelKind, x = r->x + 68, textX = x + 24, barW = chipX - 4 - textX;
        int total = v->homeRepelTotal ? v->homeRepelTotal : v->homeRepelSteps, fill;

        Fill(r->x + 64, r->y + 6, 1, r->h - 12, UI_RGB(230, 224, 224));      /* a hairline between the money and the Repel */
        ItemArt(ICON_REPEL + kind, v->homeRepelItem[kind], x, r->y + 6, 1);
        SmallText(kName[kind], textX, r->y + 1, EM_DIM, EM_DARK_SH, chipX - 4);
        /* German "Schritte" leaves no room beside the Repel's icon on this plate, so it is shortened there; English and Spanish fit whole. */
        snprintf(steps, sizeof(steps), CTR_TR("%u steps", "%u pasos", "%u Schr."), (unsigned)v->homeRepelSteps);
        Em2Text(steps, textX, r->y + 13, EM_DARK, EM_DARK_SH, chipX - 4);
        fill = (int)((unsigned)barW * v->homeRepelSteps / (unsigned)(total > 0 ? total : 1));
        if (fill > barW)
            fill = barW;
        UiRoundFill(textX, r->y + 27, barW, 3, 1, UI_RGB(120, 16, 20));
        if (fill > 0)
            Fill(textX + 1, r->y + 28, fill > 2 ? fill - 2 : fill, 1, UI_RGB(96, 214, 120));
    }
    snprintf(turbo, sizeof(turbo), "x%u", v->homeTurbo > 1 ? (unsigned)v->homeTurbo : 1u);
    fy = EmButton(chipX, r->y + 9, chipW, 17, v->homeTurbo > 1 ? &kEmFlame : &kEmCream, 1, 0);
    EmLabel(turbo, chipX, chipW, fy, 15, v->homeTurbo > 1 ? &kEmFlame : &kEmCream);
}

/* The clock plate: a small dial and the time of day. */
static void HomeClock(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sHomeClock;
    char t[8];
    int cx = r->x + 12, cy = r->y + r->h / 2;

    TsBgRect(r->x, r->y, r->w, r->h + 2);
    LlWindow(r->x, r->y, r->w, r->h);
    for (int dy = -6; dy <= 6; ++dy)
        for (int dx = -6; dx <= 6; ++dx) {
            int d2 = dx * dx + dy * dy;

            if (d2 <= 36)
                Px(cx + dx, cy + dy, sStyleBall ? (d2 > 25 ? UI_RGB(150, 22, 26) : d2 > 16 ? UI_RGB(244, 226, 226) : BL_WHITE)
                                                : d2 > 25 ? LL_SLATE : d2 > 16 ? UI_RGB(255, 226, 150) : UI_RGB(255, 250, 230));
        }
    Fill(cx, cy - 4, 1, 4, sStyleBall ? UI_RGB(150, 22, 26) : LL_SLATE);     /* Poke Ball: a white dial, its rim and hands red */
    Fill(cx, cy, 3, 1, sStyleBall ? BL_REDTXT : UI_RGB(214, 70, 28));
    if (v->homeClock < 0)
        return;
    snprintf(t, sizeof(t), "%02d:%02d", v->homeClock / 60 % 24, v->homeClock % 60);
    Em2Text(t, r->x + 22, r->y + (r->h - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, r->x + r->w - 3);
}

/* The map in FireRed's frame (the sea's blue while its image is built). */
static void HomeMapDraw(const CtrPokeTouchView *v)
{
    TsBgRect(sHomeMapArea.x, sHomeMapArea.y, sHomeMapArea.w, sHomeMapArea.h);
    HomeFrame(PT_HOME_X, PT_HOME_Y, PT_VIEW_W, PT_VIEW_H);
    if (!HomeShow(v))
        Fill(PT_HOME_X, PT_HOME_Y, PT_VIEW_W, PT_VIEW_H, C_VIEW_EMPTY);
}

/* The note under the map: the subject and the line that says more; FLY at the subject's right when the destination can be Flown to. */
static void FDTiles(const unsigned char *tiles, int w, int h, uint8_t *px);
static void FDPal(const unsigned short *in, uint16_t *out);

/* One of the zone's Pokemon in the note: a box with its icon (a question mark while unseen) and, once caught, a small ball. */
#define ZN_SLOT 36
static void ZoneSlot(int x, int species, int flags)
{
    static unsigned char tiles[1024];
    static unsigned short pal[16];
    static uint8_t px[32 * 32];
    static uint16_t pal565[16];
    int dy = sStyleBall ? 5 : 0, y = HN_Y + 3 + dy;     /* the Poke Ball note is the white under an 8 px dark band: its middle is lower */

    UiRoundFill(x, y, 32, 28, 5, UI_RGB(196, 184, 158));
    UiRoundFill(x + 1, y + 1, 30, 26, 4, UI_RGB(238, 230, 212));
    if (!(flags & 3)) {
        Em2Text("?", x + 12, y + 5, UI_RGB(170, 160, 140), UI_RGB(226, 220, 208), x + 32);
        return;
    }
    if (CtrPokeTouch_MonIcon((unsigned)species, 0, 0, tiles, pal)) {
        FDTiles(tiles, 32, 32, px);
        FDPal(pal, pal565);
        UiImage(px, pal565, 32, 32, 32, x, HN_Y + 1 + dy, 1, 1);
    }
    if (flags & 2) {
        UiRoundFill(x + 24, y + 1, 7, 7, 3, UI_RGB(90, 20, 14));
        UiRoundFill(x + 25, y + 2, 5, 5, 2, UI_RGB(230, 60, 40));
        Fill(x + 25, y + 4, 5, 1, UI_RGB(90, 20, 14));
    }
}

static void HomeNote(const CtrPokeTouchView *v)
{
    int right = HN_X + HN_W - (sStyleBall ? 12 : 10), titleRight = right;   /* Poke Ball: FLY as far from the right edge as the name from the left */
    const char *line = NULL;
    int zones = v->zoneN > PT_ZONE_MAX ? PT_ZONE_MAX : v->zoneN;
    const int dy = sStyleBall ? 5 : 0;                  /* the Poke Ball note is the white under an 8 px dark band: what is in it sits lower */

    sFlyButton.w = 0;
    TsBgRect(sHomeNote.x, sHomeNote.y, sHomeNote.w, sHomeNote.h);
    NoteWindow();
    if (sStyleBall)
        UiRoundFill(0, HN_Y + 4, W, 4, 0, BL_BAND);     /* HOME's band is 8 px, as on the whole-screen views */
    for (int i = 1; i < PT_BAR_LINES && !line; ++i)
        if (v->bar[i][0] && !(v->flyOk && i == 1))
            line = v->bar[i];
    if (v->flyOk) {
        const char *fly = CTR_TR("Fly", "Volar", "Fliegen");
        int pw = UiTextWidth(fly, 1) + 16, px, fy;

        if (pw < 40)
            pw = 40;
        px = right - pw;
        const EmPal *flyPal = sStyleBall ? &kEmCream : &kEmFlame;      /* Poke Ball: a red button with a white word */

        fy = EmButton(px, HN_Y + 8 + dy, pw, 18, flyPal, 1, 0);
        sFlyButton = (CtrPokeTouchRect){(int16_t)px, (int16_t)(HN_Y + 8 + dy), (int16_t)pw, 18};
        EmLabel(fly, px, pw, fy + (sStyleBall ? 1 : 0), 17, flyPal);   /* the flat pill's text was a pixel high */
        titleRight = px - 4;
    }
    if (zones && !v->flyOk) {
        /* Where the player is: the zone's name, what it holds, and the Pokemon the tables can give (a slot each). */
        int left = HN_X + HN_W - 6 - zones * ZN_SLOT + 4;

        for (int i = 0; i < zones; ++i)
            ZoneSlot(left + i * ZN_SLOT, v->zoneSpecies[i], v->zoneFlags[i]);
        titleRight = left - 4;
        line = v->zoneWater ? CTR_TR("In the water", "En el agua", "Im Wasser") : CTR_TR("Pokémon seen here", "Pokémon vistos aquí", "Hier gesehene Pokémon");
    }
    if (!zones && !v->flyOk && v->homeStage && v->bar[1][0]) {
        /* Nothing wild here: after the League, the next step of the Sevii postgame, and once the gems are restored the national Pokedex. */
        static const char *const kHint[3] = {
            CTR_TR("Postgame: explore the Sevii Islands", "Postgame: explora las Islas Sete", "Postgame: erkunde die Sevii-Inseln"),
            CTR_TR("You have the Ruby: find the Sapphire", "Tienes el Rubí: busca el Zafiro", "Du hast den Rubin: finde den Saphir"),
            CTR_TR("Both gems: go and see Celio", "Las dos gemas: ve a ver a Celio", "Beide Edelsteine: besuche Celio")};

        if (v->homeStage < 4) {
            line = kHint[v->homeStage - 1];
        } else {
            char count[16];
            int bx = HN_X + 140, bw = right - bx;
            unsigned dex = v->homeDex > 386 ? 386 : v->homeDex;

            line = CTR_TR("National Poké" "dex", "Poké" "dex Nacional", "Nationaler Poké" "dex");
            snprintf(count, sizeof(count), "%u / 386", dex);
            SmallText(CTR_TR("Caught", "Capturados", "Gefangen"), bx, HN_Y + 1 + dy, EM_DIM, EM_DARK_SH, right);
            SmallText(count, right - SmallWidth(count), HN_Y + 1 + dy, EM_DARK, EM_DARK_SH, right);
            UiRoundFill(bx, HN_Y + 20 + dy, bw, 6, 3, LL_SLATE);
            if (dex)
                Fill(bx + 2, HN_Y + 22 + dy, (bw - 4) * (int)dex / 386, 2, UI_RGB(96, 214, 120));
            titleRight = bx - 6;
        }
    }
    if (line) {
        Em2Text(v->bar[0], HN_X + 12, HN_Y + 5 + dy + (sStyleBall ? 2 : 0), EM_DARK, EM_DARK_SH, titleRight);
        SmallText(line, HN_X + 12, HN_Y + 17 + dy + (sStyleBall ? 2 : 0), EM_DIM, EM_DARK_SH, titleRight);
    } else if (v->bar[0][0]) {
        /* the name alone: in the middle of the note (the Poke Ball one starts under its 4 px band) */
        Em2Text(v->bar[0], HN_X + 12, sStyleBall ? HN_Y + 8 + (H - HN_Y - 8 - UI_TEXT_H) / 2 : HN_Y + (HN_H - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, titleRight);
    }
}

/* The status panel over the map: the turbo speeds, the three Repels (a tap on one uses it, while none is in force) and the money. */
#define PN_X 44
#define PN_Y 44
#define PN_W 232
#define PN_H 148
#define PN_TURBO_Y (PN_Y + 18)
#define PN_REPEL_Y (PN_Y + 62)

static CtrPokeTouchRect PanelTurbo(int k)
{
    return (CtrPokeTouchRect){(int16_t)(PN_X + 10 + k * 53), PN_TURBO_Y, 49, 22};
}

static CtrPokeTouchRect PanelRepel(int k)
{
    return (CtrPokeTouchRect){(int16_t)(PN_X + 10 + k * 71), PN_REPEL_Y, 67, 44};
}

bool CtrPokeTouch_HomeLayout(void)
{
    return sLayoutHome != 0;
}

bool CtrPokeTouch_HomeStatusHit(int x, int y)
{
    const CtrPokeTouchRect *r = sHomeWide ? &sHomeStatusWide : &sHomeStatus;

    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h + 2;
}

int CtrPokeTouch_HomePanelHit(int x, int y)
{
    for (int k = 0; k < 4; ++k) {
        CtrPokeTouchRect r = PanelTurbo(k);

        if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h)
            return PT_PANEL_TURBO + k;
    }
    for (int k = 0; k < 3; ++k) {
        CtrPokeTouchRect r = PanelRepel(k);

        if (x >= r.x && y >= r.y && x < r.x + r.w && y < r.y + r.h)
            return PT_PANEL_REPEL + k;
    }
    if (x >= PN_X && y >= PN_Y && x < PN_X + PN_W && y < PN_Y + PN_H)
        return PT_PANEL_NONE;               /* on the panel, on nothing */
    return PT_PANEL_OUT;
}

static void HomePanel(const CtrPokeTouchView *v)
{
    static const char *const kName[3] = {CTR_TR("Repel", "Repelente", "Schutz"), CTR_TR("Super Repel", "Superrep.", "Superschutz"),
                                          CTR_TR("Max Repel", "Máx. Rep.", "Top-Schutz")};
    static const unsigned kSteps[3] = {100, 200, 250};
    char text[24];
    unsigned m = v->homeMoney;

    /* the map dims under it */
    for (int y = PT_HOME_Y; y < PT_HOME_Y + PT_VIEW_H; ++y)
        for (int x = PT_HOME_X; x < PT_HOME_X + PT_VIEW_W; ++x)
            if (x < PN_X - 3 || x >= PN_X + PN_W + 3 || y < PN_Y - 3 || y >= PN_Y + PN_H + 3)
                Px(x, y, UiMix(sCanvas[(size_t)x * H + (H - 1 - (size_t)y)], UI_RGB(10, 6, 20), 3));
    LlWindow(PN_X, PN_Y, PN_W, PN_H);
    SmallText(CTR_TR("Turbo", "Turbo", "Turbo"), PN_X + 10, PN_Y + 2, EM_DIM, EM_DARK_SH, PN_X + PN_W);
    for (int k = 0; k < 4; ++k) {
        CtrPokeTouchRect r = PanelTurbo(k);
        unsigned speed = k == 0 ? 0 : (unsigned)k + 1;
        int lit = (v->homeTurbo > 1 ? v->homeTurbo : 0) == speed, fy;
        const EmPal *pal = lit ? &kEmFlame : &kEmCream;

        snprintf(text, sizeof(text), "x%u", speed);
        fy = EmButton(r.x, r.y, r.w, r.h, pal, 1, 0);
        EmLabel(k == 0 ? CTR_TR("Off", "No", "Aus") : text, r.x, r.w, fy, r.h - 1, pal);
    }
    Fill(PN_X + 8, PN_Y + 46, PN_W - 16, 1, UI_RGB(214, 206, 190));
    SmallText(CTR_TR("Repel", "Repelente", "Schutz"), PN_X + 10, PN_Y + 46, EM_DIM, EM_DARK_SH, PN_X + PN_W);
    if (v->homeRepelSteps) {
        snprintf(text, sizeof(text), CTR_TR("%u steps left", "Quedan %u pasos", "Noch %u Schritte"), (unsigned)v->homeRepelSteps);
        SmallText(text, PN_X + PN_W - 10 - UiTextWidth(text, 1), PN_Y + 46, EM_DIM, EM_DARK_SH, PN_X + PN_W - 2);
    }
    for (int k = 0; k < 3; ++k) {
        CtrPokeTouchRect r = PanelRepel(k);
        int can = v->homeRepelHave[k] > 0 && !v->homeRepelSteps, fy;
        const EmPal *pal = can ? &kEmCream : &kEmGrey;

        fy = EmButton(r.x, r.y, r.w, r.h, pal, 2, 0);
        ItemArt(ICON_REPEL + k, v->homeRepelItem[k], r.x + 4, fy + 3, can);
        snprintf(text, sizeof(text), "x%u", (unsigned)v->homeRepelHave[k]);
        SmallText(text, r.x + 27, fy - 1, pal->ink, pal->inkSh, r.x + r.w - 2);
        snprintf(text, sizeof(text), CTR_TR("%u st.", "%u p.", "%u S."), kSteps[k]);
        SmallText(text, r.x + 27, fy + 10, EM_DIM, pal->inkSh, r.x + r.w - 2);
        SmallText(kName[k], r.x + 4, fy + 24, pal->ink, pal->inkSh, r.x + r.w - 2);
    }
    Fill(PN_X + 8, PN_Y + 112, PN_W - 16, 1, UI_RGB(214, 206, 190));
    SmallText(CTR_TR("Money", "Dinero", "Geld"), PN_X + 10, PN_Y + 112, EM_DIM, EM_DARK_SH, PN_X + PN_W);
    if (m >= 1000000)
        snprintf(text, sizeof(text), "%u.%03u.%03u", m / 1000000, m / 1000 % 1000, m % 1000);
    else if (m >= 1000)
        snprintf(text, sizeof(text), "%u.%03u", m / 1000, m % 1000);
    else
        snprintf(text, sizeof(text), "%u", m);
    Em2Text(text, PN_X + 10, PN_Y + 124, EM_DARK, EM_DARK_SH, PN_X + PN_W - 10);
}

static void HomeDraw(const CtrPokeTouchView *v)
{
    SetLayout(1);
    Frame();
    sHomeWide = sStyleBall;
    if (sHomeWide)
        HomeStatusWide(v);                              /* Poke Ball: no BACK on HOME, the plate takes its place */
    else {
        HomeBack();
        HomeStatus(v);
    }
    HomeClock(v);
    HomeMapDraw(v);
    HomeNote(v);
    for (int i = 0; i < PT_COUNT; ++i)
        Button(i, v);
    if (v->homePanel)
        HomePanel(v);
}

static void HomeUpdate(const CtrPokeTouchView *old, const CtrPokeTouchView *v, CtrDirtyList *dirty)
{
    if (old->homePanel || v->homePanel) {
        /* The panel is over the map: it opens, closes or changes with a full redraw; nothing under it is kept up to date meanwhile. */
        if (old->homePanel != v->homePanel || old->homeTurbo != v->homeTurbo || old->homeMoney != v->homeMoney
            || old->homeRepelSteps != v->homeRepelSteps || old->homeRepelKind != v->homeRepelKind
            || memcmp(old->homeRepelHave, v->homeRepelHave, sizeof(v->homeRepelHave))) {
            HomeDraw(v);
            CtrDirty_Add(dirty, 0, 0, W, H);
        }
        return;
    }
    if (old->mapGroup != v->mapGroup || old->mapVersion != v->mapVersion || old->mapZoom != v->mapZoom || old->mapCX != v->mapCX
        || old->mapCY != v->mapCY || old->mapReady != v->mapReady || old->mapOpen != v->mapOpen) {
        HomeMapDraw(v);
        Mark(dirty, &sHomeMapArea);
    } else if (old->playerX != v->playerX || old->playerY != v->playerY || old->playerFemale != v->playerFemale
               || old->cursorX != v->cursorX || old->cursorY != v->cursorY || old->cursorFrame != v->cursorFrame
               || ((old->cursorX >= 0 || v->cursorX >= 0) && strcmp(old->bar[0], v->bar[0]))) {
        int player = old->playerX != v->playerX || old->playerY != v->playerY || old->playerFemale != v->playerFemale;
        int cursor = old->cursorX != v->cursorX || old->cursorY != v->cursorY || old->cursorFrame != v->cursorFrame;
        int label = old->cursorX != v->cursorX || old->cursorY != v->cursorY || strcmp(old->bar[0], v->bar[0]);
        const CtrPokeTouchView *views[2] = {old, v};
        CtrPokeTouchRect r;

        HomeSetView(v);
        for (int i = 0; i < 2; ++i) {
            if (player && PlayerRect(views[i], &r))
                MapRestoreRect(&r, dirty);
            if (cursor && RingRect(views[i], &r))
                MapRestoreRect(&r, dirty);
            if (label && LabelRect(views[i], &r))
                MapRestoreRect(&r, dirty);
        }
        HomeOverlay(v);
    }
    if (memcmp(old->bar, v->bar, sizeof(v->bar)) || old->flyOk != v->flyOk || old->zoneN != v->zoneN || old->zoneWater != v->zoneWater
        || old->homeStage != v->homeStage || old->homeDex != v->homeDex
        || memcmp(old->zoneSpecies, v->zoneSpecies, sizeof(v->zoneSpecies)) || memcmp(old->zoneFlags, v->zoneFlags, sizeof(v->zoneFlags))) {
        HomeNote(v);
        Mark(dirty, &sHomeNote);
    }
    if (old->homeMoney != v->homeMoney || old->homeTurbo != v->homeTurbo || old->homeRepelSteps != v->homeRepelSteps
        || old->homeRepelKind != v->homeRepelKind) {
        CtrPokeTouchRect r = Under(sHomeWide ? &sHomeStatusWide : &sHomeStatus);

        if (sHomeWide)
            HomeStatusWide(v);
        else
            HomeStatus(v);
        Mark(dirty, &r);
    }
    if (old->homeClock != v->homeClock) {
        CtrPokeTouchRect r = Under(&sHomeClock);

        HomeClock(v);
        Mark(dirty, &r);
    }
    for (int i = 0; i < PT_COUNT; ++i) {
        if (!Changed(i, old, v))
            continue;
        Button(i, v);
        {
            CtrPokeTouchRect area = Area(i);

            Mark(dirty, &area);
        }
    }
}

/* ---------------------------------------------------------------- Menus in HOME's layout */
/* Text in the small font, wrapped on spaces (a newline is a space) over at most `maxLines` lines `step` apart. */
static void MenuWrap(const char *s, int x, int top, int width, int maxLines, int step, uint16_t fg, uint16_t sh)
{
    char line[160], trial[160];
    int n = 0, lines = 0;

    while (*s && lines < maxLines) {
        const char *e = s;
        int wl, add;

        while (*e && *e != ' ' && *e != '\n')
            ++e;
        wl = (int)(e - s);
        add = n ? 1 : 0;
        if (wl > 0 && wl < (int)sizeof(trial) - 2 && n + add + wl < (int)sizeof(line) - 1) {
            memcpy(trial, line, (size_t)n);
            if (add)
                trial[n] = ' ';
            memcpy(trial + n + add, s, (size_t)wl);
            trial[n + add + wl] = 0;
            if (n && SmallWidth(trial) > width) {
                line[n] = 0;
                SmallText(line, x, top + lines * step, fg, sh, x + width);
                ++lines;
                n = 0;
                continue;
            }
            memcpy(line, trial, (size_t)(n + add + wl) + 1);
            n += add + wl;
        }
        s = *e ? e + 1 : e;
    }
    if (n && lines < maxLines) {
        line[n] = 0;
        SmallText(line, x, top + lines * step, fg, sh, x + width);
    }
}

/* The compact notes of the native screens, in the note's window under the floor: a title and two small lines, or only text (up to three small lines). */
static void NoteWindow(void)
{
    if (sStyleBall) {                                   /* Poke Ball: a dark band, and the note on the white under it */
        UiRoundFill(0, HN_Y, W, 4, 0, BL_BAND);
        UiRoundFill(0, HN_Y + 4, W, H - HN_Y - 4, 0, BL_WHITE);
        return;
    }
    LlWindow(HN_X, HN_Y, HN_W, HN_H);
    Fill(HN_X + 5, HN_Y + 5, 2, HN_H - 10, UI_RGB(232, 112, 36));
}

static void NoteTitleBody(const char *title, const char *body)
{
    NoteWindow();
    if (body && body[0]) {
        Em2Text(title, HN_X + 12, HN_Y + 2, EM_DARK, EM_DARK_SH, HN_X + HN_W - 12);
        MenuWrap(body, HN_X + 12, HN_Y + 11, HN_W - 24, 2, 9, EM_DIM, EM_DARK_SH);
    } else {
        Em2Text(title, HN_X + 12, HN_Y + (HN_H - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, HN_X + HN_W - 12);
    }
}

static void NoteText(const char *text, uint16_t ink)
{
    NoteWindow();
    MenuWrap(text, HN_X + 12, HN_Y - 1, HN_W - 24, 3, 9, ink, EM_DARK_SH);
}

/* BACK of the top band (a screen with a way back): the slate button, grey when there is nothing to go back to. */
static void BackTop(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sHomeBack;
    const EmPal *pal = v->backEnabled ? &kEmSlate : &kEmGrey;
    int fy, fh = r->h - BTN_DEPTH;

    Em2CardShadow(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, pal, BTN_DEPTH, v->backEnabled && v->backPressed);
    LlArrow(r->x + 8, fy + (fh - 10) / 2, 6, 10, 2, v->backEnabled ? EM_WHITE : pal->ink);
    UiText(CTR_TR("Back", "Atrás", "Zurück"), r->x + 18, fy + (fh - UI_TEXT_H) / 2, pal->ink, pal->inkSh, 1, r->x + r->w - 2);
}

bool CtrPokeTouch_HitBack(int x, int y)
{
    const CtrPokeTouchRect *r = sBallFull ? &sBallExitR : sLayoutHome ? &sHomeBack : &gCtrPokeTouchBack;

    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

/* PAGE of OPTIONS in the plate's place: a dot for each page in that page's colour, the shown one larger with a ring, and the page's name. */
static void OptPageTop(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sHomeStatus;
    int dots = OptDotCount(v), step = dots > 6 ? 8 : 10;
    int fy, x0;

    Em2CardShadow(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, COL_DEPTH, v->optPagePressed);
    x0 = r->x + (r->w - ((dots - 1) * step + 7)) / 2;
    for (int i = 0; i < dots; ++i) {
        uint16_t colour = OptDotColour(v, i);

        if (i == v->optPage)
            Em2Dot(x0 + i * step, fy + 4, colour, 1);
        else
            Em2Dot(x0 + i * step + 1, fy + 5, colour, 0);
    }
    EmLabel(v->optPageName, r->x, r->w, fy + 14, 13, &kEmCream);
}

bool CtrPokeTouch_HitOptPage(int x, int y)
{
    static const CtrPokeTouchRect ballPage = {8, 4, 304, 26};
    const CtrPokeTouchRect *r = sBallFull ? &ballPage : sLayoutHome ? &sHomeStatus : &gCtrPokeTouchOptPage;

    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

int CtrPokeTouch_OptPageDot(int x, int y, int pageCount)
{
    int dots = pageCount > 8 ? 8 : pageCount;          /* as many as BallOptionsScreen draws (up to eight pages) */

    if (!sBallFull || y < 4 || y >= 30)
        return -1;
    for (int d = 0; d < dots; ++d) {
        int left = 312 - (dots - d) * 12 - 2;           /* the dot is 7 px at 312 - (dots - d) * 12 (BallOptionsScreen): 2 px of air each side, 12 in all */

        if (x >= left && x < left + 12)
            return d;
    }
    return -1;
}

#define MN_X PT_HOME_X
#define MN_Y PT_HOME_Y
#define MN_W PT_VIEW_W
#define MN_H PT_VIEW_H

/* The floor in the middle: the map's frame round the dark red it sits on. */
static void MenuFloor(void)
{
    HomeFrame(MN_X, MN_Y, MN_W, MN_H);
    for (int y = 0; y < MN_H; ++y)
        Fill(MN_X, MN_Y + y, MN_W, 1, UI_RGB(84 - 30 * y / (MN_H - 1), 24 - 8 * y / (MN_H - 1), 22 - 8 * y / (MN_H - 1)));
}

/* The top band (BACK, the plate, the clock, RUN), the columns and the floor in the middle. */
static void MenuChrome(const CtrPokeTouchView *v)
{
    SetLayout(1);
    Frame();
    sHomeWide = 0;
    if (v->backVisible)
        BackTop(v);
    else
        HomeBack();
    if (v->options && v->optPageCount > 1)
        OptPageTop(v);
    else if (v->bag.on && v->bag.count > 0)
        FBPositionTop(&v->bag);
    else
        HomeStatus(v);
    HomeClock(v);
    sKeepBg = 1;
    MenuFloor();
    for (int i = 0; i < PT_COUNT; ++i)
        Button(i, v);
}

/* A menu is drawn whole (the band, the columns, the floor and the note) or, when only what is on the floor and in the note changed, those two
 * alone: `sMenuFull` says which (a redraw of the whole screen at every step of a list's scroll made it flicker). */
static int sMenuFull = 1;

static void MenuBegin(const CtrPokeTouchView *v)
{
    if (sMenuFull) {
        MenuChrome(v);
        return;
    }
    SetLayout(1);
    sKeepBg = 0;
    TsBgRect(MN_X - 3, MN_Y - 3, MN_W + 6, MN_H + 6);   /* the corners of the frame leave the backdrop, and the note's glow lies on it */
    sKeepBg = 1;
    MenuFloor();
}

static void MenuNoteBegin(void)
{
    sKeepBg = 0;
    if (!sMenuFull)
        TsBgRect(0, HN_Y, W, H - HN_Y);             /* the note's window and its shadow are laid over the backdrop again */
}

/* The note under the floor: what the screen is (or the selection's name), and one line that says more. */
static void MenuNote(const char *title, const char *body, int frame)
{
    int right = HN_X + HN_W - 10;

    sFlyButton.w = 0;
    NoteWindow();
    if (frame >= 0 && FrameSampleH(frame, HN_X + HN_W - FRAME_W - 6, HN_Y + 2, HN_H - 4))
        right -= FRAME_W + 6;
    if (body && body[0]) {
        Em2Text(title, HN_X + 12, HN_Y + 5, EM_DARK, EM_DARK_SH, right);
        SmallText(body, HN_X + 12, HN_Y + 17, EM_DIM, EM_DARK_SH, right);
    } else {
        Em2Text(title, HN_X + 12, HN_Y + (HN_H - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, right);
    }
}

/* ---- SAVE */
#define SV_X (MN_X + 2)
#define SV_Y (MN_Y + 2)
#define SV_W (MN_W - 4)
#define SV_H 98
#define SV_VALUE_RIGHT 186

static void SaveValueText(int f, const CtrPokeTouchView *v, char *text, size_t size)
{
    switch (f) {
    case SF_NAME:   snprintf(text, size, "%s", v->playerName); break;
    case SF_BADGES: snprintf(text, size, "%u", (unsigned)v->saveBadges); break;
    case SF_DEX:
        if (v->saveHasDex)
            snprintf(text, size, "%u", (unsigned)v->saveDex);
        else
            snprintf(text, size, "---");
        break;
    default:        snprintf(text, size, "%u:%02u", (unsigned)(v->saveHours > 999 ? 999 : v->saveHours),
                             (unsigned)(v->saveMinutes > 59 ? 59 : v->saveMinutes)); break;
    }
}

/* The player's portrait: FireRed's own map icon of Red or Leaf, enlarged by 3, on a round cream plate in a sunk well. */
static void SaveSprite(const CtrPokeTouchView *v)
{
    const int scale = 3, x = 210, y = 75;

    LlWell(200, 62, 68, 74);
    UiRoundFill(x, y, MARKER * scale, MARKER * scale, 6, UI_RGB(250, 242, 222));
    if (sPlayer.key != v->playerFemale)
        IconBuild(&sPlayer, v->playerFemale, CtrPokeTouch_MapPlayerIcon);
    if (!sPlayer.ok)
        return;
    for (int py = 0; py < MARKER; ++py)
        for (int px = 0; px < MARKER; ++px) {
            uint8_t index = sPlayer.pixels[0][py * MARKER + px];

            if (index)
                Fill(x + px * scale, y + py * scale, scale, scale, sPlayer.pal[index]);
        }
}

/* YES cream, NO cream too until chosen; the chosen one is flame with the 2 px ring, a lone OK is flame; the one under a touch sinks.
 * While writing, three flame dots instead. */
static void SaveButtons(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *b = &sSaveButtons;
    int confirm = v->saveState == SAVE_CONFIRM;

    if (v->saveState == SAVE_WRITING) {
        static const int8_t dy[3] = {0, -3, 0};

        for (int i = 0; i < 3; ++i) {
            int cx = b->x + 16 + 24 * i, cy = b->y + 29 + dy[i];

            UiDisc(cx, cy, 5, UI_RGB(138, 22, 16));
            UiDisc(cx, cy, 4, i == 1 ? UI_RGB(255, 176, 72) : UI_RGB(240, 128, 40));
            UiDisc(cx - 1, cy - 1, 2, UI_RGB(255, 226, 170));
            UiDisc(cx - 1, cy - 1, 1, EM_WHITE);
        }
        return;
    }
    for (int which = PT_SAVE_FIRST; which <= (confirm ? PT_SAVE_SECOND : PT_SAVE_FIRST); ++which) {
        CtrPokeTouchRect r = SaveButtonRect(which, v->saveState);
        int sel = !confirm || v->saveSel == which - PT_SAVE_FIRST, press = v->savePressed == which;
        const EmPal *pal = LlPick(sel, &kEmCream);
        int fy;

        if (sel)
            LlSelectRing(r.x, r.y, r.w, r.h);
        fy = EmButton(r.x, r.y, r.w, r.h, pal, 2, press);
        EmLabel(!confirm ? "OK" : which == PT_SAVE_FIRST ? CTR_TR("Yes", "Sí", "Ja") : CTR_TR("No", "No", "Nein"), r.x, r.w, fy, r.h - 2, pal);
    }
}

static void SaveScreen(const CtrPokeTouchView *v)
{
    uint16_t band[20];
    int split = 20 * 42 / 100;
    char text[24], fit[32];

    MenuBegin(v);
    LlWindow(SV_X, SV_Y, SV_W, SV_H);
    /* The flame band across the top of the card (20 tall, no outline: it covers the window's rings). */
    for (int row = 0; row < 20; ++row)
        band[row] = row < split ? Em2Lerp(UI_RGB(255, 196, 90), UI_RGB(255, 154, 56), row, split - 1)
                                : Em2Lerp(UI_RGB(248, 100, 42), UI_RGB(216, 48, 30), row - split, 20 - split - 1);
    EmFillRows(SV_X + 1, SV_Y + 1, SV_W - 2, 20, 4, band, 0, 10);
    for (int row = 10; row < 20; ++row)
        Fill(SV_X + 1, SV_Y + 1 + row, SV_W - 2, 1, band[row]);
    Em2Text(CTR_TR("Save", "Guardar", "Sichern"), SV_X + 11, SV_Y + 1 + (20 - UI_TEXT_H) / 2, EM_WHITE, kEmFlame.inkSh, SV_X + SV_W - 6);
    for (int f = 0; f < SF_COUNT; ++f) {
        int y = SV_Y + 24 + f * 18;

        Em2Text(sSaveLabel[f], SV_X + 12, y, EM_DARK, EM_DARK_SH, 120);
        LlRule(SV_X + 12, y + 16, SV_VALUE_RIGHT - SV_X - 12 + 2);
        SaveValueText(f, v, text, sizeof(text));
        Em2Fit(text, SV_VALUE_RIGHT - 120, fit, sizeof(fit));
        UiText(fit, SV_VALUE_RIGHT - UiTextWidth(fit, 1), y + 2, UI_RGB(48, 104, 216), UI_RGB(192, 208, 240), 1, SV_VALUE_RIGHT + 1);
    }
    SaveSprite(v);
    /* The question, in a second window under the card. */
    LlWindow(MN_X + 2, 146, 154, 52);
    Em2Wrap(v->bar[0], MN_X + 14, 155, 130, 2, 17, EM_DARK, EM_DARK_SH);
    SaveButtons(v);
    MenuNoteBegin();
    MenuNote(CTR_TR("Save", "Guardar", "Sichern"), v->bar[1], -1);
    sKeepBg = 0;
}

static void OptionsScreen(const CtrPokeTouchView *v)
{
    const char *body = v->bar[1][0] ? v->bar[1] : v->bar[2];

    MenuBegin(v);
    if (v->optPad.on)
        OptPad(v);                                      /* the keypad of the private room's code covers the rows */
    else
        for (int pass = 0; pass < 2; ++pass)            /* the chosen row last: its ring lies on the rows next to it */
            for (int i = 0; i < v->optCount && i < PT_OPT_MAX; ++i)
                if ((pass == 1) == (v->optSelected == i))
                    OptRow(i, v);
    if (v->optLook.on && !v->optPad.on)
        OptLookPanel(v);
    MenuNoteBegin();
    MenuNote(v->bar[0], body, v->optFrameShow ? v->optFrameType : -1);
    sKeepBg = 0;
}

/* The Bag (and the TM Case, the Berry Pouch, a mart, the item PC) keeps its own rectangles, drawn for a viewport at (4, 4): the canvas is
 * handed to it moved by MENU_SHIFT, so that its 240 x 160 land on the floor, and its touches are moved back (3ds_field_bag_impl.h). */
#define MENU_SHIFT (MN_X - PT_VIEW_X)

static void NativeScreen(const CtrPokeTouchView *v)
{
    uint16_t *base = sCanvas;

    MenuBegin(v);
    sCanvas = base + (size_t)MENU_SHIFT * H - MENU_SHIFT;
    if (v->dex.on)
        FDexFrame(v);
    else if (v->card.on)
        FCardFrame(v);
    else if (v->learn.on)
        FLearnFrame(v);
    else if (v->relearn.on)
        FRelearnFrame(v);
    else if (v->mail.on)
        FMailFrame(v);
    else if (v->choice.on)
        FChoiceFrame(v);
    else if (v->box.on)
        FBoxFrame(v);
    else
        FBagFrame(v);
    sCanvas = base;
    MenuNoteBegin();
    if (v->dex.on)
        FDexNote(v, 0);
    else if (v->card.on)
        FCardNote(v, 0);
    else if (v->learn.on)
        FLearnNote(v, 0);
    else if (v->relearn.on)
        FRelearnNote(v, 0);
    else if (v->mail.on)
        FMailNote(v, 0);
    else if (v->choice.on)
        FChoiceNote(v, 0);
    else if (v->box.on)
        FBoxNote(v, 0);
    else
        FBagNote(v, 0);
    sKeepBg = 0;
}

/* The game's own picture (the Pokedex's habitat map and the like) in the middle: the floor stays black under it, the note says what the screen does. */
static void PictureScreen(const CtrPokeTouchView *v)
{
    MenuBegin(v);
    Fill(MN_X, MN_Y, MN_W, MN_H, 0);
    MenuNoteBegin();
    MenuNote(v->bar[0], v->bar[1][0] ? v->bar[1] : v->bar[2], -1);
}

static void MenuScreen(const CtrPokeTouchView *v, int full)
{
    sMenuFull = full;
    sBallFull = BallFullView(v);
    if (sBallFull) {
        if (BallOptionsActive(v))
            BallOptionsScreen(v);
        else if (BallSaveActive(v))
            BallSaveScreen(v);
        else if (BallCardActive(v))
            BallCardScreen(v);
        else if (BallDexActive(v))
            BallDexScreen(v);
        else if (BallShiftedActive(v))
            BallShiftedScreen(v);
        else if (BallMailActive(v))
            BallMailScreen(v);
        else if (BallLearnActive(v))
            BallLearnScreen(v);
        else if (BallRelearnActive(v))
            BallRelearnScreen(v);
        else if (BallChoiceActive(v))
            BallChoiceScreen(v);
        else
            BallBagScreen(v);
        sMenuFull = 1;
        return;
    }
    if (v->viewport)
        PictureScreen(v);
    else if (v->options)
        OptionsScreen(v);
    else if (v->save)
        SaveScreen(v);
    else
        NativeScreen(v);
    sMenuFull = 1;
}

/* What a menu shows of a view: the keys are never drawn greyed, nor BACK (a screen that is busy for a frame, as a list scrolls, would blink). */
static void MenuNormalize(CtrPokeTouchView *d, const CtrPokeTouchView *s)
{
    *d = *s;
    for (int i = 0; i < PT_COUNT; ++i)
        d->enabled[i] = 1;
    d->backEnabled = d->backVisible;
}

static int MenuKind(const CtrPokeTouchView *v)
{
    return v->viewport ? 11 : v->save ? 1 : v->options ? 2 : v->dex.on ? 3 : v->card.on ? 4 : v->learn.on ? 5 : v->relearn.on ? 6 : v->mail.on ? 7
         : v->choice.on ? 8 : v->box.on ? 9 : 10;
}

/* Whether the top band or the columns look different between two views of the same menu. */
static int MenuBandChanged(const CtrPokeTouchView *a, const CtrPokeTouchView *b)
{
    if (a->backVisible != b->backVisible || a->backEnabled != b->backEnabled || a->backPressed != b->backPressed || a->homeMoney != b->homeMoney
        || a->homeTurbo != b->homeTurbo || a->homeClock != b->homeClock || a->homeRepelSteps != b->homeRepelSteps
        || a->homeRepelKind != b->homeRepelKind || a->optPage != b->optPage || a->optPageCount != b->optPageCount
        || a->optSectionCount != b->optSectionCount || a->optPagePressed != b->optPagePressed || strcmp(a->optPageName, b->optPageName)
        || a->bag.on != b->bag.on || a->bag.first != b->bag.first || a->bag.count != b->bag.count)
        return 1;
    for (int i = 0; i < PT_COUNT; ++i)
        if (Changed(i, a, b))
            return 1;
    return 0;
}

void CtrPokeTouch_Draw(uint16_t *canvas, const CtrPokeTouchView *v)
{
    sCanvas = canvas;
    sKeepBg = 0;
    sBallFull = BallFullView(v);
    if (NewMenu(v)) {
        static CtrPokeTouchView shown;

        MenuNormalize(&shown, v);
        MenuScreen(&shown, 1);
        return;
    }
    if (IsHome(v)) {
        HomeDraw(v);
        return;
    }
    SetLayout(0);
    if (v->ec.on) {
        FEasyDraw(v);
        return;
    }
    if (v->name.on) {
        FNameDraw(v);
        return;
    }
    Frame();
    MenuFrame(v);
    Bar(v);
    for (int i = 0; i < PT_COUNT; ++i)
        Button(i, v);
}

/* Whether element `id` looks different between two views. */
static int Changed(int id, const CtrPokeTouchView *a, const CtrPokeTouchView *b)
{
    if (a->enabled[id] != b->enabled[id] || (a->pressed == id) != (b->pressed == id)
        || (a->selected == id) != (b->selected == id))
        return 1;
    return (id == PT_RUN && a->running != b->running)
        || (id == PT_REGISTERED && a->registeredItem != b->registeredItem)
        || (id == PT_BIKE && a->bikeItem != b->bikeItem)
        || (id == PT_CARD && strcmp(a->playerName, b->playerName));
}

static void Mark(CtrDirtyList *dirty, const CtrPokeTouchRect *r)
{
    int x0 = r->x < 0 ? 0 : r->x, y0 = r->y < 0 ? 0 : r->y, x1 = r->x + r->w > W ? W : r->x + r->w, y1 = r->y + r->h > H ? H : r->y + r->h;

    if (x1 > x0 && y1 > y0)
        CtrDirty_Add(dirty, x0, y0, x1, y1);
}

/* A rectangle of the HOME map put back (the part of it inside the viewport) and marked. */
static void MapRestoreRect(const CtrPokeTouchRect *r, CtrDirtyList *dirty)
{
    int x0 = r->x < PT_HOME_X ? PT_HOME_X : r->x, y0 = r->y < PT_HOME_Y ? PT_HOME_Y : r->y;
    int x1 = r->x + r->w > PT_HOME_X + PT_VIEW_W ? PT_HOME_X + PT_VIEW_W : r->x + r->w;
    int y1 = r->y + r->h > PT_HOME_Y + PT_VIEW_H ? PT_HOME_Y + PT_VIEW_H : r->y + r->h;
    CtrPokeTouchRect c = {(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};

    if (c.w <= 0 || c.h <= 0)
        return;
    MapRestore(x0 - PT_HOME_X, y0 - PT_HOME_Y, c.w, c.h);
    Mark(dirty, &c);
}

void CtrPokeTouch_Update(uint16_t *canvas, const CtrPokeTouchView *old, const CtrPokeTouchView *v, CtrDirtyList *dirty)
{
    int oldHasPage = old->options && !old->viewport && old->optPageCount > 1;
    int newHasPage = v->options && !v->viewport && v->optPageCount > 1;

    sCanvas = canvas;
    sBallFull = BallFullView(v);
    if (BallFullView(old) && !sBallFull) {              /* out of a whole-screen view: nothing of it is kept, the screen is drawn again */
        CtrPokeTouch_Draw(canvas, v);
        CtrDirty_Add(dirty, 0, 0, W, H);
        return;
    }
    if (NewMenu(v)) {
        static CtrPokeTouchView a, b;

        MenuNormalize(&b, v);
        if (BallFullView(v) || BallFullView(old)) {   /* a whole-screen view: drawn again as a whole when anything changes */
            MenuScreen(&b, 1);
            CtrDirty_Add(dirty, 0, 0, W, H);
            return;
        }
        if (!NewMenu(old) || MenuKind(old) != MenuKind(v)) {
            MenuScreen(&b, 1);
            CtrDirty_Add(dirty, 0, 0, W, H);
            return;
        }
        MenuNormalize(&a, old);
        if (MenuBandChanged(&a, &b)) {
            MenuScreen(&b, 1);
            CtrDirty_Add(dirty, 0, 0, W, H);
        } else if (memcmp(&a, &b, sizeof(a))) {
            MenuScreen(&b, 0);
            CtrDirty_Add(dirty, MN_X - 4, MN_Y - 4, MN_X + MN_W + 4, MN_Y + MN_H + 4);
            CtrDirty_Add(dirty, 0, HN_Y - 3, W, H);
        }
        return;
    }
    if (IsHome(old) != IsHome(v) || sLayoutHome != IsHome(v)) {
        /* Into or out of HOME's layout (the one on the screen, whatever the old view says): everything moves, the whole screen is drawn again. */
        CtrPokeTouch_Draw(canvas, v);
        CtrDirty_Add(dirty, 0, 0, W, H);
        return;
    }
    if (IsHome(v)) {
        HomeUpdate(old, v, dirty);
        return;
    }
    if (v->ec.on || old->ec.on) {
        FEasyUpdate(canvas, old, v, dirty);
        return;
    }
    if (v->name.on || old->name.on) {
        FNameUpdate(canvas, old, v, dirty);
        return;
    }
    if (memcmp(&old->bag, &v->bag, sizeof(v->bag)) || memcmp(&old->dex, &v->dex, sizeof(v->dex))
        || memcmp(&old->card, &v->card, sizeof(v->card)) || memcmp(&old->learn, &v->learn, sizeof(v->learn))
        || memcmp(&old->relearn, &v->relearn, sizeof(v->relearn)) || memcmp(&old->mail, &v->mail, sizeof(v->mail))
        || memcmp(&old->name, &v->name, sizeof(v->name)) || memcmp(&old->box, &v->box, sizeof(v->box))
        || memcmp(&old->choice, &v->choice, sizeof(v->choice))) {
        /* The Bag or the Pokedex: its whole area and its note (the description follows the selection). */
        MenuFrame(v);
        Mark(dirty, &sMenuFrame);
        Bar(v);
        Mark(dirty, &sBar);
    } else if (old->viewport != v->viewport || old->home != v->home || old->options != v->options || old->save != v->save
        || (v->options && !v->viewport && (old->optCount != v->optCount || old->optLook.on != v->optLook.on))
        || (v->home && (old->mapGroup != v->mapGroup || old->mapVersion != v->mapVersion || old->mapZoom != v->mapZoom
                        || old->mapCX != v->mapCX || old->mapCY != v->mapCY || old->mapReady != v->mapReady || old->mapOpen != v->mapOpen))) {
        MenuFrame(v);
        Mark(dirty, &sMenuFrame);
    } else if (v->home && !v->viewport && (old->playerX != v->playerX || old->playerY != v->playerY
                                           || old->playerFemale != v->playerFemale || old->cursorX != v->cursorX
                                           || old->cursorY != v->cursorY || old->cursorFrame != v->cursorFrame
                                           || ((old->cursorX >= 0 || v->cursorX >= 0) && strcmp(old->bar[0], v->bar[0])))) {
        /* An icon moved, appeared or blinked, or the destination changed: the rectangles the changed pieces had and have (the player's icon,
         * the destination's ring and its name's window) go back to the map, and every piece is drawn over them again, in order (one may
         * have overlapped another). */
        int player = old->playerX != v->playerX || old->playerY != v->playerY || old->playerFemale != v->playerFemale;
        int cursor = old->cursorX != v->cursorX || old->cursorY != v->cursorY || old->cursorFrame != v->cursorFrame;
        int label = old->cursorX != v->cursorX || old->cursorY != v->cursorY || strcmp(old->bar[0], v->bar[0]);
        const CtrPokeTouchView *views[2] = {old, v};
        CtrPokeTouchRect r;

        HomeSetView(v);
        for (int i = 0; i < 2; ++i) {
            if (player && PlayerRect(views[i], &r))
                MapRestoreRect(&r, dirty);
            if (cursor && RingRect(views[i], &r))
                MapRestoreRect(&r, dirty);
            if (label && LabelRect(views[i], &r))
                MapRestoreRect(&r, dirty);
        }
        HomeOverlay(v);
    } else if (v->options && !v->viewport) {
        /* A row whose value, selection or press changed: only its rectangle and the 2 px round it. The chosen row's ring lies on the rows next
         * to it, so a row that was or is chosen has them redrawn too; the chosen row is drawn last, and also when a neighbour was (the redraw of
         * a row puts the backdrop back over the ring its neighbour has on it). */
        uint8_t redraw[PT_OPT_MAX] = {0};
        int sel = v->optSelected, count = v->optCount < PT_OPT_MAX ? v->optCount : PT_OPT_MAX;

        for (int i = 0; i < count; ++i)
            redraw[i] = strcmp(old->optLabel[i], v->optLabel[i]) || strcmp(old->optValue[i], v->optValue[i])
                        || old->optSwitch[i] != v->optSwitch[i] || old->optSegValue[i] != v->optSegValue[i] || old->optPage != v->optPage
                        || (old->optSelected == i) != (v->optSelected == i) || (old->optPressed == i) != (v->optPressed == i)
                        || (v->optPressed == i && old->optZone != v->optZone);
        for (int i = 0; i < count; ++i)
            if ((old->optSelected == i) != (v->optSelected == i))
                for (int k = i - 1; k <= i + 1; ++k)
                    if (k >= 0 && k < count)
                        redraw[k] = 1;
        if (sel >= 0 && sel < count && ((sel > 0 && redraw[sel - 1]) || (sel + 1 < count && redraw[sel + 1])))
            redraw[sel] = 1;
        for (int pass = 0; pass < 2; ++pass)
            for (int i = 0; i < count; ++i)
                if (redraw[i] && (pass == 1) == (sel == i)) {
                    CtrPokeTouchRect r = OptRect(i);

                    OptRow(i, v);
                    r.x -= 2;
                    r.y -= 2;
                    r.w += 4;
                    r.h += 4;
                    Mark(dirty, &r);
                }
        /* The preview: its look (or the one chosen for another slot) changed. */
        if (v->optLook.on && memcmp(&old->optLook, &v->optLook, sizeof(v->optLook))) {
            CtrPokeTouchRect r = OptLookRect(v);

            r.x -= 2;
            r.y -= 2;
            r.w += 4;
            r.h += 4;
            OptLookPanel(v);
            Mark(dirty, &r);
        }
    }
    if (memcmp(old->bar, v->bar, sizeof(v->bar)) || old->flyOk != v->flyOk || old->optFrameShow != v->optFrameShow
        || old->optFrameType != v->optFrameType || old->backVisible != v->backVisible || old->save != v->save
        || oldHasPage != newHasPage) {
        Bar(v);
        Mark(dirty, &sBar);
    } else {
        if (newHasPage && (old->optPage != v->optPage || old->optSection != v->optSection || old->optPageCount != v->optPageCount
                           || old->optPagePressed != v->optPagePressed)) {
            CtrPokeTouchRect page = Under(&gCtrPokeTouchOptPage);

            OptPageButton(v);
            Mark(dirty, &page);
        }
        if (v->backVisible && (old->backEnabled != v->backEnabled || old->backPressed != v->backPressed)) {
            CtrPokeTouchRect back = Under(&gCtrPokeTouchBack);

            Back(v);                        /* a press or a state: only BACK's rectangle and its shadow */
            Mark(dirty, &back);
        }
    }
    for (int i = 0; i < PT_COUNT; ++i) {
        if (!Changed(i, old, v))
            continue;
        Button(i, v);
        {
            CtrPokeTouchRect area = Area(i);

            Mark(dirty, &area);
        }
    }
}

/* The embers over whatever screen is on the canvas (3ds_bottom.c calls it once a frame): kept off the GBA picture
 * when one is shown, only taken away when `on` is false. */
#include "idle_logo.h"

/* The lower screen while FireRed's screen above has nothing for it: the ember backdrop and the project's logo. */
void CtrPokeTouch_DrawIdle(uint16_t *canvas, int logo)
{
    int x0 = (W - IDLE_LOGO_W) / 2, y0 = (H - IDLE_LOGO_H_PX) / 2 - 6;

    sCanvas = canvas;
    TsBackdrop();
    for (int y = 0; logo && y < IDLE_LOGO_H_PX; ++y)
        for (int x = 0; x < IDLE_LOGO_W; ++x) {
            int k = kIdleLogo[y * IDLE_LOGO_W + x], a = kIdleLogoAlpha[k];
            unsigned c = kIdleLogoColour[k], b;

            if (a == 0)
                continue;
            if (a < 15) {                          /* a/15 of the logo over the backdrop, channel by channel */
                b = TsGet(x0 + x, y0 + y);
                c = ((((c >> 11) * a + (b >> 11) * (15 - a)) / 15) << 11)
                  | (((((c >> 5) & 63) * a + ((b >> 5) & 63) * (15 - a)) / 15) << 5)
                  | (((c & 31) * a + (b & 31) * (15 - a)) / 15);
            }
            Px(x0 + x, y0 + y, (uint16_t)c);
        }
}

void CtrPokeTouch_SetStyle(int style)
{
    TsApplyStyle(style);
}

int CtrPokeTouch_Style(void)
{
    return sStyleBall;
}

void CtrPokeTouch_Embers(uint16_t *canvas, int on, int low, int pictureShown, CtrDirtyList *dirty)
{
    if (sStyleBall)
        return;                                     /* the Poke Ball style has no embers */
    static const CtrPokeTouchRect picture = {PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H};
    static const CtrPokeTouchRect homeMap = {PT_HOME_X, PT_HOME_Y, PT_VIEW_W, PT_VIEW_H};

    EmbersStep(canvas, on, low, pictureShown ? (sLayoutHome ? &homeMap : &picture) : NULL, dirty);
}

#include "3ds_battle_ui_impl.h"
#include "3ds_summary_ui_impl.h"
#include "3ds_field_bag_impl.h"
#include "3ds_field_dex_impl.h"
#include "3ds_field_card_impl.h"
#include "3ds_field_learn_impl.h"
#include "3ds_field_relearn_impl.h"
#include "3ds_field_mail_impl.h"
#include "3ds_field_choice_impl.h"
#include "3ds_field_naming_impl.h"
#include "3ds_ball_options_impl.h"
#include "3ds_ball_menus_impl.h"
#include "3ds_ball_learn_impl.h"
#include "3ds_ball_naming_impl.h"
#include "3ds_ball_pieces_impl.h"
#include "3ds_field_box_impl.h"
#include "3ds_field_trade_impl.h"
#include "3ds_field_chat_impl.h"
#include "3ds_field_easychat_impl.h"
