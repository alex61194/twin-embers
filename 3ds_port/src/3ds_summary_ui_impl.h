/* The Pokemon Summary on the lower screen (3ds_poketouch.h CtrSummaryUi_*), included at the end of 3ds_poketouch.c
 * after the battle screen, whose cards, PP colours, text wrapping and Party icon cache it shares: one Summary for the
 * field's Party and the battle's, in the same design as the battle screen: PokeTouch's red (sUiRed). It only shows what FireRed's Summary reports;
 * every page, Pokemon and move cursor is FireRed's own (3ds_bottom.c walks them with FireRed's keys). */

static const CtrPokeTouchRect sSHeader = {3, 3, 314, 38};
static const CtrPokeTouchRect sSPrev = {244, 6, 34, 32}, sSNext = {281, 6, 34, 32};
static const CtrPokeTouchRect sSTab[SUI_TABS] = {{3, 45, 77, 24}, {82, 45, 77, 24}, {161, 45, 77, 24}, {240, 45, 77, 24}};
static const CtrPokeTouchRect sSBody = {3, 72, 314, 138};
static const CtrPokeTouchRect sSFooter = {3, 213, 230, 24};
static const CtrPokeTouchRect sSBack = {236, 213, 81, 24};
static const char *const sSTabLabel[SUI_TABS] = {"INFO", "STATS", "MOVES", "TRAINER"};

/* The moves page's cards (2x2) and the details' rows (four moves, then CANCEL) and panel. */
static CtrPokeTouchRect SCard(int i)
{
    CtrPokeTouchRect r = {(int16_t)(i & 1 ? 162 : 7), (int16_t)(i & 2 ? 142 : 76), 151, 64};
    return r;
}

static CtrPokeTouchRect SRow(int i)
{
    CtrPokeTouchRect r = {7, (int16_t)(76 + 26 * i), 151, 24};
    return r;
}
static const CtrPokeTouchRect sSDetail = {162, 76, 151, 130};

int CtrSummaryUi_Tab(const CtrSummaryUiView *v)
{
    int page = v->data.page;

    return page == SUI_PAGE_SKILLS ? SUI_TAB_STATS : page >= SUI_PAGE_MOVES ? SUI_TAB_MOVES
         : v->trainer ? SUI_TAB_TRAINER : SUI_TAB_INFO;
}

/* What can be used now: an egg has only its first page (INFO, TRAINER); the previous and next Pokemon are FireRed's
 * UP and DOWN, which move the cursor instead in the move details. */
static int STabEnabled(const CtrSummaryUiView *v, int tab)
{
    return !v->loading && (!v->data.egg || tab == SUI_TAB_INFO || tab == SUI_TAB_TRAINER);
}

static int SSwitchEnabled(const CtrSummaryUiView *v)
{
    return !v->loading && v->data.page != SUI_PAGE_MOVES_INFO;
}

int CtrSummaryUi_Hit(const CtrSummaryUiView *v, int x, int y)
{
    const CtrSummaryView *d = &v->data;

    if (v->loading)
        return UiIn(&sSBack, x, y) ? SUI_HIT_BACK : -1;
    if (UiIn(&sSBack, x, y))
        return SUI_HIT_BACK;
    if (SSwitchEnabled(v) && UiIn(&sSPrev, x, y))
        return SUI_HIT_PREV;
    if (SSwitchEnabled(v) && UiIn(&sSNext, x, y))
        return SUI_HIT_NEXT;
    for (int i = 0; i < SUI_TABS; ++i)
        if (STabEnabled(v, i) && UiIn(&sSTab[i], x, y))
            return SUI_HIT_TAB + i;
    if (d->page == SUI_PAGE_MOVES)
        for (int i = 0; i < 4; ++i) {
            CtrPokeTouchRect r = SCard(i);
            if (d->moves[i] && UiIn(&r, x, y))
                return SUI_HIT_MOVE + i;
        }
    if (d->page == SUI_PAGE_MOVES_INFO) {
        for (int i = 0; i < 4; ++i) {
            CtrPokeTouchRect r = SRow(i);
            if (d->moves[i] && UiIn(&r, x, y))
                return SUI_HIT_MOVE + i;
        }
        {
            CtrPokeTouchRect r = SRow(4);
            if (!d->swapping && UiIn(&r, x, y))
                return SUI_HIT_CANCEL;
        }
    }
    return -1;
}

