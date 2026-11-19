/* The Poke Ball OPTIONS on the whole lower screen (included at the end of 3ds_poketouch.c). The page's name and its dots on top (the whole header is
 * the PAGE button: a tap goes to the next page), up to seven rows 20 apart, a dark band at y 176 with the exit piece on it, and under it, on the
 * white, the row's note. A row: its label at the left; at the right its value between two small triangles (a tap on the left or the right half of
 * the value steps it back or on), a switch as a track with a knob (the whole value toggles), or the levels side by side (a tap picks one); the chosen
 * row is a white pill with a red dot. The hit zones (CtrPokeTouch_OptionHit, ...Segment, ...HitOptPage) follow this geometry while it is up. */

#define BO_BAND_Y 176
#define BO_ROW_Y 34
#define BO_ROW_STEP 20
#define BO_ROW_X 8
#define BO_ROW_W 224
#define BO_VAL_X 128
#define BO_VAL_W 104

static void BallOptRow(const CtrPokeTouchView *v, int i)
{
    int y = BO_ROW_Y + i * BO_ROW_STEP, selected = v->optSelected == i, pressed = v->optPressed == i, zone = pressed ? v->optZone : PT_OPT_ZONE_NONE;
    uint16_t ink = selected ? BL_REDTXT : EM_WHITE, bg = selected ? BL_WHITE : BL_DEEP, dim = selected ? UI_RGB(150, 140, 140) : BL_PINK;
    int cx = BO_VAL_X + BO_VAL_W / 2;

    if (selected)
        UiRoundFill(BO_ROW_X, y, BO_ROW_W, 19, 5, BL_WHITE);
    else if (i)
        Fill(BO_ROW_X + 8, y - 1, BO_ROW_W - 16, 1, BL_HAIR);
    if (selected)
        TsDisc(BO_ROW_X + 9, y + 9, 3, BL_REDTXT);
    UiText(v->optLabel[i], BO_ROW_X + (selected ? 18 : 10), y + 5, ink, bg, 1, BO_VAL_X - 4);
    if (v->optSegCount[i]) {
        int count = v->optSegCount[i] > PT_OPT_SEGS ? PT_OPT_SEGS : v->optSegCount[i], w = BO_VAL_W / count;

        for (int k = 0; k < count; ++k) {
            int on = v->optSegValue[i] == k, x = BO_VAL_X + k * w, tw = UiTextWidth(v->optSegName[i][k], 1);
            uint16_t sb = on ? (selected ? BL_REDTXT : BL_WHITE) : bg;

            if (on || zone == PT_OPT_ZONE_SEG + k)
                UiRoundFill(x, y + 2, w - 2, 15, 5, on ? sb : (selected ? UI_RGB(240, 224, 224) : UI_RGB(210, 56, 58)));
            UiText(v->optSegName[i][k], x + (w - 2 - tw) / 2, y + 5, on ? (selected ? EM_WHITE : BL_REDTXT) : dim, on ? sb : bg, 1, x + w - 1);
        }
        return;
    }
    if (v->optSwitch[i]) {
        int on = v->optSwitch[i] == 2, tx = cx - 15;

        UiRoundFill(tx, y + 3, 30, 13, 5, on ? (selected ? BL_REDTXT : BL_WHITE) : (selected ? UI_RGB(214, 208, 208) : UI_RGB(120, 18, 22)));
        TsDisc(on ? tx + 23 : tx + 7, y + 9, 5, on ? (selected ? BL_WHITE : BL_REDTXT) : (selected ? BL_WHITE : BL_PINK));
        return;
    }
    {
        int tw = UiTextWidth(v->optValue[i], 1);

        for (int k = 0; k < 5; ++k) {                   /* a triangle pointing out at each end: small at its point, tall at its base */
            UiRoundFill(BO_VAL_X + 3 + k, y + 9 - k, 1, 2 * k + 1, 0, zone == PT_OPT_ZONE_PREV ? ink : dim);
            UiRoundFill(BO_VAL_X + BO_VAL_W - 3 - k, y + 9 - k, 1, 2 * k + 1, 0, zone == PT_OPT_ZONE_NEXT ? ink : dim);
        }
        if (tw > BO_VAL_W - 24)
            tw = BO_VAL_W - 24;
        UiText(v->optValue[i], cx - tw / 2, y + 5, ink, bg, 1, BO_VAL_X + BO_VAL_W - 12);
    }
}

static void BallOptionsScreen(const CtrPokeTouchView *v)
{
    const char *body = v->bar[1][0] ? v->bar[1] : v->bar[2];
    int dots = v->optPageCount > 7 ? 7 : v->optPageCount;

    sBallExitR = (CtrPokeTouchRect){240, 162, 74, 38};
    EmSurface();
    TsBallGround(BO_BAND_Y);
    UiText(v->optPageName, 14, 10, v->optPagePressed ? BL_PINK : EM_WHITE, BL_DEEP, 1, 200);
    for (int d = 0; d < dots; ++d)
        UiRoundFill(312 - (dots - d) * 12, 13, 7, 7, 3, d == v->optPage ? EM_WHITE : BL_HAIR);
    Fill(8, 30, 304, 1, BL_HAIR);
    for (int i = 0; i < v->optCount && i < PT_OPT_MAX; ++i)
        BallOptRow(v, i);
    {
        const char *label = CTR_TR("EXIT", "SALIR", "ZUR\xC3\x9C" "CK");
        int tw = UiTextWidth(label, 1);

        UiRoundFill(sBallExitR.x, sBallExitR.y, sBallExitR.w, sBallExitR.h, 5, BL_BAND);
        UiRoundFill(sBallExitR.x + 3, sBallExitR.y + 3, sBallExitR.w - 6, sBallExitR.h - 6, 4, v->backPressed ? UI_RGB(222, 218, 218) : BL_WHITE);
        UiText(label, sBallExitR.x + (sBallExitR.w - tw) / 2, sBallExitR.y + (sBallExitR.h - UI_TEXT_H) / 2, BL_BAND, BL_WHITE, 1, sBallExitR.x + sBallExitR.w - 4);
    }
    UiText(v->bar[0], 14, BO_BAND_Y + 14, BL_BAND, BL_WHITE, 1, 232);
    if (body[0])
        MenuWrap(body, 14, BO_BAND_Y + 30, 216, 3, 12, UI_RGB(132, 128, 130), BL_WHITE);
    sUiS = &sUiNavy;
}
