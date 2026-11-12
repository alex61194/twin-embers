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
const CtrPokeTouchRect gCtrPokeTouchRects[PT_COUNT] = {
    [PT_POKEDEX]    = {250,   4, 68, 21},
    [PT_POKEMON]    = {250,  29, 68, 21},
    [PT_BAG]        = {250,  54, 68, 21},
    [PT_CARD]       = {250,  79, 68, 21},
    [PT_SAVE]       = {250, 104, 68, 21},
    [PT_OPTIONS]    = {250, 129, 68, 21},
    [PT_REGISTERED] = {250, 158, 32, 40},
    [PT_BIKE]       = {286, 158, 32, 40},
    [PT_RUN]        = {250, 204, 68, 30},
};

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
enum { ICON_REGISTERED, ICON_BIKE, ICON_BOX_HAND, ICON_BOX_NOTE, ICON_COUNT };

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

/* The HOME map: the 240x160 screen of FireRed's Region Map (its tilemap,
 * tiles and palette, decoded by the game bridge) composed once per map into
 * a cache laid out like the canvas: column-major, each column bottom-to-top,
 * so showing it is one copy per column. Nothing is read from the game while
 * the player walks; only the cache is. */
extern int CtrPokeTouch_MapArt(int group, uint16_t *tilemap, uint8_t *tiles, uint16_t *palette, uint16_t *bgTilemap,
                               uint8_t *bgTiles);

#define MAP_TILEMAP_W 30
#define MAP_TILEMAP_H 20
#define MAP_TILES 320
#define MAP_COLORS 80

static struct {
    int group;                          /* the map cached, -1 none */
    int version;                        /* and the version of its art */
    int ok;                             /* it was usable */
    uint16_t cols[PT_VIEW_W][PT_VIEW_H];
} sMap = {-1, 0, 0, {{0}}};

/* A GBA colour (15 bits, red lowest) as RGB565. */
static uint16_t Gba(uint16_t c)
{
    unsigned r = c & 31, g = (c >> 5) & 31, b = (c >> 10) & 31;
    return (uint16_t)(r << 11 | (g << 1 | g >> 4) << 5 | b);
}

/* The palette entry the Region Map's screen shows at its pixel (px, py): the map's layer over the
 * background's; index 0 is clear in both, and the screen shows the backdrop, colour 0 of palette 0,
 * only where both are. */
static unsigned MapColor(const uint16_t *tilemap, const uint8_t *tiles, const uint16_t *bgTilemap, const uint8_t *bgTiles,
                         int px, int py)
{
    int tx = px / 8, ty = py / 8, x = px % 8, y = py % 8;
    unsigned entry[2] = {tilemap[ty * MAP_TILEMAP_W + tx], bgTilemap[ty * MAP_TILEMAP_W + tx]};

    for (int layer = 0; layer < 2; ++layer) {
        unsigned tile = entry[layer] & 0x3FF, bank = entry[layer] >> 12, index = 0;
        int sx = entry[layer] & 0x400 ? 7 - x : x, sy = entry[layer] & 0x800 ? 7 - y : y;
        if (tile < MAP_TILES) {
            uint8_t byte = (layer ? bgTiles : tiles)[tile * 32 + sy * 4 + sx / 2];
            index = (sx & 1 ? byte >> 4 : byte) & 15;
        }
        if (index && bank * 16 + index < MAP_COLORS)
            return bank * 16 + index;
    }
    return 0;
}

static void MapBuild(int group, int version)
{
    static uint16_t tilemap[MAP_TILEMAP_W * MAP_TILEMAP_H];
    static uint8_t tiles[MAP_TILES * 32];
    static uint16_t bgTilemap[MAP_TILEMAP_W * MAP_TILEMAP_H];
    static uint8_t bgTiles[MAP_TILES * 32];
    static uint16_t gba[MAP_COLORS];
    uint16_t pal[MAP_COLORS];

    sMap.group = group;
    sMap.version = version;
    sMap.ok = group >= 0 && group < PT_MAP_COUNT && CtrPokeTouch_MapArt(group, tilemap, tiles, gba, bgTilemap, bgTiles);
    if (!sMap.ok)
        return;
    for (int i = 0; i < MAP_COLORS; ++i)
        pal[i] = Gba(gba[i]);
    /* The Region Map's 240x160 picture is sampled, nearest neighbour, over its 192x128 at (24, 28) into the
     * viewport (exactly 5/4), once, here: what is cached is what is shown. */
    for (int dx = 0; dx < PT_VIEW_W; ++dx)
        for (int dy = 0; dy < PT_VIEW_H; ++dy) {
            int sx = PT_HOME_SRC_X + dx * 4 / 5, sy = PT_HOME_SRC_Y + dy * 4 / 5;
            sMap.cols[dx][PT_VIEW_H - 1 - dy] =
                pal[MapColor(tilemap, tiles, bgTilemap, bgTiles, sx, sy)];
        }
    /* The map sits in its frame: the first three rows are shaded (the mockup's inset shadow), once, baked into what is cached. */
    for (int dx = 0; dx < PT_VIEW_W; ++dx)
        for (int dy = 0; dy < 3; ++dy)
            sMap.cols[dx][PT_VIEW_H - 1 - dy] = Em2Lerp(sMap.cols[dx][PT_VIEW_H - 1 - dy], UI_RGB(0, 0, 0), 64 >> dy, 255);
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
} sPlayer = {-1, 0, 1, {{0}}, {0}}, sCursor = {-1, 0, 2, {{0}}, {0}};

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

