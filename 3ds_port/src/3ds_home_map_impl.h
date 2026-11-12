/* HOME's town map, redrawn at 4x from the game's own Region Map picture (included by 3ds_poketouch.c).
 *
 * The user's ROM provides the picture (CtrPokeTouch_MapArt: tiles, tilemap, palette); nothing of it ships. At run time, once per map, the 192x144 part of that
 * 240x160 screen that holds the land is read pixel by pixel (HmFeed) and a 768x576 palette image is built from it in small steps (HmStep, one per frame):
 * the colours of the game's map are told apart (the greens by height, the roads, the pale paths, the sea, the town orbs), smoothed into fields on the small grid,
 * and every pixel of the large image is then lit from those fields: the greens in half steps with relief shading, a dark outline and a pale shore for the coast,
 * roads with their tones, an outline, a lit edge, a shaded edge and a shadow, sea stripes, and a round orb with a slate ring at each town. No houses, no
 * numbers, and a town has no green: the road goes straight on through it. The image is 8 bits a pixel (its own palette, at most 255 colours).
 *
 * The viewport (240 x 160) shows the image at a zoom (HM_ZOOMS) round a centre, in world pixels (the image's own). Scales below 1 average 2x2 or 3x3 samples. */
#define HM_SW 192                       /* the part of the game's picture used, and where it begins in the 240x160 screen */
#define HM_SH 144
#define HM_X0 24
#define HM_Y0 16
#define HM_Z 4                          /* the image is 4x the picture */
#define HM_W (HM_SW * HM_Z)
#define HM_H (HM_SH * HM_Z)
#define HM_ORBS 16
#define HM_BAND 32                      /* rows of the image built in a step */
#define HM_MARGIN 3                     /* rows and columns round a band that its outlines look at */
#define HM_ZOOMS 3
#define HM_CELL_X0 12                   /* a cell's centre is (8 cx + 12, 8 cy + 20) in the picture's part */
#define HM_CELL_Y0 20

/* The scale of each zoom, in 1/256: the whole width (240/768), 0.8 and 1.45. */
#define HM_SCALE_0 80
#define HM_SCALE_1 205
#define HM_SCALE_2 371

static const uint8_t kHmGreens[5][3] = {{24, 131, 8}, {57, 172, 8}, {82, 205, 8}, {115, 230, 32}, {172, 246, 57}};
static const uint8_t kHmRoads[4][3] = {{230, 164, 0}, {238, 189, 57}, {246, 213, 82}, {238, 230, 115}};
static const uint8_t kHmPales[3][3] = {{205, 205, 148}, {230, 222, 164}, {246, 238, 139}};
static const uint8_t kHmSeas[2][3] = {{82, 148, 213}, {90, 172, 230}};
static const uint8_t kHmMarks[11][3] = {{255, 57, 16}, {148, 0, 0}, {255, 172, 16}, {255, 222, 49}, {189, 156, 41}, {246, 246, 238},
                                         {205, 205, 205}, {98, 98, 98}, {115, 115, 98}, {255, 255, 255}, {164, 172, 172}};

enum { HM_ROAD = 1, HM_PALE = 2, HM_SEA = 4, HM_MARK = 8, HM_RED = 16, HM_PAD = 128, HM_TONE_SHIFT = 5 };
enum { HMF_LAND, HMF_LV, HMF_SHADE, HMF_WAY, HMF_DPR, HMF_TONE, HMF_SEA, HMF_COUNT };
enum { HMS_FEED, HMS_LAND, HMS_LV1, HMS_LV2, HMS_WAY, HMS_TONE, HMS_SEA, HMS_BANDS, HMS_ORBS, HMS_DONE, HMS_FAILED };

static uint8_t sHmCls[HM_SH * HM_SW];            /* what each picture pixel is (HM_*) */
static uint8_t sHmLevel[HM_SH * HM_SW];          /* its green, 0..4, or 255 */
static uint8_t sHmAux[HM_SH * HM_SW];            /* bit 0: it is road once the towns' cells are, bit 1: pale path */
static uint8_t sHmWorld[HM_H][HM_W];            /* the image: palette indexes */
static uint16_t sHmPal[256];                    /* RGB565 */
static int sHmPalN;
static int16_t sHmOrb[HM_ORBS][2];              /* the towns' orbs, in image pixels */
static int sHmOrbN;
static struct {
    int stage, row;
    int16_t *field[HMF_COUNT];                  /* Q12 fields on the picture's grid, while the image is built */
    float *plane[4];
    uint8_t *lut;                               /* RGB565 -> palette index + 1 */
} sHm = {HMS_FEED, 0, {0}, {0}, 0};

