/* The battle's healthboxes, drawn by the port (see include/3ds_battle_hud.h).
 *
 * The look ("Llama", proposal A): a cream body (FireRed's) with a glass band over its top and a slate outline, FireRed's
 * cap at one end in flame red with the Poke Ball mark (the foe's red when its species is caught, grey until then, none
 * for a trainer's Pokemon), a thin flame line under the name, a small red "Lv"/"Nv" before the level, the HP bar as a
 * slate capsule with its label in orange and a glossy green, yellow or red fill, and under the player's HP numbers its
 * EXP bar on a dark slate base. A status is a coloured pill with its own icon. Every pixel is drawn here, at the
 * screen's own resolution; text is the game's font. */
#include <math.h>
#include <string.h>

#include "3ds_battle_hud.h"
#include "3ds_lang.h"
#include "3ds_video.h"

/* The level mark before the number: FireRed's "Lv", the Spanish game's "Nv". */
#define LEVEL CTR_TR("Lv", "Nv", "Lv")

/* The game's fonts (text.c, recipes 0096 and 0120): 16x16 two-bit glyphs and 8x16 ones, and their widths. */
extern const uint8_t *CtrTwinFontData(void);
extern unsigned CtrTwinFontWidth(unsigned code);
extern const uint8_t *CtrTwinSmallFontData(void);
extern unsigned CtrTwinSmallFontWidth(unsigned code);

#define EOS 0xFF

typedef struct { uint8_t r, g, b; } Rgb;
typedef struct {
    uint32_t *px;
    int w, h;
} Canvas;

static const Rgb SLATE = {64, 72, 104}, OUTLINE = {44, 50, 84}, FLAME_RING = {138, 22, 16}, INK = {72, 72, 72},
                 INK_SHADOW = {216, 216, 196};

static void Put(Canvas *c, int x, int y, Rgb v)
{
    if (x >= 0 && y >= 0 && x < c->w && y < c->h)
        c->px[y * CTR_HUD_IMAGE_W + x] = (uint32_t)v.r << 24 | (uint32_t)v.g << 16 | (uint32_t)v.b << 8 | 255u;
}

static Rgb Add(Rgb v, int r, int g, int b)
{
    Rgb out = {(uint8_t)(v.r + r > 255 ? 255 : v.r + r), (uint8_t)(v.g + g > 255 ? 255 : v.g + g),
               (uint8_t)(v.b + b > 255 ? 255 : v.b + b)};
    return out;
}

static Rgb Scale(Rgb v, float k)
{
    Rgb out = {(uint8_t)(v.r * k), (uint8_t)(v.g * k), (uint8_t)(v.b * k)};
    return out;
}

/* ---- Shapes: a mask per box, its rounded and slanted ends ------------------------------------------------------- */

#define MAX_W CTR_HUD_IMAGE_W
#define MAX_H CTR_HUD_IMAGE_H
typedef struct {
    int w, h;
    uint8_t m[MAX_H][MAX_W];
} Mask;

/* How far a rounded end of radius r comes in on row y of a shape h rows tall. */
static int CapInset(int h, int r, int y)
{
    int d = y < h - 1 - y ? y : h - 1 - y;
    if (d >= r) return 0;
    float dy = r - d - 0.5f, s = r * r - dy * dy;
    return (int)ceilf(r - sqrtf(s > 0 ? s : 0) - 0.25f);
}

enum { END_ROUND, END_SLANT };

/* A plate w x h: each end rounded (radius r) or slanted (one pixel in every `slant` rows). */
static void Shape(Mask *m, int w, int h, int left, int right, int r, int slant)
{
    memset(m, 0, sizeof(*m));
    m->w = w;
    m->h = h;
    for (int y = 0; y < h; ++y)
    {
        int x0 = left == END_ROUND ? CapInset(h, r, y) : (h - 1 - y) / slant;
        int x1 = w - (right == END_ROUND ? CapInset(h, r, y) : y / slant);
        for (int x = x0; x < x1; ++x) m->m[y][x] = 1;
    }
}

/* The mask a pixel in: its four neighbours inside too (the edge rows and columns of the plate's own box are only
 * checked against the neighbours it has). */
static void Shrink(const Mask *in, Mask *out)
{
    *out = *in;
    for (int y = 0; y < in->h; ++y)
        for (int x = 0; x < in->w; ++x)
            if (in->m[y][x])
                out->m[y][x] = (y < 1 || in->m[y - 1][x]) && (y + 1 >= in->h || in->m[y + 1][x])
                            && (x < 1 || in->m[y][x - 1]) && (x + 1 >= in->w || in->m[y][x + 1]);
}

/* Is (x, y) on the mask's edge facing (dx, dy)? The far side wraps round, as the design was drawn. */
static bool Edge(const Mask *m, int x, int y, int dx, int dy)
{
    int nx = ((x - dx) % m->w + m->w) % m->w, ny = ((y - dy) % m->h + m->h) % m->h;
    return m->m[y][x] && !m->m[ny][nx];
}

