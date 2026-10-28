/* The lower screen's ember skin (ember_skin.h, made by tools/make_ember_skin.py): the backdrop every screen is drawn on
 * and the pieces of the modern menus drawn over it: raised rounded buttons with a lip (amber when chosen, cream
 * otherwise, violet for going back), the note card, the frame round the GBA picture, the switch, the icons; and the
 * embers that rise slowly over the backdrop. Included by 3ds_poketouch.c after its Px / Fill and the theme. */
#include "ember_skin.h"

static uint16_t TsGet(int x, int y)
{
    return sCanvas[(size_t)x * H + (H - 1 - (size_t)y)];
}

/* The backdrop in the canvas' own order (column by column, bottom up), made the first time it is drawn: a rectangle of
 * it is then a copy per column instead of a lookup per pixel, which every screen pays more than once a redraw. */
static uint16_t sTsBg[W * H];
static int sTsBgReady;

static void TsBgPrepare(void)
{
    for (int cx = 0; cx < W; ++cx)
        for (int cy = 0; cy < H; ++cy)
            sTsBg[(size_t)cx * H + (H - 1 - (size_t)cy)] = EMBER_BG(cx, cy);
    sTsBgReady = 1;
}

/* The backdrop back over a rectangle. */
static void TsBgRect(int x, int y, int w, int h)
{
    int x0 = x < 0 ? 0 : x, y0 = y < 0 ? 0 : y, x1 = x + w > W ? W : x + w, y1 = y + h > H ? H : y + h;

    if (x0 >= x1 || y0 >= y1)
        return;
    if (!sTsBgReady)
        TsBgPrepare();
    for (int cx = x0; cx < x1; ++cx) {
        size_t at = (size_t)cx * H + (H - (size_t)y1);

        memcpy(sCanvas + at, sTsBg + at, (size_t)(y1 - y0) * sizeof(uint16_t));
    }
}

static void TsBackdrop(void)
{
    TsBgRect(0, 0, W, H);
}

/* `c` over the canvas at (x, y) by a/255. */
static void EmBlend(int x, int y, uint16_t c, int a)
{
    if ((unsigned)x >= W || (unsigned)y >= H || a <= 0)
        return;
    Px(x, y, a >= 255 ? c : UiMix(TsGet(x, y), c, a * 8 / 255));
}

/* The palettes of the raised pieces: the face from `top` to `bottom`, its lip and outline, and the ink of a label. */
typedef struct { uint16_t top, bottom, lip, edge, ink, inkSh; } EmPal;

#define EM_DARK UI_RGB(98, 98, 98)                  /* FireRed's text grey and its shadow, on the light pieces */
#define EM_DARK_SH UI_RGB(213, 213, 205)
#define EM_DIM UI_RGB(136, 120, 112)
#define EM_WHITE UI_RGB(255, 255, 255)
#define EM_STRIPE UI_RGB(232, 112, 28)
#define EM_GLOW UI_RGB(255, 170, 60)
#define EM_VALUE UI_RGB(208, 48, 32)                /* a value on a light piece (the game's red) */
static const EmPal kEmAmber = {UI_RGB(255, 206, 96), UI_RGB(236, 108, 28), UI_RGB(170, 70, 14), UI_RGB(110, 40, 6),
                               EM_WHITE, UI_RGB(110, 40, 6)};
static const EmPal kEmCream = {UI_RGB(255, 252, 244), UI_RGB(232, 226, 214), UI_RGB(150, 136, 126), UI_RGB(96, 80, 72),
                               EM_DARK, EM_DARK_SH};
static const EmPal kEmGrey = {UI_RGB(200, 192, 196), UI_RGB(150, 140, 146), UI_RGB(108, 98, 104), UI_RGB(70, 60, 66),
                              UI_RGB(120, 112, 116), UI_RGB(206, 200, 202)};
static const EmPal kEmViolet = {UI_RGB(120, 112, 128), UI_RGB(70, 62, 82), UI_RGB(46, 38, 58), UI_RGB(30, 24, 40),
                                EM_WHITE, UI_RGB(30, 24, 40)};

