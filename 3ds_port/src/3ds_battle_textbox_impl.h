/* The battle's message box in the port's design (v4), included by 3ds_video.c, whose state and helpers it uses.
 *
 * FireRed's text box is the last six tile rows of BG0: a frame of its own tiles (graphics/battle_interface/textbox.bin,
 * below BOX_FRAME_TILES) and, over it, the windows the game prints into. Here the frame is not drawn: a navy panel takes
 * the bottom 48 lines of the screen, full width, and the windows' tiles are drawn over it at 1:1, moved by BOX_TEXT_X
 * and BOX_TEXT_Y, with the message window's background (colour 15) left out and its text in the design's white and
 * shadow. FireRed's prompt arrow (colours 2, 4 and 9 of the box's palette) is drawn in the design's reds at the panel's
 * right end, at the height the game gives it, so it keeps its bounce. The panel fades with the box's palette.
 */
#define BOX_W 400
#define BOX_H 48
#define BOX_CORNER 12
#define BOX_TEX_W 512
#define BOX_TEX_H 128
#define BOX_TEXT_ROW 64                 /* the windows' pixels in the texture */
#define BOX_ARROW_ROW 112               /* the prompt arrow's */
#define BOX_FRAME_TILES 0x40            /* FireRed's frame tiles; the windows' start at 0x090 */
#define BOX_TEXT_X 8
#define BOX_TEXT_Y 0
/* FireRed's second line (band rows from BOX_LINE2_ROW) comes BOX_LINE2_UP closer to the first: the two lines and their
 * spacing would not fit between the red tab and the inner rim. */
#define BOX_LINE2_ROW 27
#define BOX_LINE2_UP 4
#define BOX_ARROW_X 378
#define BOX_TAB_X0 16                   /* the red tab, rows 6 to 9 */
#define BOX_TAB_X1 68

