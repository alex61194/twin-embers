/* The battle's healthboxes, drawn by the port (see include/3ds_battle_hud.h).
 *
 * The look: a light enamel plate with a dark outline, a white name band over a cool grey tray, FireRed's red cap at
 * one end (the foe's holds a Poke Ball, red when its species is caught), the HP bar as a dark capsule with "HP" in
 * gold and a five-tone fill, the player's HP numbers over its bar and a slim EXP capsule under it, a status as a
 * coloured pill with its own icon. Every pixel is drawn here, at the screen's own resolution; text is the game's
 * font. */
#include <math.h>
#include <string.h>

#include "3ds_battle_hud.h"
#include "3ds_video.h"

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

static const Rgb OUTLINE = {28, 30, 44}, INK = {44, 46, 62}, INK_SHADOW = {200, 206, 220};

static void Put(Canvas *c, int x, int y, Rgb v)
{
    if (x >= 0 && y >= 0 && x < c->w && y < c->h)
        c->px[y * CTR_HUD_IMAGE_W + x] = (uint32_t)v.r << 24 | (uint32_t)v.g << 16 | (uint32_t)v.b << 8 | 255u;
}

static Rgb Get(const Canvas *c, int x, int y)
{
    uint32_t v = x >= 0 && y >= 0 && x < c->w && y < c->h ? c->px[y * CTR_HUD_IMAGE_W + x] : 0;
    Rgb out = {(uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8)};
    return out;
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
    Rgb ink = gender == CTR_HUD_MALE ? (Rgb){56, 136, 248} : (Rgb){240, 88, 128};
    int top = base - 7, w = (int)strlen(mark[0]);

    for (int j = 0; j < 8; ++j)
        for (int k = 0; k < w; ++k)
            if (mark[j][k] == '#') Put(c, x + k + 2, top + j + 1, INK_SHADOW);
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

enum { CAP_LEFT, CAP_RIGHT };

static void Plate(Canvas *c, int x, int y, int w, int h, int left, int right, int split, int capSide, int capW,
                  int slant)
{
    static Mask m, i1, i2, cap;
    static const Stop up[] = {{0, {255, 255, 255}}, {0, {232, 236, 244}}};
    Stop top[2] = {up[0], {split, up[1].c}}, low[2] = {{split, {210, 218, 232}}, {h - 1, {184, 194, 212}}};
    Stop red[4] = {{0, {255, 120, 96}}, {2, {236, 72, 52}}, {h / 2, {214, 52, 40}}, {h - 1, {150, 28, 28}}};

    Shape(&m, w, h, left, right, 9, slant);
    Shadow(c, x, y, &m);
    Shrink(&m, &i1);
    Shrink(&i1, &i2);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
        {
            if (!m.m[j][i]) continue;
            Put(c, x + i, y + j, OUTLINE);
            if (i1.m[j][i]) Put(c, x + i, y + j, j < split ? Gradient(top, 2, j) : Gradient(low, 2, j));
        }
    /* Rim light on top and left, rim shade below and right. */
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (Edge(&i1, i, j, 0, 1)) Put(c, x + i, y + j, (Rgb){255, 255, 255});
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (Edge(&i1, i, j, 1, 0)) Put(c, x + i, y + j, (Rgb){246, 248, 252});
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (Edge(&i1, i, j, 0, -1)) Put(c, x + i, y + j, (Rgb){150, 160, 182});
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (Edge(&i1, i, j, -1, 0)) Put(c, x + i, y + j, (Rgb){170, 178, 198});
    /* A soft sheen across the name band, and the crisp divider under it. */
    for (int j = 1; j < split; ++j)
        for (int i = 0; i < w; ++i)
            if (i2.m[j][i] && (i + j * 2) % 120 < 18) Put(c, x + i, y + j, Add(Get(c, x + i, y + j), 6, 6, 6));
    for (int i = 0; i < w; ++i)
    {
        if (i2.m[split][i]) Put(c, x + i, y + split, (Rgb){176, 186, 206});
        if (i2.m[split + 1][i]) Put(c, x + i, y + split + 1, (Rgb){236, 242, 250});
    }
    /* FireRed's red cap at one end, behind a dark seam, with a highlight on top and a glossy streak. */
    cap = i1;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (capSide == CAP_LEFT ? i >= capW : i < w - capW) cap.m[j][i] = 0;
    int c0 = w, c1 = -1;
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (cap.m[j][i])
            {
                Put(c, x + i, y + j, Gradient(red, 4, j));
                if (i < c0) c0 = i;
                if (i > c1) c1 = i;
            }
    int seam = capSide == CAP_LEFT ? capW : w - capW - 1;
    for (int j = 0; j < h; ++j)
        if (i1.m[j][seam]) Put(c, x + seam, y + j, OUTLINE);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (Edge(&cap, i, j, 0, 1)) Put(c, x + i, y + j, (Rgb){255, 150, 128});
    if (c1 < 0) return;
    int glow[2] = {capSide == CAP_LEFT ? c0 + 2 : c1 - 3, capSide == CAP_LEFT ? c0 + 3 : c1 - 2};
    int dark = capSide == CAP_LEFT ? c1 - 1 : c0 + 1;
    for (int g = 0; g < 2; ++g)
        for (int j = 3; j < h - 3; ++j)
            if (cap.m[j][glow[g]]) Put(c, x + glow[g], y + j, Add(Get(c, x + glow[g], y + j), 22, 40, 34));
    for (int j = 0; j < h; ++j)
        if (cap.m[j][dark]) Put(c, x + dark, y + j, Scale(Get(c, x + dark, y + j), 0.78f));
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
    char digits[4];
    unsigned n = 0;
    uint8_t codes[4];

    do digits[n++] = (char)('0' + level % 10); while ((level /= 10) && n < 3);
    for (unsigned i = 0; i < n; ++i) codes[i] = Code(digits[n - 1 - i]);
    codes[n] = EOS;
    int w = (int)TextWidth(false, codes);
    Text(c, false, right - w, base, codes, INK, &INK_SHADOW);
    Ascii(c, true, right - w - (int)AsciiWidth(true, "Lv") - 1, base, "Lv", (Rgb){196, 64, 48}, NULL);
}

/* A dark capsule: the bars' housing. */
static void Capsule(Canvas *c, int x, int y, int w, int h, int r)
{
    static Mask m, in;
    Shape(&m, w, h, END_ROUND, END_ROUND, r, 1);
    Shrink(&m, &in);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (m.m[j][i]) Put(c, x + i, y + j, OUTLINE);
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i)
            if (Edge(&in, i, j, 0, 1)) Put(c, x + i, y + j, (Rgb){58, 62, 84});
}