/* Rows [y, y + h) of a rounded rectangle, each row a blend from `top` to `bottom`. */
static void EmGradFill(int x, int y, int w, int h, int radius, uint16_t top, uint16_t bottom)
{
    if (w <= 0 || h <= 0)
        return;
    if (radius > 5)
        radius = 5;
    if (radius * 2 > h)
        radius = h / 2;
    /* Column by column, the canvas' own order: a row at a time would step a whole column of memory per pixel. A pixel
     * is drawn where its row's corner cut does not reach its column. */
    uint16_t colour[H];
    int r0 = y < 0 ? -y : 0, r1 = y + h > H ? H - y : h;

    for (int row = r0; row < r1; ++row)
        colour[row - r0] = UiMix(top, bottom, h > 1 ? row * 8 / (h - 1) : 0);
    for (int col = 0; col < w; ++col) {
        int cx = x + col, edgeX = col < w - 1 - col ? col : w - 1 - col, first = r0, last = r1;

        if ((unsigned)cx >= W)
            continue;
        while (first < last && first < 4 && sUiCut[radius][first] > edgeX)
            ++first;
        while (last > first && h - last < 4 && sUiCut[radius][h - last] > edgeX)
            --last;
        uint16_t *p = sCanvas + (size_t)cx * H + (H - 1 - (size_t)(y + first));

        for (int row = first; row < last; ++row)
            *p-- = colour[row - r0];
    }
}

/* A raised piece: the lip `depth` pixels under the face, both outlined; pressed, the face sinks onto the lip. The
 * caller has put the backdrop back under it. Returns where the face starts (for its label). */
static int EmButton(int x, int y, int w, int h, const EmPal *p, int depth, int pressed)
{
    int face = h - depth, fy = pressed ? y + depth : y;

    if (!pressed) {
        UiRoundFill(x, y + depth, w, face, 5, p->edge);
        UiRoundFill(x + 1, y + depth + 1, w - 2, face - 2, 4, p->lip);
    }
    UiRoundFill(x, fy, w, face, 5, p->edge);
    EmGradFill(x + 1, fy + 1, w - 2, face - 2, 4, pressed ? UiMix(p->top, p->bottom, 4) : p->top, p->bottom);
    Fill(x + 5, fy + 1, w - 10, 1, UiMix(p->top, EM_WHITE, 4));
    return fy;
}

/* The glow round a chosen piece: two soft rings outside it. */
static void EmGlowRing(int x, int y, int w, int h)
{
    for (int i = 1; i <= 2; ++i) {
        int a = i == 1 ? 150 : 70;

        for (int cx = x + 3; cx < x + w - 3; ++cx) {
            EmBlend(cx, y - i, EM_GLOW, a);
            EmBlend(cx, y + h - 1 + i, EM_GLOW, a);
        }
        for (int cy = y + 3; cy < y + h - 3; ++cy) {
            EmBlend(x - i, cy, EM_GLOW, a);
            EmBlend(x + w - 1 + i, cy, EM_GLOW, a);
        }
        EmBlend(x - i + 2, y - i + 2, EM_GLOW, a / 2);
        EmBlend(x + w - 3 + i, y - i + 2, EM_GLOW, a / 2);
        EmBlend(x - i + 2, y + h - 3 + i, EM_GLOW, a / 2);
        EmBlend(x + w - 3 + i, y + h - 3 + i, EM_GLOW, a / 2);
    }
}

/* A label centred across [x, x + w) on a face that starts at `fy` and is `fh` tall. */
static void EmLabel(const char *s, int x, int w, int fy, int fh, const EmPal *p)
{
    int tw = UiTextWidth(s, 1);

    UiText(s, x + (w - tw) / 2, fy + (fh - UI_TEXT_H) / 2, p->ink, p->inkSh, 1, x + w);
}

/* The note card: a light card with an ember stripe on its left; the text is the caller's. */
static void EmNote(int x, int y, int w, int h)
{
    UiRoundFill(x, y, w, h, 5, UI_RGB(60, 64, 92));
    EmGradFill(x + 1, y + 1, w - 2, h - 2, 4, UI_RGB(255, 255, 255), UI_RGB(226, 230, 238));
    Fill(x + 4, y + 6, 3, h - 12, EM_STRIPE);
}

/* The frame round the GBA picture (w x h at x, y): three pixels of dark slate outside it. */
static void EmBezel(int x, int y, int w, int h)
{
    CtrPokeTouchRect outer = {(int16_t)(x - 3), (int16_t)(y - 3), (int16_t)(w + 6), (int16_t)(h + 6)};
    CtrPokeTouchRect inner = UiInset(&outer, 1);

    UiRoundRect(&outer, 5, UI_RGB(24, 18, 28));
    UiRoundFill(inner.x, inner.y, inner.w, inner.h, 4, UI_RGB(52, 44, 60));
    UiRing(&inner, 4, 1, UI_RGB(96, 86, 104));
}

