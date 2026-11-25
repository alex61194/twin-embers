/* The Poke Ball move to forget (3ds_field_learn_impl.h) and move relearner (3ds_field_relearn_impl.h) on the whole lower screen (included at the end of
 * 3ds_poketouch.c, after 3ds_ball_menus_impl.h whose card, pill and frame it uses): the red ground down to the band with the question across the top,
 * the Pokemon on a white card at the left (its picture on a light disc, the level in a red ribbon, the nickname, PP, power and accuracy of the chosen
 * move), five pills at the right for the moves (the chosen one white with a red word and the triangle, the new move and CANCEL dark, the type's badge at the
 * pill's end), the move's description in the note on the white and BACK on the band. The relearner's two arrows are at the top right; its questions and
 * messages are a white card over the dimmed list (YES and NO as pills, "Tap to continue" for a message that waits). Every word but the three labels is
 * FireRed's; the rectangles are in screen pixels and are the hit zones while the screen is up. */

#define BLR_ROW_X 112
#define BLR_ROW_Y 34
#define BLR_ROW_STEP 25
#define BLR_ROW_W 200
#define BLR_ROW_H 22
#define BLR_TEXT_W 264                                   /* the width of a question's or message's text in its card */

static const CtrPokeTouchRect sBlSide = {8, 32, 98, 126};
static const CtrPokeTouchRect sBlUp = {262, 3, 24, 21}, sBlDown = {288, 3, 24, 21};

static CtrPokeTouchRect BlRow(int i)
{
    CtrPokeTouchRect r = {BLR_ROW_X, (int16_t)(BLR_ROW_Y + BLR_ROW_STEP * i), BLR_ROW_W, BLR_ROW_H};

    return r;
}

static CtrPokeTouchRect BlAsk(const CtrPokeTouchRelearn *l)
{
    int lines = FRLines(l->message, BLR_TEXT_W, 5), h = 14 * lines + 20;
    CtrPokeTouchRect r = {16, (int16_t)((BO_BAND_Y - h - 44) / 2), 288, (int16_t)h};

    return r;
}

static CtrPokeTouchRect BlButton(const CtrPokeTouchRelearn *l, int no)
{
    CtrPokeTouchRect a = BlAsk(l), r = {(int16_t)(no ? 172 : 48), (int16_t)(a.y + a.h + 10), 100, 28};

    return r;
}

static CtrPokeTouchRect BlSay(const CtrPokeTouchRelearn *l)
{
    int lines = FRLines(l->message, BLR_TEXT_W, 4), h = 14 * lines + 20 + (l->tap ? 16 : 0);
    CtrPokeTouchRect r = {16, (int16_t)((BO_BAND_Y - h) / 2), 288, (int16_t)h};

    return r;
}

/* ---- the pieces ---- */

/* The Pokemon's card: picture on a light disc, the level's ribbon on the card's corner, the nickname, then the chosen move's numbers. */
static void BlSidePanel(unsigned species, const char *nickname, const char *level, int have, const char *pp, const char *power, const char *accuracy)
{
    const CtrPokeTouchRect *s = &sBlSide;
    CtrPokeTouchRect name = {s->x, 0, s->w, 13};
    const char *label[3] = {CtrUiText(UIS_MOVE_PP), CtrUiText(UIS_MOVE_POWER), CtrUiText(UIS_MOVE_ACCURACY)};
    const char *value[3] = {pp, power, accuracy};

    BmCard(s->x, s->y, s->w, s->h);
    TsDisc(s->x + s->w / 2, s->y + 36, 31, UI_RGB(244, 238, 238));
    FDPicDraw(species, s->x + (s->w - 64) / 2, s->y + 4, 0);
    if (level[0]) {
        int w = UiTextWidth(level, 1) + 14;

        UiRoundFill(s->x + 2, s->y + 2, w, 15, 5, BM_STRIP);
        UiText(level, s->x + 9, s->y + 4, EM_WHITE, BM_PILL_EDGE, 1, s->x + 2 + w - 3);
    }
    UiTextCentered(nickname, &name, s->y + 69, BM_INK, BM_CARD_FACE, 1);
    Fill(s->x + 8, s->y + 83, s->w - 16, 1, BM_RULE);
    if (have)
        for (int k = 0; k < 3; ++k) {
            /* The label has the room up to its value, less a 4 px gap: German "Genauigkeit" beside "100" stays whole, and a label that is still
             * too long ends in "..." rather than losing letters. */
            int y = s->y + 88 + 12 * k, labelRight = s->x + s->w - 10 - UiTextWidth(value[k], 1) - 4;
            char fit[32];

            Em2Fit(label[k], labelRight - (s->x + 10), fit, sizeof(fit));
            UiText(fit, s->x + 10, y, BM_DIM, BM_CARD_FACE, 1, labelRight);
            UiTextRight(value[k], s->x + s->w - 10, y, BL_REDTXT, BM_CARD_FACE, 1);
        }
}