/* A vertical gradient through colour stops (row, colour); rows outside them are black. */
typedef struct { int y; Rgb c; } Stop;
static Rgb Gradient(const Stop *stops, int count, int y)
{
    Rgb out = {0, 0, 0};
    for (int i = 0; i + 1 < count; ++i)
    {
        int y0 = stops[i].y, y1 = stops[i + 1].y;
        if (y < y0 || y > y1) continue;
        double t = (double)(y - y0) / (double)(y1 - y0 > 1 ? y1 - y0 : 1);
        if (t < 0) t = 0;
        if (t > 1) t = 1;
        out.r = (uint8_t)(stops[i].c.r * (1 - t) + stops[i + 1].c.r * t);
        out.g = (uint8_t)(stops[i].c.g * (1 - t) + stops[i + 1].c.g * t);
        out.b = (uint8_t)(stops[i].c.b * (1 - t) + stops[i + 1].c.b * t);
    }
    return out;
}

/* ---- Text --------------------------------------------------------------------------------------------------------- */

static const uint8_t *Glyph(bool small, unsigned code, unsigned *width)
{
    const uint8_t *font = small ? CtrTwinSmallFontData() : CtrTwinFontData();
    unsigned w = small ? CtrTwinSmallFontWidth(code) : CtrTwinFontWidth(code);
    unsigned limit = small ? 8 : 16;

    *width = w > limit ? limit : w;
    return font ? font + code * (small ? 32 : 64) : NULL;
}

static unsigned GlyphValue(bool small, const uint8_t *src, unsigned x, unsigned y)
{
    unsigned tile = small ? y / 8 : (y / 8) * 2 + x / 8, row = y % 8;
    unsigned bits = (unsigned)src[tile * 16 + row * 2 + 1] << 8 | src[tile * 16 + row * 2];
    return (bits >> ((7 - x % 8) * 2)) & 3;
}

/* The ASCII the boxes print themselves, in the game's charset ('*' for its multiplication sign). */
static uint8_t Code(char c)
{
    if (c >= '0' && c <= '9') return (uint8_t)(0xA1 + (c - '0'));
    if (c >= 'A' && c <= 'Z') return (uint8_t)(0xBB + (c - 'A'));
    if (c >= 'a' && c <= 'z') return (uint8_t)(0xD5 + (c - 'a'));
    if (c == '/') return 0xBA;
    if (c == '*') return 0xB9;                    /* the multiplication sign */
    return 0x00;
}

static unsigned TextWidth(bool small, const uint8_t *codes)
{
    unsigned total = 0, w;
    for (; *codes != EOS; ++codes)
    {
        Glyph(small, *codes, &w);
        total += w;
    }
    return total;
}

static void Encode(const char *ascii, uint8_t *out, unsigned size)
{
    unsigned i = 0;
    for (; ascii[i] && i + 1 < size; ++i) out[i] = Code(ascii[i]);
    out[i] = EOS;
}

/* The design's gender marks, drawn rather than taken from the font: 8 rows standing on the baseline, a shadow a
 * pixel down and right. */
static const char *const MALE_MARK[8] = {"....###", ".....##", "....#.#", ".###...", "#...#..", "#...#..", "#...#..",
                                         ".###..."};
static const char *const FEMALE_MARK[8] = {".###.", "#...#", "#...#", "#...#", ".###.", "..#..", ".###.", "..#.."};

static int Mark(Canvas *c, int x, int base, unsigned gender)
{
    const char *const *mark = gender == CTR_HUD_MALE ? MALE_MARK : FEMALE_MARK;
    Rgb ink = gender == CTR_HUD_MALE ? (Rgb){48, 104, 216} : (Rgb){232, 72, 128};
    Rgb shade = gender == CTR_HUD_MALE ? (Rgb){192, 208, 240} : (Rgb){248, 200, 216};
    int top = base - 7, w = (int)strlen(mark[0]);

    for (int j = 0; j < 8; ++j)
        for (int k = 0; k < w; ++k)
            if (mark[j][k] == '#') Put(c, x + k + 2, top + j + 1, shade);
    for (int j = 0; j < 8; ++j)
        for (int k = 0; k < w; ++k)
            if (mark[j][k] == '#') Put(c, x + 1 + k, top + j, ink);
    return x + w + 2;
}

/* Text whose ink stands on row `base` (a glyph's row 10); `shadow` NULL leaves the shadow pixels out. */
static int Text(Canvas *c, bool small, int x, int base, const uint8_t *codes, Rgb ink, const Rgb *shadow)
{
    for (; *codes != EOS; ++codes)
    {
        unsigned w;
        const uint8_t *g = Glyph(small, *codes, &w);
        if (!g) return x;
        for (unsigned j = 0; j < 16; ++j)
            for (unsigned i = 0; i < w; ++i)
            {
                unsigned v = GlyphValue(small, g, i, j);
                if (v == 1) Put(c, x + (int)i, base - 10 + (int)j, ink);
                else if (v == 2 && shadow) Put(c, x + (int)i, base - 10 + (int)j, *shadow);
            }
        x += (int)w;
    }
    return x;
}

