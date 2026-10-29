/* The Pokemon Summary on the lower screen (3ds_poketouch.h CtrSummaryUi_*), included at the end of 3ds_poketouch.c
 * after the battle screen, whose cards, PP colours, text wrapping and Party icon cache it shares: one Summary for the
 * field's Party and the battle's, in PokeTouch's ember look (light cards, cream and amber buttons, BACK violet). It only shows what FireRed's Summary reports;
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

/* The Pokemon's icon (FireRed's 32x32 icon of the Pokemon shown, a box's too) at `scale`, decoded once per icon. */
static void SIcon(const CtrSummaryUiView *v, int x, int y, int scale)
{
    static unsigned shown;
    static int ok;
    static uint8_t pixels[32 * 32];
    static uint16_t pal[16];

    if (v->loading || !v->icon)
        return;
    if (shown != v->icon + 1u) {
        static unsigned char tiles[512];
        static unsigned short raw[16];

        shown = v->icon + 1u;
        ok = CtrBottomSummary_Icon(tiles, raw);
        for (int py = 0; ok && py < 32; ++py)
            for (int px = 0; px < 32; ++px) {
                unsigned byte = tiles[(py / 8 * 4 + px / 8) * 32 + py % 8 * 4 + px % 8 / 2];
                pixels[py * 32 + px] = (uint8_t)((byte >> (px & 1) * 4) & 15);
            }
        for (int i = 0; ok && i < 16; ++i) {
            unsigned c = raw[i];
            pal[i] = GbaColor(c);
        }
    }
    if (ok)
        UiImage(pixels, pal, 32, 32, 32, x, y, scale, 1);
}

/* The Pokemon's picture, FireRed's Summary's own (the 64x64 front picture, flipped where FireRed flips it), decoded
 * once per Pokemon shown; 0 when there is none (the caller draws the icon). With HeartGold's pictures in the pack
 * (CtrBottomSummary_Pic80) the 80x80 one, whole, centred on the same spot. */
static int SPic(const CtrSummaryUiView *v, int x, int y)
{
    static unsigned shownSerial, shownIcon;
    static int shownMon = -1, ok, size = 64;
    static uint8_t pixels[80 * 80];
    static uint16_t pal[16];

    if (v->loading || !v->icon)
        return 0;
    if (shownSerial != v->data.serial || shownMon != v->data.mon || shownIcon != v->icon) {
        static unsigned char tiles[0x800];
        static unsigned short raw[16];
        int flip = 0;

        shownSerial = v->data.serial;
        shownMon = v->data.mon;
        shownIcon = v->icon;
        static unsigned char big[80 * 80 / 2];
        size = 80;
        ok = CtrBottomSummary_Pic80(big, raw, &flip);
        for (int py = 0; ok && py < 80; ++py)
            for (int px = 0; px < 80; ++px)
                pixels[py * 80 + (flip ? 79 - px : px)] = (uint8_t)((big[py * 40 + px / 2] >> (px & 1) * 4) & 15);
        if (!ok) {
            size = 64;
            ok = CtrBottomSummary_Pic(tiles, raw, &flip);
        }
        for (int py = 0; ok && size == 64 && py < 64; ++py)
            for (int px = 0; px < 64; ++px) {
                unsigned byte = tiles[(py / 8 * 8 + px / 8) * 32 + py % 8 * 4 + px % 8 / 2];
                pixels[py * 64 + (flip ? 63 - px : px)] = (uint8_t)((byte >> (px & 1) * 4) & 15);
            }
        for (int i = 0; ok && i < 16; ++i) {
            unsigned c = raw[i];
            pal[i] = GbaColor(c);
        }
    }
    if (ok)
        UiImage(pixels, pal, size, size, size, x - (size - 64) / 2, y - (size - 64) / 2, 1, 1);
    return ok;
}

/* The Summary's pieces in PokeTouch's ember look: light cards on the embers (FireRed's grey ink, labels a softer
 * grey), cream buttons, the chosen tab amber, BACK violet, the hint on a note card. */