/* One move's pill. kind: 0 a move, 1 the new move, 2 CANCEL (the last two are dark when not chosen). The type's badge goes at the pill's end where the name
 * stays whole; a name that is cut is not given a badge. */
static void BlMoveRow(int i, const char *name, int type, const char *typeName, int kind, int chosen, int pressed)
{
    CtrPokeTouchRect r = BlRow(i);
    uint16_t face = chosen ? (pressed ? BM_PRESSED : BL_WHITE) : pressed ? BM_CARD_EDGE : kind ? BL_BAND : BM_PILL;
    int bw = 0, tx = r.x + (chosen ? 20 : 12);

    UiRoundFill(r.x, r.y, r.w, r.h, 8, chosen ? BM_CARD_EDGE : BM_PILL_EDGE);
    UiRoundFill(r.x + 2, r.y + 2, r.w - 4, r.h - 4, 6, face);
    if (chosen)
        LlArrow(r.x + 9, r.y + (r.h - 10) / 2, 6, 10, kind == 2 ? 2 : 3, BL_REDTXT);
    else if (kind == 2)
        LlArrow(r.x + 10, r.y + (r.h - 10) / 2, 6, 10, 2, EM_WHITE);
    if (kind == 2)
        tx = r.x + 22;
    if (kind != 2 && type >= 0 && UiTextWidth(name, 1) + 20 + UiTextWidth(typeName, 1) + 12 + 12 <= r.w) {
        bw = UiTextWidth(typeName, 1) + 12;
        LlTypeBadge(r.x + r.w - 7 - bw, r.y + (r.h - 13) / 2, typeName, UiTypeColor(type));
    }
    UiText(name, tx, r.y + (r.h - UI_TEXT_H) / 2, chosen ? BL_REDTXT : EM_WHITE, face, 1, r.x + r.w - 9 - bw);
}

static void BlScrollArrow(const CtrPokeTouchRect *r, int dir, int on, int pressed)
{
    UiRoundFill(r->x, r->y, r->w, r->h, 7, on ? BM_CARD_EDGE : BM_PILL_EDGE);
    UiRoundFill(r->x + 2, r->y + 2, r->w - 4, r->h - 4, 5, on ? (pressed ? BM_PRESSED : BL_WHITE) : BM_PILL);
    LlArrow(r->x + (r->w - 8) / 2, r->y + (r->h - 5) / 2, 8, 5, dir, on ? BL_REDTXT : BM_PILL_EDGE);
}

/* ---- the move to forget ---- */

static void BallLearnScreen(const CtrPokeTouchView *v)
{
    const CtrPokeTouchLearn *l = &v->learn;
    int c = l->cursor >= 0 && l->cursor < PT_LEARN_ROWS ? l->cursor : 0, have = l->row[c].move != 0;

    BmFrame(l->question, have ? l->row[c].name : "", have ? l->desc : "", CTR_TR("BACK", "ATR\xC3\x81S", "ZUR\xC3\x9C" "CK"), v->backPressed, v->backEnabled);
    BlSidePanel(l->species, l->nickname, l->level, have, l->row[c].pp, l->row[c].power, l->row[c].accuracy);
    for (int i = 0; i < PT_LEARN_ROWS; ++i)
        if (l->row[i].move)
            BlMoveRow(i, l->row[i].name, l->row[i].type, l->row[i].typeName, i == PT_LEARN_ROWS - 1, l->cursor == i, l->pressed == PT_LEARN_HIT_ROW + i);
    sUiS = &sUiNavy;
}

static int BallLearnHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchLearn *l = &v->learn;

    for (int i = 0; i < PT_LEARN_ROWS; ++i) {
        CtrPokeTouchRect r = BlRow(i);

        if (l->row[i].move && UiIn(&r, x, y))
            return PT_LEARN_HIT_ROW + i;
    }
    return -1;
}