static int HmDone(void)
{
    return sHm.stage == HMS_DONE;
}

static void HmRelease(void)
{
    for (int i = 0; i < HMF_COUNT; ++i) {
        free(sHm.field[i]);
        sHm.field[i] = NULL;
    }
    for (int i = 0; i < 4; ++i) {
        free(sHm.plane[i]);
        sHm.plane[i] = NULL;
    }
    free(sHm.lut);
    sHm.lut = NULL;
}

static int HmBegin(void)
{
    HmRelease();
    memset(sHmCls, 0, sizeof(sHmCls));
    memset(sHmLevel, 255, sizeof(sHmLevel));
    memset(sHmAux, 0, sizeof(sHmAux));
    sHmPalN = sHmOrbN = 0;
    sHm.stage = HMS_FEED;
    sHm.row = 0;
    return 1;
}

static int HmNear(const uint8_t *c, int r, int g, int b)
{
    int dr = r - c[0], dg = g - c[1], db = b - c[2];

    return dr >= -3 && dr <= 3 && dg >= -3 && dg <= 3 && db >= -3 && db <= 3;
}

/* One pixel of the game's picture, (x, y) of the 192x144 part, as 8-bit RGB. */
static void HmFeed(int x, int y, int r, int g, int b)
{
    uint8_t cls = 0, level = 255;

    for (int i = 0; i < 5; ++i)
        if (HmNear(kHmGreens[i], r, g, b))
            level = (uint8_t)i;
    for (int i = 0; i < 4; ++i)
        if (HmNear(kHmRoads[i], r, g, b))
            cls = (uint8_t)(HM_ROAD | i << HM_TONE_SHIFT);
    for (int i = 0; i < 3; ++i)
        if (HmNear(kHmPales[i], r, g, b))
            cls |= HM_PALE;
    for (int i = 0; i < 2; ++i)
        if (HmNear(kHmSeas[i], r, g, b))
            cls |= HM_SEA;
    for (int i = 0; i < 11; ++i)
        if (HmNear(kHmMarks[i], r, g, b))
            cls |= HM_MARK;
    if (HmNear(kHmMarks[0], r, g, b))
        cls |= HM_RED;
    /* The corner where the game's Cancel and Switch buttons were (a picture's own pixels there mean nothing). */
    if (y >= 115 && y < 133 && x >= 170 && x < 190) {
        cls &= (uint8_t)(HM_SEA | HM_RED);
        level = 255;
    }
    sHmCls[y * HM_SW + x] = cls;
    sHmLevel[y * HM_SW + x] = level;
}

/* ---- the small grid's fields ---- */

/* e^-x for x in [0, 8]: (1 - x/256)^256, near enough for a kernel that is normalised. */
static float HmExpNeg(float x)
{
    float t = 1.0f - x / 256.0f;

    for (int i = 0; i < 8; ++i)
        t *= t;
    return t;
}

/* A Gaussian blur in place (edges repeat), separable, through a row and a column of its own. */
static void HmBlur(float *p, float sigma)
{
    float k[20], line[HM_SW > HM_SH ? HM_SW + 20 : HM_SH + 20], sum = 0;
    int n = (int)(3.0f * sigma + 0.999f);

    for (int i = -n; i <= n; ++i) {
        k[i + n] = HmExpNeg((float)(i * i) / (2.0f * sigma * sigma));
        sum += k[i + n];
    }
    for (int i = 0; i <= 2 * n; ++i)
        k[i] /= sum;
    for (int y = 0; y < HM_SH; ++y) {
        float *row = p + y * HM_SW;

        for (int x = 0; x < HM_SW; ++x) {
            float acc = 0;

            for (int i = -n; i <= n; ++i) {
                int xx = x + i;

                acc += k[i + n] * row[xx < 0 ? 0 : xx >= HM_SW ? HM_SW - 1 : xx];
            }
            line[x] = acc;
        }
        memcpy(row, line, HM_SW * sizeof(float));
    }
    for (int x = 0; x < HM_SW; ++x) {
        for (int y = 0; y < HM_SH; ++y) {
            float acc = 0;

            for (int i = -n; i <= n; ++i) {
                int yy = y + i;

                acc += k[i + n] * p[(yy < 0 ? 0 : yy >= HM_SH ? HM_SH - 1 : yy) * HM_SW + x];
            }
            line[y] = acc;
        }
        for (int y = 0; y < HM_SH; ++y)
            p[y * HM_SW + x] = line[y];
    }
}

