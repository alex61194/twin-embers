/* The move to forget for a new one, in the viewport (3ds_poketouch.h CtrPokeTouchLearn), included at the end of
 * 3ds_poketouch.c after the Pokedex, whose picture helpers it shares. A remote for FireRed's Summary in that mode:
 * its question on a band, the Pokemon (its front picture, nickname and level) with the selected move's PP, power and
 * accuracy, the four moves and the new one (violet) as rows with their types, and the move's description in the note.
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

    UiText(label, s->x + 8, y, EM_DIM, EM_DARK_SH, 1, s->x + s->w - 8 - UiTextWidth(value, 1) - 4);
    UiTextRight(value, s->x + s->w - 8, y, EM_VALUE, EM_DARK_SH, 1);
}

static void FLSidePanel(const CtrPokeTouchLearn *l)
{
    const CtrPokeTouchRect *s = &sFLSide;
    CtrPokeTouchRect name = {s->x, 0, s->w, 13};
    int c = l->cursor >= 0 && l->cursor < PT_LEARN_ROWS ? l->cursor : 0;

    FBCard(s);
    UiRoundFill(s->x + 6, s->y + 4, s->w - 12, 68, 5, UI_RGB(214, 204, 192));
    EmGradFill(s->x + 7, s->y + 5, s->w - 14, 66, 4, UI_RGB(255, 246, 226), UI_RGB(246, 222, 186));
    FDPicDraw(l->species, s->x + (s->w - 64) / 2, s->y + 6, 0);
    UiText(l->level, s->x + 10, s->y + 8, EM_DIM, EM_DARK_SH, 1, s->x + 40);
    UiTextCentered(l->nickname, &name, s->y + 76, EM_DARK, EM_DARK_SH, 1);
    Fill(s->x + 6, s->y + 91, s->w - 12, 1, UI_RGB(214, 204, 192));
    if (l->row[c].move) {
        FLSideRow(CtrUiText(UIS_MOVE_PP), l->row[c].pp, s->y + 95);
        FLSideRow(CtrUiText(UIS_MOVE_POWER), l->row[c].power, s->y + 107);
        FLSideRow(CtrUiText(UIS_MOVE_ACCURACY), l->row[c].accuracy, s->y + 119);
    }
}

static void FLRowDraw(const CtrPokeTouchLearn *l, int i)
{
    CtrPokeTouchRect r = FLRow(i);
    const EmPal *p = l->cursor == i ? &kEmAmber : i == PT_LEARN_ROWS - 1 ? &kEmViolet : &kEmCream;
    int fy;

    if (!l->row[i].move)
        return;
    fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, l->pressed == PT_LEARN_HIT_ROW + i);
    if (l->row[i].type >= 0) {
        CtrPokeTouchRect t = {(int16_t)(r.x + 4), (int16_t)(fy + 4), 46, 15};

        UiTypeBadge(&t, l->row[i].type, l->row[i].typeName);
    }
    UiText(l->row[i].name, r.x + 56, fy + 6, p->ink, p->inkSh, 1, r.x + r.w - 4);
}

static void FLearnFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchLearn *l = &v->learn;
    CtrPokeTouchRect band = sFLBand;

    EmSurface();
    TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    UiRoundFill(band.x, band.y, band.w, band.h, 5, kEmAmber.lip);
    EmGradFill(band.x + 1, band.y + 1, band.w - 2, band.h - 2, 4, kEmAmber.top, kEmAmber.bottom);
    band.h = 13;
    UiTextCentered(l->question, &band, sFLBand.y + 5, kEmAmber.ink, kEmAmber.inkSh, 1);
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
    EmNote(NOTE_X, NOTE_Y, w, NOTE_H);
    if (l->row[c].move) {
        UiText(l->row[c].name, NOTE_X + 12, NOTE_Y + 6, EM_DARK, EM_DARK_SH, 1, NOTE_X + w - 4);
        BDescribeN(l->desc, NOTE_X + 12, NOTE_Y + 21, w - 18, 3, EM_DIM);
    }
    sUiS = &sUiNavy;
}