#define S_INK EM_DARK
#define S_LABEL EM_DIM
#define S_RULE UI_RGB(220, 210, 196)
/* A light card: outlined, cream from top to bottom. */
static void SCardBox(int x, int y, int w, int h)
{
    UiRoundFill(x, y, w, h, 5, kEmCream.edge);
    EmGradFill(x + 1, y + 1, w - 2, h - 2, 4, kEmCream.top, kEmCream.bottom);
}

static void SRule(int x, int y, int w)
{
    Fill(x, y, w, 1, S_RULE);
}

static uint16_t SPpColor(unsigned pp, unsigned max)
{
    uint16_t c = BPpColor(pp, max);

    /* The battle's yellows are for a dark card: deeper ones on the light card. */
    return c == UI_TEXT ? S_INK : c == UI_HP_MID ? UI_RGB(196, 128, 0) : c == UI_WARN ? UI_RGB(222, 96, 24) : c;
}

static void SHeader(const CtrSummaryUiView *v)
{
    const CtrSummaryView *d = &v->data;
    int on = SSwitchEnabled(v);

    TsBgRect(sSHeader.x, sSHeader.y, sSHeader.w, sSHeader.h);
    SCardBox(sSHeader.x, sSHeader.y, sSHeader.w, sSHeader.h);
    UiDisc(23, 22, 16, UiMix(kEmCream.bottom, kEmCream.edge, 2));
    UiDisc(23, 22, 15, UiMix(kEmCream.top, kEmCream.bottom, 6));
    SIcon(v, 7, 4, 1);
    if (!v->loading) {
        char line[32];
        const char *level = d->level;
        int w;

        while (*level == ' ')
            ++level;
        UiText(d->nickname, 46, 9, S_INK, EM_DARK_SH, 1, 190);
        w = UiTextWidth(d->nickname, 1);
        if (!d->egg)
            SGender(d->gender[0], 46 + w + 4, 8);
        if (d->egg)
            snprintf(line, sizeof(line), "EGG");
        else
            snprintf(line, sizeof(line), "No.%s  %s", d->dex, d->species);
        UiText(line, 46, 24, S_LABEL, EM_DARK_SH, 1, 200);
        if (!d->egg) {
            snprintf(line, sizeof(line), "Lv%s", level);
            UiTextRight(line, 238, 9, S_INK, EM_DARK_SH, 1);
            if (SStatusColor(d->status)) {
                CtrPokeTouchRect badge = {205, 22, 33, 13};

                UiBadge(&badge, SStatusColor(d->status), d->status);
            }
        }
    }
    for (int i = 0; i < 2; ++i) {
        const CtrPokeTouchRect *r = i ? &sSNext : &sSPrev;
        int pressed = on && v->pressed == (i ? SUI_HIT_NEXT : SUI_HIT_PREV);
        const EmPal *pal = on ? &kEmCream : &kEmGrey;
        int fy = EmButton(r->x, r->y, r->w, r->h, pal, 2, pressed);

        UiArrow(r->x + r->w / 2 - 4, fy + (r->h - 2 - 7) / 2 + 1, i ? 2 : -2, pal->ink);
    }
}

static void STabs(const CtrSummaryUiView *v)
{
    int shown = CtrSummaryUi_Tab(v);

    TsBgRect(sSTab[0].x, sSTab[0].y, sSTab[SUI_TABS - 1].x + sSTab[SUI_TABS - 1].w - sSTab[0].x, sSTab[0].h);
    for (int i = 0; i < SUI_TABS; ++i) {
        const CtrPokeTouchRect *r = &sSTab[i];
        const EmPal *pal = !STabEnabled(v, i) ? &kEmGrey : shown == i && !v->loading ? &kEmAmber : &kEmCream;
        int fy = EmButton(r->x, r->y, r->w, r->h, pal, 2, v->pressed == SUI_HIT_TAB + i);

        EmLabel(sSTabLabel[i], r->x, r->w, fy, r->h - 2, pal);
    }
}