static int16_t HmQ(float v)
{
    int q = (int)(v * 4096.0f + (v < 0 ? -0.5f : 0.5f));

    return (int16_t)(q < -32000 ? -32000 : q > 32000 ? 32000 : q);
}

static int HmLand(int i)
{
    return sHmLevel[i] != 255 || (sHmCls[i] & (HM_ROAD | HM_PALE | HM_MARK));
}

static void HmOrbs(void)
{
    static int16_t stack[2048];
    static uint8_t seen[HM_SH][HM_SW];

    memset(seen, 0, sizeof(seen));
    sHmOrbN = 0;
    for (int y = 0; y < HM_SH; ++y)
        for (int x = 0; x < HM_SW; ++x) {
            int sp = 0, count = 0;
            long sx = 0, sy = 0;

            if (seen[y][x] || !(sHmCls[y * HM_SW + x] & HM_RED))
                continue;
            seen[y][x] = 1;
            stack[sp++] = (int16_t)(y * HM_SW + x);
            while (sp) {
                int at = stack[--sp], cy = at / HM_SW, cx = at % HM_SW;

                ++count;
                sx += cx;
                sy += cy;
                for (int dy = -1; dy <= 1; ++dy)
                    for (int dx = -1; dx <= 1; ++dx) {
                        int ny = cy + dy, nx = cx + dx;

                        if (ny < 0 || nx < 0 || ny >= HM_SH || nx >= HM_SW || seen[ny][nx] || !(sHmCls[ny * HM_SW + nx] & HM_RED))
                            continue;
                        seen[ny][nx] = 1;
                        if (sp < 2048)
                            stack[sp++] = (int16_t)(ny * HM_SW + nx);
                    }
            }
            if (count > 6 && sHmOrbN < HM_ORBS) {
                sHmOrb[sHmOrbN][0] = (int16_t)((sx * 2 + count) * HM_Z / (2 * count));
                sHmOrb[sHmOrbN][1] = (int16_t)((sy * 2 + count) * HM_Z / (2 * count));
                ++sHmOrbN;
            }
        }
}

static float *HmPlane(int i)
{
    return sHm.plane[i];
}

