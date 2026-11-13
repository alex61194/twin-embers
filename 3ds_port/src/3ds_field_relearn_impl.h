/* The move relearner, in the viewport's place (3ds_poketouch.h CtrPokeTouchRelearn), included at the end of 3ds_poketouch.c after the
 * move to forget (3ds_field_learn_impl.h), whose look it shares: the question in a FireRed text window with two scroll buttons, the Pokemon
 * (front picture on a plate, level in a ribbon, nickname) with the selected move's PP, power and accuracy, a window of the list as rows
 * (type badge on the right, the chosen one flame with the select ring, CANCEL in slate), the move's description in the note.
 * Every word but the three labels is FireRed's (CtrBottomRelearn_*).
 * What FireRed has up besides the list (its questions "Teach X?", "Stop learning X?", "Delete an older move...?" and its messages) is
 * drawn over the dimmed list with the Field Bag's pieces (3ds_field_bag_impl.h: its text window, YES and NO as cards, the message with
 * "Tap to continue"): the screen is never FireRed's own picture in the viewport. */

/* The question's window (up to five lines of the long one) over YES and NO; the message's window with its "Tap to continue". Both are as
 * high as their text needs and stand in the middle of the area (the question with its buttons under it). */
#define FR_TEXT_W 204                                   /* the text's width in both windows (232 - 2 x 14) */

/* How many lines Em2Wrap sets `text` in at `width` (at most `maxLines`): it breaks at spaces the same way. */
static int FRLines(const char *text, int width, int maxLines)
{
    char line[100], joined[100], word[40];
    const char *p = text;
    int lines = 0;

    line[0] = 0;
    for (;;) {
        int w = 0;

        while (*p == ' ' || *p == '\n')
            ++p;
        if (!*p)
            break;
        while (*p && *p != ' ' && *p != '\n' && w < (int)sizeof(word) - 1)
            word[w++] = *p++;
        word[w] = 0;
        Em2Join(joined, sizeof(joined), line, word);
        if (line[0] && UiTextWidth(joined, 1) > width) {
            ++lines;
            Em2Join(line, sizeof(line), "", word);
        } else
            memcpy(line, joined, sizeof(line));
    }
    if (line[0])
        ++lines;
    return lines < 1 ? 1 : lines > maxLines ? maxLines : lines;
}

static CtrPokeTouchRect FRAsk(const CtrPokeTouchRelearn *l)
{
    int lines = FRLines(l->message, FR_TEXT_W, 5), h = 14 * lines + 20;
    CtrPokeTouchRect r = {8, (int16_t)((168 - h - 40) / 2), 232, (int16_t)h};

    return r;
}

static CtrPokeTouchRect FRButton(const CtrPokeTouchRelearn *l, int no)
{
    CtrPokeTouchRect a = FRAsk(l), r = {(int16_t)(no ? 128 : 30), (int16_t)(a.y + a.h + 12), 90, 30};

    return r;
}

static CtrPokeTouchRect FRSay(const CtrPokeTouchRelearn *l)
{
    int lines = FRLines(l->message, FR_TEXT_W, 4), h = 14 * lines + 20 + (l->tap ? 16 : 0);
    CtrPokeTouchRect r = {8, (int16_t)((168 - h) / 2), 232, (int16_t)h};

    return r;
}

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
    if (l->panel == PT_RL_PANEL_YESNO) {
        CtrPokeTouchRect yes = FRButton(l, 0), no = FRButton(l, 1);

        return UiIn(&yes, x, y) ? PT_RL_HIT_YES : UiIn(&no, x, y) ? PT_RL_HIT_NO : -1;
    }
    if (l->panel == PT_RL_PANEL_MESSAGE)
        return l->tap && x < 248 && y < 168 ? PT_RL_HIT_MESSAGE : -1;       /* anywhere above the note: it waits for A */
    if (l->panel == PT_RL_PANEL_BLANK)
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

/* The question over the dimmed list: FireRed's words in the text window, YES and NO under it (the cursor's one flame with the ring). */
static void FRQuestion(const CtrPokeTouchRelearn *l)
{
    CtrPokeTouchRect ask = FRAsk(l), yes = FRButton(l, 0), no = FRButton(l, 1);

    FBWindow(&ask);
    Em2Wrap(l->message, ask.x + 14, ask.y + 11, FR_TEXT_W, 5, 14, EM_DARK, EM_DARK_SH);
    FBLlButton(&yes, LlPick(l->menuCursor == 0, &kEmCream), CtrUiText(UIS_YES), l->pressed == PT_RL_HIT_YES, l->menuCursor == 0);
    FBLlButton(&no, LlPick(l->menuCursor == 1, &kEmSlate), CtrUiText(UIS_NO), l->pressed == PT_RL_HIT_NO, l->menuCursor == 1);
}

/* A message over the dimmed list; "Tap to continue" and its arrow only where it waits for A (the last one: a fade or a fanfare is not asked). */
static void FRMessage(const CtrPokeTouchRelearn *l)
{
    CtrPokeTouchRect box = FRSay(l);
    const CtrPokeTouchRect *m = &box;
    const char *tap = CtrUiText(UIS_TAP_TO_CONTINUE);

    FBWindow(m);
    Em2Wrap(l->message, m->x + 14, m->y + 11, FR_TEXT_W, 4, 14, EM_DARK, EM_DARK_SH);
    if (l->tap) {
        UiText(tap, m->x + m->w - 14 - UiTextWidth(tap, 1) - 9, m->y + m->h - 17, EM_DIM, EM_DARK_SH, 1, m->x + m->w - 8);
        LlArrow(m->x + m->w - 19, m->y + m->h - 16, 8, 5, 1, LL_CURSOR);
    }
}

static void FRelearnFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchRelearn *l = &v->relearn;
    CtrPokeTouchRect band = sFRBand;

    EmSurface();
    TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    if (l->panel == PT_RL_PANEL_BLANK) {                /* being set up: the backdrop, nothing of FireRed's */
        sUiS = &sUiNavy;
        return;
    }
    LlWindow(band.x, band.y, band.w, band.h);
    Em2Text(l->question, band.x + 12, band.y + (band.h - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, sFRUp.x - 2);
    FRArrow(&sFRUp, 0, l->first > 0 || l->cursor > 0, l->pressed == PT_RL_HIT_UP);
    FRArrow(&sFRDown, 1, l->cursor < l->count, l->pressed == PT_RL_HIT_DOWN);
    FRSidePanel(l);
    for (int pass = 0; pass < 2; ++pass)                /* the chosen row last: its ring lies in the gaps its neighbours' cards leave */
        for (int i = 0; i < PT_RL_ROWS; ++i)
            if ((pass == 1) == (l->first + i == l->cursor))
                FRRowDraw(l, i);
    if (l->panel == PT_RL_PANEL_YESNO || l->panel == PT_RL_PANEL_MESSAGE) {
        CtrPokeTouchRect dim = {0, 0, 248, 168};

        UiDim(&dim);
        if (l->panel == PT_RL_PANEL_YESNO)
            FRQuestion(l);
        else
            FRMessage(l);
    }
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