/* The panel, from the approved design: its colours, its left and right ends, and the column between them. */
static const uint8_t kBoxRgb[][3] = {{20, 22, 34}, {28, 30, 44}, {84, 96, 132}, {132, 146, 186}, {74, 86, 121}, {65, 76, 110}, {56, 66, 100}, {55, 65, 98}, {104, 120, 164}, {54, 64, 97}, {24, 28, 46}, {53, 63, 96}, {53, 62, 95}, {52, 62, 94}, {51, 61, 93}, {51, 60, 92}, {50, 59, 91}, {49, 58, 90}, {49, 58, 89}, {48, 57, 88}, {47, 56, 87}, {47, 55, 86}, {46, 54, 85}, {45, 54, 84}, {45, 53, 83}, {44, 52, 82}, {43, 51, 81}, {43, 51, 80}, {42, 50, 78}, {41, 49, 77}, {40, 48, 76}, {40, 47, 75}, {39, 47, 74}, {38, 46, 73}, {38, 45, 72}, {37, 44, 71}, {36, 43, 70}, {36, 43, 69}, {35, 42, 68}, {34, 41, 67}, {34, 40, 66}, {18, 20, 32}, {33, 39, 65}, {32, 39, 64}, {32, 38, 63}, {31, 37, 62}, {30, 36, 61}, {30, 36, 60}};
static const uint8_t kBoxLeft[BOX_H][BOX_CORNER] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 2, 2},
    {0, 0, 0, 0, 0, 0, 1, 1, 3, 4, 4, 4},
    {0, 0, 0, 0, 0, 1, 3, 3, 5, 5, 5, 5},
    {0, 0, 0, 0, 0, 1, 6, 6, 6, 6, 6, 6},
    {0, 0, 0, 0, 1, 3, 7, 7, 7, 7, 7, 8},
    {0, 0, 0, 0, 3, 9, 9, 9, 9, 9, 8, 10},
    {0, 0, 0, 0, 11, 11, 11, 11, 11, 8, 11, 11},
    {0, 0, 0, 0, 12, 12, 12, 12, 8, 10, 12, 12},
    {0, 0, 0, 0, 13, 13, 13, 13, 8, 10, 13, 13},
    {0, 0, 0, 0, 14, 14, 14, 14, 8, 10, 14, 14},
    {0, 0, 0, 0, 15, 15, 15, 15, 8, 10, 15, 15},
    {0, 0, 0, 0, 16, 16, 16, 16, 8, 10, 16, 16},
    {0, 0, 0, 0, 17, 17, 17, 17, 8, 10, 17, 17},
    {0, 0, 0, 0, 18, 18, 18, 18, 8, 10, 18, 18},
    {0, 0, 0, 0, 19, 19, 19, 19, 8, 10, 19, 19},
    {0, 0, 0, 0, 20, 20, 20, 20, 8, 10, 20, 20},
    {0, 0, 0, 0, 21, 21, 21, 21, 8, 10, 21, 21},
    {0, 0, 0, 0, 22, 22, 22, 22, 8, 10, 22, 22},
    {0, 0, 0, 0, 23, 23, 23, 23, 8, 10, 23, 23},
    {0, 0, 0, 0, 24, 24, 24, 24, 8, 10, 24, 24},
    {0, 0, 0, 0, 25, 25, 25, 25, 8, 10, 25, 25},
    {0, 0, 0, 0, 26, 26, 26, 26, 8, 10, 26, 26},
    {0, 0, 0, 0, 27, 27, 27, 27, 8, 10, 27, 27},
    {0, 0, 0, 0, 28, 28, 28, 28, 8, 10, 28, 28},
    {0, 0, 0, 0, 29, 29, 29, 29, 8, 10, 29, 29},
    {0, 0, 0, 0, 30, 30, 30, 30, 8, 10, 30, 30},
    {0, 0, 0, 0, 31, 31, 31, 31, 8, 10, 31, 31},
    {0, 0, 0, 0, 32, 32, 32, 32, 8, 10, 32, 32},
    {0, 0, 0, 0, 33, 33, 33, 33, 8, 10, 33, 33},
    {0, 0, 0, 0, 34, 34, 34, 34, 8, 10, 34, 34},
    {0, 0, 0, 0, 35, 35, 35, 35, 8, 10, 35, 35},
    {0, 0, 0, 0, 36, 36, 36, 36, 8, 10, 36, 36},
    {0, 0, 0, 0, 37, 37, 37, 37, 8, 10, 37, 37},
    {0, 0, 0, 0, 38, 38, 38, 38, 8, 10, 38, 38},
    {0, 0, 0, 0, 39, 39, 39, 39, 8, 10, 39, 39},
    {0, 0, 0, 0, 40, 40, 40, 40, 40, 8, 40, 40},
    {0, 0, 0, 0, 41, 42, 42, 42, 42, 42, 8, 42},
    {0, 0, 0, 0, 1, 41, 43, 43, 43, 43, 43, 8},
    {0, 0, 0, 0, 0, 1, 44, 44, 44, 44, 44, 44},
    {0, 0, 0, 0, 0, 1, 41, 41, 45, 45, 45, 45},
    {0, 0, 0, 0, 0, 0, 1, 1, 41, 46, 46, 46},
    {0, 0, 0, 0, 0, 0, 0, 0, 1, 47, 47, 47},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static const uint8_t kBoxRight[BOX_H][BOX_CORNER] = {
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {4, 4, 4, 3, 1, 1, 0, 0, 0, 0, 0, 0},
    {5, 5, 5, 5, 3, 3, 1, 0, 0, 0, 0, 0},
    {6, 6, 6, 6, 6, 6, 1, 0, 0, 0, 0, 0},
    {8, 7, 7, 7, 7, 7, 3, 1, 0, 0, 0, 0},
    {10, 8, 9, 9, 9, 9, 9, 3, 0, 0, 0, 0},
    {11, 11, 8, 11, 11, 11, 11, 11, 0, 0, 0, 0},
    {12, 12, 12, 8, 12, 12, 12, 12, 0, 0, 0, 0},
    {13, 13, 13, 8, 13, 13, 13, 13, 0, 0, 0, 0},
    {14, 14, 14, 8, 14, 14, 14, 14, 0, 0, 0, 0},
    {15, 15, 15, 8, 15, 15, 15, 15, 0, 0, 0, 0},
    {16, 16, 16, 8, 16, 16, 16, 16, 0, 0, 0, 0},
    {17, 17, 17, 8, 17, 17, 17, 17, 0, 0, 0, 0},
    {18, 18, 18, 8, 18, 18, 18, 18, 0, 0, 0, 0},
    {19, 19, 19, 8, 19, 19, 19, 19, 0, 0, 0, 0},
    {20, 20, 20, 8, 20, 20, 20, 20, 0, 0, 0, 0},
    {21, 21, 21, 8, 21, 21, 21, 21, 0, 0, 0, 0},
    {22, 22, 22, 8, 22, 22, 22, 22, 0, 0, 0, 0},
    {23, 23, 23, 8, 23, 23, 23, 23, 0, 0, 0, 0},
    {24, 24, 24, 8, 24, 24, 24, 24, 0, 0, 0, 0},
    {25, 25, 25, 8, 25, 25, 25, 25, 0, 0, 0, 0},
    {26, 26, 26, 8, 26, 26, 26, 26, 0, 0, 0, 0},
    {27, 27, 27, 8, 27, 27, 27, 27, 0, 0, 0, 0},
    {28, 28, 28, 8, 28, 28, 28, 28, 0, 0, 0, 0},
    {29, 29, 29, 8, 29, 29, 29, 29, 0, 0, 0, 0},
    {30, 30, 30, 8, 30, 30, 30, 30, 0, 0, 0, 0},
    {31, 31, 31, 8, 31, 31, 31, 31, 0, 0, 0, 0},
    {32, 32, 32, 8, 32, 32, 32, 32, 0, 0, 0, 0},
    {33, 33, 33, 8, 33, 33, 33, 33, 0, 0, 0, 0},
    {34, 34, 34, 8, 34, 34, 34, 34, 0, 0, 0, 0},
    {35, 35, 35, 8, 35, 35, 35, 35, 0, 0, 0, 0},
    {36, 36, 36, 8, 36, 36, 36, 36, 0, 0, 0, 0},
    {37, 37, 37, 8, 37, 37, 37, 37, 0, 0, 0, 0},
    {38, 38, 38, 8, 38, 38, 38, 38, 0, 0, 0, 0},
    {39, 39, 39, 8, 39, 39, 39, 39, 0, 0, 0, 0},
    {40, 40, 8, 40, 40, 40, 40, 40, 0, 0, 0, 0},
    {42, 8, 42, 42, 42, 42, 42, 41, 0, 0, 0, 0},
    {8, 43, 43, 43, 43, 43, 41, 1, 0, 0, 0, 0},
    {44, 44, 44, 44, 44, 44, 1, 0, 0, 0, 0, 0},
    {45, 45, 45, 45, 41, 41, 1, 0, 0, 0, 0, 0},
    {46, 46, 46, 41, 1, 1, 0, 0, 0, 0, 0, 0},
    {47, 47, 47, 1, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
    {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0},
};
static const uint8_t kBoxMiddle[BOX_H] = {0, 0, 0, 2, 4, 5, 6, 8, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 42, 8, 44, 45, 46, 47, 0, 0, 0};

static C3D_Tex sBoxTex;
static bool sBoxReady, sBoxFailed;
static uint32_t sBoxTextHash;
static int sBoxArrowW, sBoxArrowY0, sBoxArrowH;

static uint32_t BoxRgba(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint32_t)r << 24 | (uint32_t)g << 16 | (uint32_t)b << 8 | 255u;
}