static void HmStages(void)
{
    float *w0 = HmPlane(0), *w1 = HmPlane(1), *w2 = HmPlane(2), *w3 = HmPlane(3);
    const int n = HM_SW * HM_SH;

    switch (sHm.stage) {
    case HMS_LAND:
        for (int i = 0; i < n; ++i)
            w0[i] = HmLand(i) ? 1.0f : 0.0f;
        HmBlur(w0, 0.8f);
        for (int i = 0; i < n; ++i)
            sHm.field[HMF_LAND][i] = HmQ(w0[i]);
        break;
    case HMS_LV1:
        for (int i = 0; i < n; ++i) {
            w0[i] = sHmLevel[i] != 255 ? 1.0f : 0.0f;
            w1[i] = sHmLevel[i] != 255 ? (float)sHmLevel[i] : 0.0f;
        }
        HmBlur(w0, 2.5f);
        HmBlur(w1, 2.5f);
        for (int i = 0; i < n; ++i) {       /* the level where it is known, else what its neighbours say; land only */
            float v = sHmLevel[i] != 255 ? (float)sHmLevel[i] : w1[i] / (w0[i] > 1e-3f ? w0[i] : 1e-3f);

            w2[i] = HmLand(i) ? v : 0.0f;
        }
        HmBlur(w2, 1.2f);
        break;
    case HMS_LV2: {
        for (int i = 0; i < n; ++i)
            w0[i] = HmLand(i) ? 1.0f : 0.0f;
        HmBlur(w0, 1.2f);
        for (int i = 0; i < n; ++i)
            w1[i] = w2[i] / (w0[i] > 1e-3f ? w0[i] : 1e-3f);
        for (int y = 0; y < HM_SH; ++y)
            for (int x = 0; x < HM_SW; ++x) {
                int i = y * HM_SW + x, xa = x > 0 ? x - 1 : x, xb = x < HM_SW - 1 ? x + 1 : x;
                int ya = y > 0 ? y - 1 : y, yb = y < HM_SH - 1 ? y + 1 : y;
                float gx = (w1[y * HM_SW + xb] - w1[y * HM_SW + xa]) / (float)(xb - xa);
                float gy = (w1[yb * HM_SW + x] - w1[ya * HM_SW + x]) / (float)(yb - ya);
                float s = -(gx + gy) * 10.0f;       /* light from the top left; the gradient per picture pixel, 40 per image pixel */

                sHm.field[HMF_SHADE][i] = HmQ(s < -1 ? -1.0f : s > 1 ? 1.0f : s);
                sHm.field[HMF_LV][i] = HmQ(w1[i]);
            }
        break;
    }
    case HMS_WAY: {
        for (int i = 0; i < n; ++i) {
            w0[i] = sHmCls[i] & HM_ROAD ? 1.0f : 0.0f;
            w1[i] = sHmCls[i] & HM_PALE ? 1.0f : 0.0f;
            w2[i] = w0[i] > w1[i] ? w0[i] : w1[i];
        }
        HmBlur(w0, 2.0f);
        HmBlur(w1, 2.0f);
        HmBlur(w2, 2.0f);
        /* Every town's cell is road (or pale path, as the paths round it): the road goes straight on through it. The red orb's cell is the
         * 8x8 block that has 6 pixels of it. */
        for (int by = 0; by < HM_SH; by += 8)
            for (int bx = 0; bx < HM_SW; bx += 8) {
                int red = 0;

                for (int y = by; y < by + 8; ++y)
                    for (int x = bx; x < bx + 8; ++x)
                        red += (sHmCls[y * HM_SW + x] & HM_RED) != 0;
                if (red < 6)
                    continue;
                for (int y = by; y < by + 8; ++y)
                    for (int x = bx; x < bx + 8; ++x)
                        if (!(y >= 115 && y < 133 && x >= 170 && x < 190))
                            sHmCls[y * HM_SW + x] |= HM_PAD;
            }
        for (int i = 0; i < n; ++i) {
            int isPale = w2[i] > 0.12f && w0[i] < w1[i], pad = (sHmCls[i] & HM_PAD) != 0;

            sHmAux[i] = (uint8_t)((sHmCls[i] & HM_ROAD || (pad && !isPale) ? 1 : 0) | (sHmCls[i] & HM_PALE || (pad && isPale) ? 2 : 0));
        }
        for (int i = 0; i < n; ++i)
            w0[i] = sHmAux[i] & 3 ? 1.0f : 0.0f;
        HmBlur(w0, 0.6f);
        for (int i = 0; i < n; ++i)
            sHm.field[HMF_WAY][i] = HmQ(w0[i]);
        for (int i = 0; i < n; ++i) {
            w0[i] = sHmAux[i] & 2 ? 1.0f : 0.0f;
            w1[i] = sHmAux[i] & 1 ? 1.0f : 0.0f;
        }
        HmBlur(w0, 0.9f);
        HmBlur(w1, 0.9f);
        for (int i = 0; i < n; ++i)
            sHm.field[HMF_DPR][i] = HmQ(w0[i] - w1[i]);
        break;
    }
    case HMS_TONE: {
        for (int i = 0; i < n; ++i) {
            int road = sHmCls[i] & HM_ROAD;

            w0[i] = road ? (float)(sHmCls[i] >> HM_TONE_SHIFT & 3) : 0.0f;
            w1[i] = road ? 1.0f : 0.0f;
        }
        HmBlur(w0, 2.5f);
        HmBlur(w1, 2.5f);
        for (int i = 0; i < n; ++i) {       /* the road's own tones; a town's cell takes its neighbours' */
            float fill = w1[i] > 0.02f ? w0[i] / w1[i] : 1.0f;

            w2[i] = sHmCls[i] & HM_ROAD ? (float)(sHmCls[i] >> HM_TONE_SHIFT & 3) : sHmAux[i] & 1 ? fill : 0.0f;
            w3[i] = sHmAux[i] & 1 ? 1.0f : 0.0f;
        }
        HmBlur(w2, 1.6f);
        HmBlur(w3, 1.6f);
        for (int i = 0; i < n; ++i)
            sHm.field[HMF_TONE][i] = HmQ(w2[i] / (w3[i] > 1e-3f ? w3[i] : 1e-3f));
        break;
    }
    case HMS_SEA:
        for (int i = 0; i < n; ++i)
            w0[i] = sHmCls[i] & HM_SEA ? 1.0f : 0.0f;
        HmBlur(w0, 0.8f);
        for (int i = 0; i < n; ++i)
            sHm.field[HMF_SEA][i] = HmQ(w0[i]);
        for (int i = 0; i < 4; ++i) {
            free(sHm.plane[i]);
            sHm.plane[i] = NULL;
        }
        break;
    default:
        break;
    }
}

