/* Easy Chat over the whole lower screen (3ds_poketouch.h CtrPokeTouchEasy), included at the end of 3ds_poketouch.c after the PC box, whose
 * pieces (the raised cards FB_DEPTH, the dark slot) it shares. A remote for the game's own screen, in the Llama look (the approved mockup): the
 * phrase as chips in two rows with the mode buttons and DELETE ALL / QUIT / OK; the group menu (a grid of the groups, A-Z and BACK) or, in the
 * A-Z order, the letters; a group's words in two columns with a rail (a slider) and a letter strip (one slider); and the game's Yes/No
 * questions in a window over the dimmed phrase. A word or group that does not fit its card ends in "..." (Em2Fit).
 * The screen is drawn in regions (each a rectangle, with the 2 px its select ring may use); a region is drawn again only when what it shows
 * changed (FEasySig), so the incremental redraw is the whole one. A change of screen or of question redraws everything. */

#define FE_SLOT_TOP UI_RGB(62, 14, 14)
#define FE_SLOT_BOT UI_RGB(42, 8, 9)
#define FE_SLOT_EDGE UI_RGB(20, 3, 4)
#define FE_DIM UI_RGB(20, 0, 0)
#define FE_LIT UI_RGB(255, 196, 90)
#define FE_LETTER_SH UI_RGB(20, 24, 44)

enum { FER_HEADER, FER_CHIPS, FER_MODES, FER_NOTE, FER_FOOTER, FER_GRID, FER_ROWS, FER_RAIL, FER_STRIP, FER_PREVIEW, FER_BUTTONS, FER_COUNT };

static const CtrPokeTouchRect sFERect[FER_COUNT] = {
    {4, 4, 312, 22},        /* the header band */
    {4, 30, 312, 80},       /* the phrase's window */
    {4, 114, 312, 28},      /* the two mode buttons */
    {4, 160, 312, 40},      /* the note (the group screens' too: it is drawn at the bottom) */
    {4, 204, 312, 30},      /* DELETE ALL, QUIT, OK */
    {4, 28, 312, 174},      /* the groups or the letters, with A-Z and BACK under them */
    {4, 28, 250, 142},      /* the list's rows */
    {258, 28, 20, 142},     /* the rail between its arrows */
    {284, 28, 32, 140},     /* the letter strip */
    {4, 174, 312, 26},      /* the phrase as a line */
    {4, 204, 312, 30},      /* BACK, the order, USE WORD */
};
static const uint8_t sFEMargin[FER_COUNT] = {0, 0, 2, 0, 2, 2, 2, 0, 0, 0, 2};
static const CtrPokeTouchRect sFEModeGroups = {4, 114, 152, 28}, sFEModeAlpha = {160, 114, 156, 28};
static const CtrPokeTouchRect sFEFooter[3] = {{4, 204, 88, 30}, {96, 204, 104, 30}, {204, 204, 112, 30}};
static const CtrPokeTouchRect sFEToggle = {82, 173, 75, 26}, sFEBack = {160, 173, 156, 26};
static const CtrPokeTouchRect sFEListBack = {4, 204, 80, 30}, sFEListToggle = {88, 204, 110, 30}, sFEListPlace = {202, 204, 114, 30};
static const CtrPokeTouchRect sFEUp = {258, 28, 20, 18}, sFEDown = {258, 150, 20, 18}, sFERail = {258, 48, 20, 100};
static const CtrPokeTouchRect sFEStrip = {284, 28, 32, 140};
static const CtrPokeTouchRect sFEAskWin = {30, 60, 260, 104}, sFEYes = {50, 122, 100, 32}, sFENo = {170, 122, 100, 32};

static CtrPokeTouchRect FEChip(const CtrPokeTouchEasy *e, int i)
{
    int n = e->numWords < 1 ? 1 : e->numWords > PT_EC_WORDS ? PT_EC_WORDS : e->numWords, per = (n + 1) / 2, row = i / per, col = i % per;
    int w = (292 - 4 * (per - 1)) / per, left = 12 + (292 - (per * w + 4 * (per - 1))) / 2;
    CtrPokeTouchRect r = {(int16_t)(left + col * (w + 4)), (int16_t)(50 + 27 * row), (int16_t)w, 23};

    return r;
}

