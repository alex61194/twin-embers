#ifndef CTR_ICONS_H
#define CTR_ICONS_H
/* The lower screen's icons (included by 3ds_poketouch.c and by the battle screen, whose helpers it needs: the pack's LZ
 * loader, UiImage, sCanvas). Port-made 24x24 art (battle_ui_art.h), the game's item icons at full size from the user's
 * pack, and the soft side-menu icons made from them. */
#include "battle_ui_art.h"

/* A pack item icon at its full 24x24 (the menu icons above keep a 12x12 sample): the battle buttons draw it as it is.
 * 0 when the pack or the entry is missing or malformed; the caller keeps the port's own art then. */
static int PokeTouch_PackIcon24(const char *art, uint8_t pixels[24 * 24], uint16_t pal[16])
{
    char path[96];
    uint8_t tiles[512], colors[32], out[24 * 24];
    int pitch, n, visible = 0;

    n = snprintf(path, sizeof(path), "graphics/items/icons/%s.4bpp.lz", art);
    if (n < 0 || (size_t)n >= sizeof(path))
        return 0;
    n = PokeTouch_LoadLz(path, tiles, sizeof(tiles));
    if (n != 9 * 32 && n != 16 * 32)
        return 0;
    pitch = n == 9 * 32 ? 3 : 4;
    n = snprintf(path, sizeof(path), "graphics/items/icon_palettes/%s.gbapal.lz", art);
    if (n < 0 || (size_t)n >= sizeof(path) || PokeTouch_LoadLz(path, colors, sizeof(colors)) != 32)
        return 0;
    pal[0] = 0;
    for (int i = 1; i < 16; ++i) {
        uint16_t c = (uint16_t)colors[i * 2] | (uint16_t)(colors[i * 2 + 1] << 8);
        unsigned g = (c >> 5) & 31;
        pal[i] = (uint16_t)((c & 31) << 11 | (g << 1 | g >> 4) << 5 | (c >> 10 & 31));
    }
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x) {
            uint8_t packed = tiles[(y / 8 * pitch + x / 8) * 32 + y % 8 * 4 + x % 8 / 2];
            uint8_t v = (x & 1) ? packed >> 4 : packed & 15;

            out[y * 24 + x] = v;
            visible |= v != 0;
        }
    if (!visible)
        return 0;
    memcpy(pixels, out, sizeof(out));
    return 1;
}

/* The battle's icons, all 24x24 so every label sits at the same height: the game's own Bag and Poke Ball at their full
 * size from the pack (the 12x12 menu samples, doubled, when it has none), the port's own burst, shoe, rock and bait. */
enum { BI_BAG, BI_POKEBALL, BI_SHOE, BI_BURST, BI_ROCK, BI_BAIT, BI_COUNT };
static struct {
    PtImage image;
    uint8_t pixels[24 * 24];
    int scale;                  /* 1 for 24x24 art, 2 for the 12x12 samples used as a fallback */
} sBImg[BI_COUNT];

static void BArt(int i, const char (*rows)[25], const uint16_t *pal, int n)
{
    sBImg[i].image.w = sBImg[i].image.h = 24;
    sBImg[i].image.pixels = sBImg[i].pixels;
    memcpy(sBImg[i].image.pal, pal, (size_t)n * sizeof(uint16_t));
    sBImg[i].scale = 1;
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x)
            sBImg[i].pixels[y * 24 + x] = (uint8_t)(rows[y][x] - '0');
}

static void BIconsPrepare(void)
{
    static int ready;
    static const char *const pack[2] = {"berry_pouch", "poke_ball"};

    if (ready)
        return;
    ready = 1;
    TwinIconsPrepare();
    for (int i = 0; i < 2; ++i) {
        if (PokeTouch_PackIcon24(pack[i], sBImg[i].pixels, sBImg[i].image.pal)) {
            sBImg[i].image.w = sBImg[i].image.h = 24;
            sBImg[i].image.pixels = sBImg[i].pixels;
            sBImg[i].scale = 1;
        } else {
            sBImg[i].image = i == BI_BAG ? sPt_icon_bag : sPt_icon_pokemon;
            sBImg[i].scale = 2;
        }
    }
    BArt(BI_SHOE, sBArtShoe, sBArtShoePal, 9);
    BArt(BI_BURST, sBArtBurst, sBArtBurstPal, 4);
    BArt(BI_ROCK, sBArtRock, sBArtRockPal, 6);
    BArt(BI_BAIT, sBArtBait, sBArtBaitPal, 7);
}