/* ---- the image ---- */

/* A field at image pixel (X, Y): bilinear from the small grid (a picture pixel is 4 image pixels, centres in line). */
static int HmSamp(const int16_t *f, int X, int Y)
{
    int ux = 2 * X - 3, uy = 2 * Y - 3, ix = ux >> 3, iy = uy >> 3, fx = ux & 7, fy = uy & 7;
    int x0 = ix < 0 ? 0 : ix >= HM_SW ? HM_SW - 1 : ix, x1 = ix + 1 < 0 ? 0 : ix + 1 >= HM_SW ? HM_SW - 1 : ix + 1;
    int y0 = iy < 0 ? 0 : iy >= HM_SH ? HM_SH - 1 : iy, y1 = iy + 1 < 0 ? 0 : iy + 1 >= HM_SH ? HM_SH - 1 : iy + 1;
    int a = f[y0 * HM_SW + x0] * (8 - fx) + f[y0 * HM_SW + x1] * fx;
    int b = f[y1 * HM_SW + x0] * (8 - fx) + f[y1 * HM_SW + x1] * fx;

    return (a * (8 - fy) + b * fy) >> 6;
}

static int HmIndex(int r, int g, int b)
{
    int key, best = 0, bestD = 1 << 30;

    r = r < 0 ? 0 : r > 255 ? 255 : r;
    g = g < 0 ? 0 : g > 255 ? 255 : g;
    b = b < 0 ? 0 : b > 255 ? 255 : b;
    key = (r >> 3) << 11 | (g >> 2) << 5 | (b >> 3);
    if (sHm.lut[key])
        return sHm.lut[key] - 1;
    if (sHmPalN < 255) {
        sHmPal[sHmPalN] = (uint16_t)key;
        sHm.lut[key] = (uint8_t)(sHmPalN + 1);
        return sHmPalN++;
    }
    for (int i = 0; i < sHmPalN; ++i) {
        int pr = (sHmPal[i] >> 11) << 3, pg = (sHmPal[i] >> 5 & 63) << 2, pb = (sHmPal[i] & 31) << 3;
        int d = (pr - r) * (pr - r) + (pg - g) * (pg - g) + (pb - b) * (pb - b);

        if (d < bestD) {
            bestD = d;
            best = i;
        }
    }
    sHm.lut[key] = (uint8_t)(best + 1);
    return best;
}

enum { HB_LAND = 1, HB_WAY = 2, HB_PALE = 4, HB_SE = 8 };
#define HB_W (HM_W + 2 * HM_MARGIN)
#define HB_H (HM_BAND + 2 * HM_MARGIN)
static uint8_t sHmBandMask[HB_H][HB_W];

static int HmMask(int gx, int gy)
{
    int v = 0, land = HmSamp(sHm.field[HMF_LAND], gx, gy) > 2048, way = HmSamp(sHm.field[HMF_WAY], gx, gy) > 1843;
    int sea = HmSamp(sHm.field[HMF_SEA], gx, gy) > 2048;

    if (land)
        v |= HB_LAND;
    if (way) {
        v |= HB_WAY;
        if (HmSamp(sHm.field[HMF_DPR], gx, gy) > 0)
            v |= HB_PALE;
    }
    if (sea && !land && !way)
        v |= HB_SE;
    return v;
}