static int Ascii(Canvas *c, bool small, int x, int base, const char *ascii, Rgb ink, const Rgb *shadow)
{
    uint8_t codes[16];
    Encode(ascii, codes, sizeof(codes));
    return Text(c, small, x, base, codes, ink, shadow);
}

static unsigned AsciiWidth(bool small, const char *ascii)
{
    uint8_t codes[16];
    Encode(ascii, codes, sizeof(codes));
    return TextWidth(small, codes);
}

/* ---- The plate ---------------------------------------------------------------------------------------------------- */

static void Shadow(Canvas *c, int x, int y, const Mask *m)
{
    for (int j = 0; j < m->h; ++j)
        for (int i = 0; i < m->w; ++i)
            if (m->m[j][i])
            {
                int px = x + 1 + i, py = y + 2 + j;
                if (px < c->w && py < c->h)
                    c->px[py * CTR_HUD_IMAGE_W + px] = (12u << 24) | (16u << 16) | (28u << 8) | 107u;   /* 42% */
            }
}

/* The mask a pixel in on every side (the plate's own edge counts as outside, so the outline closes all round). */
static void Inset(const Mask *in, Mask *out)
{
    *out = *in;
    for (int y = 0; y < in->h; ++y)
        for (int x = 0; x < in->w; ++x)
            if (in->m[y][x])
                out->m[y][x] = y > 0 && in->m[y - 1][x] && y + 1 < in->h && in->m[y + 1][x]
                            && x > 0 && in->m[y][x - 1] && x + 1 < in->w && in->m[y][x + 1];
}

enum { CAP_LEFT, CAP_RIGHT };

/*
 * A plate w x h: a slate outline, a cream body (a glass band over the top two fifths, a step down to the cream
 * below), FireRed's cap at one end in flame red with a dark ring and a lit top, and from row `strip` down (0: none) a
 * dark slate base under the body, the player's EXP bar housing. `slant` is the slanted end's one pixel in so many rows.
 */
static void Plate(Canvas *c, int x, int y, int w, int h, int left, int right, int capSide, int capW, int slant,
                  int strip)
{
    static Mask m, in, cap, capIn;
    const int glass = h * 2 / 5;
    const Stop top[2] = {{1, {255, 255, 255}}, {glass, {255, 255, 240}}};
    const Stop low[2] = {{glass + 1, {246, 246, 220}}, {h - 2, {230, 230, 196}}};
    const Stop base[2] = {{strip, {76, 84, 124}}, {h - 2, {52, 60, 98}}};
    const Stop flame[4] = {{1, {255, 196, 90}}, {glass, {255, 154, 56}}, {glass + 1, {248, 100, 42}}, {h - 2, {216, 48, 30}}};

    Shape(&m, w, h, left, right, 2, slant);
    Shadow(c, x, y, &m);
    Inset(&m, &in);
    cap = m;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            if (!m.m[j][i]) continue;
            Put(c, x + i, y + j, SLATE);
            if (capSide == CAP_LEFT ? i >= capW : i < w - capW) cap.m[j][i] = 0;
            if (!in.m[j][i]) continue;
            Put(c, x + i, y + j, j <= glass ? Gradient(top, 2, j) : Gradient(low, 2, j));
            if (strip && j >= strip) Put(c, x + i, y + j, Gradient(base, 2, j));
        }
    Inset(&cap, &capIn);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (cap.m[j][i])
                Put(c, x + i, y + j, !capIn.m[j][i] ? FLAME_RING : Gradient(flame, 4, j));
    for (int j = 1; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (capIn.m[j][i] && !capIn.m[j - 1][i]) Put(c, x + i, y + j, (Rgb){255, 233, 168});
}

/* The thin flame line under a name: a horizontal run from deep orange to amber. */
static void FlameLine(Canvas *c, int x0, int x1, int y, int rows)
{
    for (int i = x0; i < x1; ++i)
    {
        float t = (float)(i - x0) / (float)(x1 - x0 > 1 ? x1 - x0 - 1 : 1);
        Rgb v = {(uint8_t)(248 + 7 * t), (uint8_t)(100 + 76 * t), (uint8_t)(42 + 30 * t)};
        for (int j = 0; j < rows; ++j) Put(c, i, y + j, j ? v : Add(v, 0, 18, 10));
    }
}

static const char *const BALL[11] = {"...####...", "..#rrrr#..", ".#rRrrrr#.", "#rRrrrrrr#", "#rrr##rrr#",
                                     "####WW####", "#www##www#", "#wwwwwwww#", ".#wwwwww#.", "..#wwww#..",
                                     "...####..."};