/* A switch: a 26x12 track, green and the knob on the right when on, grey with the knob on the left when off. */
static void EmSwitch(int x, int y, int on)
{
    UiRoundFill(x, y, 26, 12, 5, UI_RGB(70, 60, 66));
    UiRoundFill(x + 1, y + 1, 24, 10, 4, on ? UI_RGB(72, 196, 96) : UI_RGB(176, 172, 180));
    UiDisc(on ? x + 19 : x + 6, y + 6, 6, UI_RGB(70, 60, 66));
    UiDisc(on ? x + 19 : x + 6, y + 6, 5, UI_RGB(250, 250, 252));
}

/* A shortcut's key: its button's letter on a small blue pill, centred at the top of the tile (cx its centre). */
static void EmKeyTab(int cx, int y, const char *letter)
{
    UiRoundFill(cx - 10, y, 20, 11, 5, UI_RGB(30, 40, 96));
    EmGradFill(cx - 9, y + 1, 18, 9, 4, UI_RGB(96, 132, 236), UI_RGB(52, 80, 200));
    UiText(letter, cx - UiTextWidth(letter, 1) / 2, y + 1, EM_WHITE, UI_RGB(30, 40, 96), 1, cx + 9);
}

/* The soft well a shortcut's item sits in: the face's own colour, a little deeper, with a shade along its top. */
static void EmWell(int x, int y, int w, int h, const EmPal *p)
{
    uint16_t deep = UiMix(p->bottom, p->edge, 2);

    UiRoundFill(x, y, w, h, 4, deep);
    EmGradFill(x + 1, y + 2, w - 2, h - 3, 3, UiMix(deep, p->bottom, 5), UiMix(p->top, p->bottom, 4));
}

/* One of the column's icons (ember_skin.h), blended onto what is there; dim, it is toned grey. */
static void EmIcon(int icon, int x, int y, int on)
{
    for (int v = 0; v < EMBER_ICON; ++v)
        for (int u = 0; u < EMBER_ICON; ++u) {
            int a = kEmberIconA[icon][v * EMBER_ICON + u];

            if (a)
                EmBlend(x + u, y + v, Tone(kEmberIcon[icon][v * EMBER_ICON + u], on), a);
        }
}

/* The embers: a couple of dozen sparks rising slowly over the backdrop, drawn only on pixels that show it (so they
 * pass behind every piece drawn over it) and outside `exclude` (the GBA picture). Each spark remembers the pixels it
 * painted and puts the backdrop back on those still showing its colour. */
#define EM_SPARKS 22
#define EM_SPARKS_LOW 7                     /* LOW: these, moved every EM_LOW_EVERY-th frame by that many steps */
#define EM_LOW_EVERY 3
#define EM_SPARK_PX 9
static struct {
    int ready;
    int frame;
    uint32_t seed;
    struct {
        int32_t x, y, vy;                   /* 16.16 */
        uint16_t phase, step;
        uint8_t big, hue, n;
        int16_t px[EM_SPARK_PX], py[EM_SPARK_PX];
        uint16_t col[EM_SPARK_PX];
    } s[EM_SPARKS];
} sEm;

static uint32_t EmRand(void)
{
    sEm.seed = sEm.seed * 1664525u + 1013904223u;
    return sEm.seed >> 8;
}

static void EmSpawn(int i, int anywhere)
{
    sEm.s[i].x = (int32_t)(EmRand() % W) << 16;
    sEm.s[i].y = (int32_t)(anywhere ? EmRand() % H : H + 2 + EmRand() % 20) << 16;
    sEm.s[i].vy = (int32_t)(0x3000 + EmRand() % 0x7000);           /* 0.19 .. 0.62 px a step */
    sEm.s[i].phase = (uint16_t)EmRand();
    sEm.s[i].step = (uint16_t)(200 + EmRand() % 500);
    sEm.s[i].big = EmRand() % 4 == 0;
    sEm.s[i].hue = (uint8_t)(EmRand() % 3);
}

/* The sway: phase 0..65535 to about -3..3 pixels (16.16). */
static int32_t EmSway(uint16_t phase)
{
    static const int8_t k[16] = {0, 9, 17, 22, 24, 22, 17, 9, 0, -9, -17, -22, -24, -22, -17, -9};

    return (int32_t)k[phase >> 12] * 0x2000;
}

