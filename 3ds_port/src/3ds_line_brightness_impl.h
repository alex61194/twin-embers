/* Per-line brightness (the title's BLDY fades), part of 3ds_video.c (included there, in this place; kept apart
 * for the 256 KiB source limit). Uses the compositor's own state; not a header of its own. */

/*
 * Only brightness effects need the title's BLDY scanline values. Adjacent
 * lines with equal values share a band; each band draws every layer with its
 * value. The title's fades give up to 160 bands - a full scene walk each - so
 * on the stencil path each distinct value is drawn once, over exactly its
 * bands. Without the stencil the bands draw one by one, as before.
 */
static unsigned sBrightnessKeys[160];
static unsigned sBrightnessKeyCount;

static unsigned LineBrightnessAt(int y)
{
    if (NativeIntroTitle()) y = y * 160 / 240;
    unsigned line = y < 0 ? 0 : (unsigned)y >= sLineBrightnessCount
                  ? sLineBrightnessCount - 1 : (unsigned)y;
    return sLineBrightness[line];
}

static void BrightnessDrawValue(int x0, int y0, int x1, int y1, unsigned value)
{
    C2D_Flush();
    Scissor(x0, y0, x1, y1);
    sScissored = true;
    sClipX0 = x0; sClipX1 = x1;
    sClipY0 = y0; sClipY1 = y1;
    sBrightnessOverride = true;
    sBrightnessValue = value;
    Layers(63);
}

static void BrightnessDrawKey(int x0, int y0, int x1, int y1, unsigned key)
{
    BrightnessDrawValue(x0, y0, x1, y1, sBrightnessKeys[key]);
}

static void ComposeLineBrightness(void)
{
    int left = VIEW_LEFT, right = VIEW_RIGHT, top = VIEW_TOP, bottom = VIEW_BOTTOM;
    bool stencil = StencilTarget();
    /* The rectangles of this target's set (sWindowSets); the OBJ window's partitions are made again after it. */
    sWindowCacheValid = false;
    sWin = &sWindowSets[sDrawTarget && sDrawTarget == sScene ? 0 : 1];
    sWin->count = 0;
    sWin->serial = ++sWindowSerials;
    sBrightnessKeyCount = 0;
    for (int y = top; stencil && y < bottom; )
    {
        unsigned value = LineBrightnessAt(y);
        int end = y + 1;
        while (end < bottom && LineBrightnessAt(end) == value) ++end;
        unsigned key = 0;
        while (key < sBrightnessKeyCount && sBrightnessKeys[key] != value) ++key;
        if (key == sBrightnessKeyCount)
        {
            if (sBrightnessKeyCount == 160) { stencil = false; break; }
            sBrightnessKeys[sBrightnessKeyCount++] = value;
        }
        WindowCollectRect(left, y, right, end, key);
        y = end;
    }
    stencil = stencil && sWin->count <= WINDOW_RECTS
           && sStats.tiles + sWin->count < MAX_DRAWS / 2 && StencilReady();
    if (stencil)
    {
        ComposeStencil(BrightnessDrawKey);
    }
    else
    {
        for (int y = top; y < bottom; )
        {
            unsigned value = LineBrightnessAt(y);
            int end = y + 1;
            while (end < bottom && LineBrightnessAt(end) == value) ++end;
            BrightnessDrawValue(left, y, right, end, value);
            y = end;
        }
    }
    C2D_Flush();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    sScissored = sBrightnessOverride = false;
    ClipToView();
}