/* An icon's rectangle in the viewport; false when the cell is not on the map. */
static int MarkerRect(int cx, int cy, int *x, int *y)
{
    if (cx < 0 || cy < 0 || cx >= MAP_CELLS_X || cy >= MAP_CELLS_Y)
        return 0;
    /* The same transform as the map: source (8 cx + 28, 8 cy + 28) minus the crop, times 5/4. */
    *x = (8 * cx + 28 - PT_HOME_SRC_X) * 5 / 4;
    *y = (8 * cy + 28 - PT_HOME_SRC_Y) * 5 / 4;
    return 1;
}

static void IconDraw(const struct Icon *icon, int frame, int cx, int cy)
{
    int x, y;

    if (!icon->ok || !MarkerRect(cx, cy, &x, &y))
        return;
    for (int py = 0; py < PT_HOME_ICON; ++py)
        for (int px = 0; px < PT_HOME_ICON; ++px) {
            uint8_t index = icon->pixels[frame % icon->frames][(py * 4 / 5) * MARKER + px * 4 / 5];
            if (index)
                Px(PT_VIEW_X + x + px, PT_VIEW_Y + y + py, icon->pal[index]);
        }
}

/* The pieces over the HOME map, in the order they are drawn: the region's red ribbon at the map's corner (the mockup's KANTO), the player's
 * icon and, for the destination chosen, its ring (a px of white and a px of the cursor's red round the 20 x 20 icon), FireRed's cursor in it
 * and its name on a small window above (below, if there is no room). Every piece paints inside the viewport only. */
#define RING_D 24
#define LABEL_H 21

static const char *RegionName(int group)
{
    return group == PT_MAP_KANTO ? "KANTO" : "SEVII";
}

/* The destination's ring: the cursor icon's rectangle and 2 px round it; false when nothing is chosen or its cell is off the map. */
static int RingRect(const CtrPokeTouchView *v, CtrPokeTouchRect *r)
{
    int x, y;

    if (v->cursorX < 0 || !MarkerRect(v->cursorX, v->cursorY, &x, &y))
        return 0;
    r->x = (int16_t)(PT_VIEW_X + x - 2);
    r->y = (int16_t)(PT_VIEW_Y + y - 2);
    r->w = r->h = RING_D;
    return 1;
}