static void Fill(Canvas *c, int x, int y, int w, int h, Rgb v)
{
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) Put(c, x + i, y + j, v);
}

/* The HP capsule, 11 pixels: "HP" in gold, a recessed track and a glossy fill coloured as FireRed's (over half
 * green, over a fifth yellow, else red). */
static void HpBar(Canvas *c, int x, int y, int w, unsigned pixels)
{
    static const Rgb green[5] = {{200, 255, 216}, {120, 240, 160}, {72, 212, 120}, {64, 196, 108}, {36, 150, 80}};
    static const Rgb yellow[5] = {{255, 250, 190}, {255, 226, 96}, {248, 200, 40}, {236, 184, 32}, {184, 132, 16}};
    static const Rgb red[5] = {{255, 200, 190}, {255, 120, 100}, {236, 68, 52}, {220, 56, 44}, {164, 32, 32}};
    const Rgb *fill = pixels > CTR_HUD_HP_PIXELS * 50 / 100 ? green : pixels > CTR_HUD_HP_PIXELS * 20 / 100 ? yellow : red;
    int tx = x + 16, tw = w - 19, ty = y + 3;

    Capsule(c, x, y, w, 11, 4);
    Ascii(c, true, x + 4, y + 8, "HP", (Rgb){255, 212, 80}, &(Rgb){120, 84, 16});
    Fill(c, tx, ty, tw, 5, (Rgb){70, 76, 96});
    Fill(c, tx, ty, tw, 1, (Rgb){48, 52, 68});
    if (pixels > CTR_HUD_HP_PIXELS) pixels = CTR_HUD_HP_PIXELS;
    int fw = (int)lrintf((float)tw * (float)pixels / CTR_HUD_HP_PIXELS);
    if (pixels && !fw) fw = 1;
    for (int k = 0; k < 5; ++k) Fill(c, tx, ty + k, fw, 1, fill[k]);
    if (fw > 1 && fw < tw) Fill(c, tx + fw - 1, ty, 1, 5, fill[0]);
}