static void HmBand(int y0)
{
    for (int gy = -HM_MARGIN; gy < HM_BAND + HM_MARGIN; ++gy)
        for (int gx = -HM_MARGIN; gx < HM_W + HM_MARGIN; ++gx)
            sHmBandMask[gy + HM_MARGIN][gx + HM_MARGIN] = (uint8_t)HmMask(gx, y0 + gy);
    for (int gy = 0; gy < HM_BAND; ++gy) {
        int Y = y0 + gy, stripe = (Y >> 1) & 1;

        for (int X = 0; X < HM_W; ++X) {
#define M(dx, dy) (sHmBandMask[gy + HM_MARGIN + (dy)][X + HM_MARGIN + (dx)])
            int m = M(0, 0), land = m & HB_LAND, way = m & HB_WAY, pale = m & HB_PALE, r, g, b;

            if (land) {
                int v = HmSamp(sHm.field[HMF_LV], X, Y), q2, i, k, s = HmSamp(sHm.field[HMF_SHADE], X, Y);
                const uint8_t *a, *c;

                v = v < 0 ? 0 : v > 16384 ? 16384 : v;
                q2 = (v * 2 + 2048) >> 12;
                i = q2 >> 1;
                a = kHmGreens[i];
                c = kHmGreens[i < 4 ? i + 1 : 4];
                k = 256 + ((s * 18) >> 12);
                r = (q2 & 1 ? (a[0] + c[0]) / 2 : a[0]) * k >> 8;
                g = (q2 & 1 ? (a[1] + c[1]) / 2 : a[1]) * k >> 8;
                b = (q2 & 1 ? (a[2] + c[2]) / 2 : a[2]) * k >> 8;
                if (!(M(1, 0) & HB_LAND) || !(M(-1, 0) & HB_LAND) || !(M(0, 1) & HB_LAND) || !(M(0, -1) & HB_LAND)) {
                    r = r * 141 >> 8;
                    g = g * 141 >> 8;
                    b = b * 141 >> 8;
                }
            } else if (HmSamp(sHm.field[HMF_SEA], X, Y) > 2048) {
                r = stripe ? kHmSeas[0][0] : kHmSeas[1][0];
                g = stripe ? kHmSeas[0][1] : kHmSeas[1][1];
                b = stripe ? kHmSeas[0][2] : kHmSeas[1][2];
            } else {
                r = stripe ? 164 : 156;
                g = stripe ? 180 : 213;
                b = stripe ? 255 : 255;
                if (HmSamp(sHm.field[HMF_LAND], X, Y) > 330) {              /* the shore: a pale band in the water beside the coast */
                    r = (r * 140 + 222 * 116) >> 8;
                    g = (g * 140 + 238 * 116) >> 8;
                    b = (b * 140 + 255 * 116) >> 8;
                }
            }
            if (!way && land && (M(-2, -3) & HB_WAY)) {                     /* the shadow the ways throw, down and to the right */
                r = r * 184 >> 8;
                g = g * 184 >> 8;
                b = b * 184 >> 8;
            }
            if (way) {
                if (pale) {
                    r = 232;
                    g = 224;
                    b = 168;
                } else {
                    int t = HmSamp(sHm.field[HMF_TONE], X, Y), i, f;

                    t = t < 0 ? 0 : t > 12287 ? 12287 : t;
                    i = t >> 12;
                    f = t & 4095;
                    r = kHmRoads[i][0] + ((kHmRoads[i + 1][0] - kHmRoads[i][0]) * f >> 12);
                    g = kHmRoads[i][1] + ((kHmRoads[i + 1][1] - kHmRoads[i][1]) * f >> 12);
                    b = kHmRoads[i][2] + ((kHmRoads[i + 1][2] - kHmRoads[i][2]) * f >> 12);
                }
                if (!(M(0, -3) & HB_WAY) && (M(0, -1) & HB_WAY)) {         /* its lit upper edge */
                    r += 26;
                    g += 26;
                    b += 26;
                }
                if (!(M(0, 3) & HB_WAY)) {                                  /* its shaded lower edge */
                    r = r * 230 >> 8;
                    g = g * 230 >> 8;
                    b = b * 230 >> 8;
                }
                if (!(M(1, 0) & HB_WAY) || !(M(-1, 0) & HB_WAY) || !(M(0, 1) & HB_WAY) || !(M(0, -1) & HB_WAY)) {   /* and its outline */
                    r = pale ? 170 : 176;
                    g = pale ? 160 : 104;
                    b = pale ? 104 : 0;
                }
            } else if ((m & HB_SE) && (!(M(1, 0) & HB_SE) || !(M(-1, 0) & HB_SE) || !(M(0, 1) & HB_SE) || !(M(0, -1) & HB_SE))) {
                r = 64;                                                     /* a sea route's rim */
                g = 124;
                b = 196;
            }
#undef M
            sHmWorld[Y][X] = (uint8_t)HmIndex(r, g, b);
        }
    }
}