/* ---- the move relearner ---- */

static void BallRelearnScreen(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRelearn *l = &v->relearn;
    int c = FRCursorRow(l), have = c >= 0 && l->row[c].move != 0;
    CtrPokeTouchRect ground = {0, 0, 320, BO_BAND_Y};

    BmFrame(l->panel == PT_RL_PANEL_BLANK ? "" : l->question, have ? l->row[c].name : "", have ? l->desc : "",
            CTR_TR("BACK", "ATR\xC3\x81S", "ZUR\xC3\x9C" "CK"), v->backPressed, v->backEnabled);
    if (l->panel == PT_RL_PANEL_BLANK) {                /* being set up: the ground, nothing of FireRed's */
        sUiS = &sUiNavy;
        return;
    }
    BlScrollArrow(&sBlUp, 0, l->first > 0 || l->cursor > 0, l->pressed == PT_RL_HIT_UP);
    BlScrollArrow(&sBlDown, 1, l->cursor < l->count, l->pressed == PT_RL_HIT_DOWN);
    BlSidePanel(l->species, l->nickname, l->level, have, have ? l->row[c].pp : "", have ? l->row[c].power : "", have ? l->row[c].accuracy : "");
    for (int i = 0; i < PT_RL_ROWS; ++i) {
        int cancel = l->first + i == l->count;

        if (l->first + i > l->count)
            break;
        BlMoveRow(i, l->row[i].name, cancel ? -1 : l->row[i].type, l->row[i].typeName, cancel ? 2 : 0, l->cursor == l->first + i,
                  l->pressed == PT_RL_HIT_ROW + i);
    }
    if (l->panel == PT_RL_PANEL_YESNO || l->panel == PT_RL_PANEL_MESSAGE) {
        UiDim(&ground);
        if (l->panel == PT_RL_PANEL_YESNO) {
            CtrPokeTouchRect ask = BlAsk(l), yes = BlButton(l, 0), no = BlButton(l, 1);

            BmCard(ask.x, ask.y, ask.w, ask.h);
            Em2Wrap(l->message, ask.x + 12, ask.y + 10, BLR_TEXT_W, 5, 14, BM_INK, BM_CARD_FACE);
            BmPill(&yes, CtrUiText(UIS_YES), l->menuCursor == 0, l->pressed == PT_RL_HIT_YES, 1);
            BmPill(&no, CtrUiText(UIS_NO), l->menuCursor == 1, l->pressed == PT_RL_HIT_NO, 1);
        } else {
            CtrPokeTouchRect box = BlSay(l);
            const char *tap = CtrUiText(UIS_TAP_TO_CONTINUE);

            BmCard(box.x, box.y, box.w, box.h);
            Em2Wrap(l->message, box.x + 12, box.y + 10, BLR_TEXT_W, 4, 14, BM_INK, BM_CARD_FACE);
            if (l->tap) {
                UiText(tap, box.x + box.w - 30 - UiTextWidth(tap, 1), box.y + box.h - 17, BM_DIM, BM_CARD_FACE, 1, box.x + box.w - 22);
                LlArrow(box.x + box.w - 24, box.y + box.h - 16, 8, 5, 1, BL_REDTXT);
            }
        }
    }
    sUiS = &sUiNavy;
}

static int BallRelearnHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchRelearn *l = &v->relearn;

    if (l->panel == PT_RL_PANEL_YESNO) {
        CtrPokeTouchRect yes = BlButton(l, 0), no = BlButton(l, 1);

        return UiIn(&yes, x, y) ? PT_RL_HIT_YES : UiIn(&no, x, y) ? PT_RL_HIT_NO : -1;
    }
    if (l->panel == PT_RL_PANEL_MESSAGE)
        return l->tap && y < BO_BAND_Y ? PT_RL_HIT_MESSAGE : -1;     /* anywhere above the band: it waits for A */
    if (l->panel == PT_RL_PANEL_BLANK)
        return -1;
    if (UiIn(&sBlUp, x, y))
        return PT_RL_HIT_UP;
    if (UiIn(&sBlDown, x, y))
        return PT_RL_HIT_DOWN;
    for (int i = 0; i < PT_RL_ROWS; ++i) {
        CtrPokeTouchRect r = BlRow(i);

        if (l->first + i <= l->count && UiIn(&r, x, y))
            return PT_RL_HIT_ROW + i;
    }
    return -1;
}