/* The EXP capsule, 9 pixels: "EXP" in light blue, a 3-pixel track and a blue fill with a bright tip. */
static void ExpBar(Canvas *c, int x, int y, int w, unsigned pixels)
{
    static const Rgb blue[3] = {{176, 236, 255}, {72, 168, 248}, {40, 112, 216}};
    int tx = x + 4 + (int)AsciiWidth(true, "EXP") + 3, tw = x + w - 3 - tx, ty = y + 3;

    Capsule(c, x, y, w, 9, 3);
    Ascii(c, true, x + 4, y + 7, "EXP", (Rgb){140, 214, 255}, &(Rgb){20, 60, 120});
    Fill(c, tx, ty, tw, 3, (Rgb){70, 76, 96});
    Fill(c, tx, ty, tw, 1, (Rgb){48, 52, 68});
    if (pixels > CTR_HUD_EXP_PIXELS) pixels = CTR_HUD_EXP_PIXELS;
    int fw = (int)lrintf((float)tw * (float)pixels / CTR_HUD_EXP_PIXELS);
    if (!fw) return;
    for (int k = 0; k < 3; ++k) Fill(c, tx, ty + k, fw, 1, blue[k]);
    Fill(c, tx + fw - 1, ty, 1, 3, (Rgb){230, 250, 255});
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
#define PLAYER_W 164
#define PLAYER_H 56
#define LOW_H 36                           /* the player's box in a double battle, and the Safari Zone's */

static void FoeBox(Canvas *c, const CtrHudState *s)
{
    const int w = FOE_W, h = FOE_H, split = 17, cap = 17;
    Plate(c, 0, 0, w, h, END_ROUND, END_SLANT, split, CAP_LEFT, cap, 2);
    Ball(c, 5, (h - 11) / 2, s->caught);
    int nx = cap + 5;
    Name(c, nx, 13, s);
    Level(c, w - 14, 13, s->level);
    int hx = nx;
    if (HasStatus(s)) hx += Status(c, hx, 20, s->status) + 3;
    HpBar(c, hx, 21, w - 23 - hx, s->hpPixels);
}

static void PlayerBox(Canvas *c, const CtrHudState *s)
{
    const int w = PLAYER_W, h = PLAYER_H, split = 17, cap = 10;
    char numbers[12];

    Plate(c, 0, 0, w, h, END_SLANT, END_ROUND, split, CAP_RIGHT, cap, 3);
    int nx = 20;
    Name(c, nx, 13, s);
    Level(c, w - cap - 6, 13, s->level);
    int hx = nx - 4;
    if (HasStatus(s)) hx += Status(c, hx, 30, s->status) + 3;
    int end = w - cap - 5;
    /* "hp/max", right-aligned to the bar's end, over it. */
    Numbers(s, numbers);
    Ascii(c, false, end - 3 - (int)AsciiWidth(false, numbers) + 1, 27, numbers, INK, &INK_SHADOW);
    HpBar(c, hx, 31, end - hx, s->hpPixels);
    ExpBar(c, 10, 44, w - cap - 5 - 10, s->expPixels);
}

/* A double battle's player box: the single one's plate, as low as a foe's, its HP numbers beside the bar. */
static void PlayerDoubleBox(Canvas *c, const CtrHudState *s)
{
    const int w = PLAYER_W, h = LOW_H, split = 17, cap = 10;
    char numbers[12];

    Plate(c, 0, 0, w, h, END_SLANT, END_ROUND, split, CAP_RIGHT, cap, 3);
    Name(c, 20, 13, s);
    Level(c, w - cap - 6, 13, s->level);
    int end = w - cap - 5;
    Numbers(s, numbers);
    int nw = (int)AsciiWidth(false, numbers);
    Ascii(c, false, end - nw, 31, numbers, INK, &INK_SHADOW);
    int hx = 16;
    if (HasStatus(s)) hx += Status(c, hx, 20, s->status) + 3;
    HpBar(c, hx, 21, end - nw - 4 - hx, s->hpPixels);
}

/* The Safari Zone's box: SAFARI BALLS on the name band, a Poke Ball and how many are left below. */
static void SafariBox(Canvas *c, const CtrHudState *s)
{
    const int w = PLAYER_W, h = LOW_H, split = 17, cap = 10;
    char count[8];
    unsigned n = 0, t = 0, v = s->balls;
    char tmp[4];

    Plate(c, 0, 0, w, h, END_SLANT, END_ROUND, split, CAP_RIGHT, cap, 3);
    Ascii(c, false, 20, 13, "SAFARI BALLS", INK, &INK_SHADOW);
    Ball(c, 22, 21, true);
    count[n++] = '*';
    do tmp[t++] = (char)('0' + v % 10); while ((v /= 10) && t < 3);
    while (t) count[n++] = tmp[--t];
    count[n] = 0;
    Ascii(c, false, 36, 31, count, INK, &INK_SHADOW);
}

void CtrHud_Render(const CtrHudState *s, uint32_t *pixels, unsigned *width, unsigned *height)
{
    int w = s->kind == CTR_HUD_FOE ? FOE_W : PLAYER_W, h = s->kind == CTR_HUD_FOE ? FOE_H
                                                       : s->kind == CTR_HUD_PLAYER ? PLAYER_H : LOW_H;
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
 */
static const struct { uint8_t kind; bool doubles; uint8_t position; int16_t gbaX, gbaY, x, y; } ANCHORS[] = {
    {CTR_HUD_FOE, false, 1, 12, 14, 10, 22},
    {CTR_HUD_PLAYER, false, 0, 126, 56, 232, 134},
    {CTR_HUD_SAFARI, false, 0, 126, 56, 232, 150},
    {CTR_HUD_FOE, true, 1, 12, 3, 24, 6},
    {CTR_HUD_FOE, true, 3, 0, 28, 6, 44},
    {CTR_HUD_PLAYER_DOUBLE, true, 0, 127, 59, 214, 112},
    {CTR_HUD_PLAYER_DOUBLE, true, 2, 139, 84, 232, 150},
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
