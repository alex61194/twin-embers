/* The move to forget for a new one, in the viewport (3ds_poketouch.h CtrPokeTouchLearn), included at the end of
 * 3ds_poketouch.c after the Pokedex, whose picture helpers it shares. A remote for FireRed's Summary in that mode:
 * its question in a FireRed text window, the Pokemon (its front picture on a plate, level in a ribbon, nickname) with the selected move's PP, power and
 * accuracy, the four moves and the new one (purple; the chosen one flame with the select ring) as rows with their types, and the move's description in the note.
 * Every word but the three labels is FireRed's (CtrBottomLearn_*). */

static const CtrPokeTouchRect sFLBand = {4, 4, 240, 22};
static const CtrPokeTouchRect sFLSide = {4, 30, 96, 134};

static CtrPokeTouchRect FLRow(int i)
{
    CtrPokeTouchRect r = {104, (int16_t)(30 + 27 * i), 140, 25};
    return r;
}

int CtrPokeTouch_LearnHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchLearn *l = &v->learn;

    if (!l->on)
        return -1;
    for (int i = 0; i < PT_LEARN_ROWS; ++i) {
        CtrPokeTouchRect r = FLRow(i);
        if (l->row[i].move && UiIn(&r, x, y))
            return PT_LEARN_HIT_ROW + i;
    }
    return -1;
}

static void FLSideRow(const char *label, const char *value, int y)
{
    const CtrPokeTouchRect *s = &sFLSide;

    Em2Text(label, s->x + 10, y, EM_DIM, EM_DARK_SH, s->x + s->w - 10 - UiTextWidth(value, 1) - 4);
    UiTextRight(value, s->x + s->w - 10, y, EM_DARK, EM_DARK_SH, 1);
}

static void FLSidePanel(const CtrPokeTouchLearn *l)
{
    const CtrPokeTouchRect *s = &sFLSide;
    CtrPokeTouchRect name = {s->x, 0, s->w, 13};
    int c = l->cursor >= 0 && l->cursor < PT_LEARN_ROWS ? l->cursor : 0;

    EmButton(s->x, s->y, s->w, s->h, &kEmCream, 2, 0);
    LlPlate(s->x + 16, s->y + 8, 64, LL_SLATE, EM_WHITE, 1);
    FDPicDraw(l->species, s->x + (s->w - 64) / 2, s->y + 8, 0);
    LlRibbonBox(s->x, s->y, UiTextWidth(l->level, 1) + 18, 11, 7, LL_TL, LL_RIB_TOP, LL_RIB_BOT, l->level);
    UiTextCentered(l->nickname, &name, s->y + 83, EM_DARK, EM_DARK_SH, 1);
    LlRule(s->x + 8, s->y + 97, s->w - 16);
    if (l->row[c].move) {
        FLSideRow(CtrUiText(UIS_MOVE_PP), l->row[c].pp, s->y + 100);
        FLSideRow(CtrUiText(UIS_MOVE_POWER), l->row[c].power, s->y + 110);
        FLSideRow(CtrUiText(UIS_MOVE_ACCURACY), l->row[c].accuracy, s->y + 120);
    }
}

static void FLRowDraw(const CtrPokeTouchLearn *l, int i)
{
    CtrPokeTouchRect r = FLRow(i);
    int chosen = l->cursor == i, fy, bw = 0;
    const EmPal *p = LlPick(chosen, i == PT_LEARN_ROWS - 1 ? &kEmPurple : &kEmCream);

    if (!l->row[i].move)
        return;
    if (chosen)
        LlSelectRing(r.x, r.y, r.w, r.h);
    fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, l->pressed == PT_LEARN_HIT_ROW + i);
    if (chosen)
        LlCursor(r.x + 8, fy + (r.h - FB_DEPTH - 10) / 2, EM_WHITE);
    if (l->row[i].type >= 0 && UiTextWidth(l->row[i].name, 1) + 18 + UiTextWidth(l->row[i].typeName, 1) + 12 + 14 <= r.w) {   /* the type's badge only where the name stays whole */
        bw = UiTextWidth(l->row[i].typeName, 1) + 12;
        LlTypeBadge(r.x + r.w - 5 - bw, fy + (r.h - FB_DEPTH - 13) / 2, l->row[i].typeName, UiTypeColor(l->row[i].type));
    }
    if (!bw && l->row[i].type >= 0 && !chosen)
        UiRoundFill(r.x + 1, fy + 1, 5, r.h - FB_DEPTH - 2, 2, UiTypeColor(l->row[i].type));
    Em2Text(l->row[i].name, r.x + 18, fy + (r.h - FB_DEPTH - UI_TEXT_H) / 2, p->ink, p->inkSh, r.x + r.w - 9 - bw);
}

static void FLearnFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchLearn *l = &v->learn;
    CtrPokeTouchRect band = sFLBand;

    EmSurface();
    TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    LlWindow(band.x, band.y, band.w, band.h);
    Em2Text(l->question, band.x + 12, band.y + (band.h - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, band.x + band.w - 10);
    FLSidePanel(l);
    for (int i = 0; i < PT_LEARN_ROWS; ++i)
        FLRowDraw(l, i);
    sUiS = &sUiNavy;
}

static void FLearnNote(const CtrPokeTouchView *v, int w)
{
    const CtrPokeTouchLearn *l = &v->learn;
    int c = l->cursor >= 0 && l->cursor < PT_LEARN_ROWS ? l->cursor : 0;

    EmSurface();
    LlWindow(NOTE_X, NOTE_Y, w, NOTE_H);
    if (l->row[c].move) {
        Em2Text(l->row[c].name, NOTE_X + 13, NOTE_Y + 8, EM_DARK, EM_DARK_SH, NOTE_X + w - 13);
        LlRule(NOTE_X + 13, NOTE_Y + 21, w - 26);
        BDescribeN(l->desc, NOTE_X + 13, NOTE_Y + 25, w - 26, 3, EM_DIM);
    }
    sUiS = &sUiNavy;
}