static CtrPokeTouchRect FEGroupCell(int i)
{
    CtrPokeTouchRect r = {(int16_t)(4 + 78 * (i % 4)), (int16_t)(28 + 29 * (i / 4)), 75, 26};

    return r;
}

/* The letters lie as the game's keys do: rows of 6, 6, 7 and 7. */
static CtrPokeTouchRect FELetterCell(int letter)
{
    static const uint8_t kStart[4] = {0, 6, 12, 19};
    int row = letter < 6 ? 0 : letter < 12 ? 1 : letter < 19 ? 2 : 3;
    CtrPokeTouchRect r = {(int16_t)(4 + 45 * (letter - kStart[row])), (int16_t)(28 + 29 * row), 41, 26};

    return r;
}

static CtrPokeTouchRect FEListRow(int k)
{
    CtrPokeTouchRect r = {(int16_t)(4 + 126 * (k % 2)), (int16_t)(28 + 24 * (k / 2)), 124, 21};

    return r;
}

static int FERows(const CtrPokeTouchEasy *e)
{
    return (e->listCount + 1) / 2;
}

/* The rail's thumb: its top and height in the rail's 100 px. */
static void FEThumb(const CtrPokeTouchEasy *e, int *top, int *h)
{
    int total = FERows(e), range = total - PT_EC_ROWS;

    *top = 0;
    *h = sFERail.h;
    if (range < 1)
        return;
    *h = sFERail.h * PT_EC_ROWS / total;
    if (*h < 14)
        *h = 14;
    *top = (int)e->first * (sFERail.h - *h) / range;
}

int CtrPokeTouch_EasyRailTarget(const CtrPokeTouchEasy *e, int y)
{
    int range = FERows(e) - PT_EC_ROWS, top, h, t;

    if (range < 1)
        return -1;
    FEThumb(e, &top, &h);
    t = y - sFERail.y - h / 2;
    t = t < 0 ? 0 : t > sFERail.h - h ? sFERail.h - h : t;
    return (t * range * 2 + (sFERail.h - h)) / (2 * (sFERail.h - h));
}

int CtrPokeTouch_EasyStripLetter(int x, int y)
{
    int col, row;

    if (x < sFEStrip.x || x >= sFEStrip.x + sFEStrip.w || y < sFEStrip.y || y >= sFEStrip.y + sFEStrip.h)
        return -1;
    col = (x - sFEStrip.x) / 16;
    row = (y - 32) / 10;
    row = row < 0 ? 0 : row > 12 ? 12 : row;
    return col * 13 + row;
}

int CtrPokeTouch_EasyHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchEasy *e = &v->ec;

    if (!e->on)
        return -1;
    if (e->ask) {
        if (UiIn(&sFEYes, x, y))
            return PT_EC_HIT_YES;
        return UiIn(&sFENo, x, y) ? PT_EC_HIT_NO : -1;
    }
    if (e->screen == PT_EC_EDIT) {
        for (int i = 0; i < e->numWords && i < PT_EC_WORDS; ++i) {
            CtrPokeTouchRect r = FEChip(e, i);
            if (UiIn(&r, x, y))
                return PT_EC_HIT_CHIP + i;
        }
        if (UiIn(&sFEModeGroups, x, y))
            return PT_EC_HIT_MODE_GROUPS;
        if (UiIn(&sFEModeAlpha, x, y))
            return PT_EC_HIT_MODE_ALPHA;
        for (int i = 0; i < 3; ++i)
            if (UiIn(&sFEFooter[i], x, y))
                return PT_EC_HIT_DELETE + i;
        return -1;
    }
    if (e->screen == PT_EC_GROUP || e->screen == PT_EC_LETTER) {
        int n = e->screen == PT_EC_GROUP ? e->groupCount : PT_EC_LETTERS;

        for (int i = 0; i < n && i < PT_EC_GROUPS + 5; ++i) {
            CtrPokeTouchRect r = e->screen == PT_EC_GROUP ? FEGroupCell(i) : FELetterCell(i);
            if (UiIn(&r, x, y))
                return PT_EC_HIT_CELL + i;
        }
        if (UiIn(&sFEToggle, x, y))
            return PT_EC_HIT_CELL + PT_EC_CELL_TOGGLE;
        return UiIn(&sFEBack, x, y) ? PT_EC_HIT_CELL + PT_EC_CELL_BACK : -1;
    }
    for (int k = 0; k < 2 * PT_EC_ROWS; ++k) {
        CtrPokeTouchRect r = FEListRow(k);
        if (e->list[k][0] && UiIn(&r, x, y))
            return PT_EC_HIT_ROW + k;
    }
    if (UiIn(&sFEUp, x, y))
        return PT_EC_HIT_UP;
    if (UiIn(&sFEDown, x, y))
        return PT_EC_HIT_DOWN;
    if (UiIn(&sFERail, x, y))
        return PT_EC_HIT_RAIL;
    if (!e->alpha && UiIn(&sFEStrip, x, y))
        return PT_EC_HIT_STRIP;
    if (UiIn(&sFEListBack, x, y))
        return PT_EC_HIT_BACK;
    if (UiIn(&sFEListToggle, x, y))
        return PT_EC_HIT_TOGGLE;
    return UiIn(&sFEListPlace, x, y) ? PT_EC_HIT_PLACE : -1;
}