static uint32_t BoxPanelPixel(int x, int y)
{
    unsigned index = x < BOX_CORNER ? kBoxLeft[y][x] : x >= BOX_W - BOX_CORNER ? kBoxRight[y][x - (BOX_W - BOX_CORNER)]
                   : kBoxMiddle[y];

    if (y >= 6 && y <= 9 && x >= BOX_TAB_X0 + (y >= 8) && x < BOX_TAB_X1 - (y >= 8))
        return y == 6 ? BoxRgba(255, 128, 104) : y == 9 ? BoxRgba(160, 32, 30) : BoxRgba(232, 64, 48);
    return BoxRgba(kBoxRgb[index][0], kBoxRgb[index][1], kBoxRgb[index][2]);
}

static bool BoxInit(void)
{
    if (sBoxReady) return true;
    if (sBoxFailed || !C3D_TexInit(&sBoxTex, BOX_TEX_W, BOX_TEX_H, GPU_RGBA8)) { sBoxFailed = true; return false; }
    C3D_TexSetFilter(&sBoxTex, GPU_NEAREST, GPU_NEAREST);
    uint32_t *texels = (uint32_t *)sBoxTex.data;
    memset(texels, 0, BOX_TEX_W * BOX_TEX_H * 4);
    for (int y = 0; y < BOX_H; ++y)
        for (int x = 0; x < BOX_W; ++x)
            texels[CtrVideo_Texel((unsigned)x, (unsigned)y, BOX_TEX_W)] = BoxPanelPixel(x, y);
    C3D_TexFlush(&sBoxTex);
    sBoxTextHash = 0;
    sBoxReady = true;
    return true;
}