static void Ball(Canvas *c, int x, int y, bool caught)
{
    for (int j = 0; j < 11; ++j)
        for (int i = 0; i < 10; ++i)
        {
            Rgb v;
            switch (BALL[j][i])
            {
            case '#': v = OUTLINE; break;
            case 'r': v = caught ? (Rgb){232, 56, 44} : (Rgb){170, 174, 186}; break;
            case 'R': v = caught ? (Rgb){255, 170, 150} : (Rgb){230, 232, 238}; break;
            case 'w': v = (Rgb){250, 250, 252}; break;
            case 'W': v = (Rgb){255, 255, 255}; break;
            default: continue;
            }
            Put(c, x + i, y + j, v);
        }
}

static void Level(Canvas *c, int right, int base, unsigned level)
{
    static const Rgb red = {224, 56, 40}, redShadow = {248, 200, 184};
    char digits[4];
    unsigned n = 0;
    uint8_t codes[4];

    do digits[n++] = (char)('0' + level % 10); while ((level /= 10) && n < 3);
    for (unsigned i = 0; i < n; ++i) codes[i] = Code(digits[n - 1 - i]);
    codes[n] = EOS;
    int w = (int)TextWidth(false, codes);
    Text(c, false, right - w, base, codes, INK, &INK_SHADOW);
    Ascii(c, true, right - w - (int)AsciiWidth(true, LEVEL) - 1, base, LEVEL, red, &redShadow);
}

/* A pill (round ends, h rows) of one colour, its top row a shade darker; `glow` lays the white lip under it. */
static void Pill(Canvas *c, int x, int y, int w, int h, Rgb v, Rgb topRow, bool glow)
{
    static Mask m;
    Shape(&m, w, h, END_ROUND, END_ROUND, h / 2, 1);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (m.m[j][i]) Put(c, x + i, y + j, j == 0 ? topRow : v);
    if (glow)
        for (int i = 0; i < w; ++i)
        {
            int j = h - 1;
            while (j >= 0 && !m.m[j][i]) --j;
            if (j >= 0) Put(c, x + i, y + j + 1, (Rgb){255, 255, 248});
        }
}

/* The HP capsule, 10 pixels: slate, "HP" in orange, a recessed track and a glossy fill coloured as FireRed's (over
 * half green, over a fifth yellow, else red), the track and the fill rounded at both ends. */
static void HpBar(Canvas *c, int x, int y, int w, unsigned pixels)
{
    static const Rgb green[5] = {{200, 255, 216}, {152, 246, 176}, {48, 200, 72}, {40, 176, 64}, {28, 132, 52}};
    static const Rgb yellow[5] = {{255, 250, 190}, {255, 232, 112}, {248, 200, 40}, {228, 176, 28}, {176, 124, 16}};
    static const Rgb red[5] = {{255, 204, 194}, {255, 140, 120}, {232, 64, 48}, {208, 48, 40}, {156, 28, 28}};
    static const uint8_t rows[6] = {0, 1, 2, 2, 3, 4};
    static Mask track, fillMask;
    const Rgb *fill = pixels > CTR_HUD_HP_PIXELS * 50 / 100 ? green : pixels > CTR_HUD_HP_PIXELS * 20 / 100 ? yellow : red;
    int tx = x + 16, tw = w - 19, ty = y + 2;

    Pill(c, x, y, w, 10, SLATE, (Rgb){52, 58, 90}, true);
    Ascii(c, true, x + 4, y + 7, CTR_TR("HP", "PS", "KP"), (Rgb){248, 176, 32}, &(Rgb){44, 50, 80});
    Shape(&track, tw, 6, END_ROUND, END_ROUND, 3, 1);
    for (int j = 0; j < 6; ++j)
        for (int i = 0; i < tw; ++i)
            if (track.m[j][i]) Put(c, tx + i, ty + j, j == 0 ? (Rgb){72, 80, 112} : (Rgb){88, 96, 128});
    if (pixels > CTR_HUD_HP_PIXELS) pixels = CTR_HUD_HP_PIXELS;
    int fw = (int)lrintf((float)tw * (float)pixels / CTR_HUD_HP_PIXELS);
    if (pixels && !fw) fw = 1;
    Shape(&fillMask, fw, 6, END_ROUND, END_ROUND, fw >= 6 ? 3 : 0, 1);
    for (int j = 0; j < 6; ++j)
        for (int i = 0; i < fw; ++i)
            if (fillMask.m[j][i]) Put(c, tx + i, ty + j, fill[rows[j]]);
}

/* The EXP bar on the player's slate base (9 rows from y): "EXP" in light blue, a dark 3-pixel track and a glossy blue
 * fill. */