static void EmGrow(CtrDirtyRect *box, int x, int y)
{
    if (x < box->x0) box->x0 = x;
    if (y < box->y0) box->y0 = y;
    if (x + 1 > box->x1) box->x1 = x + 1;
    if (y + 1 > box->y1) box->y1 = y + 1;
}

static void EmPaint(int i, int x, int y, uint16_t c, CtrDirtyRect *box)
{
    if ((unsigned)x >= W || (unsigned)y >= H || sEm.s[i].n >= EM_SPARK_PX || TsGet(x, y) != EMBER_BG(x, y))
        return;
    Px(x, y, c);
    sEm.s[i].px[sEm.s[i].n] = (int16_t)x;
    sEm.s[i].py[sEm.s[i].n] = (int16_t)y;
    sEm.s[i].col[sEm.s[i].n++] = c;
    EmGrow(box, x, y);
}

/* One step of the embers on `canvas`; the rectangles that changed are added to `dirty`. With `on` false the sparks
 * are only taken away. With `low` only EM_SPARKS_LOW of them stay, and a step is made once in EM_LOW_EVERY frames
 * (as long as the others took, so they rise as fast): the lower screen is then touched a third as often. */
static void EmbersStep(uint16_t *canvas, int on, int low, const CtrPokeTouchRect *exclude, CtrDirtyList *dirty)
{
    static const uint8_t hue[3][3] = {{255, 200, 80}, {255, 150, 40}, {255, 96, 32}};

    sCanvas = canvas;
    if (!sEm.ready) {
        sEm.ready = 1;
        sEm.seed = 0x7e3a11u;
        for (int i = 0; i < EM_SPARKS; ++i)
            EmSpawn(i, 1);
    }
    int steps = 1, count = EM_SPARKS;

    if (on && low) {
        steps = EM_LOW_EVERY;
        count = EM_SPARKS_LOW;
        if (++sEm.frame % EM_LOW_EVERY != 0)
            return;
    }
    for (int i = 0; i < EM_SPARKS; ++i) {
        CtrDirtyRect box = {W, H, 0, 0};
        int x, y;

        for (int k = 0; k < sEm.s[i].n; ++k) {
            int px = sEm.s[i].px[k], py = sEm.s[i].py[k];

            if (TsGet(px, py) == sEm.s[i].col[k])
                Px(px, py, EMBER_BG(px, py));
            EmGrow(&box, px, py);
        }
        sEm.s[i].n = 0;
        if (on && i < count) {
            sEm.s[i].y -= sEm.s[i].vy * steps;
            sEm.s[i].phase = (uint16_t)(sEm.s[i].phase + sEm.s[i].step * steps);
            if (sEm.s[i].y < -(4 << 16))
                EmSpawn(i, 0);
            x = (int)((sEm.s[i].x + EmSway(sEm.s[i].phase)) >> 16);
            y = (int)(sEm.s[i].y >> 16);
            if (!(exclude && x >= exclude->x - 2 && x < exclude->x + exclude->w + 2 && y >= exclude->y - 2
                  && y < exclude->y + exclude->h + 2)) {
                /* Brighter low down, where the fire is: the core, and a halo round it. */
                int a = 120 + (y < 0 ? 0 : y > H ? 135 : y * 135 / H);
                const uint8_t *c = hue[sEm.s[i].hue];
                uint16_t core = UI_RGB(255, 236, 170), halo = UI_RGB(c[0], c[1], c[2]);
                int size = sEm.s[i].big ? 2 : 1;
                static const int8_t ring[4][2] = {{-1, 0}, {0, -1}, {1, 0}, {0, 1}};

                for (int v = 0; v < size; ++v)
                    for (int u = 0; u < size; ++u)
                        if ((unsigned)(x + u) < W && (unsigned)(y + v) < H)
                            EmPaint(i, x + u, y + v, UiMix(EMBER_BG(x + u, y + v), core, a * 8 / 256), &box);
                for (int k = 0; k < 4; ++k) {
                    int hx = x + ring[k][0] + (ring[k][0] > 0 ? size - 1 : 0);
                    int hy = y + ring[k][1] + (ring[k][1] > 0 ? size - 1 : 0);

                    if ((unsigned)hx < W && (unsigned)hy < H)
                        EmPaint(i, hx, hy, UiMix(EMBER_BG(hx, hy), halo, a * 4 / 256), &box);
                }
            }
        }
        if (box.x1 > box.x0)
            CtrDirty_Add(dirty, box.x0, box.y0, box.x1, box.y1);
    }
}