/* A number at the start of a Summary string ("15/15": 15; after the slash with `after`). */
static unsigned SNumber(const char *s, int after)
{
    unsigned n = 0;

    if (after) {
        const char *slash = strchr(s, '/');
        if (!slash)
            return 0;
        s = slash + 1;
    }
    while (*s == ' ')
        ++s;
    while (*s >= '0' && *s <= '9')
        n = n * 10 + (unsigned)(*s++ - '0');
    return n;
}

/* The gender symbols, 7x9: blue for male, red for female. */
static void SGender(char gender, int x, int y)
{
    static const uint8_t sMale[9] = {0x0F, 0x03, 0x05, 0x38, 0x44, 0x44, 0x44, 0x38, 0x00};
    static const uint8_t sFemale[9] = {0x1C, 0x22, 0x22, 0x22, 0x1C, 0x08, 0x3E, 0x08, 0x08};
    const uint8_t *g = gender == 'M' ? sMale : sFemale;
    uint16_t c = gender == 'M' ? UI_RGB(110, 170, 255) : UI_RGB(255, 110, 120);

    if (gender != 'M' && gender != 'F')
        return;
    for (int row = 0; row < 9; ++row)
        for (int col = 0; col < 7; ++col)
            if (g[row] & (0x40 >> col)) {
                Px(x + col + 1, y + row + 1, UIS_TEXT_SH);
                Px(x + col, y + row, c);
            }
}

/* The status badge's colours (the Summary's codes). */
static uint16_t SStatusColor(const char *status)
{
    static const struct { const char *code; uint16_t color; } sColors[] = {
        {"PSN", UI_RGB(170, 80, 190)}, {"PRZ", UI_RGB(214, 170, 30)}, {"SLP", UI_RGB(120, 128, 150)},
        {"FRZ", UI_RGB(80, 170, 220)}, {"BRN", UI_RGB(226, 96, 48)}, {"PKR", UI_RGB(200, 90, 160)}, {"FNT", UI_DANGER},
    };

    for (unsigned i = 0; i < sizeof(sColors) / sizeof(sColors[0]); ++i)
        if (!strcmp(status, sColors[i].code))
            return sColors[i].color;
    return 0;
}

/* The Pokemon's icon (FireRed's 32x32 party icon of its slot) at `scale`. */
static void SIcon(const CtrSummaryUiView *v, int x, int y, int scale)
{
    int slot = v->data.mon;

    if (v->loading || slot < 0 || slot >= 6 || !BPartyIcon(slot, v->icon))
        return;
    UiImage(sBPIcon[slot].pixels, sBPIcon[slot].pal, 32, 32, 32, x, y, scale, 1);
}