/* A label on the left and a value on the right of a row, a rule under it. */
static void SRowText(int x, int y, int w, const char *label, const char *value, int rule)
{
    UiText(label, x, y, S_LABEL, EM_DARK_SH, 1, x + w / 2);
    UiTextRight(value, x + w, y, S_INK, EM_DARK_SH, 1);
    if (rule)
        SRule(x, y + 12, w);
}

static void SInfo(const CtrSummaryUiView *v)
{
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect plate = {7, 76, 84, 84}, side = {95, 76, 218, 130};

    SCardBox(plate.x, plate.y, plate.w, plate.h);
    UiDisc(plate.x + 42, plate.y + 42, 34, UiMix(kEmCream.top, kEmCream.bottom, 7));
    if (!SPic(v, plate.x + 10, plate.y + 10))
        SIcon(v, plate.x + 10, plate.y + 8, 2);
    SCardBox(side.x, side.y, side.w, side.h);
    if (d->egg) {
        UiText("EGG", 101, 82, S_INK, EM_DARK_SH, 1, 307);
        SRule(101, 94, 206);
        BDescribeN(d->hatch, 101, 100, 206, 5, S_LABEL);
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
        BDescribeN(d->abilityDesc, 101, 141, 206, 2, S_LABEL);
        SRule(101, 165, 206);
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
    char hp[16];

    SCardBox(7, 76, 151, 130);
    SCardBox(162, 76, 151, 130);
    snprintf(hp, sizeof(hp), "%u/%u", d->hpCur, d->hpMax);
    SRowText(13, 82, 139, "HP", hp, 0);
    {
        CtrPokeTouchRect bar = {13, 95, 139, 5};
        UiHpBar(&bar, d->hpCur, d->hpMax);
    }
    for (int i = 0; i < 5; ++i)
        SRowText(13, 106 + 20 * i, 139, sLabels[i], d->stats[i], i < 4);
    UiText("EXP. POINTS", 168, 82, S_LABEL, EM_DARK_SH, 1, 307);
    UiTextRight(d->exp, 307, 96, S_INK, EM_DARK_SH, 1);
    SRule(168, 109, 139);
    UiText("TO NEXT LV.", 168, 114, S_LABEL, EM_DARK_SH, 1, 307);
    UiTextRight(d->nextExp, 307, 128, S_INK, EM_DARK_SH, 1);
    SRule(168, 141, 139);
    SRowText(168, 146, 139, "ABILITY", d->ability, 0);
    BDescribeN(d->abilityDesc, 168, 162, 139, 4, S_LABEL);
}

/* A move card of the moves page: a cream button with the type's colour as its edge, the name, the type, power,
 * accuracy and PP. An empty slot is a hollow. */
static void SMoveCard(const CtrSummaryUiView *v, int i)
{
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect r = SCard(i);
    int fy;

    if (!d->moves[i]) {
        UiRoundFill(r.x, r.y, r.w, r.h - 2, 5, UI_RGB(140, 70, 60));
        UiRoundFill(r.x + 1, r.y + 1, r.w - 2, r.h - 4, 4, UI_RGB(70, 18, 16));
        return;
    }
    fy = EmButton(r.x, r.y, r.w, r.h, &kEmCream, 2, v->pressed == SUI_HIT_MOVE + i);
    Fill(r.x + 3, fy + 4, 4, r.h - 2 - 8, UiTypeColor(d->moveTypeIds[i]));
    UiText(d->moveNames[i], r.x + 12, fy + 6, S_INK, EM_DARK_SH, 1, r.x + r.w - 4);
    {
        CtrPokeTouchRect badge = {(int16_t)(r.x + 12), (int16_t)(fy + 23), 52, 14};
        unsigned pp = SNumber(d->pp[i], 0), max = SNumber(d->pp[i], 1);

        UiTypeBadge(&badge, d->moveTypeIds[i], d->moveTypes[i]);
        UiText("PWR", r.x + r.w - 52, fy + 26, S_LABEL, EM_DARK_SH, 1, r.x + r.w);
        UiTextRight(d->power[i], r.x + r.w - 8, fy + 26, S_INK, EM_DARK_SH, 1);
        UiText("PP", r.x + 12, fy + 43, S_LABEL, EM_DARK_SH, 1, r.x + 30);
        UiText(d->pp[i], r.x + 28, fy + 43, SPpColor(pp, max), EM_DARK_SH, 1, r.x + 84);
        UiText("ACC", r.x + r.w - 52, fy + 43, S_LABEL, EM_DARK_SH, 1, r.x + r.w);
        UiTextRight(d->accuracy[i], r.x + r.w - 8, fy + 43, S_INK, EM_DARK_SH, 1);
    }
}

/* A row of the move details: cream with the type's colour as its edge; FireRed's cursor amber, the move being moved
 * (a swap) ringed in ember, CANCEL violet. */
static void SMoveRow(const CtrSummaryUiView *v, int i)
{
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect r = SRow(i);
    int cursor = d->cursor == i, swap = d->swapping && d->swapCursor == i && i < 4;
    int pressed = v->pressed == (i < 4 ? SUI_HIT_MOVE + i : SUI_HIT_CANCEL);
    const EmPal *pal = cursor ? &kEmAmber : i == 4 ? &kEmViolet : &kEmCream;
    int fy;

    if (i == 4 && d->swapping)
        return;
    fy = EmButton(r.x, r.y, r.w, r.h, pal, 2, pressed);
    if (i == 4) {
        EmLabel("CANCEL", r.x, r.w, fy, r.h - 2, pal);
    } else if (d->moves[i]) {
        Fill(r.x + 3, fy + 4, 3, r.h - 2 - 8, UiTypeColor(d->moveTypeIds[i]));
        UiText(d->moveNames[i], r.x + 10, fy + 7, pal->ink, pal->inkSh, 1, r.x + 100);
        UiTextRight(d->pp[i], r.x + r.w - 7, fy + 7, cursor ? pal->ink : SPpColor(SNumber(d->pp[i], 0), SNumber(d->pp[i], 1)),
                    pal->inkSh, 1);
    } else
        UiText("-", r.x + 10, fy + 7, S_LABEL, EM_DARK_SH, 1, r.x + r.w);
    if (swap)
        UiRing(&r, 5, 2, EM_STRIPE);
}

static void SMoveDetail(const CtrSummaryUiView *v)
{
    const CtrSummaryView *d = &v->data;
    int i = d->cursor;
    const CtrPokeTouchRect *p = &sSDetail;

    SCardBox(p->x, p->y, p->w, p->h);
    if (i < 0 || i >= 4 || !d->moves[i]) {
        UiTextCentered("Back to the moves", p, -1, S_LABEL, EM_DARK_SH, 1);
        return;
    }
    {
        uint16_t color = UiTypeColor(d->moveTypeIds[i]);
        CtrPokeTouchRect band = {(int16_t)(p->x + 1), (int16_t)(p->y + 1), (int16_t)(p->w - 2), 20};
        CtrPokeTouchRect badge = {(int16_t)(p->x + 6), (int16_t)(p->y + 26), 52, 14};

        UiRoundFill(band.x, band.y, band.w, band.h, 4, color);
        Fill(band.x + 4, band.y + 1, band.w - 8, 7, UiMix(color, EM_WHITE, 2));
        UiText(d->moveNames[i], p->x + 8, p->y + 7, EM_WHITE, UiMix(color, UI_RGB(0, 0, 0), 4), 1, p->x + p->w - 4);
        UiTypeBadge(&badge, d->moveTypeIds[i], d->moveTypes[i]);
        UiText("PWR", p->x + 66, p->y + 29, S_LABEL, EM_DARK_SH, 1, p->x + p->w);
        UiTextRight(d->power[i], p->x + p->w - 8, p->y + 29, S_INK, EM_DARK_SH, 1);
        UiText("ACC", p->x + 66, p->y + 43, S_LABEL, EM_DARK_SH, 1, p->x + p->w);
        UiTextRight(d->accuracy[i], p->x + p->w - 8, p->y + 43, S_INK, EM_DARK_SH, 1);
        SRule(p->x + 6, p->y + 57, p->w - 12);
        BDescribeN(d->moveDesc, p->x + 6, p->y + 63, p->w - 12, 6, S_LABEL);
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
    char ribbons[8];

    SCardBox(7, 76, 306, 130);
    snprintf(ribbons, sizeof(ribbons), "%u", d->ribbons);
    SRowText(13, 82, 294, "OT", d->ot, 1);
    SRowText(13, 97, 294, "ID No.", d->id, 1);
    if (!d->egg) {
        SRowText(13, 112, 294, "NATURE", d->nature, 1);
        SRowText(13, 127, 294, "RIBBONS", ribbons, 1);
    }
    UiText("TRAINER MEMO", 13, d->egg ? 113 : 143, EM_STRIPE, EM_DARK_SH, 1, 307);
    BDescribeN(d->memo, 13, d->egg ? 127 : 157, 294, d->egg ? 2 : 4, S_INK);
    if (d->egg)
        BDescribeN(d->hatch, 13, 154, 294, 4, S_LABEL);
}

static void SBody(const CtrSummaryUiView *v)
{
    TsBgRect(sSBody.x, sSBody.y, sSBody.w, sSBody.h);
    if (v->loading)
        return;
    switch (CtrSummaryUi_Tab(v)) {
    case SUI_TAB_INFO: SInfo(v); break;
    case SUI_TAB_STATS: SStats(v); break;
    case SUI_TAB_MOVES: SMoves(v); break;
    default: STrainer(v); break;
    }
}

/* The footer: what the page lets one do on a note card, and BACK (FireRed's B: out of the move details, else out of
 * the Summary), violet. */
static const char *SHint(const CtrSummaryUiView *v)
{
    return v->loading ? "" : v->data.page == SUI_PAGE_MOVES ? "Tap a move for its details."
         : v->data.page == SUI_PAGE_MOVES_INFO ? (v->data.swapping ? "Tap where to move it." : "Tap the selected move to move it.")
         : v->data.egg ? "" : "UP / DOWN: another POK\xc3\xa9MON";
}

static void SFooter(const CtrSummaryUiView *v)
{
    const char *hint = SHint(v);
    int pressed = v->pressed == SUI_HIT_BACK, w = UiTextWidth("BACK", 1), fy, x;

    TsBgRect(sSFooter.x, sSFooter.y, sSBack.x + sSBack.w - sSFooter.x, sSFooter.h);
    EmNote(sSFooter.x, sSFooter.y, sSFooter.w, sSFooter.h);
    UiText(hint, sSFooter.x + 12, sSFooter.y + (sSFooter.h - UI_TEXT_H) / 2, S_INK, EM_DARK_SH, 1, sSFooter.x + sSFooter.w - 4);
    fy = EmButton(sSBack.x, sSBack.y, sSBack.w, sSBack.h, &kEmViolet, 2, pressed);
    x = sSBack.x + (sSBack.w - w - 9) / 2;
    UiArrow(x, fy + (sSBack.h - 2 - UI_TEXT_H) / 2 + 1, -1, EM_WHITE);
    UiText("BACK", x + 9, fy + (sSBack.h - 2 - UI_TEXT_H) / 2, EM_WHITE, kEmViolet.inkSh, 1, sSBack.x + sSBack.w);
}

void CtrSummaryUi_Draw(uint16_t *canvas, const CtrSummaryUiView *v)
{
    sCanvas = canvas;
    EmSurface();
    TsBackdrop();                       /* the embers, as PokeTouch's */
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
    EmSurface();
    CtrDirty_Clear(dirty);
    if (SHeaderChanged(old, v)) {
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