/* ---- what a region shows (its signature) ---- */

static uint32_t FEHash(uint32_t h, const void *p, size_t n)
{
    const uint8_t *b = (const uint8_t *)p;

    for (size_t i = 0; i < n; ++i)
        h = (h ^ b[i]) * 16777619u;
    return h;
}

static uint32_t FEHashStr(uint32_t h, const char *s)
{
    return FEHash(h, s, strlen(s) + 1);
}

static uint32_t FEHashInt(uint32_t h, int v)
{
    return FEHash(h, &v, sizeof(v));
}

static int FEIn(const CtrPokeTouchEasy *e, int lo, int hi)
{
    return e->pressed >= lo && e->pressed <= hi ? e->pressed : -1;
}

static int FEFilled(const CtrPokeTouchEasy *e)
{
    int n = 0;

    for (int i = 0; i < e->numWords && i < PT_EC_WORDS; ++i)
        n += e->chip[i][0] != 0;
    return n;
}

static uint32_t FESig(const CtrPokeTouchEasy *e, int region)
{
    uint32_t h = 2166136261u;

    switch (region) {
    case FER_HEADER:
        h = FEHashInt(h, e->screen * 4 + e->alpha * 2 + 1);
        h = FEHashInt(FEHashInt(FEHashInt(h, e->numWords), FEFilled(e)), e->groupCount);
        h = FEHashInt(h, e->listCount);
        h = FEHashStr(FEHashStr(h, e->title), e->listTitle);
        break;
    case FER_CHIPS:
        h = FEHashInt(FEHashInt(h, e->numWords), e->cursor);
        h = FEHashInt(h, FEIn(e, PT_EC_HIT_CHIP, PT_EC_HIT_CHIP + PT_EC_WORDS - 1));
        for (int i = 0; i < PT_EC_WORDS; ++i)
            h = FEHashStr(h, e->chip[i]);
        break;
    case FER_MODES:
        h = FEHashInt(FEHashInt(h, e->wantAlpha), FEIn(e, PT_EC_HIT_MODE_GROUPS, PT_EC_HIT_MODE_ALPHA));
        break;
    case FER_FOOTER:
        h = FEHashInt(FEHashInt(h, e->footer), FEIn(e, PT_EC_HIT_DELETE, PT_EC_HIT_CONFIRM));
        break;
    case FER_GRID:
        h = FEHashInt(FEHashInt(FEHashInt(h, e->groupCount), e->cell), e->alpha);
        h = FEHashInt(h, FEIn(e, PT_EC_HIT_CELL, PT_EC_HIT_CELL + PT_EC_CELL_BACK));
        for (int i = 0; i < PT_EC_GROUPS; ++i)
            h = FEHashStr(h, e->group[i]);
        break;
    case FER_ROWS:
        h = FEHashInt(FEHashInt(FEHashInt(h, e->listCount), e->first), e->word);
        h = FEHashInt(h, FEIn(e, PT_EC_HIT_ROW, PT_EC_HIT_ROW + 2 * PT_EC_ROWS - 1));
        for (int k = 0; k < 2 * PT_EC_ROWS; ++k)
            h = FEHashStr(h, e->list[k]);
        break;
    case FER_RAIL:
        h = FEHashInt(FEHashInt(h, e->listCount), e->first);
        h = FEHashInt(h, FEIn(e, PT_EC_HIT_UP, PT_EC_HIT_RAIL));
        break;
    case FER_STRIP:
        h = FEHashInt(FEHashInt(h, e->letter), e->alpha);
        break;
    case FER_PREVIEW:
        for (int i = 0; i < PT_EC_WORDS; ++i)
            h = FEHashStr(h, e->chip[i]);
        h = FEHashInt(h, e->numWords);
        break;
    case FER_BUTTONS:
        h = FEHashInt(FEHashInt(h, e->alpha), FEIn(e, PT_EC_HIT_BACK, PT_EC_HIT_PLACE));
        break;
    default:                                /* the note: the same whenever it is drawn */
        break;
    }
    return h;
}