static void SHeader(const CtrSummaryUiView *v)
{
    const UiButtonStyle *sStyle = &sUiS->neutral;
    const CtrSummaryView *d = &v->data;
    int on = SSwitchEnabled(v);

    UiPanel(&sSHeader, UIS_PANEL, UIS_BORDER);
    Fill(sSHeader.x + 1, sSHeader.y + sSHeader.h - 2, sSHeader.w - 2, 1, UI_FOCUS);
    UiRoundFill(6, 5, 34, 34, 3, UIS_PANEL_LO);
    SIcon(v, 7, 4, 1);
    if (!v->loading) {
        char line[32];
        const char *level = d->level;
        int w;

        while (*level == ' ')
            ++level;
        UiText(d->nickname, 46, 9, UI_TEXT, UIS_TEXT_SH, 1, 190);
        w = UiTextWidth(d->nickname, 1);
        if (!d->egg)
            SGender(d->gender[0], 46 + w + 4, 8);
        if (d->egg)
            snprintf(line, sizeof(line), "EGG");
        else
            snprintf(line, sizeof(line), "No.%s  %s", d->dex, d->species);
        UiText(line, 46, 24, UIS_TEXT_DIM, UIS_TEXT_SH, 1, 200);
        if (!d->egg) {
            snprintf(line, sizeof(line), "Lv%s", level);
            UiTextRight(line, 238, 9, UI_TEXT, UIS_TEXT_SH, 1);
            if (SStatusColor(d->status)) {
                CtrPokeTouchRect badge = {205, 22, 33, 13};

                UiBadge(&badge, SStatusColor(d->status), d->status);
            }
        }
    }
    for (int i = 0; i < 2; ++i) {
        const CtrPokeTouchRect *r = i ? &sSNext : &sSPrev;
        int hit = i ? SUI_HIT_NEXT : SUI_HIT_PREV;
        int state = (on ? 0 : UI_DISABLED) | (on && v->pressed == hit ? UI_PRESSED : 0);
        int top = UiButtonLabelTop(r, state, 1);

        UiButton(r, sStyle, state);
        UiArrow(r->x + r->w / 2 - 4, top + 2, i ? 2 : -2, on ? UI_FOCUS_LIT : UI_TEXT_OFF);
    }
}

static void STabs(const CtrSummaryUiView *v)
{
    int shown = CtrSummaryUi_Tab(v);

    Fill(sSTab[0].x, sSTab[0].y, sSTab[SUI_TABS - 1].x + sSTab[SUI_TABS - 1].w - sSTab[0].x, sSTab[0].h, UIS_BG);
    for (int i = 0; i < SUI_TABS; ++i)
        UiTab(&sSTab[i], sSTabLabel[i], (shown == i && !v->loading ? UI_SELECTED : 0)
                                        | (v->pressed == SUI_HIT_TAB + i ? UI_PRESSED : 0)
                                        | (STabEnabled(v, i) ? 0 : UI_DISABLED));
}

/* A label on the left and a value on the right of a row, a rule under it. */
static void SRowText(int x, int y, int w, const char *label, const char *value, int rule)
{
    UiText(label, x, y, UIS_TEXT_DIM, UIS_TEXT_SH, 1, x + w / 2);
    UiTextRight(value, x + w, y, UI_TEXT, UIS_TEXT_SH, 1);
    if (rule)
        UiSeparator(x, y + 12, w);
}

static void SInfo(const CtrSummaryUiView *v)
{
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect plate = {7, 76, 84, 84}, side = {95, 76, 218, 130};

    UiPanel(&plate, UIS_PANEL_LO, UIS_BORDER);
    SIcon(v, plate.x + 10, plate.y + 8, 2);
    UiPanel(&side, UIS_PANEL, UIS_BORDER);
    if (d->egg) {
        UiText("EGG", 101, 82, UI_TEXT, UIS_TEXT_SH, 1, 307);
        UiSeparator(101, 94, 206);
        BDescribeN(d->hatch, 101, 100, 206, 5, UIS_TEXT_DIM);
        return;
    }
    {
        int second = strcmp(d->types[0], d->types[1]) != 0 && d->types[1][0];
        CtrPokeTouchRect t0 = {7, 164, (int16_t)(second ? 41 : 84), 15}, t1 = {50, 164, 41, 15};
        char hp[16];

        UiTypeBadge(&t0, d->typeIds[0], d->types[0]);
        if (second)
            UiTypeBadge(&t1, d->typeIds[1], d->types[1]);
        SRowText(101, 82, 206, "SPECIES", d->species, 1);
        SRowText(101, 97, 206, "ITEM", d->item[0] ? d->item : "NONE", 1);
        SRowText(101, 112, 206, "NATURE", d->nature, 1);
        SRowText(101, 127, 206, "ABILITY", d->ability, 0);
        BDescribeN(d->abilityDesc, 101, 141, 206, 2, UIS_TEXT_DIM);
        UiSeparator(101, 165, 206);
        snprintf(hp, sizeof(hp), "%u/%u", d->hpCur, d->hpMax);
        SRowText(101, 171, 206, "HP", hp, 0);
        {
            CtrPokeTouchRect bar = {101, 186, 206, 7};
            UiHpBar(&bar, d->hpCur, d->hpMax);
        }
    }
}

