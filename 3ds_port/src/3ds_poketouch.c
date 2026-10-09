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

const CtrPokeTouchRect gCtrPokeTouchRects[PT_COUNT] = {
    [PT_POKEDEX]    = {248,   2, 70, 25},
    [PT_POKEMON]    = {248,  30, 70, 25},
    [PT_BAG]        = {248,  58, 70, 25},
    [PT_CARD]       = {248,  86, 70, 25},
    [PT_SAVE]       = {248, 114, 70, 25},
    [PT_OPTIONS]    = {248, 142, 70, 25},
    [PT_REGISTERED] = {248, 170, 34, 33},
    [PT_BIKE]       = {284, 170, 34, 33},
    [PT_RUN]        = {248, 205, 70, 33},
};

/* Areas that are not buttons. */
static const CtrPokeTouchRect sMenuFrame = {2, 2, 244, 164};
static const CtrPokeTouchRect sBar = {2, 168, 244, 70};
const CtrPokeTouchRect gCtrPokeTouchBack = {176, 207, 64, 25};

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

/* FireRed's latin_normal glyphs, text and shadow pixels apart. Capitals
 * cover glyph rows 3-11, so a line drawn at `top` has its capitals there. */
static const PtGlyph *Glyph(const char **s)
{
    TwinIconsPrepare();
    unsigned char c = (unsigned char)**s;
    unsigned code = 0;
    ++*s;
    if (c >= 'A' && c <= 'Z') code = 0xbb + c - 'A';
    else if (c >= 'a' && c <= 'z') code = 0xd5 + c - 'a';
    else if (c >= '0' && c <= '9') code = 0xa1 + c - '0';
    else if (c == 0xc3 && (unsigned char)**s == 0xa9) { ++*s; code = 0x1b; }
    else if (c == 0xc2 && (unsigned char)**s == 0xa5) { ++*s; code = 0xb7; }
    else switch (c) {
        case '!': code=0xab; break; case '?': code=0xac; break;
        case '.': code=0xad; break; case '-': code=0xae; break;
        case ',': code=0xb8; break; case '/': code=0xba; break;
        case ':': code=0xf0; break; case '(': code=0x5c; break;
        case ')': code=0x5d; break; case '%': code=0x5b; break;
        default: break;
    }
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

static void Image(const PtImage *im, int x, int y, int on)
{
    TwinIconsPrepare();
    Art(im->pixels, im->pal, im->w, 0, 0, im->w, im->h, x, y, on);
}

#include "3ds_ui_theme_impl.h"
#include "3ds_icons.h"

/* Exterior chrome only. The shared theme and all viewport/game/map pixels
 * keep their existing palette. Integer fills retain crisp pixel corners. */
#define PT_RED_BG UI_RGB(64, 4, 6)
#define PT_RED_PANEL UI_RGB(132, 12, 16)
#define PT_RED_EDGE UI_RGB(238, 58, 50)
#define PT_RED_LIGHT UI_RGB(255, 132, 92)
/* The OPTIONS and SAVE screens wear the same red as the chrome around them: the viewport behind them, cards,
 * insets, the selected row and the dim text (the navy UI_* tones stay for the screens that are still navy). */
#define PT_RED_VIEW UI_RGB(52, 4, 6)
#define PT_RED_CARD UI_RGB(104, 8, 12)
#define PT_RED_INSET UI_RGB(70, 5, 8)
#define PT_RED_SEL UI_RGB(176, 22, 20)
#define PT_RED_SEL_HI UI_RGB(214, 36, 28)
#define PT_RED_RULE UI_RGB(150, 24, 26)
#define PT_RED_DIM UI_RGB(246, 196, 184)
static const UiButtonStyle sChrome = {UI_RGB(190, 22, 22), UI_RGB(255, 100, 70), UI_RGB(102, 8, 8)};
static const UiButtonStyle sChromeOn = {UI_RGB(218, 32, 24), UI_RGB(255, 164, 98), UI_RGB(120, 10, 8)};

static void ChromePanel(const CtrPokeTouchRect *r)
{
    UiPanel(r, PT_RED_PANEL, PT_RED_EDGE);
    Fill(r->x + 3, r->y + 1, r->w - 6, 1, PT_RED_LIGHT);
    Fill(r->x + 3, r->y + r->h - 2, r->w - 6, 1, UI_RGB(92, 6, 8));
}

static void ChromeButton(const CtrPokeTouchRect *r, int state)
{
    /* Restore uncovered corner/pressed-face pixels before a partial repaint. */
    Fill(r->x, r->y, r->w, r->h, r->x >= 248 ? PT_RED_BG : PT_RED_PANEL);
    UiButton(r, state & UI_SELECTED ? &sChromeOn : &sChrome, state);
    if (!(state & (UI_DISABLED | UI_SELECTED | UI_FOCUSED))) {
        CtrPokeTouchRect rim = UiInset(r, 1);
        UiRing(&rim, UI_RADIUS_BUTTON - 1, 1, PT_RED_EDGE);
    }
    if ((state & UI_SELECTED) && !(state & UI_DISABLED)) {
        CtrPokeTouchRect rim = UiInset(r, 1);
        UiRing(&rim, UI_RADIUS_BUTTON - 1, 1, PT_RED_LIGHT);
    }
}

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

/* One row: a card with the label, then the value box "<" value ">". The selected row is the blue of a selection with a
 * bright edge on the left; the zone under a held touch sinks into the pressed blue. */
static void OptRow(int row, const CtrPokeTouchView *v)
{
    CtrPokeTouchRect r = OptRect(row);
    int selected = v->optSelected == row, pressed = v->optPressed == row;
    int boxX = r.x + r.w - OPT_BOX_W, zone = pressed ? v->optZone : PT_OPT_ZONE_NONE;
    CtrPokeTouchRect box = {(int16_t)(boxX + 1), (int16_t)(r.y + 3), OPT_BOX_W - 4, (int16_t)(r.h - 6)};

    UiPanel(&r, selected ? PT_RED_SEL : PT_RED_CARD, selected ? PT_RED_LIGHT : PT_RED_EDGE);
    if (selected)
        Fill(r.x + 2, r.y + 4, 2, r.h - 8, PT_RED_LIGHT);
    if (zone == PT_OPT_ZONE_LABEL)
        UiRoundFill(r.x + 5, r.y + 3, boxX - r.x - 8, r.h - 6, 2, PT_RED_SEL_HI);
    UiRoundRect(&box, 2, PT_RED_INSET);
    if (zone == PT_OPT_ZONE_PREV)
        UiRoundFill(box.x, box.y, OPT_ARROW_W - 1, box.h, 2, PT_RED_SEL_HI);
    if (zone == PT_OPT_ZONE_NEXT)
        UiRoundFill(boxX + OPT_ARROW_W, box.y, box.x + box.w - boxX - OPT_ARROW_W, box.h, 2, PT_RED_SEL_HI);
    UiText(v->optLabel[row], r.x + 9, r.y + 6, UI_TEXT, UI_TEXT_SH, 1, boxX - 2);
    UiArrow(boxX + 10, r.y + 6, -1, PT_RED_LIGHT);
    UiArrow(boxX + OPT_BOX_W - 15, r.y + 6, 1, PT_RED_LIGHT);
    {
        int mid = boxX + OPT_BOX_W / 2, w = TextWidth(v->optValue[row]);

        UiText(v->optValue[row], mid - w / 2, r.y + 6, UI_TEXT, UI_TEXT_SH, 1, boxX + OPT_BOX_W - 2);
    }
}

/* The SAVE screen: FireRed's save information over the whole viewport, a card on the dark frame: the title band
 * (FireRed's red), the player's icon (FireRed's own Red or Leaf map icon, enlarged by exactly 3, nearest neighbour)
 * on its plate and five lines, labels dim on the left and values white on the right, a rule between them. Each value
 * has its own rectangle, filled with the card's colour before its text, so one that changes (the play clock) is
 * redrawn alone. The dialogue is the contextual bar: text on the left, the buttons in SAVE_BUTTONS on the right. */
enum { SF_NAME, SF_BADGES, SF_DEX, SF_TIME, SF_MONEY, SF_COUNT };
static const CtrPokeTouchRect sSavePanel = {8, 8, 232, 152};
static const CtrPokeTouchRect sSaveButtons = {170, 174, 68, 60};   /* inside the bar, right of its text */

static CtrPokeTouchRect SaveField(int f)
{
    CtrPokeTouchRect r = {140, (int16_t)(35 + 21 * f), 92, 16};
    return r;
}

static void SaveFieldDraw(int f, const CtrPokeTouchView *v)
{
    CtrPokeTouchRect r = SaveField(f);
    char text[24];

    Fill(r.x, r.y, r.w, r.h, PT_RED_CARD);
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
    UiText(text, r.x + r.w - 2 - TextWidth(text), r.y + 3, UI_TEXT, UI_TEXT_SH, 1, r.x + r.w);
}

/* The player's icon, 3x, on a round-cornered plate. */
static void SaveSprite(const CtrPokeTouchView *v)
{
    const int x0 = 18, y0 = 40, scale = 3;
    CtrPokeTouchRect plate = {(int16_t)(x0 - 5), (int16_t)(y0 - 5), MARKER * scale + 10, MARKER * scale + 10};

    UiPanel(&plate, PT_RED_INSET, PT_RED_EDGE);
    if (sPlayer.key != v->playerFemale)
        IconBuild(&sPlayer, v->playerFemale, CtrPokeTouch_MapPlayerIcon);
    if (!sPlayer.ok)
        return;
    for (int py = 0; py < MARKER; ++py)
        for (int px = 0; px < MARKER; ++px) {
            uint8_t index = sPlayer.pixels[0][py * MARKER + px];
            if (index)
                Fill(x0 + px * scale, y0 + py * scale, scale, scale, sPlayer.pal[index]);
        }
}

static void SaveDraw(const CtrPokeTouchView *v)
{
    static const struct { const char *text; int y; } sLabels[SF_COUNT] = {
        {"PLAYER", 38}, {"BADGES", 59}, {"POK\xc3\xa9" "DEX", 80}, {"PLAY TIME", 101}, {"MONEY", 122}};
    static const char title[] = "POK\xc3\xa9MON FIRE RED";
    static const UiButtonStyle sTitle = UI_STYLE_FIGHT;
    CtrPokeTouchRect band = {10, 10, 228, 19};

    UiPanel(&sSavePanel, PT_RED_CARD, PT_RED_EDGE);
    UiRoundFill(band.x, band.y, band.w, band.h, 2, sTitle.body);
    Fill(band.x + 2, band.y + 1, band.w - 4, 7, UiMix(sTitle.body, sTitle.lit, 3));
    Fill(band.x + 2, band.y + band.h - 1, band.w - 4, 1, sTitle.shade);
    UiTextCentered(title, &band, 15, UI_TEXT, sTitle.shade, 1);
    SaveSprite(v);
    for (int f = 0; f < SF_COUNT; ++f) {
        UiText(sLabels[f].text, 84, sLabels[f].y, PT_RED_DIM, UI_TEXT_SH, 1, 140);
        if (f + 1 < SF_COUNT)
            Fill(84, sLabels[f].y + 15, 148, 1, PT_RED_RULE);
        SaveFieldDraw(f, v);
    }
    UiTextCentered("Your progress is written to the save file.", &sSavePanel, 142, PT_RED_DIM, UI_TEXT_SH, 1);
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

/* The dialogue's buttons: YES over NO, or a lone OK, on the right of the bar's text. */
static CtrPokeTouchRect SaveButtonRect(int which, int state)
{
    CtrPokeTouchRect r = {172, 0, 64, 24};

    if (state == SAVE_CONFIRM)
        r.y = (int16_t)(which == PT_SAVE_FIRST ? 178 : 206);
    else
        r.y = 192;
    return r;
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

/* YES is the green of going ahead, NO the navy of going back, OK the primary blue; the selected one carries the focus
 * ring, the one under a touch is pressed. While writing, a progress strip of dots instead. */
static void SaveButtons(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *b = &sSaveButtons;
    static const char *const sConfirm[2] = {"YES", "NO"};

    Fill(b->x, b->y, b->w, b->h, PT_RED_PANEL);
    if (v->saveState == SAVE_WRITING) {
        for (int i = 0; i < 3; ++i)
            UiDisc(b->x + 22 + 12 * i, b->y + 30, 3, i == 1 ? PT_RED_DIM : PT_RED_LIGHT);
        return;
    }
    for (int which = PT_SAVE_FIRST; which <= (v->saveState == SAVE_CONFIRM ? PT_SAVE_SECOND : PT_SAVE_FIRST); ++which) {
        CtrPokeTouchRect r = SaveButtonRect(which, v->saveState);
        int selected = v->saveState != SAVE_CONFIRM || v->saveSel == which - PT_SAVE_FIRST;
        int state = (selected ? UI_FOCUSED : 0) | (v->savePressed == which ? UI_PRESSED : 0);
        const char *label = v->saveState == SAVE_CONFIRM ? sConfirm[which - PT_SAVE_FIRST] : "OK";
        ChromeButton(&r, state);
        UiTextCentered(label, &r, UiButtonLabelTop(&r, state, 1), UiLabelColor(state), UI_TEXT_SH, 1);
    }
}

/* The exterior FireRed-red backdrop. */
static void Frame(void)
{
    Fill(0, 0, W, H, PT_RED_BG);
    Fill(0, 0, W, 2, PT_RED_EDGE);
}

/* The viewport's frame: a light rim round the picture (the map, the GBA screen, OPTIONS or SAVE). */
static void MenuFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sMenuFrame;

    UiRoundRect(r, UI_RADIUS_PANEL, PT_RED_EDGE);
    Fill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, UI_OUTLINE);
    if (v->save && !v->viewport) {
        Fill(PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H, PT_RED_VIEW);
        SaveDraw(v);
        return;
    }
    if (v->options && !v->viewport) {
        Fill(PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H, PT_RED_VIEW);
        for (int i = 0; i < v->optCount && i < PT_OPT_MAX; ++i)
            OptRow(i, v);
        return;
    }
    /* Covered by the GPU's picture when there is one; else the HOME map. */
    if (v->viewport || !(v->home && HomeShow(v)))
        Fill(PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H, v->viewport ? 0 : C_VIEW_EMPTY);
}

/* BACK: the navy button of going back, "< BACK"; pressed it sinks, disabled it is grey. */
static void Back(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchBack;
    int state = (v->backEnabled ? 0 : UI_DISABLED) | (v->backPressed ? UI_PRESSED : 0);
    int top = UiButtonLabelTop(r, state, 1), w = TextWidth("BACK");
    uint16_t ink = UiLabelColor(state);
    int x = r->x + (r->w - w - 9) / 2;

    ChromeButton(r, state);
    UiArrow(x, top, -1, ink);
    UiText("BACK", x + 9, top, ink, UI_TEXT_SH, 1, r->x + r->w);
}

/* The contextual bar: up to three lines, 18 pixels apart, the block centred on the bar's middle line (y = 181 / 199 /
 * 217 with three); the first line is the subject (white), the others what is known about it (dim). A blue edge on the
 * left marks it as the screen's caption. */
static void Bar(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &sBar;
    int lines = 0;

    ChromePanel(r);
    Fill(r->x + 2, r->y + 6, 2, r->h - 12, PT_RED_EDGE);
    for (int i = 0; i < PT_BAR_LINES; ++i)
        if (v->bar[i][0])
            lines = i + 1;
    int top = 203 - ((lines - 1) * 18 + 9) / 2;
    /* The text stops short of BACK, or of SAVE's buttons, when they are there. */
    int limit = v->backVisible ? gCtrPokeTouchBack.x - 4
              : v->save && v->saveState != SAVE_WRITING ? sSaveButtons.x - 2 : r->x + r->w - 8;
    for (int i = 0; i < lines; ++i)
        UiText(v->bar[i], 10, top + i * 18, i == 0 ? UI_TEXT : (v->save || v->options) && !v->viewport ? PT_RED_DIM : UI_TEXT_DIM, UI_TEXT_SH, 1, limit);
    if (v->backVisible)
        Back(v);
    if (v->save)
        SaveButtons(v);
}

/* The colour that marks each destination on its button: the same as its role everywhere (POKeMON blue, BAG green). */
static const uint16_t sAccent[PT_COUNT] = {
    [PT_POKEDEX] = UI_RGB(255, 194, 40), [PT_POKEMON] = UI_RGB(64, 190, 240), [PT_BAG] = UI_RGB(60, 224, 100),
    [PT_CARD] = UI_RGB(236, 150, 50), [PT_SAVE] = UI_RGB(150, 110, 230), [PT_OPTIONS] = UI_RGB(130, 150, 186),
    [PT_REGISTERED] = UI_RGB(64, 140, 240), [PT_BIKE] = UI_RGB(60, 186, 100), [PT_RUN] = UI_RGB(60, 186, 100),
};

/* A shortcut's key: a small round badge with the letter (X blue, Y green, as on the console). */
static void KeyBadge(char letter, int x, int y, uint16_t color)
{
    static const uint8_t sX[5] = {5, 5, 2, 5, 5}, sY[5] = {5, 5, 2, 2, 2};
    const uint8_t *g = letter == 'X' ? sX : sY;

    UiRoundFill(x, y, 9, 9, 2, UI_OUTLINE);
    UiRoundFill(x + 1, y + 1, 7, 7, 2, color);
    for (int row = 0; row < 5; ++row)
        for (int col = 0; col < 3; ++col)
            if (g[row] & (4 >> col))
                Px(x + 3 + col, y + 2 + row, UI_TEXT);
}

/* A sidebar button: a dark raised card with its destination's colour as an edge on the left, the icon and the label.
 * Selected (the screen shown, or the running mode on) it turns the selection blue with a bright ring; pressed it sinks;
 * disabled the icon is grey and the label dim. */
/* The side menu's icons from the pack's own 24x24 art, reduced by area to 16x16 (the 12x12 samples stay as the fallback). */
static PtSoft sSoft[6];

static void SoftIconsPrepare(void)
{
    static bool ready;

    if (ready)
        return;
    ready = true;
    for (int n = 0; n < 6; ++n)
        PokeTouch_SoftIcon(sMenuItemArt[n], &sSoft[n]);
}

static void Button(int id, const CtrPokeTouchView *v)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchRects[id];
    int on = v->enabled[id] || v->selected == id;
    int pressed = v->enabled[id] && v->pressed == id;
    int selected = v->selected == id || (id == PT_RUN && v->enabled[id] && v->running);
    int state = (pressed ? UI_PRESSED : 0) | (selected ? UI_SELECTED : 0) | (on ? 0 : UI_DISABLED);
    int dy = pressed ? 1 : 0, face = r->h - 2 - (pressed ? 1 : UI_DEPTH);
    uint16_t ink = on ? UI_TEXT : UI_TEXT_OFF;

    ChromeButton(r, state);
    if (id <= PT_OPTIONS) {
        static const PtImage *const sSideIcons[] = {
            [PT_POKEDEX] = &sPt_icon_pokedex, [PT_POKEMON] = &sPt_icon_pokemon, [PT_BAG] = &sPt_icon_bag,
            [PT_CARD] = &sPt_icon_card, [PT_SAVE] = &sPt_icon_save, [PT_OPTIONS] = &sPt_icon_options,
        };
        static const char *const sLabels[] = {
            [PT_POKEDEX] = "POK\xc3\xa9" "DEX", [PT_POKEMON] = "POK\xc3\xa9MON", [PT_BAG] = "BAG",
            [PT_CARD] = "CARD", [PT_SAVE] = "SAVE", [PT_OPTIONS] = "OPTIONS",
        };
        /* The Trainer Card is the player's own: it carries their name (7 characters at most). */
        const char *label = id == PT_CARD && v->playerName[0] ? v->playerName : sLabels[id];
        int mid = r->y + 1 + dy + face / 2;

        SoftIconsPrepare();
        Fill(r->x + 3, mid - 6, 2, 12, on ? sAccent[id] : UI_TEXT_OFF);
        if (sSoft[id].ok)
            PokeTouch_SoftDraw(&sSoft[id], r->x + 6, mid - 8, on);
        else
            Image(sSideIcons[id], r->x + 8, mid - 6, on);
        UiText(label, r->x + 23, mid - 4, ink, UI_TEXT_SH, 1, r->x + r->w - 3);
        return;
    }
    switch (id) {
    case PT_REGISTERED:
    case PT_BIKE:
        ItemArt(id == PT_REGISTERED ? ICON_REGISTERED : ICON_BIKE, id == PT_REGISTERED ? v->registeredItem : v->bikeItem,
                r->x + 10, r->y + 9 + dy, on);
        KeyBadge(id == PT_REGISTERED ? 'X' : 'Y', r->x + 3, r->y + 3 + dy, on ? sAccent[id] : UI_TEXT_OFF);
        break;
    case PT_RUN:
        Fill(r->x + 3, r->y + 9 + dy, 2, 12, on ? (selected ? UI_OK : UI_TEXT_DIM) : UI_TEXT_OFF);
        UiText("RUN", r->x + 9, r->y + 11 + dy, ink, UI_TEXT_SH, 1, r->x + 40);
        BIconsPrepare();
        UiImage(sBImg[BI_SHOE].image.pixels, sBImg[BI_SHOE].image.pal, 24, 24, 24, r->x + 40, r->y + 4 + dy, 1, on);
        break;
    }
}