/* ---- the pieces ---- */

static void FEButton(const CtrPokeTouchRect *r, const EmPal *p, const char *label, int pressed, int ring)
{
    char fit[48];
    int fy;

    if (ring)
        LlSelectRing(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, p, FB_DEPTH, pressed);
    Em2Fit(label, r->w - 8, fit, sizeof(fit));
    EmLabel(fit, r->x, r->w, fy, r->h - FB_DEPTH, p);
}

static void FESlot(const CtrPokeTouchRect *r)
{
    UiRoundFill(r->x, r->y, r->w, r->h, 5, FE_SLOT_EDGE);
    EmGradFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, 4, FE_SLOT_TOP, FE_SLOT_BOT);
}

static void FEArrowButton(const CtrPokeTouchRect *r, int dir, int on, int pressed)
{
    int fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, FB_DEPTH, pressed);

    LlArrow(r->x + (r->w - 8) / 2, fy + (r->h - FB_DEPTH - 5) / 2, 8, 5, dir, on ? LL_CURSOR : FB_IDLE_ARROW);
}

/* The phrase as one line (the preview, the editor's word count): the words with a space between, empty slots left out. */
static void FEPhrase(const CtrPokeTouchEasy *e, char *out, size_t cap)
{
    out[0] = 0;
    for (int i = 0; i < e->numWords && i < PT_EC_WORDS; ++i)
        if (e->chip[i][0])
            Em2Join(out, cap, out, e->chip[i]);
}

static void FEHeader(const CtrPokeTouchEasy *e)
{
    char right[64], left[PT_EC_ASK_LEN];
    int fy;

    if (e->screen == PT_EC_EDIT || e->ask) {
        snprintf(right, sizeof(right), CTR_TR("Words %d of %d", "Palabras %d de %d", "W\xC3\xB6rter %d von %d"), FEFilled(e), e->numWords);
        snprintf(left, sizeof(left), "%.47s", e->title);
    } else if (e->screen == PT_EC_GROUP) {
        snprintf(right, sizeof(right), CTR_TR("%d groups", "%d grupos", "%d Gruppen"), e->groupCount);
        snprintf(left, sizeof(left), "%s", CTR_TR("Pick a group", "Elige un grupo", "Gruppe w\xC3\xA4hlen"));
    } else if (e->screen == PT_EC_LETTER) {
        snprintf(right, sizeof(right), "A-Z");
        snprintf(left, sizeof(left), "%s", CTR_TR("Pick a letter", "Elige una letra", "Buchstabe w\xC3\xA4hlen"));
    } else {
        snprintf(right, sizeof(right), "%s", CTR_TR("Order: A-Z", "Orden: A-Z", "Reihenfolge: A-Z"));
        snprintf(left, sizeof(left), "%.19s", e->listTitle);
    }
    fy = EmButton(4, 4, 312, 22, &kEmCream, FB_DEPTH, 0);
    LlRibbonBox(5, fy + 1, 150, 18, 7, LL_TL | LL_BL, LL_RIB_TOP, LL_RIB_BOT, left);
    Em2Text(right, 164, fy + (20 - UI_TEXT_H) / 2, EM_DIM, EM_DARK_SH, 312);
}

static void FEChips(const CtrPokeTouchEasy *e)
{
    int n = e->numWords < 1 ? 1 : e->numWords > PT_EC_WORDS ? PT_EC_WORDS : e->numWords, per = (n + 1) / 2;

    LlWindow(4, 30, 312, 78);
    LlRibbon(5, 31, CTR_TR("Message", "Mensaje", "Nachricht"), LL_RIB_TOP, LL_RIB_BOT);
    for (int i = 0; i < 2 * per; ++i) {
        CtrPokeTouchRect r = FEChip(e, i);
        const char *word = i < n ? e->chip[i] : "";
        int sel = i < n && e->cursor == i;

        if (!sel && !word[0]) {
            FESlot(&r);
            continue;
        }
        FEButton(&r, sel ? &kEmFlame : &kEmCream, word[0] ? word : CTR_TR("(empty)", "(vac\xC3\xADo)", "(leer)"), e->pressed == PT_EC_HIT_CHIP + i, sel);
    }
}