static void ExpBar(Canvas *c, int x, int y, int w, unsigned pixels)
{
    static const Rgb blue[3] = {{168, 224, 255}, {64, 152, 240}, {52, 124, 212}};
    static Mask track, fillMask;
    int tx = x + (int)AsciiWidth(true, "EXP") + 3, tw = x + w - tx, ty = y + 3;

    Ascii(c, true, x, y + 7, "EXP", (Rgb){112, 208, 248}, &(Rgb){28, 44, 92});
    Shape(&track, tw, 3, END_ROUND, END_ROUND, 2, 1);
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < tw; ++i)
            if (track.m[j][i]) Put(c, tx + i, ty + j, j == 0 ? (Rgb){24, 28, 48} : (Rgb){32, 38, 63});
    if (pixels > CTR_HUD_EXP_PIXELS) pixels = CTR_HUD_EXP_PIXELS;
    int fw = (int)lrintf((float)tw * (float)pixels / CTR_HUD_EXP_PIXELS);
    if (!fw) return;
    Shape(&fillMask, fw, 3, END_ROUND, END_ROUND, fw >= 4 ? 2 : 0, 1);
    for (int j = 0; j < 3; ++j)
        for (int i = 0; i < fw; ++i)
            if (fillMask.m[j][i]) Put(c, tx + i, ty + j, blue[j]);
}

/* A status as a pill: light top, the status colour, a darker base, an outline in its own dark, a white icon. */
static const struct {
    const char *label;
    Rgb light, base, dark, outline;
    const char *icon[9];
} STATUS[] = {
    [CTR_HUD_PSN] = {"PSN", {236, 170, 240}, {184, 88, 200}, {132, 48, 152}, {80, 24, 96},
                     {"..#####..", ".#######.", "#########", "##..#..##", "##..#..##", "####.####", ".#######.",
                      "..#.#.#..", "..#####.."}},
    [CTR_HUD_PAR] = {"PAR", {255, 240, 150}, {240, 196, 40}, {196, 148, 16}, {120, 84, 8},
                     {".....###.", "....###..", "...###...", "..######.", ".######..", "....##...", "...##....",
                      "..##.....", ".#......."}},
    [CTR_HUD_SLP] = {"SLP", {214, 216, 228}, {150, 154, 174}, {108, 112, 134}, {60, 62, 80},
                     {"#####....", "...#.....", "..#......", ".#.......", "#####....", "....####.", ".....#...",
                      "....#....", "....####."}},
    [CTR_HUD_FRZ] = {"FRZ", {200, 240, 255}, {104, 196, 244}, {52, 140, 200}, {24, 80, 128},
                     {"....#....", ".#..#..#.", "..#.#.#..", "...###...", "#########", "...###...", "..#.#.#..",
                      ".#..#..#.", "....#...."}},
    [CTR_HUD_BRN] = {"BRN", {255, 186, 140}, {244, 112, 56}, {196, 64, 28}, {120, 32, 16},
                     {"....#....", "...##....", "...###...", "..####.#.", "..#####..", ".###.###.", ".##...##.",
                      ".##...##.", "..#####.."}},
};

static int Status(Canvas *c, int x, int y, unsigned s)
{
    static Mask m, in;
    int w = 4 + 9 + 3 + (int)AsciiWidth(false, STATUS[s].label) + 4, h = 13;
    Stop stops[3] = {{1, STATUS[s].light}, {5, STATUS[s].base}, {h - 2, STATUS[s].dark}};

    Shape(&m, w, h, END_ROUND, END_ROUND, 5, 1);
    Shrink(&m, &in);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            if (m.m[j][i]) Put(c, x + i, y + j, STATUS[s].outline);
            if (in.m[j][i]) Put(c, x + i, y + j, Gradient(stops, 3, j));
        }
    Rgb lightEdge = Add(STATUS[s].light, 24, 24, 24), darkEdge = Scale(STATUS[s].dark, 0.8f);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (Edge(&in, i, j, 0, 1)) Put(c, x + i, y + j, lightEdge);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (Edge(&in, i, j, 0, -1)) Put(c, x + i, y + j, darkEdge);
    for (int j = 0; j < 9; ++j)
        for (int k = 0; k < 9; ++k)
            if (STATUS[s].icon[j][k] == '#')
            {
                if (j == 8 || STATUS[s].icon[j + 1][k] != '#') Put(c, x + 4 + k + 1, y + 2 + j + 1, STATUS[s].dark);
                Put(c, x + 4 + k, y + 2 + j, (Rgb){255, 255, 255});
            }
    Ascii(c, false, x + 16, y + 10, STATUS[s].label, (Rgb){255, 255, 255}, &STATUS[s].dark);
    return w;
}

/* ---- The boxes -------------------------------------------------------------------------------------------------- */

static const char *const STAR[7] = {"...#...", "...#...", "..###..", "#######", ".#####.", "..#.#..", ".#...#."};

