/* The compositor's side of the port's healthboxes (included by 3ds_video.c, whose state and helpers it uses). */

/*
 * The port's own healthboxes (3ds_battle_hud.c): a box replaces its healthbox's main sprite, where that sprite is
 * drawn (so in its place among the sprites), 1:1 on the screen at a whole pixel; the healthbox's other sprites are
 * left out. Each battler's box is a 256x64 band of sHudTex, written again when its picture changes. The box fades
 * with the healthbox's palette: against the game's unfaded copy, the palette's brightest colour tells a fade to
 * black and its darkest a fade to white.
 */
#define HUD_TEX 256
#define HUD_BAND (HUD_TEX / CTR_HUD_BATTLERS)
_Static_assert(CTR_HUD_IMAGE_W <= HUD_TEX && CTR_HUD_IMAGE_H <= HUD_BAND, "a box fits its band");
static unsigned (*sHudMatch)(unsigned tile, unsigned *battler);
static const CtrHudImage *(*sHudImage)(unsigned battler);
static C3D_Tex sHudTex;
static bool sHudReady, sHudFailed;
static uint32_t sHudVersion[CTR_HUD_BATTLERS];

void CtrVideo_SetBattleHud(unsigned (*match)(unsigned tile, unsigned *battler),
                           const CtrHudImage *(*image)(unsigned battler))
{
    sHudMatch = match;
    sHudImage = image;
}

static unsigned HudLuma(uint16_t c) { return (c & 31) + ((c >> 5) & 31) + ((c >> 10) & 31); }

static void HudTint(const CtrHudImage *image, unsigned palette, C2D_ImageTint *tint)
{
    *tint = sTint;
    if (!image->unfaded || !sMemory.palette) return;
    const uint16_t *unfaded = image->unfaded + 256 + palette * 16, *shown = sMemory.palette + 256 + palette * 16;
    unsigned bright = 1, dark = 1;
    for (unsigned i = 2; i < 16; ++i)
    {
        if (HudLuma(unfaded[i]) > HudLuma(unfaded[bright])) bright = i;
        if (HudLuma(unfaded[i]) < HudLuma(unfaded[dark])) dark = i;
    }
    unsigned ub = HudLuma(unfaded[bright]), sb = HudLuma(shown[bright]);
    unsigned ud = HudLuma(unfaded[dark]), sd = HudLuma(shown[dark]);
    if (ub && sb < ub)
        C2D_PlainImageTint(tint, C2D_Color32(0, 0, 0, 255), 1.0f - (float)sb / (float)ub);
    else if (ud < 93 && sd > ud)
        C2D_PlainImageTint(tint, C2D_Color32(255, 255, 255, 255), (float)(sd - ud) / (float)(93 - ud));
}

/* A field NPC's tint. A silhouette (a wild Pokemon of the overworld with WILD DISPLAY on SILHOUETTE, and in the Pokemon Tower without the
 * Silph Scope) is the picture's shape in a flat dark slate, dimmed by the game's fades the way HudTint dims a picture. The shape comes from
 * the picture's alpha, so one combiner pass does it with no extra picture. */
static void HudNpcTint(const CtrHudImage *image, unsigned palette, bool silhouette, C2D_ImageTint *tint)
{
    float dim = 1.0f;

    if (!silhouette)
    {
        HudTint(image, palette, tint);
        return;
    }
    if (image->unfaded && sMemory.palette)
    {
        const uint16_t *unfaded = image->unfaded + 256 + palette * 16, *shown = sMemory.palette + 256 + palette * 16;
        unsigned ub = 0, sb = 0;

        for (unsigned i = 1; i < 16; ++i)
            if (HudLuma(unfaded[i]) > ub)
            {
                ub = HudLuma(unfaded[i]);
                sb = HudLuma(shown[i]);
            }
        if (ub && sb < ub)
            dim = (float)sb / (float)ub;
    }
    *tint = sTint;
    C2D_PlainImageTint(tint, C2D_Color32((uint8_t)(16 * dim), (uint8_t)(16 * dim), (uint8_t)(24 * dim), 255), 1.0f);
}

static void DrawHud(unsigned battler, int x, int y, unsigned palette)
{
    const CtrHudImage *image = sHudImage ? sHudImage(battler) : NULL;
    if (!image || battler >= CTR_HUD_BATTLERS || image->width > HUD_TEX || image->height > HUD_BAND) return;
    if (!sHudReady)
    {
        if (sHudFailed) return;
        if (!C3D_TexInit(&sHudTex, HUD_TEX, HUD_TEX, GPU_RGBA8)) { sHudFailed = true; return; }
        C3D_TexSetFilter(&sHudTex, GPU_NEAREST, GPU_NEAREST);
        memset(sHudTex.data, 0, HUD_TEX * HUD_TEX * 4);
        memset(sHudVersion, 0, sizeof(sHudVersion));
        sHudReady = true;
    }
    unsigned top = battler * HUD_BAND;
    if (sHudVersion[battler] != image->version)
    {
        uint32_t *texels = (uint32_t *)sHudTex.data;
        /* Only what is shown (the sub-texture is width x height, sampled with NEAREST): the rest of the band is never read, so it is
         * not written (a band is 256x64 texels, a box 168x60 at most). A smaller picture over an older one leaves its old texels
         * outside the sub-texture, where nothing looks. */
        for (unsigned ty = 0; ty < image->height; ++ty)
            for (unsigned tx = 0; tx < image->width; ++tx)
                texels[CtrVideo_Texel(tx, top + ty, HUD_TEX)] = image->pixels[ty * CTR_HUD_IMAGE_W + tx];
        C3D_TexFlush(&sHudTex);
        sHudVersion[battler] = image->version;
    }
    Tex3DS_SubTexture sub = {image->width, image->height, 0.0f, 1.0f - top / (float)HUD_TEX,
                             image->width / (float)HUD_TEX, 1.0f - (top + image->height) / (float)HUD_TEX};
    C2D_ImageTint tint;
    HudTint(image, palette, &tint);
    C2D_ViewReset();
    if (!C2D_DrawImageAt((C2D_Image){&sHudTex, &sub},
                         roundf((x + CTR_VIEW_X + sLayerShift) * sZoom + sOffX + image->dx),
                         roundf((y + CTR_VIEW_Y) * sZoom + sOffY + image->dy), 0, &tint, 1, 1))
        Error(3, "Citro2D geometry submission failed");
    ViewBase();
}