static void FEModes(const CtrPokeTouchEasy *e)
{
    FEButton(&sFEModeGroups, e->wantAlpha ? &kEmCream : &kEmFlame, CTR_TR("Pick by group", "Elegir por grupos", "Nach Gruppen"),
             e->pressed == PT_EC_HIT_MODE_GROUPS, 0);
    FEButton(&sFEModeAlpha, e->wantAlpha ? &kEmFlame : &kEmCream, CTR_TR("Pick from A to Z", "Elegir de la A a la Z", "Von A bis Z"),
             e->pressed == PT_EC_HIT_MODE_ALPHA, 0);
}

static void FENote(const CtrPokeTouchEasy *e)
{
    const char *a, *b = NULL;

    if (e->screen == PT_EC_EDIT || e->ask) {
        a = CTR_TR("Tap a box, then pick its word.", "Toca una casilla y elige la palabra.", "Tippe ein Feld und w\xC3\xA4hle das Wort.");
        b = CTR_TR("Tap a word to change it.", "Toca una palabra para cambiarla.", "Tippe ein Wort, um es zu \xC3\xA4ndern.");
    } else if (e->screen == PT_EC_GROUP)
        a = CTR_TR("Tap a group to see its words.", "Toca un grupo para ver sus palabras.", "Tippe eine Gruppe, um ihre W\xC3\xB6rter zu sehen.");
    else
        a = CTR_TR("Tap a letter to see its words.", "Toca una letra para ver sus palabras.", "Tippe einen Buchstaben, um die W\xC3\xB6rter zu sehen.");
    if (b) {
        LlWindow(4, 160, 312, 38);
        Em2Text(a, 16, 166, EM_DARK, EM_DARK_SH, 308);
        Em2Text(b, 16, 181, EM_DIM, EM_DARK_SH, 308);
    } else {
        LlWindow(4, 204, 312, 30);
        Em2Text(a, 16, 213, EM_DIM, EM_DARK_SH, 308);
    }
}

static void FEFooter(const CtrPokeTouchEasy *e)
{
    static const EmPal *const kPal[3] = {&kEmCream, &kEmSlate, &kEmGold};
    const char *label[3] = {CTR_TR("Clear", "Borrar", "Leeren"), CTR_TR("Cancel", "Cancelar", "Abbrechen"),
                            CTR_TR("Confirm", "Confirmar", "Best\xC3\xA4tigen")};

    for (int i = 0; i < 3; ++i)
        FEButton(&sFEFooter[i], kPal[i], label[i], e->pressed == PT_EC_HIT_DELETE + i, e->footer == i);
}

static void FEGrid(const CtrPokeTouchEasy *e)
{
    int letters = e->screen == PT_EC_LETTER;
    int n = letters ? PT_EC_LETTERS : e->groupCount > PT_EC_GROUPS ? PT_EC_GROUPS : e->groupCount;

    for (int pass = 0; pass < 2; ++pass)               /* the selected last: its ring lies in the gaps its neighbours leave */
        for (int i = 0; i < n; ++i) {
            CtrPokeTouchRect r = letters ? FELetterCell(i) : FEGroupCell(i);
            char one[2] = {(char)('A' + i), 0};
            int sel = e->cell == i;

            if ((pass == 1) != sel)
                continue;
            FEButton(&r, sel ? &kEmFlame : &kEmCream, letters ? one : e->group[i], e->pressed == PT_EC_HIT_CELL + i, sel);
        }
    FEButton(&sFEToggle, &kEmBlue, letters ? CTR_TR("Groups", "Grupos", "Gruppen") : "A-Z", e->pressed == PT_EC_HIT_CELL + PT_EC_CELL_TOGGLE,
             e->cell == PT_EC_CELL_TOGGLE);
    FEButton(&sFEBack, &kEmSlate, CTR_TR("Back", "Atr\xC3\xA1s", "Zur\xC3\xBC" "ck"), e->pressed == PT_EC_HIT_CELL + PT_EC_CELL_BACK,
             e->cell == PT_EC_CELL_BACK);
}

