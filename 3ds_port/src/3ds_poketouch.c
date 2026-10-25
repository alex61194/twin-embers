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

/* Game bridge (src/ctr_poketouch.c): an item's 24x24 icon decompressed from
 * FireRed's own icon table. */
extern int CtrPokeTouch_ItemIcon(unsigned item, unsigned char *tiles, unsigned short *palette);

#define W CTR_BOTTOM_WIDTH
#define H CTR_BOTTOM_HEIGHT
#define RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))


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

static void Fill(int x, int y, int w, int h, uint16_t c)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y;
    int x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;
    for (int cx = x0; cx < x1; ++cx) {
        uint16_t *p = sCanvas + (size_t)cx * H + (H - (size_t)y1);
        for (int n = y1 - y0; n > 0; --n)
            *p++ = c;
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
    else if (c == 0xc3 && (unsigned char)**s == 0xa9) { ++*s; code = 0x1b; }
    else if (c == 0xc2 && (unsigned char)**s == 0xa5) { ++*s; code = 0xb7; }
    else if (c == 0x01 && **s) code = (unsigned char)*(*s)++;   /* a FireRed code as it is (CtrPokeTouch name text) */
    else switch (c) {
        case '!': code=0xab; break; case '?': code=0xac; break;
        case '.': code=0xad; break; case '-': code=0xae; break;
        case ',': code=0xb8; break; case '/': code=0xba; break;
        case ':': code=0xf0; break; case '(': code=0x5c; break;
        case ')': code=0x5d; break; case '%': code=0x5b; break;
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
        const uint8_t *src = CtrTwinFontData() + code * 64;
        PtGlyph *g = &sPtFont[code];
        g->width = CtrTwinFontWidth(code);
        if (g->width > 16) g->width = 16;
        TwinGlyphDecode(src, g->width, g->rows);
        sPtFontReady[code] = true;
    }
    return &sPtFont[code];
}

/* The game's small font (latin_small): 5 pixels wide for most letters, capitals on glyph rows 4-10. */
static const PtGlyph *GlyphSmall(const char **s)
{
    unsigned code;

    TwinIconsPrepare();
    code = GlyphCode(s);
    if (!sPtFontSmallReady[code]) {
        const uint8_t *src = CtrTwinSmallFontData() + code * 32;
        PtGlyph *g = &sPtFontSmall[code];
        g->width = CtrTwinSmallFontWidth(code);
        if (g->width > 8) g->width = 8;
        TwinSmallGlyphDecode(src, g->width, g->rows);
        sPtFontSmallReady[code] = true;
    }
    return &sPtFontSmall[code];
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
    for (int py = 0; py < h; ++py)
        for (int px = 0; px < w; ++px) {
            uint8_t v = pixels[(y0 + py) * stride + x0 + px];
            if (v)
                Px(x + px, y + py, Tone(pal[v], on));
        }
}

#include "3ds_ui_theme_impl.h"
#include "3ds_icons.h"
#include "3ds_touch_skin_impl.h"

#define BTN_DEPTH 3                     /* the lip under a big button (BACK, PAGE, SAVE's) */
#define COL_DEPTH 2                     /* and under the column's and the rows' */

/* Shortcut item icons from FireRed's icon table, decoded once per item and
 * shown as the 20x20 around their drawn part. */