/* PIL's ellipse(x0, y0, x1, y1) (both ends inside), filled. */
static void HmDisc(int x0, int y0, int x1, int y1, int r, int g, int b)
{
    int idx = HmIndex(r, g, b);
    long ax2 = x1 - x0 + 1, ay2 = y1 - y0 + 1;      /* the axes, doubled */

    for (int y = y0; y <= y1; ++y)
        for (int x = x0; x <= x1; ++x) {
            long dx = 2L * x + 1 - (x0 + x1 + 1), dy = 2L * y + 1 - (y0 + y1 + 1);

            if (x < 0 || y < 0 || x >= HM_W || y >= HM_H)
                continue;
            if (dx * dx * ay2 * ay2 + dy * dy * ax2 * ax2 <= ax2 * ax2 * ay2 * ay2)
                sHmWorld[y][x] = (uint8_t)idx;
        }
}

static void HmOrbDraw(int px, int py)
{
    int m = HmMask(px, py), road = (m & HB_WAY) && !(m & HB_PALE);

    if (road)
        HmDisc(px - 9, py - 7, px + 11, py + 12, 120, 76, 0);
    else
        HmDisc(px - 9, py - 7, px + 11, py + 12, 20, 92, 8);
    HmDisc(px - 11, py - 11, px + 11, py + 11, 64, 72, 104);
    HmDisc(px - 10, py - 10, px + 10, py + 10, 255, 255, 255);
    HmDisc(px - 7, py - 7, px + 7, py + 7, 148, 0, 0);
    HmDisc(px - 6, py - 6, px + 6, py + 6, 255, 57, 16);
    HmDisc(px - 6, py - 6, px + 4, py + 4, 255, 96, 32);
    HmDisc(px - 4, py - 4, px + 1, py + 1, 255, 172, 16);
    HmDisc(px - 3, py - 3, px - 1, py - 1, 255, 238, 120);
}

/* One step (a frame's work) of building the image after the picture was fed; the image is usable once HmDone. */
static void HmStep(void)
{
    if (sHm.stage == HMS_DONE || sHm.stage == HMS_FAILED)
        return;
    if (sHm.stage == HMS_FEED) {
        HmOrbs();
        for (int i = 0; i < 4; ++i)
            sHm.plane[i] = (float *)malloc(HM_SW * HM_SH * sizeof(float));
        for (int i = 0; i < HMF_COUNT; ++i)
            sHm.field[i] = (int16_t *)malloc(HM_SW * HM_SH * sizeof(int16_t));
        sHm.lut = (uint8_t *)calloc(65536, 1);
        for (int i = 0; i < 4; ++i)
            if (!sHm.plane[i])
                sHm.lut = (free(sHm.lut), NULL);
        for (int i = 0; i < HMF_COUNT; ++i)
            if (!sHm.field[i])
                sHm.lut = (free(sHm.lut), NULL);
        if (!sHm.lut) {
            HmRelease();
            sHm.stage = HMS_FAILED;
            return;
        }
        memset(sHmWorld, 0, sizeof(sHmWorld));
        sHm.stage = HMS_LAND;
        return;
    }
    if (sHm.stage < HMS_BANDS) {
        HmStages();
        ++sHm.stage;
        return;
    }
    if (sHm.stage == HMS_BANDS) {
        HmBand(sHm.row);
        sHm.row += HM_BAND;
        if (sHm.row >= HM_H)
            sHm.stage = HMS_ORBS;
        return;
    }
    for (int i = 0; i < sHmOrbN; ++i)
        HmOrbDraw(sHmOrb[i][0], sHmOrb[i][1]);
    HmRelease();
    sHm.stage = HMS_DONE;
}

/* ---- the picture's cells and the viewport ---- */

static int HmScale(int zoom)
{
    return zoom <= 0 ? HM_SCALE_0 : zoom == 1 ? HM_SCALE_1 : HM_SCALE_2;
}

/* A cell's centre in the image, on its town's orb when there is one (an orb within 6 picture pixels). */
static void HmCellWorld(int cx, int cy, int *wx, int *wy)
{
    int x = (8 * cx + HM_CELL_X0) * HM_Z, y = (8 * cy + HM_CELL_Y0) * HM_Z;

    for (int i = 0; i < sHmOrbN; ++i)
        if (sHmOrb[i][0] - x >= -6 * HM_Z && sHmOrb[i][0] - x <= 6 * HM_Z && sHmOrb[i][1] - y >= -6 * HM_Z && sHmOrb[i][1] - y <= 6 * HM_Z) {
            x = sHmOrb[i][0];
            y = sHmOrb[i][1];
            break;
        }
    *wx = x;
    *wy = y;
}