static unsigned BoxEntry(unsigned map, unsigned size, unsigned first, unsigned scrollY, unsigned rows, unsigned tx,
                         unsigned ty)
{
    return MapEntry(map, size, first + tx, ((BATTLE_BAND_TOP + ty * 8 + scrollY) >> 3) & (rows - 1));
}

/* Whether the six rows hold FireRed's frame: the panel replaces it only then. */
static bool BoxFramed(unsigned map, unsigned size, unsigned first, unsigned scrollY, unsigned rows)
{
    for (unsigned ty = 0; ty < 6; ++ty)
    {
        unsigned tile = BoxEntry(map, size, first, scrollY, rows, 0, ty) & 1023;
        if (tile && tile < BOX_FRAME_TILES) return true;
    }
    return false;
}

static uint32_t BoxColour(unsigned bank, unsigned c)
{
    uint16_t rgb = sMemory.palette[bank * 16 + c];

    if (bank == 0 && c == 1) return BoxRgba(250, 250, 252);
    if (bank == 0 && c == 6) return BoxRgba(40, 46, 78);
    return BoxRgba((uint8_t)((rgb & 31) * 255 / 31), (uint8_t)(((rgb >> 5) & 31) * 255 / 31),
                   (uint8_t)(((rgb >> 10) & 31) * 255 / 31));
}

/* The windows' pixels of the six rows into the texture (only when they changed), the arrow's apart. */
static void BoxText(unsigned map, unsigned size, unsigned chars, unsigned first, unsigned scrollY, unsigned rows)
{
    uint32_t hash = 2166136261u;
    for (unsigned ty = 0; ty < 6; ++ty)
        for (unsigned tx = 0; tx < 30; ++tx)
        {
            unsigned entry = BoxEntry(map, size, first, scrollY, rows, tx, ty), address = chars + (entry & 1023) * 32;
            hash = (hash ^ entry) * 16777619u;
            if ((entry & 1023) >= BOX_FRAME_TILES && address + 32 <= 0x10000)
                for (unsigned i = 0; i < 32; ++i)
                    hash = (hash ^ sMemory.vram[address + i]) * 16777619u;
        }
    for (unsigned i = 0; i < 16; ++i) hash = (hash ^ sMemory.palette[i]) * 16777619u;
    if (hash == sBoxTextHash) return;
    sBoxTextHash = hash;

    uint32_t *texels = (uint32_t *)sBoxTex.data;
    int arrowX0 = 240, arrowX1 = -1, arrowY0 = BOX_H, arrowY1 = -1;
    for (unsigned y = BOX_TEXT_ROW; y < BOX_TEX_H; ++y)
        for (unsigned x = 0; x < 256; ++x)
            texels[CtrVideo_Texel(x, y, BOX_TEX_W)] = 0;
    /* First the arrow's extent, then the pixels: the arrow's go to its own row, from its top left. */
    for (int pass = 0; pass < 2; ++pass)
        for (unsigned ty = 0; ty < 6; ++ty)
            for (unsigned tx = 0; tx < 30; ++tx)
            {
                unsigned entry = BoxEntry(map, size, first, scrollY, rows, tx, ty);
                unsigned tile = entry & 1023, bank = entry >> 12, address = chars + tile * 32;
                if (tile < BOX_FRAME_TILES || address + 32 > 0x10000) continue;
                for (unsigned py = 0; py < 8; ++py)
                    for (unsigned px = 0; px < 8; ++px)
                    {
                        unsigned sx = (entry & 1024) ? 7 - px : px, sy = (entry & 2048) ? 7 - py : py;
                        unsigned c = (sMemory.vram[address + sy * 4 + sx / 2] >> ((sx & 1) * 4)) & 15;
                        int x = (int)(tx * 8 + px), y = (int)(ty * 8 + py);
                        bool arrow = bank == 0 && (c == 2 || c == 4 || c == 9);

                        if (!c || (bank == 0 && c == 15)) continue;
                        if (pass == 0)
                        {
                            if (!arrow) continue;
                            if (x < arrowX0) arrowX0 = x;
                            if (x > arrowX1) arrowX1 = x;
                            if (y < arrowY0) arrowY0 = y;
                            if (y > arrowY1) arrowY1 = y;
                        }
                        else if (arrow)
                        {
                            if (x - arrowX0 < 16 && y - arrowY0 < 16)
                                texels[CtrVideo_Texel((unsigned)(x - arrowX0), (unsigned)(BOX_ARROW_ROW + y - arrowY0),
                                                      BOX_TEX_W)] =
                                    c == 4 ? BoxRgba(255, 128, 104) : c == 9 ? BoxRgba(160, 32, 30) : BoxRgba(232, 64, 48);
                        }
                        else
                            texels[CtrVideo_Texel((unsigned)x, (unsigned)(BOX_TEXT_ROW + y), BOX_TEX_W)] =
                                BoxColour(bank, c);
                    }
            }
    sBoxArrowW = arrowX1 >= arrowX0 ? (arrowX1 - arrowX0 < 16 ? arrowX1 - arrowX0 + 1 : 16) : 0;
    sBoxArrowH = arrowY1 >= arrowY0 ? (arrowY1 - arrowY0 < 16 ? arrowY1 - arrowY0 + 1 : 16) : 0;
    sBoxArrowY0 = arrowY0;
    C3D_TexFlush(&sBoxTex);
}