static void FERowsDraw(const CtrPokeTouchEasy *e)
{
    for (int pass = 0; pass < 2; ++pass)
        for (int k = 0; k < 2 * PT_EC_ROWS; ++k) {
            CtrPokeTouchRect r = FEListRow(k);
            int sel = e->word == 2 * (e->first + k / 2) + k % 2, fy;
            const EmPal *p = sel ? &kEmFlame : &kEmCream;
            char fit[PT_EC_WORD_LEN + 4];

            if (!e->list[k][0] || (pass == 1) != sel)
                continue;
            if (sel)
                LlSelectRing(r.x, r.y, r.w, r.h);
            fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, e->pressed == PT_EC_HIT_ROW + k);
            if (sel)
                LlCursor(r.x + 6, fy + (r.h - FB_DEPTH - 10) / 2, EM_WHITE);
            Em2Fit(e->list[k], r.w - (sel ? 22 : 16), fit, sizeof(fit));
            UiText(fit, r.x + (sel ? 16 : 10), fy + (r.h - FB_DEPTH - UI_TEXT_H) / 2, p->ink, p->inkSh, 1, r.x + r.w - 4);
        }
}

static void FERailDraw(const CtrPokeTouchEasy *e)
{
    int top, h, rows = FERows(e);

    FEThumb(e, &top, &h);
    FEArrowButton(&sFEUp, 0, e->first > 0, e->pressed == PT_EC_HIT_UP);
    FEArrowButton(&sFEDown, 1, rows > PT_EC_ROWS && e->first + PT_EC_ROWS < rows, e->pressed == PT_EC_HIT_DOWN);
    LlScrollRail(sFERail.x, sFERail.y, sFERail.w, sFERail.h, top, h);
}

static void FEStripDraw(const CtrPokeTouchEasy *e)
{
    LlScrollRail(sFEStrip.x, sFEStrip.y, sFEStrip.w, sFEStrip.h, 0, 0);
    for (int i = 0; i < PT_EC_LETTERS; ++i) {
        char one[2] = {(char)('A' + i), 0};
        int cx = sFEStrip.x + 8 + 16 * (i / 13), lit = !e->alpha && e->letter == i;

        UiText(one, cx - UiTextWidth(one, 1) / 2, 32 + 10 * (i % 13), lit ? FE_LIT : e->alpha ? EM_DIM : EM_WHITE, FE_LETTER_SH, 1, cx + 8);
    }
}

static void FEButtons(const CtrPokeTouchEasy *e)
{
    FEButton(&sFEListBack, &kEmSlate, CTR_TR("Back", "Atr\xC3\xA1s", "Zur\xC3\xBC" "ck"), e->pressed == PT_EC_HIT_BACK, 0);
    FEButton(&sFEListToggle, &kEmCream, e->alpha ? CTR_TR("By groups", "Por grupos", "Nach Gruppen") : CTR_TR("A to Z", "De la A a la Z", "Von A bis Z"),
             e->pressed == PT_EC_HIT_TOGGLE, 0);
    FEButton(&sFEListPlace, &kEmGold, CTR_TR("Use word", "Poner palabra", "Wort setzen"), e->pressed == PT_EC_HIT_PLACE, 0);
}

static void FERegion(const CtrPokeTouchEasy *e, int id)
{
    char line[PT_EC_WORDS * PT_EC_WORD_LEN];

    EmSurface();
    switch (id) {
    case FER_HEADER: FEHeader(e); break;
    case FER_CHIPS: FEChips(e); break;
    case FER_MODES: FEModes(e); break;
    case FER_NOTE: FENote(e); break;
    case FER_FOOTER: FEFooter(e); break;
    case FER_GRID: FEGrid(e); break;
    case FER_ROWS: FERowsDraw(e); break;
    case FER_RAIL: FERailDraw(e); break;
    case FER_STRIP: FEStripDraw(e); break;
    case FER_PREVIEW:
        FEPhrase(e, line, sizeof(line));
        LlWindow(4, 174, 312, 24);
        Em2Text(line, 14, 180, EM_DARK, EM_DARK_SH, 308);
        break;
    default: FEButtons(e); break;
    }
    sUiS = &sUiNavy;
}

