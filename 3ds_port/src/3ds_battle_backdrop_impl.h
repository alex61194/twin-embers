/* Part of 3ds_video.c (included where DrawBattleBg needs it): the native-resolution battle scenes. */
/*
 * A battle scene at the 3DS's own resolution (CtrBattleBackdrop, 3ds_battle_gen4.c): a full-colour picture of the top
 * screen above the message box, drawn 1:1 where BG3's scene would go. The game loads an empty BG3 for it, so the layer
 * is the picture alone; a move's background that is drawn into BG3 afterwards is drawn over it as the layer always is.
 */
static C3D_Tex sBackdropTex;
static bool sBackdropReady, sBackdropFailed;
static unsigned sBackdropUploaded;

static bool BackdropPrepare(const uint16_t *picture, unsigned generation)
{
    if (sBackdropFailed) return false;
    if (!sBackdropReady)
    {
        if (!C3D_TexInit(&sBackdropTex, 512, 256, GPU_RGB565)) { sBackdropFailed = true; return false; }
        C3D_TexSetFilter(&sBackdropTex, GPU_NEAREST, GPU_NEAREST);
        C3D_TexSetWrap(&sBackdropTex, GPU_CLAMP_TO_EDGE, GPU_CLAMP_TO_EDGE);
        memset(sBackdropTex.data, 0, sBackdropTex.size);
        sBackdropReady = true;
        sBackdropUploaded = generation - 1;
    }
    if (sBackdropUploaded != generation)
    {
        uint16_t *texels = (uint16_t *)sBackdropTex.data;

        for (unsigned y = 0; y < CTR_BACKDROP_H; ++y)
            for (unsigned x = 0; x < CTR_BACKDROP_W; ++x)
                texels[CtrVideo_Texel(x, y, 512)] = picture[y * CTR_BACKDROP_W + x];
        C3D_TexFlush(&sBackdropTex);
        sBackdropUploaded = generation;
    }
    return true;
}

/* Whether BG3's map holds nothing at all (the empty scene the game loads for a native picture). */
static bool BattleBg3Empty(void)
{
    unsigned control = Reg(8 + 3 * 2), size = control >> 14;
    unsigned map = ((control >> 8) & 31) * 0x800;
    unsigned entries = 1024u * ((size & 1) ? 2u : 1u) * ((size & 2) ? 2u : 1u);

    for (unsigned i = 0; i < entries; ++i)
        if (Read16(map + i * 2)) return false;
    return true;
}

/* Draws the native picture when the scene is one; true when BG3 has nothing else to draw. */
static bool DrawBattleBackdrop(void)
{
    unsigned generation = 0;
    const uint16_t *picture = CtrBattleBackdrop(&generation);
    const Tex3DS_SubTexture cut = {CTR_BACKDROP_W, CTR_BACKDROP_H, 0.0f, 1.0f, CTR_BACKDROP_W / 512.0f,
                                   1.0f - CTR_BACKDROP_H / 256.0f};

    C2D_ImageTint fade;
    const C2D_ImageTint *tint = &sTint;

    if (!picture || !BackdropPrepare(picture, generation)) return false;
    /* A whole-screen fade (the end of the battle, the flash of its transition) is a blend of the palettes towards one colour, which the sprites and
     * the boxes follow; the picture has no palette, so it was left at full brightness while everything else went black (black silhouettes on the
     * scene). It takes the same tint the field's layers take (UpdateFade's coefficient and colour). */
    if (sFadeBaseValid && sFadeCoeff)
    {
        uint32_t rgb = CtrVideo_RGBA8(sFadeTarget, true);

        C2D_PlainImageTint(&fade, C2D_Color32(rgb >> 24, rgb >> 16, rgb >> 8, 255), sFadeCoeff / 16.0f);
        tint = &fade;
    }
    C2D_ViewReset();
    ++sStats.tiles;
    C2D_DrawImageAt((C2D_Image){&sBackdropTex, &cut}, sLayerShift * sZoom, 0, 0, tint, 1, 1);
    ViewBase();
    return BattleBg3Empty();
}