/* The destination's name, on a window centred over its ring 3 px above it (below if that is off the map), kept inside the map. */
static int LabelRect(const CtrPokeTouchView *v, CtrPokeTouchRect *r)
{
    CtrPokeTouchRect ring;
    int w, x, y, left = PT_VIEW_X + 3, right = PT_VIEW_X + PT_VIEW_W - 3, top = PT_VIEW_Y + 3, bottom = PT_VIEW_Y + PT_VIEW_H - 3;

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

static void RingDraw(const CtrPokeTouchRect *r)
{
    for (int row = 0; row < RING_D; ++row)
        for (int col = 0; col < RING_D; ++col) {
            int dx = 2 * col + 1 - RING_D, dy = 2 * row + 1 - RING_D, d2 = dx * dx + dy * dy, x = r->x + col, y = r->y + row;

            if (x < PT_VIEW_X || y < PT_VIEW_Y || x >= PT_VIEW_X + PT_VIEW_W || y >= PT_VIEW_Y + PT_VIEW_H || d2 > RING_D * RING_D)
                continue;
            if (d2 > (RING_D - 2) * (RING_D - 2))
                Px(x, y, LL_CURSOR);
            else if (d2 > (RING_D - 4) * (RING_D - 4))
                Px(x, y, EM_WHITE);
        }
}

static void HomeOverlay(const CtrPokeTouchView *v)
{
    CtrPokeTouchRect ring, label;

    LlRibbon(PT_VIEW_X, PT_VIEW_Y, RegionName(v->mapGroup), LL_RIB_TOP, LL_RIB_BOT);
    if (sPlayer.key != v->playerFemale)
        IconBuild(&sPlayer, v->playerFemale, CtrPokeTouch_MapPlayerIcon);
    IconDraw(&sPlayer, 0, v->playerX, v->playerY);
    if (!RingRect(v, &ring))
        return;
    RingDraw(&ring);
    if (sCursor.key != 0)
        IconBuild(&sCursor, 0, CtrPokeTouch_MapCursorIcon);
    IconDraw(&sCursor, v->cursorFrame, v->cursorX, v->cursorY);
    if (LabelRect(v, &label)) {
        LlWindowPlain(label.x, label.y, label.w, label.h);
        Em2Text(v->bar[0], label.x + 10, label.y + 6, EM_DARK, EM_DARK_SH, label.x + label.w - 6);
    }
}

/* The map's own pixels back over a rectangle of the viewport (where an icon
 * was). */
static void MapRestore(int x, int y, int w, int h)
{
    for (int cx = x < 0 ? 0 : x; cx < x + w && cx < PT_VIEW_W; ++cx) {
        int y0 = y < 0 ? 0 : y, y1 = y + h > PT_VIEW_H ? PT_VIEW_H : y + h;
        memcpy(sCanvas + (size_t)(PT_VIEW_X + cx) * H + (H - PT_VIEW_Y - y1), &sMap.cols[cx][PT_VIEW_H - y1],
               (size_t)(y1 - y0) * sizeof(uint16_t));
    }
}

/* Draw the map's picture over the viewport; false when there is none. */
static int MapShow(int group, int version)
{
    if (sMap.group != group || sMap.version != version)
        MapBuild(group, version);
    if (!sMap.ok)
        return 0;
    for (int x = 0; x < PT_VIEW_W; ++x)
        memcpy(sCanvas + (size_t)(PT_VIEW_X + x) * H + (H - PT_VIEW_Y - PT_VIEW_H), sMap.cols[x],
               PT_VIEW_H * sizeof(uint16_t));
    return 1;
}

/* HOME's picture: the map, the player and the selection on it. */
static int HomeShow(const CtrPokeTouchView *v)
{
    if (!MapShow(v->mapGroup, v->mapVersion))
        return 0;
    HomeOverlay(v);
    return 1;
}

/* The OPTIONS rows (the Llama mockup's cream cards): 236x22 with their lip, 22 apart from (7, 7), so they touch. The value sits in a well at the
 * right: 120 wide inside OPT_BOX_W: "<" (28), the value and ">" (28) for the hit zones. */
#define OPT_X 7
#define OPT_Y 7
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
    int at = x - OPT_SEG_X(OPT_X + OPT_W - OPT_BOX_W), k;

    if (count <= 0)
        return 0;
    k = at < 0 ? 0 : at * count / OPT_SEG_W;
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
    UiRoundFill(r.x + 4, fy + 4, 3, 12, 1, selected ? EM_WHITE : OptPageColor(v->optPage));
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
        const char *tag = CTR_TR("ART", "ARTE", "Kunst");
        int y = r.y + (r.h >= 56 ? 31 : 22), pw, lines = big ? 3 : r.h >= 56 ? 2 : 1;

        pw = LlRibbon(textX, y, tag, LL_RIB_TOP, LL_RIB_BOT);
        Em2Wrap(l->credit, textX + pw + 4, y + 3, right - textX - pw - 4, lines, UI_LINE_H, EM_DIM, EM_DARK_SH);
    }
}

/* The SAVE screen (Llama): FireRed's text window over the frame where the picture goes: a flame title band, the five
 * lines (label on the left, blue value on the right, a rule under each) and the player's own map icon, enlarged by 3, in a
 * sunk well. Each value has its own rectangle, filled with the window's paper before its text, so one that changes (the
 * play clock) is redrawn alone. The dialogue is the bar: the text in a second window (title, rule, dimmer lines), YES over
 * NO (or a lone OK) on its right. */
