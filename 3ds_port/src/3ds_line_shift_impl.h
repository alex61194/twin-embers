/* Part of 3ds_video.c (included there, before RenderEye; kept apart for the 256 KiB source limit): the battle
 * transitions that bend the field line by line. Uses the compositor's own state; not a header of its own. */

/*
 * Swirl and Slice move each line of the field across, Shuffle and Ripple up and down, from HBlank callbacks that
 * write BG1-3's scroll before each line (battle_transition.c, CtrTransition_LineShift). The field here is the native
 * 400x240 canvas drawn from its own camera, not BG1-3 read line by line, so the bend is applied where the composed
 * canvas is put on the screen: one strip per run of rows with the same offset. The effect is authored for the
 * 240x160 display and is a whole-screen one, like the transitions' windows (WindowSpan): its 160 lines are spread
 * over the canvas's rows and its offsets scaled to the canvas (x 400/240, y 240/160), so a slice slides off the whole
 * width. What a strip leaves uncovered is black, as the transitions' own windows make it on the GBA.
 */
static int16_t sLineShift[160];
static int sLineShiftKind;          /* 0 none, 1 across, 2 up and down */

void CtrVideo_SetFieldLineShift(int kind, const int16_t *shift, unsigned lines)
{
    sLineShiftKind = shift && lines >= 160 && (kind == 1 || kind == 2) ? kind : 0;
    if (sLineShiftKind) memcpy(sLineShift, shift, sizeof(sLineShift));
}

/* The logical surface on the screen, bent: false (nothing drawn) when no bend is on. */
static bool DrawLogicalShifted(void)
{
    const int w = sPresentation.width, h = sPresentation.height;
    const float texH = (float)sPresentation.textureHeight, sc = sPresentation.scale;

    if (!sLineShiftKind || !sFieldLayers || h <= 0) return false;
    for (int y = 0, end; y < h; y = end)
    {
        int d = sLineShift[y * 160 / h];
        int top = y, bottom;

        for (end = y + 1; end < h && sLineShift[end * 160 / h] == d; ++end) {}
        bottom = end;
        if (sLineShiftKind == 1)
        {
            float dx = -(float)d * w / 240.0f;
            const Tex3DS_SubTexture strip = {(u16)w, (u16)(bottom - top), 0, 1 - top / texH, w / 512.0f, 1 - bottom / texH};

            C2D_DrawImageAt((C2D_Image){&sSurface, &strip}, sPresentation.x + dx * sc, sPresentation.y + top * sc, 0,
                            NULL, sc, sc);
            continue;
        }
        /* Up and down: row r of the screen shows row r + dy of the canvas; rows past its edges stay black. */
        int dy = (int)lroundf((float)d * h / 160.0f);
        int from = top + dy, to = bottom + dy;

        if (from < 0) { top -= from; from = 0; }
        if (to > h) { bottom -= to - h; to = h; }
        if (from >= to) continue;
        {
            const Tex3DS_SubTexture strip = {(u16)w, (u16)(to - from), 0, 1 - from / texH, w / 512.0f, 1 - to / texH};

            C2D_DrawImageAt((C2D_Image){&sSurface, &strip}, sPresentation.x, sPresentation.y + top * sc, 0, NULL, sc, sc);
        }
    }
    return true;
}
