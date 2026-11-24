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

/* The look rows' preview, on the whole screen: a white card under the rows with the look's walking picture facing down, its name and the artists
 * behind an ART pill, as many lines as the card holds. */
static void BallLookPanel(const CtrPokeTouchView *v)
{
    const CtrPokeTouchLook *l = &v->optLook;
    int x = BO_ROW_X, y = BO_ROW_Y + v->optCount * BO_ROW_STEP + 4, w = BO_ROW_W, h = BO_BAND_Y - 6 - y;
    int textX = x + 54, right = x + w - 12, lines;
    const char *tag = CTR_TR("Art", "Arte", "Kunst");
    int pw = UiTextWidth(tag, 1) + 12;

    if (h < 34)
        return;
    UiRoundFill(x, y, w, h, 6, UI_RGB(150, 22, 26));
    UiRoundFill(x + 2, y + 2, w - 4, h - 4, 5, UI_RGB(250, 250, 248));
    if (l->has) {
        static uint8_t pixels[32 * 32];
        uint16_t pal[16];

        for (int py = 0; py < 32; ++py)
            for (int px = 0; px < 32; ++px)
                pixels[py * 32 + px] = (uint8_t)((l->tiles[(py / 8 * 4 + px / 8) * 32 + py % 8 * 4 + px % 8 / 2] >> (px & 1) * 4) & 15);
        for (int i = 0; i < 16; ++i)
            pal[i] = GbaColor(l->palette[i]);
        UiImage(pixels + 2 * 32, pal, 32, 32, 30, x + 12, y + (h - 30) / 2, 1, 1);   /* the first two rows of a person are empty */
    }
    Em2Text(l->name, textX, y + 7, UI_RGB(60, 52, 56), UI_RGB(236, 232, 232), right);
    Fill(textX, y + 22, right - textX, 1, UI_RGB(226, 220, 220));
    lines = h >= 50 ? 2 : 1;
    UiRoundFill(textX, y + 27, pw, 13, 4, UI_RGB(176, 30, 34));
    UiText(tag, textX + 6, y + 29, EM_WHITE, UI_RGB(120, 16, 20), 1, textX + pw);
    Em2Wrap(l->credit, textX + pw + 4, y + 29, right - textX - pw - 4, lines, UI_LINE_H, UI_RGB(110, 104, 108), UI_RGB(236, 232, 232));
}

static void BallOptionsScreen(const CtrPokeTouchView *v)
{
    const char *body = v->bar[1][0] ? v->bar[1] : v->bar[2];
    int dots = v->optPageCount > 8 ? 8 : v->optPageCount;      /* eight pages now: the title keeps its 200 px, the dots start at 216 */

    sBallExitR = (CtrPokeTouchRect){240, 162, 74, 38};
    EmSurface();
    TsBallGround(BO_BAND_Y);
    UiText(v->optPageName, 14, 10, v->optPagePressed ? BL_PINK : EM_WHITE, BL_DEEP, 1, 200);
    for (int d = 0; d < dots; ++d)
        UiRoundFill(312 - (dots - d) * 12, 13, 7, 7, 3, d == v->optPage ? EM_WHITE : BL_HAIR);
    Fill(8, 30, 304, 1, BL_HAIR);
    if (v->optPad.on)
        OptPad(v);                                      /* the keypad of the private room's code, in place of the rows */
    else {
        for (int i = 0; i < v->optCount && i < PT_OPT_MAX; ++i)
            BallOptRow(v, i);
        if (v->optLook.on)
            BallLookPanel(v);
    }
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
