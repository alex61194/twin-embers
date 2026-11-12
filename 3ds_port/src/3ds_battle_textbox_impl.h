/* The battle's message box in the port's design ("Llama", proposal A), included by 3ds_video.c, whose state and helpers
 * it uses.
 *
 * FireRed's text box is the last six tile rows of BG0: a frame of its own tiles (graphics/battle_interface/textbox.bin,
 * below BOX_FRAME_TILES) and, over it, the windows the game prints into. Here the frame is not drawn: a slate-blue panel
 * takes the bottom 48 lines of the screen, full width (a gradient face, FireRed's light-blue fillet round it, a flame
 * line fading out along its top edge), and the windows' tiles are drawn over it at 1:1, moved by BOX_TEXT_X and
 * BOX_TEXT_Y, with the message window's background (colour 15) left out and its text in white over a dark shadow.
 * FireRed's prompt arrow (colours 2, 4 and 9 of the box's palette) is drawn in the design's reds at the panel's right
 * end, at the height the game gives it, so it keeps its bounce. The panel fades with the box's palette.
 * The panel is computed, pixel by pixel, when the texture is made (BoxPanelPixel): there is no art for it.
 */
#define BOX_W 400
#define BOX_H 48
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
/* The panel's face: a rounded rectangle (x 3 to 396, y 3 to 44, corners of radius 6) on a darker ground. */
#define BOX_FACE_X 3
#define BOX_FACE_Y 3
#define BOX_FACE_R 6
#define BOX_FLAME_X 10                  /* the flame line: x 10 to 159, rows 1 to 3 */
#define BOX_FLAME_W 150

static C3D_Tex sBoxTex;
static bool sBoxReady, sBoxFailed;
static uint32_t sBoxTextHash;
static int sBoxArrowW, sBoxArrowY0, sBoxArrowH;

static uint32_t BoxRgba(uint8_t r, uint8_t g, uint8_t b)
{
    return (uint32_t)r << 24 | (uint32_t)g << 16 | (uint32_t)b << 8 | 255u;
}

static float BoxMix(float a, float b, float t)
{
    return a + (b - a) * t;
}

/* Whether (x, y) is inside the face, its edge moved `grow` pixels out (negative: in): a rounded rectangle, its corners
 * tested at the pixel centres. */
static bool BoxFace(int x, int y, int grow)
{
    const int x0 = BOX_FACE_X - grow, x1 = BOX_W - BOX_FACE_X + grow;
    const int y0 = BOX_FACE_Y - grow, y1 = BOX_H - BOX_FACE_Y + grow;
    const int r = BOX_FACE_R + grow > 0 ? BOX_FACE_R + grow : 0;

    if (x < x0 || x >= x1 || y < y0 || y >= y1) return false;
    if ((x < x0 + r || x >= x1 - r) && (y < y0 + r || y >= y1 - r))
    {
        float dx = x + 0.5f - (x < x0 + r ? x0 + r : x1 - r), dy = y + 0.5f - (y < y0 + r ? y0 + r : y1 - r);
        return dx * dx + dy * dy <= (float)(r * r);
    }
    return true;
}

static uint32_t BoxPanelPixel(int x, int y)
{
    float r = 20, g = 24, b = 44;                                   /* the ground */

    if (BoxFace(x, y, 0))
    {
        float t = (y - BOX_FACE_Y) / (float)(BOX_H - 2 * BOX_FACE_Y - 1);
        if (t < 0.55f)
        {
            t /= 0.55f;
            r = BoxMix(70, 44, t), g = BoxMix(82, 52, t), b = BoxMix(126, 88, t);
        }
        else
        {
            t = (t - 0.55f) / 0.45f;
            r = BoxMix(44, 37, t), g = BoxMix(52, 44, t), b = BoxMix(88, 76, t);
        }
        if (y < BOX_FACE_Y + 3) r = BoxMix(r, 255, 0.07f), g = BoxMix(g, 255, 0.07f), b = BoxMix(b, 255, 0.07f);
        if (!BoxFace(x, y, -1))                                      /* FireRed's light-blue fillet */
            r = 122, g = 180, b = 216;
        else if (!BoxFace(x, y, -2))                                 /* and its faint second line */
            r = BoxMix(r, 200, 0.35f), g = BoxMix(g, 228, 0.35f), b = BoxMix(b, 240, 0.35f);
    }
    else if (BoxFace(x, y, 1))
        r = 10, g = 12, b = 24;                                      /* a dark line round the face */

    if (y >= 1 && y <= 3 && x >= BOX_FLAME_X && x < BOX_FLAME_X + BOX_FLAME_W && !(x == BOX_FLAME_X && y != 2))
    {
        /* The flame line: amber to orange to red, fading out along its length. */
        float t = (x - BOX_FLAME_X) / (float)BOX_FLAME_W, a = t < 0.45f ? 1.0f : 1.0f - (t - 0.45f) / 0.55f;
        float fr = t < 0.45f ? BoxMix(255, 248, t / 0.45f) : BoxMix(248, 216, (t - 0.45f) / 0.55f);
        float fg = t < 0.45f ? BoxMix(176, 100, t / 0.45f) : BoxMix(100, 48, (t - 0.45f) / 0.55f);
        float fb = t < 0.45f ? BoxMix(72, 42, t / 0.45f) : BoxMix(42, 30, (t - 0.45f) / 0.55f);
        r = BoxMix(r, fr, a), g = BoxMix(g, fg, a), b = BoxMix(b, fb, a);
    }
    return BoxRgba((uint8_t)(r + 0.5f), (uint8_t)(g + 0.5f), (uint8_t)(b + 0.5f));
}

static bool BoxInit(void)
{
    if (sBoxReady) return true;
    if (sBoxFailed || !C3D_TexInit(&sBoxTex, BOX_TEX_W, BOX_TEX_H, GPU_RGBA8)) { sBoxFailed = true; return false; }
    C3D_TexSetFilter(&sBoxTex, GPU_NEAREST, GPU_NEAREST);
    uint32_t *texels = (uint32_t *)sBoxTex.data;
    memset(texels, 0, BOX_TEX_W * BOX_TEX_H * 4);
    /* From x = BOX_FACE_X + BOX_FACE_R to BOX_W - that, outside the flame's rows (1 to 3), a row is one colour (the face's corners,
     * fillet and edge all end by then; BoxFace's corner tests are the only x-dependent part): one pixel a row, copied. */
    for (int y = 0; y < BOX_H; ++y)
    {
        const bool flat = y < 1 || y > 3;
        const int from = BOX_FACE_X + BOX_FACE_R, to = BOX_W - BOX_FACE_X - BOX_FACE_R;
        const uint32_t middle = flat ? BoxPanelPixel(from, y) : 0;

        for (int x = 0; x < BOX_W; ++x)
            texels[CtrVideo_Texel((unsigned)x, (unsigned)y, BOX_TEX_W)] = flat && x >= from && x < to ? middle : BoxPanelPixel(x, y);
    }
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
    if (bank == 0 && c == 6) return BoxRgba(16, 20, 42);
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
                                    c == 4 ? BoxRgba(255, 120, 90) : c == 9 ? BoxRgba(88, 18, 14) : BoxRgba(232, 56, 32);
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