/* The cell of an image point; false if it is off the cells. */
static int HmWorldCell(int wx, int wy, int *cx, int *cy)
{
    int x = wx / HM_Z - 8, y = wy / HM_Z - 16;

    if (x < 0 || y < 0)
        return 0;
    *cx = x / 8;
    *cy = y / 8;
    return *cx < 22 && *cy < 15;
}

/* The centre of the viewport kept so that the image covers it. */
static void HmClamp(int zoom, int *cx, int *cy)
{
    int s = HmScale(zoom), hw = PT_VIEW_W * 256 / (2 * s), hh = PT_VIEW_H * 256 / (2 * s);

    *cx = *cx < hw ? hw : *cx > HM_W - hw ? HM_W - hw : *cx;
    *cy = *cy < hh ? hh : *cy > HM_H - hh ? HM_H - hh : *cy;
}

/* Viewport pixel <-> image point, in 1/256 image pixels for the way in. */
static void HmViewToWorld(int zoom, int cx, int cy, int vx, int vy, int *wx, int *wy)
{
    int s = HmScale(zoom);

    *wx = cx + ((2 * vx + 1 - PT_VIEW_W) * 128) / s;
    *wy = cy + ((2 * vy + 1 - PT_VIEW_H) * 128) / s;
}

static void HmWorldToView(int zoom, int cx, int cy, int wx, int wy, int *vx, int *vy)
{
    int s = HmScale(zoom);

    *vx = PT_VIEW_W / 2 + ((wx - cx) * s) / 256;
    *vy = PT_VIEW_H / 2 + ((wy - cy) * s) / 256;
}

static void HmColour(int idx, int *r, int *g, int *b)
{
    uint16_t c = sHmPal[idx];

    *r = (c >> 11) << 3 | (c >> 13);
    *g = (c >> 5 & 63) << 2 | (c >> 9 & 3);
    *b = (c & 31) << 3 | (c >> 2 & 7);
}

/* The rectangle (x, y, w, h) of the viewport (its own pixels) drawn from the image: 1 sample a pixel at the scales from 1, else 2x2 or 3x3 averaged. */
static __attribute__((noipa)) void HmDrawRect(int zoom, int cx, int cy, int x, int y, int w, int h)
{
    static int16_t tx[PT_VIEW_W * 3], ty[PT_VIEW_H * 3];
    uint8_t rgb[256][3];
    int s = HmScale(zoom), n = s >= 256 ? 1 : s >= 160 ? 2 : 3;
    for (int i = 0; i < 256; ++i) {
        int r, g, b;

        HmColour(i, &r, &g, &b);
        rgb[i][0] = (uint8_t)r;
        rgb[i][1] = (uint8_t)g;
        rgb[i][2] = (uint8_t)b;
    }
    for (int i = 0; i < w * n; ++i) {
        long u = (long)(2 * (x + i / n) + 1 - PT_VIEW_W) * n + (2 * (i % n) + 1 - n);
        int v = cx + (int)(u * 128 / ((long)n * s));

        tx[i] = (int16_t)(v < 0 ? 0 : v >= HM_W ? HM_W - 1 : v);
    }
    for (int j = 0; j < h * n; ++j) {
        long u = (long)(2 * (y + j / n) + 1 - PT_VIEW_H) * n + (2 * (j % n) + 1 - n);
        int v = cy + (int)(u * 128 / ((long)n * s));

        ty[j] = (int16_t)(v < 0 ? 0 : v >= HM_H ? HM_H - 1 : v);
    }
    for (int j = 0; j < h; ++j)
        for (int i = 0; i < w; ++i) {
            int sr = 0, sg = 0, sb = 0;

            for (int b = 0; b < n; ++b)
                for (int a = 0; a < n; ++a) {
                    const uint8_t *c = rgb[sHmWorld[ty[j * n + b]][tx[i * n + a]]];

                    sr += c[0];
                    sg += c[1];
                    sb += c[2];
                }
            sr /= n * n;
            sg /= n * n;
            sb /= n * n;
            Px(PT_VIEW_X + x + i, PT_VIEW_Y + y + j, (uint16_t)((sr >> 3) << 11 | (sg >> 2) << 5 | (sb >> 3)));
        }
}
