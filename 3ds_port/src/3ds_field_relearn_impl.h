/* The move relearner's list, in the viewport's place (3ds_poketouch.h CtrPokeTouchRelearn), included at the end of 3ds_poketouch.c after the
 * move to forget (3ds_field_learn_impl.h), whose look it shares: the question in a FireRed text window with two scroll buttons, the Pokemon
 * (front picture on a plate, level in a ribbon, nickname) with the selected move's PP, power and accuracy, a window of the list as rows
 * (type badge on the right, the chosen one flame with the select ring, CANCEL in slate), the move's description in the note.
 * Every word but the three labels is FireRed's (CtrBottomRelearn_*). */

static const CtrPokeTouchRect sFRBand = {4, 4, 240, 22};
static const CtrPokeTouchRect sFRSide = {4, 30, 96, 134};
static const CtrPokeTouchRect sFRUp = {206, 4, 18, 22}, sFRDown = {224, 4, 18, 22};

static CtrPokeTouchRect FRRow(int i)
{
    CtrPokeTouchRect r = {104, (int16_t)(30 + 27 * i), 140, 25};
    return r;
}

int CtrPokeTouch_RelearnHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchRelearn *l = &v->relearn;

    if (!l->on)
        return -1;
    if (UiIn(&sFRUp, x, y))
        return PT_RL_HIT_UP;
    if (UiIn(&sFRDown, x, y))
        return PT_RL_HIT_DOWN;
    for (int i = 0; i < PT_RL_ROWS; ++i) {
        CtrPokeTouchRect r = FRRow(i);
        if (l->first + i <= l->count && UiIn(&r, x, y))
            return PT_RL_HIT_ROW + i;
    }
    return -1;
}

static int FRCursorRow(const CtrPokeTouchRelearn *l)
{
    int c = l->cursor - l->first;
    return c >= 0 && c < PT_RL_ROWS ? c : -1;
}

static void FRSideRow(const char *label, const char *value, int y)
{
    const CtrPokeTouchRect *s = &sFRSide;

    Em2Text(label, s->x + 10, y, EM_DIM, EM_DARK_SH, s->x + s->w - 10 - UiTextWidth(value, 1) - 4);
    UiTextRight(value, s->x + s->w - 10, y, EM_DARK, EM_DARK_SH, 1);
}

static void FRSidePanel(const CtrPokeTouchRelearn *l)
{
    const CtrPokeTouchRect *s = &sFRSide;
    CtrPokeTouchRect name = {s->x, 0, s->w, 13};
    int c = FRCursorRow(l);

    EmButton(s->x, s->y, s->w, s->h, &kEmCream, 2, 0);
    LlPlate(s->x + 16, s->y + 8, 64, LL_SLATE, EM_WHITE, 1);
    FDPicDraw(l->species, s->x + (s->w - 64) / 2, s->y + 8, 0);
    LlRibbonBox(s->x, s->y, UiTextWidth(l->level, 1) + 18, 11, 7, LL_TL, LL_RIB_TOP, LL_RIB_BOT, l->level);
    UiTextCentered(l->nickname, &name, s->y + 83, EM_DARK, EM_DARK_SH, 1);
    LlRule(s->x + 8, s->y + 97, s->w - 16);
    if (c >= 0 && l->row[c].move) {
        FRSideRow(CtrUiText(UIS_MOVE_PP), l->row[c].pp, s->y + 100);
        FRSideRow(CtrUiText(UIS_MOVE_POWER), l->row[c].power, s->y + 110);
        FRSideRow(CtrUiText(UIS_MOVE_ACCURACY), l->row[c].accuracy, s->y + 120);
    }
}

static void FRRowDraw(const CtrPokeTouchRelearn *l, int i)
{
    CtrPokeTouchRect r = FRRow(i);
    int chosen = l->cursor == l->first + i, cancel = l->first + i == l->count, fy, bw = 0;
    const EmPal *p = LlPick(chosen, cancel ? &kEmSlate : &kEmCream);

    if (l->first + i > l->count)
        return;
    if (chosen)
        LlSelectRing(r.x, r.y, r.w, r.h);
    fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, l->pressed == PT_RL_HIT_ROW + i);
    if (chosen)
        LlCursor(r.x + 8, fy + (r.h - FB_DEPTH - 10) / 2, EM_WHITE);
    if (cancel)
        LlArrow(r.x + (chosen ? 18 : 10), fy + (r.h - FB_DEPTH - 10) / 2, 6, 10, 2, EM_WHITE);
    else if (l->row[i].type >= 0 && UiTextWidth(l->row[i].name, 1) + 18 + UiTextWidth(l->row[i].typeName, 1) + 12 + 14 <= r.w) {
        bw = UiTextWidth(l->row[i].typeName, 1) + 12;     /* the type's badge only where the name stays whole */
        LlTypeBadge(r.x + r.w - 5 - bw, fy + (r.h - FB_DEPTH - 13) / 2, l->row[i].typeName, UiTypeColor(l->row[i].type));
    }
    if (!cancel && !bw && l->row[i].type >= 0 && !chosen)
        UiRoundFill(r.x + 1, fy + 1, 5, r.h - FB_DEPTH - 2, 2, UiTypeColor(l->row[i].type));
    Em2Text(l->row[i].name, r.x + (cancel && chosen ? 28 : cancel ? 20 : 18), fy + (r.h - FB_DEPTH - UI_TEXT_H) / 2, p->ink, p->inkSh,
            r.x + r.w - 9 - bw);
}

static void FRArrow(const CtrPokeTouchRect *r, int dir, int on, int pressed)
{
    int fy = EmButton(r->x + 1, r->y + 4, r->w - 2, 16, &kEmCream, FB_DEPTH, pressed);

    LlArrow(r->x + (r->w - 8) / 2, fy + 4, 8, 5, dir, on ? LL_CURSOR : FB_IDLE_ARROW);
}

static void FRelearnFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRelearn *l = &v->relearn;
    CtrPokeTouchRect band = sFRBand;

    EmSurface();
    TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    LlWindow(band.x, band.y, band.w, band.h);
    Em2Text(l->question, band.x + 12, band.y + (band.h - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, sFRUp.x - 2);
    FRArrow(&sFRUp, 0, l->first > 0 || l->cursor > 0, l->pressed == PT_RL_HIT_UP);
    FRArrow(&sFRDown, 1, l->cursor < l->count, l->pressed == PT_RL_HIT_DOWN);
    FRSidePanel(l);
    for (int pass = 0; pass < 2; ++pass)                /* the chosen row last: its ring lies in the gaps its neighbours' cards leave */
        for (int i = 0; i < PT_RL_ROWS; ++i)
            if ((pass == 1) == (l->first + i == l->cursor))
                FRRowDraw(l, i);
    sUiS = &sUiNavy;
}

static void FRelearnNote(const CtrPokeTouchView *v, int w)
{
    const CtrPokeTouchRelearn *l = &v->relearn;
    int c = FRCursorRow(l);

    EmSurface();
    LlWindow(NOTE_X, NOTE_Y, w, NOTE_H);
    if (c >= 0 && l->row[c].move) {
        Em2Text(l->row[c].name, NOTE_X + 13, NOTE_Y + 8, EM_DARK, EM_DARK_SH, NOTE_X + w - 13);
        LlRule(NOTE_X + 13, NOTE_Y + 21, w - 26);
        BDescribeN(l->desc, NOTE_X + 13, NOTE_Y + 25, w - 26, 3, EM_DIM);
    }
    sUiS = &sUiNavy;
}