/* The box's fade: FireRed fades its palette; the panel follows its white (to black) and its background (to white). */
static void BoxTint(C2D_ImageTint *tint)
{
    unsigned white = sMemory.palette[1], back = sMemory.palette[15];
    unsigned lw = (white & 31) + ((white >> 5) & 31) + ((white >> 10) & 31);
    unsigned lb = (back & 31) + ((back >> 5) & 31) + ((back >> 10) & 31);

    *tint = sTint;
    if (lw < 93)
        C2D_PlainImageTint(tint, C2D_Color32(0, 0, 0, 255), 1.0f - lw / 93.0f);
    else if (lb > 28)
        C2D_PlainImageTint(tint, C2D_Color32(255, 255, 255, 255), (lb - 28) / 65.0f);
}

static Tex3DS_SubTexture BoxSub(int x, int y, int w, int h)
{
    return (Tex3DS_SubTexture){(u16)w, (u16)h, x / (float)BOX_TEX_W, 1.0f - y / (float)BOX_TEX_H,
                               (x + w) / (float)BOX_TEX_W, 1.0f - (y + h) / (float)BOX_TEX_H};
}

static void BoxDraw(Tex3DS_SubTexture sub, float x, float y, const C2D_ImageTint *tint)
{
    if (!C2D_DrawImageAt((C2D_Image){&sBoxTex, &sub}, x, y, 0, tint, 1, 1))
        Error(3, "Citro2D geometry submission failed");
}

/* The panel and the windows over it, in place of the frame. False when there is no frame to replace: the GBA's band
 * is drawn then. */
static bool DrawBattleBox(unsigned map, unsigned size, unsigned chars, unsigned first, unsigned scrollY, unsigned rows)
{
    if (!BoxFramed(map, size, first, scrollY, rows) || !BoxInit()) return false;
    BoxText(map, size, chars, first, scrollY, rows);

    C2D_ImageTint tint;
    float left = VIEW_LEFT + CTR_VIEW_X + sLayerShift, top = BATTLE_BAND_TOP + CTR_VIEW_Y;

    int x0 = VIEW_LEFT, x1 = VIEW_RIGHT, y0 = BATTLE_BAND_TOP, y1 = 160;

    if (!ClipCell(&x0, &x1, &y0, &y1)) return true;
    BoxTint(&tint);
    C2D_Flush();
    Scissor(x0, y0, x1, y1);
    ViewBase();
    BoxDraw(BoxSub(0, 0, BOX_W, BOX_H), left, top, &tint);
    BoxDraw(BoxSub(0, BOX_TEXT_ROW, 240, BOX_LINE2_ROW), left + BOX_TEXT_X, top + BOX_TEXT_Y, &tint);
    BoxDraw(BoxSub(0, BOX_TEXT_ROW + BOX_LINE2_ROW, 240, BOX_H - BOX_LINE2_ROW), left + BOX_TEXT_X,
            top + BOX_TEXT_Y + BOX_LINE2_ROW - BOX_LINE2_UP, &tint);
    if (sBoxArrowW && sBoxArrowH)
        BoxDraw(BoxSub(0, BOX_ARROW_ROW, sBoxArrowW, sBoxArrowH), left + BOX_ARROW_X,
                top + BOX_TEXT_Y + sBoxArrowY0 - (sBoxArrowY0 >= BOX_LINE2_ROW ? BOX_LINE2_UP : 0), &tint);
    C2D_Flush();
    RestoreScissor();
    return true;
}