bool CtrPokeTouch_HitBack(int x, int y)
{
    const CtrPokeTouchRect *r = &gCtrPokeTouchBack;

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
    sCanvas = canvas;
    if (old->viewport != v->viewport || old->home != v->home || old->options != v->options || old->save != v->save || (v->home && (old->mapGroup != v->mapGroup || old->mapVersion != v->mapVersion))) {
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
    if (memcmp(old->bar, v->bar, sizeof(v->bar)) || old->backVisible != v->backVisible || old->save != v->save) {
        Bar(v);
        Mark(dirty, &sBar);
    } else if (v->backVisible && (old->backEnabled != v->backEnabled || old->backPressed != v->backPressed)) {
        Back(v);                        /* a press or a state: only BACK's rectangle */
        Mark(dirty, &gCtrPokeTouchBack);
    } else if (v->save && (old->saveState != v->saveState || old->saveSel != v->saveSel
                           || old->savePressed != v->savePressed)) {
        SaveButtons(v);                 /* the selection or a press: only the buttons' area */
        Mark(dirty, &sSaveButtons);
    }
    for (int i = 0; i < PT_COUNT; ++i) {
        if (!Changed(i, old, v))
            continue;
        Button(i, v);
        Mark(dirty, &gCtrPokeTouchRects[i]);
    }
}

#include "3ds_battle_ui_impl.h"
#include "3ds_summary_ui_impl.h"