static void BIconAt(int i, int x, int y)
{
    UiImage(sBImg[i].image.pixels, sBImg[i].image.pal, sBImg[i].image.w, sBImg[i].image.w, sBImg[i].image.h, x, y,
            sBImg[i].scale, 1);
}


/* A soft icon: RGB565 and an 8-bit coverage per pixel, so its edge blends into whatever it is drawn on. */
#define PT_SOFT 16
typedef struct {
    uint16_t rgb[PT_SOFT * PT_SOFT];
    uint8_t alpha[PT_SOFT * PT_SOFT];
    int ok;
} PtSoft;

/* The pack's 24x24 icon `art` reduced to PT_SOFT x PT_SOFT by area (each output pixel covers 1.5 x 1.5 source pixels;
 * colours averaged over the pixels that are drawn only, so a dark outline stays dark and a transparent edge does not
 * darken or lighten it). 0 when the icon is missing: the caller keeps the 12x12 sample. */
UI_FN int PokeTouch_SoftIcon(const char *art, PtSoft *out)
{
    uint8_t px[24 * 24];
    uint16_t pal[16];
    const float step = 24.0f / PT_SOFT;

    if (!PokeTouch_PackIcon24(art, px, pal))
        return 0;
    for (int oy = 0; oy < PT_SOFT; ++oy)
        for (int ox = 0; ox < PT_SOFT; ++ox) {
            float x0 = ox * step, x1 = x0 + step, y0 = oy * step, y1 = y0 + step;
            float wsum = 0, wopaque = 0, r = 0, g = 0, b = 0;

            for (int sy = (int)y0; sy < 24 && sy < y1; ++sy)
                for (int sx = (int)x0; sx < 24 && sx < x1; ++sx) {
                    float wx = (sx + 1 < x1 ? sx + 1 : x1) - (sx > x0 ? sx : x0);
                    float wy = (sy + 1 < y1 ? sy + 1 : y1) - (sy > y0 ? sy : y0);
                    float w = wx * wy;
                    uint8_t v = px[sy * 24 + sx];

                    wsum += w;
                    if (v) {
                        wopaque += w;
                        r += w * (pal[v] >> 11);
                        g += w * ((pal[v] >> 5) & 63);
                        b += w * (pal[v] & 31);
                    }
                }
            if (wopaque <= 0.0f || wsum <= 0.0f) {
                out->rgb[oy * PT_SOFT + ox] = 0;
                out->alpha[oy * PT_SOFT + ox] = 0;
                continue;
            }
            out->rgb[oy * PT_SOFT + ox] = (uint16_t)(((int)(r / wopaque + 0.5f)) << 11 | ((int)(g / wopaque + 0.5f)) << 5
                                                     | (int)(b / wopaque + 0.5f));
            out->alpha[oy * PT_SOFT + ox] = (uint8_t)(wopaque / wsum * 255.0f + 0.5f);
        }
    out->ok = 1;
    return 1;
}

/* Blends a soft icon onto the canvas; off, it is toned grey like the other icons. */
UI_FN void PokeTouch_SoftDraw(const PtSoft *icon, int x, int y, int on)
{
    for (int py = 0; py < PT_SOFT; ++py)
        for (int px = 0; px < PT_SOFT; ++px) {
            int a = icon->alpha[py * PT_SOFT + px], dx = x + px, dy = y + py;
            uint16_t c = on ? icon->rgb[py * PT_SOFT + px] : Tone(icon->rgb[py * PT_SOFT + px], 0);
            uint16_t *p;
            int dr, dg, db;

            if (!a || (unsigned)dx >= W || (unsigned)dy >= H)
                continue;
            p = &sCanvas[(size_t)dx * H + (H - 1 - (size_t)dy)];
            if (a >= 250) {
                *p = c;
                continue;
            }
            dr = *p >> 11; dg = (*p >> 5) & 63; db = *p & 31;
            dr += ((c >> 11) - dr) * a / 255;
            dg += (((c >> 5) & 63) - dg) * a / 255;
            db += ((c & 31) - db) * a / 255;
            *p = (uint16_t)(dr << 11 | dg << 5 | db);
        }
}
#endif