/* A shiny Pokemon's gold star after its name. */
static void Star(Canvas *c, int x, int y)
{
    for (int j = 0; j < 7; ++j)
        for (int k = 0; k < 7; ++k)
            if (STAR[j][k] == '#') Put(c, x + k + 1, y + j + 1, (Rgb){150, 96, 16});
    for (int j = 0; j < 7; ++j)
        for (int k = 0; k < 7; ++k)
            if (STAR[j][k] == '#') Put(c, x + k, y + j, j < 4 ? (Rgb){255, 214, 64} : (Rgb){240, 168, 32});
}

static int Name(Canvas *c, int x, int base, const CtrHudState *s)
{
    x = Text(c, false, x, base, s->name, INK, &INK_SHADOW);
    if (s->gender != CTR_HUD_NO_GENDER) x = Mark(c, x, base, s->gender);
    if (s->shiny) Star(c, x + 2, base - 7);
    return x;
}

static bool HasStatus(const CtrHudState *s)
{
    return s->status >= CTR_HUD_PSN && s->status <= CTR_HUD_BRN;
}

/* "hp/max" in the game's charset. */
static void Numbers(const CtrHudState *s, char *out)
{
    char tmp[6];
    unsigned n = 0, t = 0, v = s->hp;

    do tmp[t++] = (char)('0' + v % 10); while ((v /= 10) && t < 5);
    while (t) out[n++] = tmp[--t];
    out[n++] = '/';
    v = s->maxHp;
    do tmp[t++] = (char)('0' + v % 10); while ((v /= 10) && t < 5);
    while (t) out[n++] = tmp[--t];
    out[n] = 0;
}

#define FOE_W 152
#define FOE_H 36
/* The player's boxes are kept clear of the foe's ground: narrower and lower than the design's first 164x56. */
#define PLAYER_W 136
#define PLAYER_H 49
#define LOW_H 36                           /* the Safari Zone's box */
/* The player's boxes in a double battle: one row for the name and one for the bar, so both fit under the foe's ground. */
#define DOUBLE_H 26
#define PLAYER_CAP 8                       /* the player's cap is a plain flame end: no Poke Ball */

/* The cap's Poke Ball: always there on the foe's side, in a wild battle red once the species is caught. */
static void CapBall(Canvas *c, int x, int h, bool caught)
{
    Ball(c, x, (h - 11) / 2, caught);
}

static void FoeBox(Canvas *c, const CtrHudState *s)
{
    const int w = FOE_W, h = FOE_H, cap = 17;
    Plate(c, 0, 0, w, h, END_ROUND, END_SLANT, CAP_LEFT, cap, 2, 0);
    if (!s->noBall)
        CapBall(c, 4, h, s->caught);
    int nx = cap + 5;
    FlameLine(c, nx, w - 20, 17, 2);
    Name(c, nx, 12, s);
    Level(c, w - 20, 12, s->level);
    int hx = nx;
    if (HasStatus(s)) hx += Status(c, hx, 19, s->status) + 3;
    HpBar(c, hx, 21, w - 20 - hx, s->hpPixels);
}

static void PlayerBox(Canvas *c, const CtrHudState *s)
{
    const int w = PLAYER_W, h = PLAYER_H, cap = PLAYER_CAP, strip = 39;
    char numbers[12];
    int end = w - cap - 4;

    Plate(c, 0, 0, w, h, END_SLANT, END_ROUND, CAP_RIGHT, cap, 3, strip);
    int nx = 20;
    FlameLine(c, nx, end, 14, 2);
    Name(c, nx, 10, s);
    Level(c, end - 2, 10, s->level);
    int hx = nx - 4;
    if (HasStatus(s)) hx += Status(c, hx, 16, s->status) + 3;
    HpBar(c, hx, 17, end - hx, s->hpPixels);
    /* "hp/max", right-aligned to the bar's end, under it. */
    Numbers(s, numbers);
    Ascii(c, false, end - 3 - (int)AsciiWidth(false, numbers) + 1, 37, numbers, INK, &INK_SHADOW);
    ExpBar(c, 6, strip, end - 6, s->expPixels);
}

/* A double battle's player box: the single one's plate, 26 high, its HP numbers beside the bar. */
static void PlayerDoubleBox(Canvas *c, const CtrHudState *s)
{
    const int w = PLAYER_W, h = DOUBLE_H, cap = PLAYER_CAP;
    char numbers[12];
    int end = w - cap - 4;

    Plate(c, 0, 0, w, h, END_SLANT, END_ROUND, CAP_RIGHT, cap, 3, 0);
    FlameLine(c, 20, end, 13, 1);
    Name(c, 20, 10, s);
    Level(c, end - 2, 10, s->level);
    Numbers(s, numbers);
    int nw = (int)AsciiWidth(false, numbers);
    Ascii(c, false, end - nw, 23, numbers, INK, &INK_SHADOW);
    int hx = 16;
    if (HasStatus(s)) hx += Status(c, hx, 12, s->status) + 3;
    HpBar(c, hx, 14, end - nw - 4 - hx, s->hpPixels);
}

