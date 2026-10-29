/* A battle's OBJ window from a small surface of its own, part of 3ds_video.c (included there, in this place; kept
 * apart for the 256 KiB source limit). Uses the compositor's own state; not a header of its own. */

/*
 * The OBJ window of a battle (ZallaxDev's pokeemerald-3Ds-dualscreen ObjWindowCompose, MIT, adapted): with no
 * window 0 or 1 over a pixel where a window sprite is opaque, the layers shown are WINOUT's upper byte instead of its
 * lower. A battle keeps it on with no such sprite, so it changes nothing, until a stat change
 * (StatsChangeAnimation_Step1): an invisible copy of the Pokemon in window mode, and BG1 - the rising or falling
 * pattern - shown only through it, blended over the Pokemon.
 *
 * ComposeObjWindow reproduces that per pixel with a stencil, but walks the whole scene once per mask: a battle twice
 * over, near 30 fps for the second a stat change lasts. Here the layers the window adds are composed once a frame,
 * at 1:1, into a small surface of their own over the window's sprites as a mask, and that surface is drawn over each
 * rectangle outside windows 0 and 1, after its own layers (ObjWindowDraw). The added layers go on top of that
 * rectangle - the stat pattern is - and a window taking layers away is not reproduced: such a frame is left to
 * ComposeObjWindow.
 */
#define OBJWIN_SIZE 128
static C3D_Tex sObjWinTex;
static C3D_RenderTarget *sObjWinTarget;
static bool sObjWinWanted, sObjWinFailed;
static uint32_t sObjWinUsedFrame, sObjWinFailFrame;
/* This frame's: its box in GBA pixels and the layers it adds. */
static struct { bool ready; int x0, y0, x1, y1; unsigned layers; } sObjWin;

static void ObjWindowRelease(void)
{
    if (sObjWinTarget) C3D_RenderTargetDelete(sObjWinTarget);
    if (sObjWinTex.data) C3D_TexDelete(&sObjWinTex);
    sObjWinTarget = NULL;
    memset(&sObjWinTex, 0, sizeof(sObjWinTex));
}