static void SStats(const CtrSummaryUiView *v)
{
    static const char *const sLabels[5] = {"ATTACK", "DEFENSE", "SP. ATK", "SP. DEF", "SPEED"};
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect left = {7, 76, 151, 130}, right = {162, 76, 151, 130};
    char hp[16];

    UiPanel(&left, UIS_PANEL, UIS_BORDER);
    UiPanel(&right, UIS_PANEL, UIS_BORDER);
    snprintf(hp, sizeof(hp), "%u/%u", d->hpCur, d->hpMax);
    SRowText(13, 82, 139, "HP", hp, 0);
    {
        CtrPokeTouchRect bar = {13, 95, 139, 5};
        UiHpBar(&bar, d->hpCur, d->hpMax);
    }
    for (int i = 0; i < 5; ++i)
        SRowText(13, 106 + 20 * i, 139, sLabels[i], d->stats[i], i < 4);
    UiText("EXP. POINTS", 168, 82, UIS_TEXT_DIM, UIS_TEXT_SH, 1, 307);
    UiTextRight(d->exp, 307, 96, UI_TEXT, UIS_TEXT_SH, 1);
    UiSeparator(168, 109, 139);
    UiText("TO NEXT LV.", 168, 114, UIS_TEXT_DIM, UIS_TEXT_SH, 1, 307);
    UiTextRight(d->nextExp, 307, 128, UI_TEXT, UIS_TEXT_SH, 1);
    UiSeparator(168, 141, 139);
    SRowText(168, 146, 139, "ABILITY", d->ability, 0);
    BDescribeN(d->abilityDesc, 168, 162, 139, 4, UIS_TEXT_DIM);
}

/* A move card of the moves page: the battle's card (the name on the type's colour), the type, power, accuracy and PP. */
static void SMoveCard(const CtrSummaryUiView *v, int i)
{
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect r = SCard(i);
    int dy;

    if (!d->moves[i]) {
        BCardFrame(&r, 0, NULL, 0);
        return;
    }
    dy = BCardFrame(&r, UiTypeColor(d->moveTypeIds[i]), d->moveNames[i], v->pressed == SUI_HIT_MOVE + i ? UI_PRESSED : 0);
    {
        CtrPokeTouchRect badge = {(int16_t)(r.x + 8), (int16_t)(r.y + 27 + dy), 52, 14};
        unsigned pp = SNumber(d->pp[i], 0), max = SNumber(d->pp[i], 1);

        UiTypeBadge(&badge, d->moveTypeIds[i], d->moveTypes[i]);
        UiText("PWR", r.x + r.w - 52, r.y + 30 + dy, UIS_TEXT_DIM, UIS_TEXT_SH, 1, r.x + r.w);
        UiTextRight(d->power[i], r.x + r.w - 8, r.y + 30 + dy, UI_TEXT, UIS_TEXT_SH, 1);
        UiText("PP", r.x + 8, r.y + 47 + dy, UIS_TEXT_DIM, UIS_TEXT_SH, 1, r.x + 30);
        UiText(d->pp[i], r.x + 24, r.y + 47 + dy, BPpColor(pp, max), UIS_TEXT_SH, 1, r.x + 80);
        UiText("ACC", r.x + r.w - 52, r.y + 47 + dy, UIS_TEXT_DIM, UIS_TEXT_SH, 1, r.x + r.w);
        UiTextRight(d->accuracy[i], r.x + r.w - 8, r.y + 47 + dy, UI_TEXT, UIS_TEXT_SH, 1);
    }
}