/* The Safari Zone's box: SAFARI BALLS on the name band, a Poke Ball and how many are left below. */
static void SafariBox(Canvas *c, const CtrHudState *s)
{
    const int w = PLAYER_W, h = LOW_H, cap = PLAYER_CAP;
    char count[8];
    unsigned n = 0, t = 0, v = s->balls;
    char tmp[4];

    Plate(c, 0, 0, w, h, END_SLANT, END_ROUND, CAP_RIGHT, cap, 3, 0);
    FlameLine(c, 20, w - cap - 4, 17, 2);
    Ascii(c, false, 20, 12, "SAFARI BALLS", INK, &INK_SHADOW);
    Ball(c, 22, 21, true);
    count[n++] = '*';
    do tmp[t++] = (char)('0' + v % 10); while ((v /= 10) && t < 3);
    while (t) count[n++] = tmp[--t];
    count[n] = 0;
    Ascii(c, false, 36, 30, count, INK, &INK_SHADOW);
}

void CtrHud_Render(const CtrHudState *s, uint32_t *pixels, unsigned *width, unsigned *height)
{
    int w = s->kind == CTR_HUD_FOE ? FOE_W : PLAYER_W, h = s->kind == CTR_HUD_FOE ? FOE_H
                                                       : s->kind == CTR_HUD_PLAYER ? PLAYER_H
                                                       : s->kind == CTR_HUD_PLAYER_DOUBLE ? DOUBLE_H : LOW_H;
    Canvas c = {pixels, w + 1, h + 2};

    for (int y = 0; y < c.h; ++y) memset(pixels + y * CTR_HUD_IMAGE_W, 0, (size_t)c.w * 4);
    switch (s->kind)
    {
    case CTR_HUD_FOE: FoeBox(&c, s); break;
    case CTR_HUD_PLAYER: PlayerBox(&c, s); break;
    case CTR_HUD_PLAYER_DOUBLE: PlayerDoubleBox(&c, s); break;
    case CTR_HUD_SAFARI: SafariBox(&c, s); break;
    default: break;
    }
    *width = (unsigned)c.w;
    *height = (unsigned)c.h;
}

/* ---- State ------------------------------------------------------------------------------------------------------- */

typedef struct {
    CtrHudState state;
    bool dirty;
    CtrHudImage image;
    uint32_t pixels[CTR_HUD_IMAGE_W * CTR_HUD_IMAGE_H];
} Box;

static Box sBoxes[CTR_HUD_BATTLERS];
static const uint16_t *sUnfaded;
static uint32_t sVersion;

/*
 * Where each box goes. FireRed's healthboxes rest with their main sprite's top left at these GBA points
 * (InitBattlerHealthboxCoords: the centre less half the sprite); the design puts the box's top left at these screen
 * points with the battle at CTR_BATTLE_ZOOM. The box follows its sprite from there: a slide-in or a bob moves both.
 * The player's single and Safari boxes are made 64x32 and turned 64x64 afterwards, keeping the 64x32 centre: their
 * top left is 16 rows lower than a 64x64 sprite's would be (the centre's y less 16, not less 32).
 */
static const struct { uint8_t kind; bool doubles; uint8_t position; int16_t gbaX, gbaY, x, y; } ANCHORS[] = {
    {CTR_HUD_FOE, false, 1, 12, 14, 10, 22},
    {CTR_HUD_PLAYER, false, 0, 126, 72, 260, 141},
    {CTR_HUD_SAFARI, false, 0, 126, 72, 260, 154},
    {CTR_HUD_FOE, true, 1, 12, 3, 24, 6},
    {CTR_HUD_FOE, true, 3, 0, 28, 6, 44},
    {CTR_HUD_PLAYER_DOUBLE, true, 0, 127, 59, 252, 136},
    {CTR_HUD_PLAYER_DOUBLE, true, 2, 139, 84, 260, 164},
};

static void Anchor(const CtrHudState *s, float *dx, float *dy)
{
    const float offX = 200.0f - 120.0f * CTR_BATTLE_ZOOM, offY = 192.0f - 112.0f * CTR_BATTLE_ZOOM;
    *dx = *dy = 0.0f;
    for (unsigned i = 0; i < sizeof(ANCHORS) / sizeof(ANCHORS[0]); ++i)
        if (ANCHORS[i].kind == s->kind && ANCHORS[i].doubles == s->doubles
         && (ANCHORS[i].position == s->position || !s->doubles))
        {
            *dx = ANCHORS[i].x - (ANCHORS[i].gbaX * CTR_BATTLE_ZOOM + offX);
            *dy = ANCHORS[i].y - (ANCHORS[i].gbaY * CTR_BATTLE_ZOOM + offY);
            return;
        }
}

static Box *BoxOf(unsigned battler)
{
    return battler < CTR_HUD_BATTLERS && sBoxes[battler].state.kind != CTR_HUD_OFF ? &sBoxes[battler] : NULL;
}

