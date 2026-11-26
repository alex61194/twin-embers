/* A choice of the field in the viewport (3ds_poketouch.h CtrPokeTouchChoice), included at the end of 3ds_poketouch.c
 * after the Bag, whose cards and buttons it shares. A remote for FireRed's menu (a script's multichoice, YES/NO or
 * grid, the PC's menus): its rows as ember buttons, one column (two past five rows, or the grid's own columns) centred
 * on the embers, the cursor's flame with the select ring; the question of the message box in the note. Every word is FireRed's
 * (CtrFieldChoice_*). */

/* The trade's choice of a Pokemon takes the place of the rows while it is on (3ds_field_trade_impl.h, included after the PC box). */
static void FTradeFrame(const CtrPokeTouchView *v);
static int FTradeHit(const CtrPokeTouchView *v, int x, int y);
static void FChatFrame(const CtrPokeTouchView *v);
static int FChatHit(const CtrPokeTouchView *v, int x, int y);

static int FChColumns(const CtrPokeTouchChoice *c)
{
    return c->cols > 0 ? c->cols : c->count > 5 ? 2 : 1;
}

static CtrPokeTouchRect FChRow(const CtrPokeTouchChoice *c, int i)
{
    int cols = FChColumns(c), rows = (c->count + cols - 1) / cols, gap = 4;
    int h = rows ? (PT_VIEW_H - 16 - (rows - 1) * gap) / rows : 0;
    int w = cols == 1 ? 184 : (PT_VIEW_W - 16 - (cols - 1) * gap) / cols;
    int x0, y0;

    if (h > 30)
        h = 30;
    x0 = PT_VIEW_X + (PT_VIEW_W - (cols * w + (cols - 1) * gap)) / 2;
    y0 = PT_VIEW_Y + (PT_VIEW_H - (rows * h + (rows - 1) * gap)) / 2;
    CtrPokeTouchRect r = {(int16_t)(x0 + (i % cols) * (w + gap)), (int16_t)(y0 + (i / cols) * (h + gap)),
                          (int16_t)w, (int16_t)h};
    return r;
}

/* ---- Poke Ball: a choice of the field (YES/NO, a multichoice, a grid) on the whole lower screen ----
 * Its rows are pills in the red above the band (the chosen one white with a red dot); the question is the note on the white; the exit piece on the
 * band is the menu's own B (HitBack). A YES/NO is two tall pills. */
static CtrPokeTouchRect FBallChRow(const CtrPokeTouchChoice *c, int i)
{
    int cols = FChColumns(c), rows = (c->count + cols - 1) / cols, gap = 8, areaTop = 14, areaH = 136;
    int h = rows ? (areaH - (rows - 1) * gap) / rows : 0, w, x0, y0;

    if (h > 54)
        h = 54;
    w = cols == 1 ? 248 : (296 - (cols - 1) * gap) / cols;
    x0 = (320 - (cols * w + (cols - 1) * gap)) / 2;
    y0 = areaTop + (areaH - (rows * h + (rows - 1) * gap)) / 2;
    CtrPokeTouchRect r = {(int16_t)(x0 + (i % cols) * (w + gap)), (int16_t)(y0 + (i / cols) * (h + gap)), (int16_t)w, (int16_t)h};
    return r;
}

static void BallChoiceScreen(const CtrPokeTouchView *v)
{
    const CtrPokeTouchChoice *c = &v->choice;

    sBallExitR = (CtrPokeTouchRect){238, 146, 76, 38};
    EmSurface();
    TsBallGround(160);
    for (int i = 0; i < c->count && i < PT_CHOICE_MAX; ++i) {
        CtrPokeTouchRect r = FBallChRow(c, i);
        int on = c->cursor == i, press = c->pressed == PT_CHOICE_HIT_ROW + i, tw = UiTextWidth(c->label[i], 1);
        uint16_t bg = on ? BL_WHITE : press ? UI_RGB(222, 70, 66) : UI_RGB(196, 34, 38);

        UiRoundFill(r.x, r.y, r.w, r.h, 5, bg);
        if (on)
            TsDisc(r.x + 14, r.y + r.h / 2, 4, BL_REDTXT);
        UiText(c->label[i], r.x + (r.w - tw) / 2, r.y + (r.h - UI_TEXT_H) / 2, on ? BL_REDTXT : EM_WHITE, on ? BL_WHITE : BL_DEEP, 1, r.x + r.w - 6);
    }
    {
        static const CtrPokeTouchRect exitR = {238, 146, 76, 38};
        const char *label = CTR_TR("BACK", "VOLVER", "ZUR\xC3\x9C" "CK");
        int tw = UiTextWidth(label, 1);

        UiRoundFill(exitR.x, exitR.y, exitR.w, exitR.h, 5, BL_BAND);
        UiRoundFill(exitR.x + 3, exitR.y + 3, exitR.w - 6, exitR.h - 6, 4, v->backPressed ? UI_RGB(222, 218, 218) : BL_WHITE);
        UiText(label, exitR.x + (exitR.w - tw) / 2, exitR.y + (exitR.h - UI_TEXT_H) / 2, BL_BAND, BL_WHITE, 1, exitR.x + exitR.w - 4);
    }
    MenuWrap(c->prompt, 14, 176, 216, 4, 14, BL_BAND, BL_WHITE);
    sUiS = &sUiNavy;
}

int CtrPokeTouch_ChoiceHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchChoice *c = &v->choice;

    if (sStyleBall && c->on && !v->trade.on && !v->chat.on) {
        for (int i = 0; i < c->count && i < PT_CHOICE_MAX; ++i) {
            CtrPokeTouchRect r = FBallChRow(c, i);

            if (UiIn(&r, x, y))
                return PT_CHOICE_HIT_ROW + i;
        }
        return -1;
    }

    if (v->trade.on)
        return c->on ? FTradeHit(v, x, y) : -1;
    if (v->chat.on)
        return c->on ? FChatHit(v, x, y) : -1;
    x -= MENU_SHIFT;                                    /* drawn on the floor of HOME's layout: its rectangles are at (4, 4) */
    y -= MENU_SHIFT;
    if (!c->on)
        return -1;
    for (int i = 0; i < c->count && i < PT_CHOICE_MAX; ++i) {
        CtrPokeTouchRect r = FChRow(c, i);
        if (UiIn(&r, x, y))
            return PT_CHOICE_HIT_ROW + i;
    }
    return -1;
}

static void FChoiceFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchChoice *c = &v->choice;

    if (v->trade.on) {
        FTradeFrame(v);
        return;
    }
    if (v->chat.on) {
        FChatFrame(v);
        return;
    }
    EmSurface();
    TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    for (int i = 0; i < c->count && i < PT_CHOICE_MAX; ++i) {
        CtrPokeTouchRect r = FChRow(c, i);

        FBLlButton(&r, LlPick(c->cursor == i, &kEmCream), c->label[i], c->pressed == PT_CHOICE_HIT_ROW + i, c->cursor == i);
    }
    sUiS = &sUiNavy;
}

static void FChoiceNote(const CtrPokeTouchView *v, int w)
{
    (void)w;
    EmSurface();
    NoteText(v->choice.prompt, EM_DARK);
    sUiS = &sUiNavy;
}