enum { SF_NAME, SF_BADGES, SF_DEX, SF_TIME, SF_MONEY, SF_COUNT };
static const CtrPokeTouchRect sSavePanel = {3, 3, 244, 160};     /* the window (5, 5, 240 x 154) and 2 px round it, its shadow too */
static const CtrPokeTouchRect sSaveButtons = {170, 170, 76, 70};   /* right of the note card, glow included */
static const CtrPokeTouchRect sSaveYes = {172, 172, 72, 30};
static const CtrPokeTouchRect sSaveNo = {172, 206, 72, 30};
#define SAVE_WIN_X 5
#define SAVE_WIN_Y 5
#define SAVE_WIN_W 240
#define SAVE_WIN_H 154
#define SAVE_LINE_Y 34
#define SAVE_LINE_STEP 24
#define SAVE_LABEL_X 18
#define SAVE_VALUE_RIGHT 146
#define SAVE_NOTE_X 5
#define SAVE_NOTE_Y 166
#define SAVE_NOTE_W 162
#define SAVE_NOTE_H 70
static const char *const sSaveLabel[SF_COUNT] = {CTR_TR("PLAYER", "JUGADOR", "Spieler"), CTR_TR("BADGES", "MEDALLAS", "Orden"), CTR_TR("POK\xc3\xa9" "DEX", "POK\xc3\xa9" "DEX", "Pok\xc3\xa9" "dex"), CTR_TR("TIME", "TIEMPO", "Zeit"), CTR_TR("MONEY", "DINERO", "Geld")};

static CtrPokeTouchRect SaveField(int f)
{
    CtrPokeTouchRect r = {84, (int16_t)(SAVE_LINE_Y + f * SAVE_LINE_STEP - 2), SAVE_VALUE_RIGHT + 2 - 84, 16};

    return r;
}

/* The window's paper (white to #f8f8f0 over its inside, as LlWindow lays it) on row y. */
static uint16_t SavePaper(int y)
{
    return Em2Lerp(EM_WHITE, UI_RGB(248, 248, 240), y - (SAVE_WIN_Y + 5), SAVE_WIN_H - 10 - 1);
}

static void SaveFieldDraw(int f, const CtrPokeTouchView *v)
{
    CtrPokeTouchRect r = SaveField(f);
    char text[24], fit[32];

    for (int row = 0; row < r.h; ++row)
        Fill(r.x, r.y + row, r.w, 1, SavePaper(r.y + row));
    switch (f) {
    case SF_NAME:   snprintf(text, sizeof(text), "%s", v->playerName); break;
    case SF_BADGES: snprintf(text, sizeof(text), "%u", (unsigned)v->saveBadges); break;
    case SF_DEX:
        if (v->saveHasDex)
            snprintf(text, sizeof(text), "%u", (unsigned)v->saveDex);
        else
            snprintf(text, sizeof(text), "---");
        break;
    case SF_TIME:   snprintf(text, sizeof(text), "%u:%02u", (unsigned)(v->saveHours > 999 ? 999 : v->saveHours),
                             (unsigned)(v->saveMinutes > 59 ? 59 : v->saveMinutes)); break;
    default:        snprintf(text, sizeof(text), "\xc2\xa5%u", (unsigned)(v->saveMoney > 999999u ? 999999u : v->saveMoney)); break;
    }
#if !CTR_LANG_DE
    for (char *c = text; f == SF_NAME && *c; ++c)
        if (*c >= 'a' && *c <= 'z')
            *c = (char)(*c - ('a' - 'A'));
#endif
    Em2Fit(text, SAVE_VALUE_RIGHT - 84, fit, sizeof(fit));
    UiText(fit, SAVE_VALUE_RIGHT - UiTextWidth(fit, 1), r.y + 2, UI_RGB(48, 104, 216), UI_RGB(192, 208, 240), 1,
           SAVE_VALUE_RIGHT + 1);
}