void CtrHud_Box(unsigned battler, unsigned kind, unsigned position, bool doubles, unsigned boxTile, unsigned barTile,
                const uint16_t *unfadedPalette)
{
    if (battler >= CTR_HUD_BATTLERS) return;
    /* A single battle has no right-hand battlers: what a double battle left there goes. */
    if (!doubles)
        for (unsigned i = 0; i < CTR_HUD_BATTLERS; ++i)
            if (sBoxes[i].state.doubles) sBoxes[i].state.kind = CTR_HUD_OFF;
    Box *b = &sBoxes[battler];
    memset(&b->state, 0, sizeof(b->state));
    b->state.kind = (uint8_t)(kind <= CTR_HUD_SAFARI ? kind : CTR_HUD_OFF);
    b->state.position = (uint8_t)position;
    b->state.doubles = doubles;
    b->state.boxTile = (uint16_t)boxTile;
    b->state.barTile = (uint16_t)barTile;
    b->state.gender = CTR_HUD_NO_GENDER;
    b->state.name[0] = EOS;
    b->dirty = true;
    sUnfaded = unfadedPalette;
    if (b->state.kind != CTR_HUD_OFF) CtrVideo_SetBattleHud(CtrHud_Match, CtrHud_Image);
}

#define SET(battler, field, value) do { Box *b_ = BoxOf(battler); \
        if (b_ && b_->state.field != (value)) { b_->state.field = (value); b_->dirty = true; } } while (0)

void CtrHud_Name(unsigned battler, const uint8_t *name, unsigned gender, bool shiny)
{
    Box *b = BoxOf(battler);
    uint8_t copy[CTR_HUD_NAME_LEN];
    unsigned i = 0;

    if (!b) return;
    for (; name && name[i] != EOS && i + 1 < CTR_HUD_NAME_LEN; ++i) copy[i] = name[i];
    copy[i] = EOS;
    if (memcmp(copy, b->state.name, i + 1) != 0)
    {
        memcpy(b->state.name, copy, i + 1);
        b->dirty = true;
    }
    SET(battler, gender, (uint8_t)(gender <= CTR_HUD_NO_GENDER ? gender : CTR_HUD_NO_GENDER));
    SET(battler, shiny, shiny);
}

void CtrHud_Level(unsigned battler, unsigned level) { SET(battler, level, (uint8_t)level); }

void CtrHud_Hp(unsigned battler, int value, bool max)
{
    uint16_t v = (uint16_t)(value < 0 ? 0 : value > 999 ? 999 : value);
    if (max) SET(battler, maxHp, v);
    else SET(battler, hp, v);
}

void CtrHud_HpBar(unsigned battler, unsigned pixels) { SET(battler, hpPixels, (uint8_t)pixels); }
void CtrHud_ExpBar(unsigned battler, unsigned pixels) { SET(battler, expPixels, (uint8_t)pixels); }
void CtrHud_Status(unsigned battler, unsigned status) { SET(battler, status, (uint8_t)status); }
void CtrHud_Caught(unsigned battler, bool caught) { SET(battler, caught, caught); }
void CtrHud_NoBall(unsigned battler, bool none) { SET(battler, noBall, none); }
void CtrHud_SafariBalls(unsigned battler, unsigned count) { SET(battler, balls, (uint8_t)(count > 99 ? 99 : count)); }

unsigned CtrHud_Match(unsigned tile, unsigned *battler)
{
    for (unsigned i = 0; i < CTR_HUD_BATTLERS; ++i)
    {
        const CtrHudState *s = &sBoxes[i].state;
        /* The box's two halves: 64x64 each for the player's in a single battle and the Safari Zone's, else 64x32. */
        unsigned boxTiles = s->kind == CTR_HUD_PLAYER || s->kind == CTR_HUD_SAFARI ? 128 : 64;
        if (s->kind == CTR_HUD_OFF) continue;
        *battler = i;
        if (tile == s->boxTile) return CTR_HUD_MAIN;
        if ((tile > s->boxTile && tile < s->boxTile + boxTiles) || (tile >= s->barTile && tile < s->barTile + 9u))
            return CTR_HUD_PART;
    }
    return CTR_HUD_NONE;
}

const CtrHudImage *CtrHud_Image(unsigned battler)
{
    Box *b = BoxOf(battler);
    if (!b) return NULL;
    if (b->dirty)
    {
        unsigned w, h;
        CtrHud_Render(&b->state, b->pixels, &w, &h);
        b->image.version = ++sVersion;
        b->image.width = (uint16_t)w;
        b->image.height = (uint16_t)h;
        Anchor(&b->state, &b->image.dx, &b->image.dy);
        b->image.pixels = b->pixels;
        b->dirty = false;
    }
    b->image.unfaded = sUnfaded;
    return &b->image;
}