/* A row of the move details: the type's colour as an edge, the name and the PP; FireRed's cursor is the selection
 * blue with the focus ring, the move being moved (a swap) an amber ring. */
static void SMoveRow(const CtrSummaryUiView *v, int i)
{
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect r = SRow(i);
    int cursor = d->cursor == i, swap = d->swapping && d->swapCursor == i && i < 4;
    int pressed = v->pressed == (i < 4 ? SUI_HIT_MOVE + i : SUI_HIT_CANCEL);

    if (i == 4 && d->swapping) {
        Fill(r.x, r.y, r.w, r.h, UIS_PANEL_LO);
        return;
    }
    UiPanel(&r, pressed ? UI_SELECT_HI : cursor ? UI_SELECT : UIS_PANEL_LO, cursor ? UI_FOCUS : UIS_BORDER);
    if (i == 4) {
        UiTextCentered("CANCEL", &r, -1, UI_TEXT, UIS_TEXT_SH, 1);
    } else if (d->moves[i]) {
        Fill(r.x + 2, r.y + 4, 3, r.h - 8, UiTypeColor(d->moveTypeIds[i]));
        UiText(d->moveNames[i], r.x + 10, r.y + 8, UI_TEXT, UIS_TEXT_SH, 1, r.x + 100);
        UiTextRight(d->pp[i], r.x + r.w - 7, r.y + 8, BPpColor(SNumber(d->pp[i], 0), SNumber(d->pp[i], 1)), UIS_TEXT_SH, 1);
    } else
        UiText("-", r.x + 10, r.y + 8, UI_TEXT_OFF, UIS_TEXT_SH, 1, r.x + r.w);
    if (cursor)
        UiRing(&r, UI_RADIUS_PANEL, 2, UI_TEXT);
    if (swap)
        UiRing(&r, UI_RADIUS_PANEL, 2, UI_WARN);
}