/* The player's portrait: FireRed's own map icon of Red or Leaf, enlarged by 3, on a round cream plate. */
static void SaveSprite(const CtrPokeTouchView *v)
{
    const int scale = 3, x = 166, y = 62;

    LlWell(x - 14, y - 24, 76, 96);                         /* the sunk box of the mockup, the icon in its middle */
    UiRoundFill(x, y, MARKER * scale, MARKER * scale, 6, UI_RGB(250, 242, 222));   /* the plate under it */
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

static void SaveDraw(const CtrPokeTouchView *v)
{
    uint16_t band[20];
    int split = 20 * 42 / 100;

    LlWindow(SAVE_WIN_X, SAVE_WIN_Y, SAVE_WIN_W, SAVE_WIN_H);
    /* The flame band across the top of the window (the mockup's .fl, 20 tall, no outline: it covers the window's rings). */
    for (int row = 0; row < 20; ++row)
        band[row] = row < split ? Em2Lerp(UI_RGB(255, 196, 90), UI_RGB(255, 154, 56), row, split - 1)
                                : Em2Lerp(UI_RGB(248, 100, 42), UI_RGB(216, 48, 30), row - split, 20 - split - 1);
    EmFillRows(SAVE_WIN_X + 1, SAVE_WIN_Y + 1, SAVE_WIN_W - 2, 20, 4, band, 0, 10);
    for (int row = 10; row < 20; ++row)
        Fill(SAVE_WIN_X + 1, SAVE_WIN_Y + 1 + row, SAVE_WIN_W - 2, 1, band[row]);
    Em2Text(CTR_TR("SAVE", "GUARDAR", "Sichern"), SAVE_WIN_X + 11, SAVE_WIN_Y + 1 + (20 - UI_TEXT_H) / 2, EM_WHITE, kEmFlame.inkSh,
            SAVE_WIN_X + SAVE_WIN_W - 6);
    for (int f = 0; f < SF_COUNT; ++f) {
        Em2Text(sSaveLabel[f], SAVE_LABEL_X, SAVE_LINE_Y + f * SAVE_LINE_STEP, EM_DARK, EM_DARK_SH, 84);
        LlRule(SAVE_LABEL_X, SAVE_LINE_Y + f * SAVE_LINE_STEP + 17, 130);
        SaveFieldDraw(f, v);
    }
    SaveSprite(v);
}

/* The dialogue's text: a second window under the picture, the subject (what is asked or said), a rule, then the lines
 * that say more, dimmer. One line alone is centred. */
static void SaveNote(const CtrPokeTouchView *v)
{
    const char *shown[PT_BAR_LINES];
    int count = 0, right = SAVE_NOTE_X + SAVE_NOTE_W - 5;

    LlWindow(SAVE_NOTE_X, SAVE_NOTE_Y, SAVE_NOTE_W, SAVE_NOTE_H);
    for (int i = 0; i < PT_BAR_LINES; ++i)
        if (v->bar[i][0])
            shown[count++] = v->bar[i];
    if (count == 1) {
        Em2Text(shown[0], SAVE_LABEL_X, SAVE_NOTE_Y + (SAVE_NOTE_H - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, right);
    } else if (count > 1) {
        Em2Text(shown[0], SAVE_LABEL_X, SAVE_NOTE_Y + 10, EM_DARK, EM_DARK_SH, right);
        LlRule(SAVE_LABEL_X, SAVE_NOTE_Y + 27, right - SAVE_LABEL_X);
        for (int i = 1; i < count; ++i)
            Em2Text(shown[i], SAVE_LABEL_X, SAVE_NOTE_Y + 33 + (i - 1) * 15, EM_DIM, EM_DARK_SH, right);
    }
}

static int SaveFieldChanged(int f, const CtrPokeTouchView *a, const CtrPokeTouchView *b)
{
    switch (f) {
    case SF_NAME:   return strcmp(a->playerName, b->playerName) != 0;
    case SF_BADGES: return a->saveBadges != b->saveBadges;
    case SF_DEX:    return a->saveHasDex != b->saveHasDex || a->saveDex != b->saveDex;
    case SF_TIME:   return a->saveHours != b->saveHours || a->saveMinutes != b->saveMinutes;
    default:        return a->saveMoney != b->saveMoney;
    }
}

/* The dialogue's buttons: YES over NO, or a lone OK on YES's place. */
static CtrPokeTouchRect SaveButtonRect(int which, int state)
{
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

/* YES cream, NO cream too until chosen; the chosen one is flame with the 2 px ring, a lone OK is flame; the one under a
 * touch sinks. While writing, three flame dots instead. */
static void SaveButtons(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *b = &sSaveButtons;
    int confirm = v->saveState == SAVE_CONFIRM;

    TsBgRect(b->x, b->y, b->w, b->h);
    if (v->saveState == SAVE_WRITING) {
        static const int8_t dy[3] = {0, -3, 0};

        for (int i = 0; i < 3; ++i) {
            int cx = b->x + 14 + 14 * i, cy = b->y + 30 + dy[i];

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
        EmLabel(!confirm ? "OK" : which == PT_SAVE_FIRST ? CTR_TR("YES", "S\xC3\x8D", "Ja") : CTR_TR("NO", "NO", "Nein"), r.x, r.w, fy, r.h - 2, pal);
    }
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
/* The field's choice the port draws (3ds_field_choice_impl.h, included at the end). */
static void FChoiceFrame(const CtrPokeTouchView *v);
static void FChoiceNote(const CtrPokeTouchView *v, int w);
/* The naming screen the port draws (3ds_field_naming_impl.h, included at the end). */
static void FNameFrame(const CtrPokeTouchView *v);
static void FNameNote(const CtrPokeTouchView *v, int w);
/* The PC box the port draws (3ds_field_box_impl.h, included at the end). */
static void FBoxFrame(const CtrPokeTouchView *v);
static void FBoxNote(const CtrPokeTouchView *v, int w);

/* The frame's area: the window frame round the GBA picture or the map, OPTIONS' rows, the save card, or the Bag, on the embers. */
static void MenuFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sMenuFrame;

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
    if (v->save && !v->viewport) {
        SaveDraw(v);
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
    UiText(CTR_TR("BACK", "ATR\xc3\x81S", "Zur\xc3\xbc" "ck"), r->x + 24, fy + (fh - UI_TEXT_H) / 2, pal->ink, pal->inkSh, 1, r->x + r->w - 3);
}

/* PAGE: switches between OPTIONS pages (PAGE 2/4) right above BACK: a dot for each page in that page's colour, the shown one larger with a ring. */
static void OptPageButton(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchOptPage;
    int fy, dots = v->optPageCount > 6 ? 6 : v->optPageCount, x0;
    char label[40];

    TsBgRect(r->x, r->y, r->w, r->h + 2);
    Em2CardShadow(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, COL_DEPTH, v->optPagePressed);
    x0 = r->x + (r->w - ((dots - 1) * 10 + 7)) / 2;
    for (int i = 0; i < dots; ++i) {
        uint16_t colour = i == 0 ? EM2_PAGE_A : i == 1 ? EM2_PAGE_B : i == 2 ? EM2_PAGE_C : EM2_PAGE_D;

        if (i == v->optPage)
            Em2Dot(x0 + i * 10, fy + 3, colour, 1);
        else
            Em2Dot(x0 + i * 10 + 1, fy + 4, colour, 0);
    }
    snprintf(label, sizeof(label), CTR_TR("PAGE %d/%d", "PAG. %d/%d", "Seite %d/%d"), v->optPage + 1, v->optPageCount);
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

static int FrameSample(int type, int x, int y)
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
    for (int dy = 0; dy < FRAME_H; ++dy)
        for (int dx = 0; dx < FRAME_W; ++dx) {
            int sx = dx < 8 ? dx : dx >= FRAME_W - 8 ? 16 + dx - (FRAME_W - 8) : 8 + dx % 8;
            int sy = dy < 8 ? dy : dy >= FRAME_H - 8 ? 16 + dy - (FRAME_H - 8) : 8 + dy % 8;

            Px(x + dx, y + dy, px[sy * 24 + sx]);
        }
    UiText(CTR_TR("YES", "S\xC3\x8D", "Ja"), x + 14, y + 7, EM_DARK, EM_DARK_SH, 1, x + FRAME_W - 6);
    UiText(CTR_TR("NO", "NO", "Nein"), x + 14, y + 21, EM_DARK, EM_DARK_SH, 1, x + FRAME_W - 6);
    return 1;
}

static void Bar(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sBar;
    int lines = 0;
    int hasPage = v->options && !v->viewport && v->optPageCount > 1;
    int side = v->backVisible || hasPage || v->save;
    int w = side ? NOTE_W_SIDE : NOTE_W_FULL;

    TsBgRect(r->x, r->y, r->w, r->h);
    for (int i = 0; i < PT_BAR_LINES; ++i)
        if (v->bar[i][0])
            lines = i + 1;
    if (!v->viewport && !v->save && (v->dex.on || v->card.on || v->learn.on || v->relearn.on || v->choice.on || v->name.on || v->box.on || v->bag.on))
        Em2NoteShadow(NOTE_X, NOTE_Y, w, NOTE_H);           /* the screens that draw their own note (each puts the shadow its note throws) */
    if (v->save && !v->viewport)
        SaveNote(v);
    else if (v->dex.on && !v->viewport)
        FDexNote(v, w);
    else if (v->card.on && !v->viewport)
        FCardNote(v, w);
    else if (v->learn.on && !v->viewport)
        FLearnNote(v, w);
    else if (v->relearn.on && !v->viewport)
        FRelearnNote(v, w);
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
        const char *fly = v->home && !v->viewport && !v->options && v->flyOk ? CTR_TR("FLY", "VOLAR", "Fliegen") : NULL;
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
    if (v->save)
        SaveButtons(v);
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

        LlRibbonBox(r->x + 1, fy + 1, UiTextWidth(key, 1) + 18, 11, 7, LL_TL, LL_BLUE_TOP, LL_BLUE_BOT, key);
        LlWell(r->x + 3, fy + 15, r->w - 6, fh - 18);
        ItemArt(id == PT_REGISTERED ? ICON_REGISTERED : ICON_BIKE, id == PT_REGISTERED ? v->registeredItem : v->bikeItem,
                r->x + (r->w - 20) / 2, fy + 15 + (fh - 18 - 20) / 2, on);
        break;
    }
    case PT_RUN:
        /* The battle menu's RUN shoe, small, beside the switch. */
        for (int v2 = 0; v2 < FLATSHOESMALL_H; ++v2)
            for (int u = 0; u < FLATSHOESMALL_W; ++u)
                if (kFlatShoeSmall[v2 * FLATSHOESMALL_W + u])
                    Px(r->x + 5 + u, fy + (fh - FLATSHOESMALL_H) / 2 + v2, kFlatShoeSmall[v2 * FLATSHOESMALL_W + u]);
        LhSwitch(r->x + 34, fy + (fh - 15) / 2, selected);
        break;
    default: {
        char name[16] = {0};
        const char *label = id == PT_POKEDEX ? CTR_TR("POK\xc3\xa9" "DEX", "POK\xc3\xa9" "DEX", "Pok\xc3\xa9" "dex")
                          : id == PT_POKEMON ? CTR_TR("POK\xc3\xa9" "MON", "POK\xc3\xa9" "MON", "Pok\xc3\xa9" "mon")
                          : id == PT_BAG ? CTR_TR("BAG", "MOCHILA", "Beutel")
                          : id == PT_SAVE ? CTR_TR("SAVE", "GUARDAR", "Sichern") : id == PT_OPTIONS ? CTR_TR("OPTION", "AJUSTES", "Option") : name;

        if (id == PT_CARD) {
            /* The player's name (in original case for modern Title Case, or uppercase otherwise). */
            int n = 0;

            for (; n < (int)sizeof(name) - 1 && v->playerName[n]; ++n) {
                char ch = v->playerName[n];
#if CTR_LANG_DE
                name[n] = ch;
#else
                name[n] = (char)(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch);
#endif
            }
            name[n] = 0;
        }
        EmIcon(ColumnIcon(id), r->x + 4, fy + (fh - EMBER_ICON) / 2, on);
        UiText(label, r->x + 23, fy + (fh - UI_TEXT_H) / 2, pal->ink, pal->inkSh, 1, r->x + r->w - 2);
        break;
    }
    }
}

bool CtrPokeTouch_HitBack(int x, int y)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchBack;

    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

bool CtrPokeTouch_HitOptPage(int x, int y)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchOptPage;

    return x >= r->x && y >= r->y && x < r->x + r->w && y < r->y + r->h;
}

int CtrPokeTouch_HitTest(int x, int y)
{
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
    x -= PT_VIEW_X;
    y -= PT_VIEW_Y;
    if (x < 0 || y < 0 || x >= PT_VIEW_W || y >= PT_VIEW_H)
        return false;
    *gx = x;
    *gy = y;
    return true;
}

void CtrPokeTouch_Draw(uint16_t *canvas, const CtrPokeTouchView *v)
{
    sCanvas = canvas;
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
    CtrDirty_Add(dirty, r->x, r->y, r->x + r->w, r->y + r->h);
}

/* A rectangle of the HOME map put back (the part of it inside the viewport) and marked. */
static void MapRestoreRect(const CtrPokeTouchRect *r, CtrDirtyList *dirty)
{
    int x0 = r->x < PT_VIEW_X ? PT_VIEW_X : r->x, y0 = r->y < PT_VIEW_Y ? PT_VIEW_Y : r->y;
    int x1 = r->x + r->w > PT_VIEW_X + PT_VIEW_W ? PT_VIEW_X + PT_VIEW_W : r->x + r->w;
    int y1 = r->y + r->h > PT_VIEW_Y + PT_VIEW_H ? PT_VIEW_Y + PT_VIEW_H : r->y + r->h;
    CtrPokeTouchRect c = {(int16_t)x0, (int16_t)y0, (int16_t)(x1 - x0), (int16_t)(y1 - y0)};

    if (c.w <= 0 || c.h <= 0)
        return;
    MapRestore(x0 - PT_VIEW_X, y0 - PT_VIEW_Y, c.w, c.h);
    Mark(dirty, &c);
}

void CtrPokeTouch_Update(uint16_t *canvas, const CtrPokeTouchView *old, const CtrPokeTouchView *v, CtrDirtyList *dirty)
{
    int oldHasPage = old->options && !old->viewport && old->optPageCount > 1;
    int newHasPage = v->options && !v->viewport && v->optPageCount > 1;

    sCanvas = canvas;
    if (memcmp(&old->bag, &v->bag, sizeof(v->bag)) || memcmp(&old->dex, &v->dex, sizeof(v->dex))
        || memcmp(&old->card, &v->card, sizeof(v->card)) || memcmp(&old->learn, &v->learn, sizeof(v->learn))
        || memcmp(&old->relearn, &v->relearn, sizeof(v->relearn))
        || memcmp(&old->name, &v->name, sizeof(v->name)) || memcmp(&old->box, &v->box, sizeof(v->box))
        || memcmp(&old->choice, &v->choice, sizeof(v->choice))) {
        /* The Bag or the Pokedex: its whole area and its note (the description follows the selection). */
        MenuFrame(v);
        Mark(dirty, &sMenuFrame);
        Bar(v);
        Mark(dirty, &sBar);
    } else if (old->viewport != v->viewport || old->home != v->home || old->options != v->options || old->save != v->save
        || (v->options && !v->viewport && (old->optCount != v->optCount || old->optLook.on != v->optLook.on))
        || (v->home && (old->mapGroup != v->mapGroup || old->mapVersion != v->mapVersion))) {
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
        int x, y;

        for (int i = 0; i < 2; ++i) {
            if (player && MarkerRect(views[i]->playerX, views[i]->playerY, &x, &y)) {
                r.x = (int16_t)(PT_VIEW_X + x);
                r.y = (int16_t)(PT_VIEW_Y + y);
                r.w = r.h = PT_HOME_ICON;
                MapRestoreRect(&r, dirty);
            }
            if (cursor && RingRect(views[i], &r))
                MapRestoreRect(&r, dirty);
            if (label && LabelRect(views[i], &r))
                MapRestoreRect(&r, dirty);
        }
        HomeOverlay(v);
    } else if (v->save && !v->viewport) {
        /* A value that changed (the play clock, say): its own rectangle. */
        for (int f = 0; f < SF_COUNT; ++f)
            if (SaveFieldChanged(f, old, v)) {
                CtrPokeTouchRect r = SaveField(f);

                SaveFieldDraw(f, v);
                Mark(dirty, &r);
            }
        if (old->playerFemale != v->playerFemale) {
            SaveDraw(v);
            Mark(dirty, &sSavePanel);
        }
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
        if (newHasPage && (old->optPage != v->optPage || old->optPageCount != v->optPageCount
                           || old->optPagePressed != v->optPagePressed)) {
            CtrPokeTouchRect page = Under(&gCtrPokeTouchOptPage);

            OptPageButton(v);
            Mark(dirty, &page);
        }
        if (v->backVisible && (old->backEnabled != v->backEnabled || old->backPressed != v->backPressed)) {
            CtrPokeTouchRect back = Under(&gCtrPokeTouchBack);

            Back(v);                        /* a press or a state: only BACK's rectangle and its shadow */
            Mark(dirty, &back);
        } else if (v->save && (old->saveState != v->saveState || old->saveSel != v->saveSel
                               || old->savePressed != v->savePressed)) {
            SaveButtons(v);                 /* the selection or a press: only the buttons' area */
            Mark(dirty, &sSaveButtons);
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

void CtrPokeTouch_Embers(uint16_t *canvas, int on, int low, int pictureShown, CtrDirtyList *dirty)
{
    static const CtrPokeTouchRect picture = {PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H};

    EmbersStep(canvas, on, low, pictureShown ? &picture : NULL, dirty);
}

#include "3ds_battle_ui_impl.h"
#include "3ds_summary_ui_impl.h"
#include "3ds_field_bag_impl.h"
#include "3ds_field_dex_impl.h"
#include "3ds_field_card_impl.h"
#include "3ds_field_learn_impl.h"
#include "3ds_field_relearn_impl.h"
#include "3ds_field_choice_impl.h"
#include "3ds_field_naming_impl.h"
#include "3ds_field_box_impl.h"