enum { ICON_REGISTERED, ICON_BIKE, ICON_COUNT };

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
        sIcons[slot].pal[i] = RGB((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
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

/* The icons the view asks for, player then cursor. */
static void IconsDraw(const CtrPokeTouchView *v)
{
    if (sPlayer.key != v->playerFemale)
        IconBuild(&sPlayer, v->playerFemale, CtrPokeTouch_MapPlayerIcon);
    IconDraw(&sPlayer, 0, v->playerX, v->playerY);
    if (v->cursorX >= 0) {
        if (sCursor.key != 0)
            IconBuild(&sCursor, 0, CtrPokeTouch_MapCursorIcon);
        IconDraw(&sCursor, v->cursorFrame, v->cursorX, v->cursorY);
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
    IconsDraw(v);
    return 1;
}

/* The OPTIONS rows: 228x20 on the red frame, 22 apart from (PT_VIEW_X + 6, PT_VIEW_Y + 4). The value
 * sits in a 124 wide box at the right: "<" (28), the value (68) and ">" (28). */
#define OPT_X (PT_VIEW_X + 6)
#define OPT_Y (PT_VIEW_Y + 4)
#define OPT_W 228
#define OPT_H 20
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

static int TextWidth(const char *s)
{
    int w = 0;

    while (*s)
        w += Glyph(&s)->width;
    return w;
}

/* One row: a raised cream row with the label (amber, glowing, when selected) and on its right the value in a light pill
 * between "<" and ">" (a switch for ON and OFF); the zone under a held touch darkens. */
static void OptRow(int row, const CtrPokeTouchView *v)
{
    CtrPokeTouchRect r = OptRect(row);
    int selected = v->optSelected == row, pressed = v->optPressed == row;
    int boxX = r.x + r.w - OPT_BOX_W, zone = pressed ? v->optZone : PT_OPT_ZONE_NONE;
    const EmPal *pal = selected ? &kEmAmber : &kEmCream;
    int fy, mid = boxX + OPT_BOX_W / 2;
    const char *value = v->optValue[row];

    TsBgRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
    if (selected)
        EmGlowRing(r.x, r.y, r.w, r.h);
    fy = EmButton(r.x, r.y, r.w, r.h, pal, COL_DEPTH, zone == PT_OPT_ZONE_LABEL);
    UiText(v->optLabel[row], r.x + 8, fy + 4, pal->ink, pal->inkSh, 1, boxX - 2);
    UiRoundFill(boxX + 2, fy + 2, OPT_BOX_W - 6, r.h - COL_DEPTH - 4, 5, kEmAmber.lip);
    UiRoundFill(boxX + 3, fy + 3, OPT_BOX_W - 8, r.h - COL_DEPTH - 6, 4, UI_RGB(255, 248, 236));
    if (zone == PT_OPT_ZONE_PREV)
        UiRoundFill(boxX + 3, fy + 3, OPT_ARROW_W - 3, r.h - COL_DEPTH - 6, 4, UI_RGB(255, 214, 160));
    if (zone == PT_OPT_ZONE_NEXT)
        UiRoundFill(mid + OPT_BOX_W / 2 - OPT_ARROW_W, fy + 3, OPT_ARROW_W - 5, r.h - COL_DEPTH - 6, 4,
                    UI_RGB(255, 214, 160));
    UiArrow(boxX + 10, fy + 4, -1, EM_STRIPE);
    UiArrow(boxX + OPT_BOX_W - 15, fy + 4, 1, EM_STRIPE);
    if (!strcmp(value, "ON") || !strcmp(value, "OFF"))
        EmSwitch(mid - 13, fy + 3, value[1] == 'N');
    else
        UiText(value, mid - TextWidth(value) / 2, fy + 4, EM_VALUE, UI_RGB(255, 206, 180), 1, boxX + OPT_BOX_W - 18);
}

/* The SAVE screen: FireRed's save information in a card over the frame where the picture goes: a title band, the five
 * lines (label on the left, value on the right) and the player's own map icon, enlarged by 3, on a plate. Each value
 * has its own rectangle, filled with the card's colour before its text, so one that changes (the play clock) is
 * redrawn alone. The dialogue is the bar: the text in the note card, YES over NO (or a lone OK) on its right. */
enum { SF_NAME, SF_BADGES, SF_DEX, SF_TIME, SF_MONEY, SF_COUNT };
static const CtrPokeTouchRect sSavePanel = {14, 10, 220, 150};
static const CtrPokeTouchRect sSaveButtons = {170, 170, 76, 70};   /* right of the note card, glow included */
static const CtrPokeTouchRect sSaveYes = {172, 172, 72, 30};
static const CtrPokeTouchRect sSaveNo = {172, 206, 72, 30};
#define SAVE_CARD UI_RGB(255, 252, 244)
#define SAVE_LINE_Y 42
#define SAVE_LINE_STEP 22
#define SAVE_VALUE_RIGHT 150
static const char *const sSaveLabel[SF_COUNT] = {"PLAYER", "BADGES", "POK\xc3\xa9" "DEX", "TIME", "MONEY"};

static CtrPokeTouchRect SaveField(int f)
{
    CtrPokeTouchRect r = {84, (int16_t)(SAVE_LINE_Y + f * SAVE_LINE_STEP - 3), SAVE_VALUE_RIGHT + 2 - 84, 16};

    return r;
}

static void SaveFieldDraw(int f, const CtrPokeTouchView *v)
{
    CtrPokeTouchRect r = SaveField(f);
    char text[24];

    Fill(r.x, r.y, r.w, r.h, SAVE_CARD);
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
    for (char *c = text; f == SF_NAME && *c; ++c)
        if (*c >= 'a' && *c <= 'z')
            *c = (char)(*c - ('a' - 'A'));
    UiText(text, SAVE_VALUE_RIGHT - TextWidth(text), r.y + 3, UI_RGB(48, 80, 200), UI_RGB(176, 196, 240), 1,
           SAVE_VALUE_RIGHT + 1);
}

/* The player's portrait: FireRed's own map icon of Red or Leaf, enlarged by 3, on a light plate. */
static void SaveSprite(const CtrPokeTouchView *v)
{
    const int scale = 3, x = 166, y = 62;

    UiRoundFill(x - 6, y - 6, MARKER * scale + 12, MARKER * scale + 12, 5, UI_RGB(255, 226, 190));
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
    const CtrPokeTouchRect *p = &sSavePanel;

    UiRoundFill(p->x, p->y, p->w, p->h, 5, UI_RGB(60, 64, 92));
    UiRoundFill(p->x + 1, p->y + 1, p->w - 2, p->h - 2, 4, SAVE_CARD);
    EmGradFill(p->x + 1, p->y + 1, p->w - 2, 22, 4, kEmAmber.top, kEmAmber.bottom);
    Fill(p->x + 1, p->y + 23, p->w - 2, 1, kEmAmber.edge);
    UiText("SAVE", p->x + 10, p->y + 7, EM_WHITE, kEmAmber.inkSh, 1, p->x + p->w - 4);
    for (int f = 0; f < SF_COUNT; ++f) {
        UiText(sSaveLabel[f], p->x + 12, SAVE_LINE_Y + f * SAVE_LINE_STEP, EM_DARK, EM_DARK_SH, 1, 84);
        if (f + 1 < SF_COUNT)
            Fill(p->x + 10, SAVE_LINE_Y + f * SAVE_LINE_STEP + 15, SAVE_VALUE_RIGHT - p->x - 8, 1, UI_RGB(226, 220, 210));
        SaveFieldDraw(f, v);
    }
    SaveSprite(v);
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

/* YES amber (glowing when selected), NO violet (amber when selected), a lone OK amber; the one under a touch sinks.
 * While writing, three dots instead. */
static void SaveButtons(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *b = &sSaveButtons;
    int confirm = v->saveState == SAVE_CONFIRM;

    TsBgRect(b->x, b->y, b->w, b->h);
    if (v->saveState == SAVE_WRITING) {
        for (int i = 0; i < 3; ++i)
            UiDisc(b->x + 26 + 12 * i, b->y + 34, 3, i == 1 ? EM_GLOW : UI_RGB(255, 236, 170));
        return;
    }
    for (int which = PT_SAVE_FIRST; which <= (confirm ? PT_SAVE_SECOND : PT_SAVE_FIRST); ++which) {
        CtrPokeTouchRect r = SaveButtonRect(which, v->saveState);
        int sel = !confirm || v->saveSel == which - PT_SAVE_FIRST, press = v->savePressed == which;
        const EmPal *pal = sel ? &kEmAmber : &kEmViolet;
        int fy;

        if (sel)
            EmGlowRing(r.x, r.y, r.w, r.h);
        fy = EmButton(r.x, r.y, r.w, r.h, pal, BTN_DEPTH, press);
        EmLabel(!confirm ? "OK" : which == PT_SAVE_FIRST ? "YES" : "NO", r.x, r.w, fy, r.h - BTN_DEPTH, pal);
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
/* The naming screen the port draws (3ds_field_naming_impl.h, included at the end). */
static void FNameFrame(const CtrPokeTouchView *v);
static void FNameNote(const CtrPokeTouchView *v, int w);

/* The frame's area: the bezel round the GBA picture or the map, OPTIONS' rows, the save card, or the Bag, on the embers. */
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
    if (v->name.on && !v->viewport) {
        FNameFrame(v);
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
        for (int i = 0; i < v->optCount && i < PT_OPT_MAX; ++i)
            OptRow(i, v);
        return;
    }
    if (!FrameShown(v))
        return;
    EmBezel(PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H);
    /* Covered by the GPU's picture when there is one; else the HOME map. */
    if (v->viewport || !(v->home && HomeShow(v)))
        Fill(PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H, v->viewport ? 0 : C_VIEW_EMPTY);
}

/* BACK: the violet button of going back, "< BACK"; pressed it sinks, disabled it is grey. */
static void Back(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchBack;
    const EmPal *pal = v->backEnabled ? &kEmViolet : &kEmGrey;
    int w = TextWidth("BACK"), x = r->x + (r->w - w - 9) / 2, fy;

    TsBgRect(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, pal, BTN_DEPTH, v->backEnabled && v->backPressed);
    UiArrow(x, fy + (r->h - BTN_DEPTH - UI_TEXT_H) / 2, -1, pal->ink);
    UiText("BACK", x + 9, fy + (r->h - BTN_DEPTH - UI_TEXT_H) / 2, pal->ink, pal->inkSh, 1, r->x + r->w);
}

/* PAGE: switches between OPTIONS pages (PAGE 1/2, PAGE 2/2) right above BACK. */
static void OptPageButton(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchOptPage;
    char label[16];
    int fy;

    snprintf(label, sizeof(label), "PAGE %u/%u", (unsigned)(v->optPage + 1), (unsigned)v->optPageCount);
    TsBgRect(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, COL_DEPTH, v->optPagePressed);
    EmLabel(label, r->x, r->w, fy, r->h - COL_DEPTH, &kEmCream);
}

/* The bar: the note card with up to three lines, 15 pixels apart and centred on it; the first line is the subject,
 * the others what is known about it (dimmer). It is narrower when BACK, PAGE or SAVE's buttons are on its right. */
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
    if (v->dex.on && !v->viewport)
        FDexNote(v, w);
    else if (v->card.on && !v->viewport)
        FCardNote(v, w);
    else if (v->learn.on && !v->viewport)
        FLearnNote(v, w);
    else if (v->name.on && !v->viewport)
        FNameNote(v, w);
    else if (v->bag.on && !v->viewport)
        FBagNote(v, w);
    else {
        int pitch = 16, top = NOTE_Y + (NOTE_H - (lines - 1) * pitch - UI_TEXT_H) / 2;

        EmNote(NOTE_X, NOTE_Y, w, NOTE_H);
        for (int i = 0; i < lines; ++i)
            UiText(v->bar[i], NOTE_X + 12, top + i * pitch, i == 0 ? EM_DARK : EM_DIM, EM_DARK_SH, 1,
                   NOTE_X + w - 4);
    }
    if (hasPage)
        OptPageButton(v);
    if (v->backVisible)
        Back(v);
    if (v->save)
        SaveButtons(v);
}

/* An element's area: its rectangle and the glow round it. */
static CtrPokeTouchRect Area(int id)
{
    return UiInset(&gCtrPokeTouchRects[id], -2);
}

/* The column's icons, by button. */
static int ColumnIcon(int id)
{
    return id;          /* kEmberIcon is in the order PT_POKEDEX .. PT_OPTIONS */
}

/* A button: cream, amber and glowing when selected (the screen shown, or the running mode on), sunk when pressed,
 * grey when it cannot be used. The column's carry their icon and label (the Trainer Card the player's name), the
 * shortcuts their key and their item in its well, RUN its switch. */
static void Button(int id, const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchRects[id];
    CtrPokeTouchRect area = Area(id);
    int on = v->enabled[id] || v->selected == id;
    int pressed = v->enabled[id] && v->pressed == id;
    int selected = (v->selected == id && id != PT_RUN) || (id == PT_RUN && v->enabled[id] && v->running);
    const EmPal *pal = !on ? &kEmGrey : selected && id != PT_RUN ? &kEmAmber : &kEmCream;
    int fy, fh = r->h - COL_DEPTH;

    TsBgRect(area.x, area.y, area.w, area.h);
    if (selected && id != PT_RUN && on)
        EmGlowRing(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, pal, COL_DEPTH, pressed);
    switch (id) {
    case PT_REGISTERED:
        EmKeyTab(r->x + r->w / 2, fy + 2, "X");
        EmWell(r->x + 3, fy + 15, r->w - 6, fh - 18, pal);
        ItemArt(ICON_REGISTERED, v->registeredItem, r->x + (r->w - 20) / 2, fy + 15 + (fh - 18 - 20) / 2, on);
        break;
    case PT_BIKE:
        EmKeyTab(r->x + r->w / 2, fy + 2, "Y");
        EmWell(r->x + 3, fy + 15, r->w - 6, fh - 18, pal);
        ItemArt(ICON_BIKE, v->bikeItem, r->x + (r->w - 20) / 2, fy + 15 + (fh - 18 - 20) / 2, on);
        break;
    case PT_RUN:
        UiText("RUN", r->x + 7, fy + (fh - UI_TEXT_H) / 2, pal->ink, pal->inkSh, 1, r->x + 34);
        EmSwitch(r->x + r->w - 33, fy + (fh - 12) / 2, selected);
        break;
    default: {
        char name[16] = {0};
        const char *label = id == PT_POKEDEX ? "POK\xc3\xa9" "DEX" : id == PT_POKEMON ? "POK\xc3\xa9" "MON" : id == PT_BAG ? "BAG"
                          : id == PT_SAVE ? "SAVE" : id == PT_OPTIONS ? "OPTION" : name;

        if (id == PT_CARD) {
            /* The player's name (uppercase), as the game's other labels. */
            int n = 0;

            for (; n < (int)sizeof(name) - 1 && v->playerName[n]; ++n) {
                char ch = v->playerName[n];
                name[n] = (char)(ch >= 'a' && ch <= 'z' ? ch - ('a' - 'A') : ch);
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

void CtrPokeTouch_Update(uint16_t *canvas, const CtrPokeTouchView *old, const CtrPokeTouchView *v, CtrDirtyList *dirty)
{
    int oldHasPage = old->options && !old->viewport && old->optPageCount > 1;
    int newHasPage = v->options && !v->viewport && v->optPageCount > 1;

    sCanvas = canvas;
    if (memcmp(&old->bag, &v->bag, sizeof(v->bag)) || memcmp(&old->dex, &v->dex, sizeof(v->dex))
        || memcmp(&old->card, &v->card, sizeof(v->card)) || memcmp(&old->learn, &v->learn, sizeof(v->learn))
        || memcmp(&old->name, &v->name, sizeof(v->name))) {
        /* The Bag or the Pokedex: its whole area and its note (the description follows the selection). */
        MenuFrame(v);
        Mark(dirty, &sMenuFrame);
        Bar(v);
        Mark(dirty, &sBar);
    } else if (old->viewport != v->viewport || old->home != v->home || old->options != v->options || old->save != v->save
        || (v->options && !v->viewport && old->optCount != v->optCount)
        || (v->home && (old->mapGroup != v->mapGroup || old->mapVersion != v->mapVersion))) {
        MenuFrame(v);
        Mark(dirty, &sMenuFrame);
    } else if (v->home && !v->viewport && (old->playerX != v->playerX || old->playerY != v->playerY
                                           || old->playerFemale != v->playerFemale || old->cursorX != v->cursorX
                                           || old->cursorY != v->cursorY || old->cursorFrame != v->cursorFrame)) {
        /* An icon moved, appeared or blinked: the old and the new rectangles of the icons that
         * changed go back to the map, and both icons are drawn over them again (one may
         * have overlapped the other). */
        CtrPokeTouchRect r = {0, 0, PT_HOME_ICON, PT_HOME_ICON};
        int x, y;
        int cells[4][2] = {{old->playerX, old->playerY}, {v->playerX, v->playerY}, {old->cursorX, old->cursorY},
                           {v->cursorX, v->cursorY}};
        int changed[4] = {old->playerX != v->playerX || old->playerY != v->playerY || old->playerFemale != v->playerFemale,
                          old->playerX != v->playerX || old->playerY != v->playerY || old->playerFemale != v->playerFemale,
                          old->cursorX != v->cursorX || old->cursorY != v->cursorY || old->cursorFrame != v->cursorFrame,
                          old->cursorX != v->cursorX || old->cursorY != v->cursorY || old->cursorFrame != v->cursorFrame};
        for (int i = 0; i < 4; ++i)
            if (changed[i] && MarkerRect(cells[i][0], cells[i][1], &x, &y)) {
                MapRestore(x, y, PT_HOME_ICON, PT_HOME_ICON);
                r.x = (int16_t)(PT_VIEW_X + x);
                r.y = (int16_t)(PT_VIEW_Y + y);
                Mark(dirty, &r);
            }
        IconsDraw(v);
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
        /* A row whose value, selection or press changed: only its rectangle. */
        for (int i = 0; i < v->optCount && i < PT_OPT_MAX; ++i) {
            if (strcmp(old->optLabel[i], v->optLabel[i]) || strcmp(old->optValue[i], v->optValue[i])
                || (old->optSelected == i) != (v->optSelected == i) || (old->optPressed == i) != (v->optPressed == i)
                || (v->optPressed == i && old->optZone != v->optZone)) {
                CtrPokeTouchRect r = OptRect(i);

                OptRow(i, v);
                Mark(dirty, &r);
            }
        }
    }
    if (memcmp(old->bar, v->bar, sizeof(v->bar)) || old->backVisible != v->backVisible || old->save != v->save
        || oldHasPage != newHasPage) {
        Bar(v);
        Mark(dirty, &sBar);
    } else {
        if (newHasPage && (old->optPage != v->optPage || old->optPageCount != v->optPageCount
                           || old->optPagePressed != v->optPagePressed)) {
            OptPageButton(v);
            Mark(dirty, &gCtrPokeTouchOptPage);
        }
        if (v->backVisible && (old->backEnabled != v->backEnabled || old->backPressed != v->backPressed)) {
            Back(v);                        /* a press or a state: only BACK's rectangle */
            Mark(dirty, &gCtrPokeTouchBack);
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

void CtrPokeTouch_Embers(uint16_t *canvas, int on, int pictureShown, CtrDirtyList *dirty)
{
    static const CtrPokeTouchRect picture = {PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H};

    EmbersStep(canvas, on, pictureShown ? &picture : NULL, dirty);
}

#include "3ds_battle_ui_impl.h"
#include "3ds_summary_ui_impl.h"
#include "3ds_field_bag_impl.h"
#include "3ds_field_dex_impl.h"
#include "3ds_field_card_impl.h"
#include "3ds_field_learn_impl.h"
#include "3ds_field_naming_impl.h"