/* The regions of a screen, in drawing order; a question shows the phrase's. */
static int FERegions(const CtrPokeTouchEasy *e, int *ids)
{
    static const int8_t kEdit[5] = {FER_HEADER, FER_CHIPS, FER_MODES, FER_NOTE, FER_FOOTER};
    static const int8_t kGroup[3] = {FER_HEADER, FER_GRID, FER_NOTE};
    static const int8_t kList[6] = {FER_HEADER, FER_ROWS, FER_RAIL, FER_STRIP, FER_PREVIEW, FER_BUTTONS};
    const int8_t *src = e->ask || e->screen == PT_EC_EDIT ? kEdit : e->screen == PT_EC_LIST ? kList : kGroup;
    int n = e->ask || e->screen == PT_EC_EDIT ? 5 : e->screen == PT_EC_LIST ? 6 : 3;

    for (int i = 0; i < n; ++i)
        ids[i] = src[i];
    return n;
}

/* The question: the phrase dimmed, and the game's window over it with YES and NO (the game's cursor lit). */
static void FEAsk(const CtrPokeTouchEasy *e)
{
    const CtrPokeTouchRect *w = &sFEAskWin;
    char fit[PT_EC_ASK_LEN + 4];
    const char *sub = e->ask2[0] ? e->ask2 : e->ask == PT_EC_ASK_QUIT ? CTR_TR("Your changes will be lost.", "Se perder\xC3\xA1n los cambios.", "\xC3\x84nderungen gehen verloren.") : "";

    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x)
            EmBlend(x, y, FE_DIM, 153);
    EmSurface();
    LlWindow(w->x, w->y, w->w, w->h);
    Em2Fit(e->ask1, w->w - 20, fit, sizeof(fit));
    Em2Text(fit, w->x + (w->w - UiTextWidth(fit, 1)) / 2, w->y + 14, EM_DARK, EM_DARK_SH, w->x + w->w - 6);
    LlRule(w->x + 16, w->y + 34, w->w - 32);
    Em2Fit(sub, w->w - 20, fit, sizeof(fit));
    Em2Text(fit, w->x + (w->w - UiTextWidth(fit, 1)) / 2, w->y + 42, EM_DIM, EM_DARK_SH, w->x + w->w - 6);
    FEButton(&sFEYes, e->askCursor ? &kEmCream : &kEmFlame, CTR_TR("Yes", "S\xC3\xAD", "Ja"), e->pressed == PT_EC_HIT_YES, !e->askCursor);
    FEButton(&sFENo, e->askCursor ? &kEmFlame : &kEmCream, CTR_TR("No", "No", "Nein"), e->pressed == PT_EC_HIT_NO, e->askCursor);
    sUiS = &sUiNavy;
}

static void FEasyDraw(const CtrPokeTouchView *v)
{
    const CtrPokeTouchEasy *e = &v->ec;
    int ids[6], n = FERegions(e, ids);

    TsBackdrop();
    for (int i = 0; i < n; ++i)
        FERegion(e, ids[i]);
    if (e->ask)
        FEAsk(e);
}

static void FEasyUpdate(uint16_t *canvas, const CtrPokeTouchView *old, const CtrPokeTouchView *v, CtrDirtyList *dirty)
{
    const CtrPokeTouchEasy *o = &old->ec, *e = &v->ec;
    CtrPokeTouchRect whole = {0, 0, W, H};
    int ids[6], n;

    if (o->on != e->on) {                    /* it opens or closes: the whole screen, whichever it becomes */
        CtrPokeTouch_Draw(canvas, v);
        Mark(dirty, &whole);
        return;
    }
    if (o->screen != e->screen || o->ask != e->ask || e->ask || o->numWords != e->numWords) {
        FEasyDraw(v);
        Mark(dirty, &whole);
        return;
    }
    n = FERegions(e, ids);
    for (int i = 0; i < n; ++i) {
        int m = sFEMargin[ids[i]];
        CtrPokeTouchRect r = sFERect[ids[i]];

        if (FESig(o, ids[i]) == FESig(e, ids[i]))
            continue;
        r.x = (int16_t)(r.x - m);
        r.y = (int16_t)(r.y - m);
        r.w = (int16_t)(r.w + 2 * m);
        r.h = (int16_t)(r.h + 2 * m);
        TsBgRect(r.x, r.y, r.w, r.h);
        FERegion(e, ids[i]);
        Mark(dirty, &r);
    }
}