/* Outside the frame, where deleting a render target may wait for the GPU: the surface while a window wants it. */
static void ObjWindowPrepare(void)
{
    bool wanted = sObjWinWanted;

    sObjWinWanted = false;
    if (wanted) sObjWinUsedFrame = sStats.frames;
    else if (sObjWinTarget && sStats.frames - sObjWinUsedFrame > LAYER_IDLE_FRAMES)
        ObjWindowRelease();
    if (!wanted || sObjWinTarget || (sObjWinFailed && sStats.frames - sObjWinFailFrame < LAYER_RETRY_FRAMES))
        return;
    /* RGBA8: the mask's count needs more than one bit of alpha. 64 KB of VRAM; a Pokemon's 64x64 fits half. */
    for (unsigned size = OBJWIN_SIZE; size >= OBJWIN_SIZE / 2 && !sObjWinTarget; size /= 2)
    {
        if (C3D_TexInitVRAM(&sObjWinTex, size, size, GPU_RGBA8)
            && (sObjWinTarget = C3D_RenderTargetCreateFromTex(&sObjWinTex, GPU_TEXFACE_2D, 0, -1)))
            break;
        ObjWindowRelease();
    }
    sObjWinFailed = sObjWinTarget == NULL;
    if (sObjWinTarget)
    {
        /* Drawn at the scene's whole-number zoom: every texel a block. */
        C3D_TexSetFilter(&sObjWinTex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&sObjWinTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
    }
    else
        sObjWinFailFrame = sStats.frames;
    CtrLog_Write(sObjWinTarget ? CTR_LOG_VIDEO : CTR_LOG_ERROR, "VIDEO: OBJ window surface %s, %u texels (VRAM free=%lu)",
                 sObjWinTarget ? "made" : "not made", sObjWinTarget ? (unsigned)sObjWinTex.width : 0u,
                 (unsigned long)vramSpaceFree());
}

/* The box of the window's sprites, in GBA pixels; false if there is none. */
static bool ObjWindowBounds(int *x0, int *y0, int *x1, int *y1)
{
    static const uint8_t dimensions[3][4][2] = {
        {{8,8},{16,16},{32,32},{64,64}},
        {{16,8},{32,8},{32,16},{64,32}},
        {{8,16},{8,32},{16,32},{32,64}}
    };
    bool any = false;

    for (unsigned i = 0; i < 128; ++i)
    {
        unsigned attr0 = sMemory.oam[i * 4], attr1 = sMemory.oam[i * 4 + 1];
        unsigned shape = attr0 >> 14;
        bool affine = (attr0 & 0x100) != 0, twice = affine && (attr0 & 0x200);
        int x, y, w, h;

        if (((attr0 >> 10) & 3) != 2 || (!affine && (attr0 & 0x200)) || shape == 3) continue;
        w = dimensions[shape][attr1 >> 14][0] << twice;
        h = dimensions[shape][attr1 >> 14][1] << twice;
        /* The GBA's reading, as DrawObjects has it for a battle. */
        x = (int)(attr1 & 511);
        y = (int)(attr0 & 255);
        if (x + w > 512) x -= 512;
        if (y + h > 256) y -= 256;
        /* A battler's copy with a HeartGold picture (a stat change's arrows) masks with that 80x80 picture. */
        struct CtrGen4Obj gen4;
        if (sObjOverride && !twice && w == 64 && h == 64 && !(attr0 & 0x2000)
            && sObjOverride(sMemory.oam[i * 4 + 2], &gen4))
        {
            x += gen4.offsetX;
            y += gen4.offsetY;
            w = h = CTR_GEN4_SIZE;
        }
        if (!any || x < *x0) *x0 = x;
        if (!any || y < *y0) *y0 = y;
        if (!any || x + w > *x1) *x1 = x + w;
        if (!any || y + h > *y1) *y1 = y + h;
        any = true;
    }
    return any;
}

/*
 * In the frame, before anything is composed (CtrVideo_Present): the layers the OBJ window adds, over its sprites as
 * a mask, into its surface. A battle only, the one screen that uses it this way.
 */
static void ObjWindowCompose(void)
{
    unsigned display = Reg(0), out = Reg(0x4a) & 63, in = (Reg(0x4a) >> 8) & 63;
    /* Not the battle's text box (BG0, and a box layer above it): composed apart (DrawBattleTextLayer). */
    unsigned layers = in & ~out & (display >> 8) & 14u & ~BattleBoxLayers();
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0, size;

    sObjWin.ready = false;
    if (!sBattle || (display & 0x9000) != 0x9000 || (display & 7) > 2 || !layers) return;
    /* A window that takes layers away is left to ComposeObjWindow's stencil. */
    if (out & 31 & ~in) return;
    if (!ObjWindowBounds(&x0, &y0, &x1, &y1)) return;
    if (x0 < VIEW_LEFT) x0 = VIEW_LEFT;
    if (y0 < VIEW_TOP) y0 = VIEW_TOP;
    if (x1 > VIEW_RIGHT) x1 = VIEW_RIGHT;
    if (y1 > VIEW_BOTTOM) y1 = VIEW_BOTTOM;
    if (x0 >= x1 || y0 >= y1) return;
    sObjWinWanted = true;
    if (!sObjWinTarget) return;
    size = (int)sObjWinTex.width;
    /* Larger than the surface (two Pokemon at once, a link battle's letters): the stencil, whole. */
    if (x1 - x0 > size || y1 - y0 > size) return;

    float zoom = sZoom, offX = sOffX, offY = sOffY, origin = sLayerOrigin, parallax = sParallax, shift = sLayerShift;
    int viewX = sViewX, viewY = sViewY, targetW = sTargetW, targetH = sTargetH, surfaceH = sSurfaceH;
    unsigned exclude = sLayerExclude, priorities = sPriorityMask;

    /* GBA (x0, y0) at the surface's corner, at 1:1, flat. */
    sZoom = 1.0f;
    sOffX = sOffY = 0.0f;
    sViewX = -x0;
    sViewY = -y0;
    sTargetW = x1 - x0;
    sTargetH = y1 - y0;
    sSurfaceH = size;
    sLayerOrigin = sParallax = sLayerShift = 0.0f;
    sLayerExclude = 0;
    sPriorityMask = 15;
    ClipToView();

    C2D_TargetClear(sObjWinTarget, 0);
    BlendForget();
    C2D_SceneBegin(sObjWinTarget);
    C2D_ViewReset();
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    sScissored = false;
    sObjWinPass = OBJWIN_MASK;
    for (int priority = 3; priority >= 0; --priority)
        DrawObjects((unsigned)priority, false);
    C2D_Flush();
    BlendForget();
    sObjWinPass = OBJWIN_LAYERS;
    /* Without the effect bit: the blend is the surface's (ObjWindowDraw). */
    Layers(layers);
    C2D_Flush();
    sObjWinPass = OBJWIN_NONE;
    BlendForget();
    C3D_DepthTest(false, GPU_ALWAYS, GPU_WRITE_COLOR);
    C3D_SetScissor(GPU_SCISSOR_DISABLE, 0, 0, 0, 0);
    sScissored = false;
    /* Rendering to a texture and then sampling it needs a command split. */
    C3D_FrameSplit(0);

    sZoom = zoom;
    sOffX = offX;
    sOffY = offY;
    sViewX = viewX;
    sViewY = viewY;
    sTargetW = targetW;
    sTargetH = targetH;
    sSurfaceH = surfaceH;
    sLayerOrigin = origin;
    sParallax = parallax;
    sLayerShift = shift;
    sLayerExclude = exclude;
    sPriorityMask = priorities;
    ClipToView();

    sObjWin.ready = true;
    sObjWin.x0 = x0;
    sObjWin.y0 = y0;
    sObjWin.x1 = x1;
    sObjWin.y1 = y1;
    sObjWin.layers = layers;
}

/*
 * The OBJ window's surface over [x0, x1) x [y0, y1), a rectangle outside windows 0 and 1, already scissored: drawn
 * as its frontmost layer would be, with that layer's blend and depth.
 */
static void ObjWindowDraw(int x0, int y0, int x1, int y1)
{
    unsigned layers = sObjWin.layers & ~sLayerExclude, bg = 0, priority = 4;
    float shift = sLayerShift;

    if (x0 < sObjWin.x0) x0 = sObjWin.x0;
    if (y0 < sObjWin.y0) y0 = sObjWin.y0;
    if (x1 > sObjWin.x1) x1 = sObjWin.x1;
    if (y1 > sObjWin.y1) y1 = sObjWin.y1;
    if (x0 >= x1 || y0 >= y1) return;
    for (unsigned b = 0; b < 4; ++b)
        if ((layers & (1u << b)) && (Reg(8 + b * 2) & 3) < priority)
        {
            priority = Reg(8 + b * 2) & 3;
            bg = b;
        }
    if (priority > 3 || !(sPriorityMask & (1u << priority))) return;

    int w = sObjWin.x1 - sObjWin.x0, h = sObjWin.y1 - sObjWin.y0;
    float size = (float)sObjWinTex.width;
    const Tex3DS_SubTexture cut = {(u16)w, (u16)h, 0.0f, 1.0f, w / size, 1.0f - h / size};

    Blend(bg, ((Reg(0x4a) >> 8) & 32) != 0, false);
    /* Only where the window's mask and a layer's pixel both are. */
    C3D_AlphaTest(true, GPU_GREATER, 0xc0);
    ViewBase();
    sLayerShift = (sLayerOrigin + sParallax * (3 - (int)priority)) / sShiftZoom;
    C2D_DrawImageAt((C2D_Image){&sObjWinTex, &cut}, sObjWin.x0 + CTR_VIEW_X + sLayerShift,
                    sObjWin.y0 + CTR_VIEW_Y, 0, &sTint, 1, 1);
    C2D_Flush();
    C3D_AlphaTest(true, GPU_GREATER, 0);
    BlendForget();
    sLayerShift = shift;
}