static void SMoveDetail(const CtrSummaryUiView *v)
{
    const CtrSummaryView *d = &v->data;
    int i = d->cursor;
    const CtrPokeTouchRect *p = &sSDetail;

    UiPanel(p, UIS_PANEL, UIS_BORDER);
    if (i < 0 || i >= 4 || !d->moves[i]) {
        UiTextCentered("Back to the moves", p, -1, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
        return;
    }
    {
        uint16_t color = UiTypeColor(d->moveTypeIds[i]);
        CtrPokeTouchRect band = {(int16_t)(p->x + 1), (int16_t)(p->y + 1), (int16_t)(p->w - 2), 20};
        CtrPokeTouchRect badge = {(int16_t)(p->x + 6), (int16_t)(p->y + 26), 52, 14};

        UiRoundFill(band.x, band.y, band.w, band.h, 2, color);
        Fill(band.x + 2, band.y + 1, band.w - 4, 7, UiMix(color, UI_TEXT, 2));
        UiText(d->moveNames[i], p->x + 8, p->y + 7, UI_TEXT, UiMix(color, UIS_OUTLINE, 6), 1, p->x + p->w - 4);
        UiTypeBadge(&badge, d->moveTypeIds[i], d->moveTypes[i]);
        UiText("PWR", p->x + 66, p->y + 29, UIS_TEXT_DIM, UIS_TEXT_SH, 1, p->x + p->w);
        UiTextRight(d->power[i], p->x + p->w - 8, p->y + 29, UI_TEXT, UIS_TEXT_SH, 1);
        UiText("ACC", p->x + 66, p->y + 43, UIS_TEXT_DIM, UIS_TEXT_SH, 1, p->x + p->w);
        UiTextRight(d->accuracy[i], p->x + p->w - 8, p->y + 43, UI_TEXT, UIS_TEXT_SH, 1);
        UiSeparator(p->x + 6, p->y + 57, p->w - 12);
        BDescribeN(d->moveDesc, p->x + 6, p->y + 63, p->w - 12, 6, UIS_TEXT_DIM);
    }
}

static void SMoves(const CtrSummaryUiView *v)
{
    if (v->data.page == SUI_PAGE_MOVES_INFO) {
        for (int i = 0; i < 5; ++i)
            SMoveRow(v, i);
        SMoveDetail(v);
    } else
        for (int i = 0; i < 4; ++i)
            SMoveCard(v, i);
}

static void STrainer(const CtrSummaryUiView *v)
{
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect panel = {7, 76, 306, 130};
    char ribbons[8];

    UiPanel(&panel, UIS_PANEL, UIS_BORDER);
    snprintf(ribbons, sizeof(ribbons), "%u", d->ribbons);
    SRowText(13, 82, 294, "OT", d->ot, 1);
    SRowText(13, 97, 294, "ID No.", d->id, 1);
    if (!d->egg) {
        SRowText(13, 112, 294, "NATURE", d->nature, 1);
        SRowText(13, 127, 294, "RIBBONS", ribbons, 1);
    }
    UiText("TRAINER MEMO", 13, d->egg ? 113 : 143, UI_FOCUS, UIS_TEXT_SH, 1, 307);
    BDescribeN(d->memo, 13, d->egg ? 127 : 157, 294, d->egg ? 2 : 4, UI_TEXT);
    if (d->egg)
        BDescribeN(d->hatch, 13, 154, 294, 4, UIS_TEXT_DIM);
}

static void SBody(const CtrSummaryUiView *v)
{
    Fill(sSBody.x, sSBody.y, sSBody.w, sSBody.h, UIS_BG);
    if (v->loading)
        return;
    switch (CtrSummaryUi_Tab(v)) {
    case SUI_TAB_INFO: SInfo(v); break;
    case SUI_TAB_STATS: SStats(v); break;
    case SUI_TAB_MOVES: SMoves(v); break;
    default: STrainer(v); break;
    }
}

/* The footer: what the page lets one do, and BACK (FireRed's B: out of the move details, else out of the Summary). */
static const char *SHint(const CtrSummaryUiView *v)
{
    return v->loading ? "" : v->data.page == SUI_PAGE_MOVES ? "Tap a move for its details."
         : v->data.page == SUI_PAGE_MOVES_INFO ? (v->data.swapping ? "Tap where to move it." : "Tap the selected move to move it.")
         : v->data.egg ? "" : "UP / DOWN: another POK\xc3\xa9MON";
}

static void SFooter(const CtrSummaryUiView *v)
{
    const UiButtonStyle *sStyle = &sUiS->cancel;
    const char *hint = SHint(v);
    int state = v->pressed == SUI_HIT_BACK ? UI_PRESSED : 0;
    int top = UiButtonLabelTop(&sSBack, state, 1), w = UiTextWidth("BACK", 1), x = sSBack.x + (sSBack.w - w - 9) / 2;

    Fill(sSFooter.x, sSFooter.y, sSBack.x + sSBack.w - sSFooter.x, sSFooter.h, UIS_BG);
    UiFooter(&sSFooter, hint);
    UiButton(&sSBack, sStyle, state);
    UiArrow(x, top, -1, UI_TEXT);
    UiText("BACK", x + 9, top, UI_TEXT, UIS_TEXT_SH, 1, sSBack.x + sSBack.w);
}

void CtrSummaryUi_Draw(uint16_t *canvas, const CtrSummaryUiView *v)
{
    sCanvas = canvas;
    sUiS = &sUiRed;
    Fill(0, 0, W, H, UIS_BG);
    Fill(0, 0, W, 2, UIS_BG_TOP);
    SHeader(v);
    STabs(v);
    SBody(v);
    SFooter(v);
    sUiS = &sUiNavy;
}

static int SHeaderChanged(const CtrSummaryUiView *a, const CtrSummaryUiView *b)
{
    const CtrSummaryView *x = &a->data, *y = &b->data;
    int pa = a->pressed == SUI_HIT_PREV || a->pressed == SUI_HIT_NEXT ? a->pressed : -1;
    int pb = b->pressed == SUI_HIT_PREV || b->pressed == SUI_HIT_NEXT ? b->pressed : -1;

    return a->loading != b->loading || a->icon != b->icon || pa != pb || SSwitchEnabled(a) != SSwitchEnabled(b)
        || x->mon != y->mon || x->egg != y->egg || strcmp(x->nickname, y->nickname) || strcmp(x->species, y->species)
        || strcmp(x->level, y->level) || strcmp(x->gender, y->gender) || strcmp(x->status, y->status) || strcmp(x->dex, y->dex);
}

static int STabsChanged(const CtrSummaryUiView *a, const CtrSummaryUiView *b)
{
    int pa = a->pressed >= SUI_HIT_TAB && a->pressed < SUI_HIT_TAB + SUI_TABS ? a->pressed : -1;
    int pb = b->pressed >= SUI_HIT_TAB && b->pressed < SUI_HIT_TAB + SUI_TABS ? b->pressed : -1;

    return a->loading != b->loading || CtrSummaryUi_Tab(a) != CtrSummaryUi_Tab(b) || pa != pb || a->data.egg != b->data.egg;
}

static int SBodyChanged(const CtrSummaryUiView *a, const CtrSummaryUiView *b)
{
    int pa = a->pressed >= SUI_HIT_MOVE && a->pressed <= SUI_HIT_CANCEL ? a->pressed : -1;
    int pb = b->pressed >= SUI_HIT_MOVE && b->pressed <= SUI_HIT_CANCEL ? b->pressed : -1;

    return a->loading != b->loading || a->icon != b->icon || a->trainer != b->trainer || pa != pb
        || memcmp(&a->data, &b->data, sizeof(a->data));
}

static int SFooterChanged(const CtrSummaryUiView *a, const CtrSummaryUiView *b)
{
    return strcmp(SHint(a), SHint(b)) || (a->pressed == SUI_HIT_BACK) != (b->pressed == SUI_HIT_BACK);
}

void CtrSummaryUi_Update(uint16_t *canvas, const CtrSummaryUiView *old, const CtrSummaryUiView *v, CtrDirtyList *dirty)
{
    sCanvas = canvas;
    sUiS = &sUiRed;
    CtrDirty_Clear(dirty);
    if (SHeaderChanged(old, v)) {
        Fill(sSHeader.x, sSHeader.y, sSHeader.w, sSHeader.h, UIS_BG);
        SHeader(v);
        CtrDirty_Add(dirty, sSHeader.x, sSHeader.y, sSHeader.x + sSHeader.w, sSHeader.y + sSHeader.h);
    }
    if (STabsChanged(old, v)) {
        STabs(v);
        CtrDirty_Add(dirty, sSTab[0].x, sSTab[0].y, sSTab[SUI_TABS - 1].x + sSTab[SUI_TABS - 1].w, sSTab[0].y + sSTab[0].h);
    }
    if (SBodyChanged(old, v)) {
        SBody(v);
        CtrDirty_Add(dirty, sSBody.x, sSBody.y, sSBody.x + sSBody.w, sSBody.y + sSBody.h);
    }
    if (SFooterChanged(old, v)) {
        SFooter(v);
        CtrDirty_Add(dirty, sSFooter.x, sSFooter.y, sSBack.x + sSBack.w, sSFooter.y + sSFooter.h);
    }
    sUiS = &sUiNavy;
}
