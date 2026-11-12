/* The Pokemon Summary on the lower screen (3ds_poketouch.h CtrSummaryUi_*), included at the end of 3ds_poketouch.c
 * after the battle screen, whose cards, PP colours, text wrapping and Party icon cache it shares: one Summary for the
 * field's Party and the battle's, in the Llama look (cream cards, FireRed's text windows, the open tab in flame, BACK slate). It only shows what FireRed's Summary reports;
 * every page, Pokemon and move cursor is FireRed's own (3ds_bottom.c walks them with FireRed's keys). */

static const CtrPokeTouchRect sSHeader = {3, 3, 314, 38};
static const CtrPokeTouchRect sSPrev = {244, 6, 34, 32}, sSNext = {281, 6, 34, 32};
static const CtrPokeTouchRect sSTab[SUI_TABS] = {{3, 45, 77, 24}, {82, 45, 77, 24}, {161, 45, 77, 24}, {240, 45, 77, 24}};
static const CtrPokeTouchRect sSBody = {3, 72, 314, 138};
static const CtrPokeTouchRect sSFooter = {3, 213, 230, 24};
static const CtrPokeTouchRect sSBack = {236, 213, 81, 24};
/* The switch in the corner of STATS' right card: EXP and the ability, or the IVs, EVs and Hidden Power. */
static const CtrPokeTouchRect sSIvSwitch = {252, 79, 58, 14};   /* Llama: the ribbon grew 10 px to the left so that "IV / EV" fits whole */
static const char *const sSTabLabel[SUI_TABS] = {CTR_TR("Info", "Info", "Info"), CTR_TR("Stats", "Stats", "Status"), CTR_TR("Moves", "Ataques", "Attacken"), CTR_TR("Trainer", "Entren.", "Trainer")};

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
    if (d->page == SUI_PAGE_SKILLS && !d->egg && UiIn(&sSIvSwitch, x, y))
        return SUI_HIT_IVTOGGLE;
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
        {CTR_TR("Psn", "Env", "Gft"), UI_RGB(170, 80, 190)}, {CTR_TR("Prz", "Par", "Par"), UI_RGB(214, 170, 30)}, {CTR_TR("Slp", "Dor", "Slf"), UI_RGB(120, 128, 150)},
        {CTR_TR("Frz", "Con", "Efr"), UI_RGB(80, 170, 220)}, {CTR_TR("Brn", "Que", "Brn"), UI_RGB(226, 96, 48)}, {CTR_TR("PKR", "PKR", "PKR"), UI_RGB(200, 90, 160)}, {CTR_TR("Fnt", "Deb", "Bes"), UI_DANGER},
    };

    for (unsigned i = 0; i < sizeof(sColors) / sizeof(sColors[0]); ++i)
        if (CtrTextCase_Eq(status, sColors[i].code))
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

/* The Summary's pieces in the Llama look: cream raised cards for the header, the Pokemon and the moves; FireRed's text window
 * (LlWindow) for the data, with its hairlines; the chosen tab and cursor in flame, BACK slate. */
#define S_INK EM_DARK
#define S_LABEL EM_DIM
/* A cream raised card (the mockup's .c.cr: a lip 2 px under it). The caller has put the backdrop back under it. */
static void SCardBox(int x, int y, int w, int h)
{
    EmButton(x, y, w, h, &kEmCream, 2, 0);
}

static void SRule(int x, int y, int w)
{
    LlRule(x, y, w);
}

/* A mini bar of a stat: slate track 7 tall, the amber (or blue) fill with its light upper band. */
static void SMiniBar(int x, int y, int w, unsigned value, unsigned max, int blue)
{
    int fw = max ? (int)((unsigned long)(value > max ? max : value) * (unsigned)(w - 2) / max) : 0;
    uint16_t light = blue ? UI_RGB(168, 208, 255) : UI_RGB(255, 216, 144), dark = blue ? UI_RGB(74, 140, 240) : UI_RGB(240, 144, 48);

    UiRoundFill(x, y, w, 7, 3, LL_SLATE);
    if (fw > 1)
        Em2Fill3(x + 1, y + 1, fw, 5, 2, light, light, 2, dark);
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
    LlPlate(10, 9, 26, LL_SLATE, EM_WHITE, 1);
    SIcon(v, 7, 4, 1);
    if (!v->loading) {
        char line[32];
        const char *level = d->level;
        int w;

        while (*level == ' ')
            ++level;
        Em2Text(d->nickname, 46, 8, S_INK, EM_DARK_SH, 190);
        w = UiTextWidth(d->nickname, 1);
        if (!d->egg)
            SGender(d->gender[0], 46 + w + 4, 8);
        if (d->egg)
            snprintf(line, sizeof(line), CTR_TR("Egg", "Huevo", "EI"));
        else
            snprintf(line, sizeof(line), "No.%s  %s", d->dex, d->species);
        Em2Text(line, 46, 23, S_LABEL, EM_DARK_SH, 200);
        if (!d->egg) {
            const char *lv = CTR_TR("Lv", "Nv", "Lv");
            int lw = UiTextWidth(level, 1);

            UiTextRight(level, 238, 8, S_INK, EM_DARK_SH, 1);
            UiText(lv, 238 - lw - UiTextWidth(lv, 1) - 1, 8, EM_VALUE, UI_RGB(248, 200, 184), 1, 238);
            if (SStatusColor(d->status))
                LlTypeBadge(238 - UiTextWidth(d->status, 1) - 12, 22, d->status, SStatusColor(d->status));
        }
    }
    for (int i = 0; i < 2; ++i) {
        const CtrPokeTouchRect *r = i ? &sSNext : &sSPrev;
        int pressed = on && v->pressed == (i ? SUI_HIT_NEXT : SUI_HIT_PREV);
        const EmPal *pal = on ? &kEmCream : &kEmGrey;
        int fy = EmButton(r->x, r->y, r->w, r->h, pal, 2, pressed);

        LlArrow(r->x + r->w / 2 - 6, fy + (r->h - 2 - 8) / 2, 12, 8, i, on ? LL_CURSOR : pal->ink);
    }
}

static void STabs(const CtrSummaryUiView *v)
{
    int shown = CtrSummaryUi_Tab(v);

    TsBgRect(sSTab[0].x, sSTab[0].y, sSTab[SUI_TABS - 1].x + sSTab[SUI_TABS - 1].w - sSTab[0].x, sSTab[0].h);
    for (int i = 0; i < SUI_TABS; ++i) {
        const CtrPokeTouchRect *r = &sSTab[i];
        const EmPal *pal = !STabEnabled(v, i) ? &kEmGrey : LlPick(shown == i && !v->loading, &kEmCream);
        int fy = EmButton(r->x, r->y, r->w, r->h, pal, 2, v->pressed == SUI_HIT_TAB + i);

        EmLabel(sSTabLabel[i], r->x, r->w, fy, r->h - 2, pal);
    }
}

static void SRowText(int x, int y, int w, const char *label, const char *value, int rule)
{
    char fit[64];
    int lw = UiTextWidth(label, 1);

    Em2Text(label, x, y, S_LABEL, EM_DARK_SH, x + w / 2);
    Em2Fit(value, w - (lw < w / 2 ? lw : w / 2) - 8, fit, sizeof(fit));
    UiTextRight(fit, x + w, y, S_INK, EM_DARK_SH, 1);
    if (rule)
        SRule(x, y + 13, w);
}

static void SInfo(const CtrSummaryUiView *v)
{
    const CtrSummaryView *d = &v->data;

    SCardBox(7, 76, 84, 130);
    LlPlate(17, 94, 64, LL_SLATE, EM_WHITE, 1);
    if (!SPic(v, 17, 94))
        SIcon(v, 17, 94, 2);
    LlWindow(95, 76, 218, 130);
    if (d->egg) {
        Em2Text(CTR_TR("Egg", "Huevo", "Ei"), 107, 85, S_INK, EM_DARK_SH, 301);
        SRule(107, 98, 194);
        BDescribeN(d->hatch, 107, 103, 194, 5, S_LABEL);
        return;
    }
    {
        int second = strcmp(d->types[0], d->types[1]) != 0 && d->types[1][0];
        char hp[16], no[16];
        int w;

        snprintf(no, sizeof(no), "No.%s", d->dex);
        LlRibbonBox(7, 76, UiTextWidth(no, 1) + 18, 11, 7, LL_TL, LL_RIB_TOP, LL_RIB_BOT, no);
        w = UiTextWidth(d->types[0], 1) + 12;
        LlTypeBadge(7 + (84 - w) / 2, 168, d->types[0], UiTypeColor(d->typeIds[0]));
        if (second) {
            w = UiTextWidth(d->types[1], 1) + 12;
            LlTypeBadge(7 + (84 - w) / 2, 183, d->types[1], UiTypeColor(d->typeIds[1]));
        }
        SRowText(107, 85, 194, CTR_TR("Species", "Especie", "Spezies"), d->species, 1);
        SRowText(107, 102, 194, CTR_TR("Item", "Objeto", "Item"), d->item[0] ? d->item : CTR_TR("None", "Ninguno", "Keines"), 1);
        SRowText(107, 119, 194, CTR_TR("Nature", "Naturaleza", "Wesen"), d->nature, 1);
        SRowText(107, 136, 194, CTR_TR("Ability", "Habilidad", "F\xC3\xA4higkeit"), d->ability, 1);
        BDescribeN(d->abilityDesc, 107, 153, 194, 2, S_LABEL);
        snprintf(hp, sizeof(hp), "%u/%u", d->hpCur, d->hpMax);
        UiText(CTR_TR("HP", "PS", "KP"), 107, 187, S_INK, EM_DARK_SH, 1, 129);
        LlHpBar(129, 188, 112, d->hpCur, d->hpMax);
        UiTextRight(hp, 301, 187, S_INK, EM_DARK_SH, 1);
    }
}

/* The right window's other view: each stat's IV as a bar and a number, its EV as a number, and Hidden Power's type and power. */
static void SIvEv(const CtrSummaryUiView *v)
{
    static const char *const sShort[6] = {CTR_TR("HP", "PS", "KP"), CTR_TR("Atk", "Atq", "Ang"), CTR_TR("Def", "Def", "Ver"),
                                          CTR_TR("Sp.A", "A.Esp", "SpA"), CTR_TR("Sp.D", "D.Esp", "SpV"), CTR_TR("Spe", "Vel", "Ini")};
    const CtrSummaryView *d = &v->data;
    char text[8];
    int bw;

    UiTextRight("IV", 282, 96, S_LABEL, EM_DARK_SH, 1);
    UiTextRight("EV", 303, 96, S_LABEL, EM_DARK_SH, 1);
    for (int i = 0; i < 6; ++i) {
        int y = 109 + 12 * i;

        UiText(sShort[i], 172, y, S_INK, EM_DARK_SH, 1, 206);
        SMiniBar(208, y + 3, 52, d->iv[i], 31, 0);
        snprintf(text, sizeof(text), "%u", d->iv[i]);
        UiTextRight(text, 282, y, S_INK, EM_DARK_SH, 1);
        snprintf(text, sizeof(text), "%u", d->ev[i]);
        UiTextRight(text, 303, y, S_INK, EM_DARK_SH, 1);
    }
    SRule(172, 182, 131);
    snprintf(text, sizeof(text), "%u", d->hpPower);
    UiTextRight(text, 303, 187, S_INK, EM_DARK_SH, 1);
    bw = UiTextWidth(d->hpTypeName, 1) + 12;
    LlTypeBadge(303 - UiTextWidth(text, 1) - 6 - bw, 185, d->hpTypeName, UiTypeColor(d->hpType));
    Em2Text(CTR_TR("Hidden power", "Poder oculto", "Kraftreserve"), 172, 187, S_LABEL, EM_DARK_SH, 303 - UiTextWidth(text, 1) - 10 - bw);
}

static void SStats(const CtrSummaryUiView *v)
{
    static const char *const sLabels[5] = {CTR_TR("Attack", "Ataque", "Angriff"), CTR_TR("Defense", "Defensa", "Vert."), CTR_TR("Sp. atk", "At. esp.", "Sp. Ang."), CTR_TR("Sp. def", "Def. esp.", "Sp. Ver."), CTR_TR("Speed", "Velocidad", "Init.")};
    const CtrSummaryView *d = &v->data;
    char hp[16];

    LlWindow(7, 76, 151, 130);
    LlWindow(162, 76, 151, 130);
    snprintf(hp, sizeof(hp), "%u/%u", d->hpCur, d->hpMax);
    UiText(CTR_TR("HP", "PS", "KP"), 17, 84, S_INK, EM_DARK_SH, 1, 37);
    LlHpBar(37, 84, 111, d->hpCur, d->hpMax);
    UiTextRight(hp, 148, 98, S_INK, EM_DARK_SH, 1);
    for (int i = 0; i < 5; ++i) {
        int y = 114 + 18 * i;

        Em2Text(sLabels[i], 17, y, S_LABEL, EM_DARK_SH, 76);
        SMiniBar(78, y + 3, 46, SNumber(d->stats[i], 0), 180, 0);
        UiTextRight(d->stats[i], 148, y, S_INK, EM_DARK_SH, 1);
    }
    if (!d->egg) {
        const CtrPokeTouchRect *sw = &sSIvSwitch;
        int pressed = v->pressed == SUI_HIT_IVTOGGLE;

        if (v->ivView) {
            SIvEv(v);
        } else {
            Em2Text(CTR_TR("EXP. points", "Puntos EXP.", "Exp. Punkte"), 172, 84, S_LABEL, EM_DARK_SH, 250);
            UiTextRight(d->exp, 303, 99, S_INK, EM_DARK_SH, 1);
            SRule(172, 116, 131);
            Em2Text(CTR_TR("To next Lv.", "Para subir", "N\xC3\xA4" "chst. Lv."), 172, 122, S_LABEL, EM_DARK_SH, 303);
            UiTextRight(d->nextExp, 303, 137, EM2_COUNT_INK, EM2_COUNT_SH, 1);
            SRule(172, 156, 131);
            SRowText(172, 162, 131, CTR_TR("Ability", "Habilidad", "F\xC3\xA4higkeit"), d->ability, 0);
        }
        /* the ribbon that turns the page: orange, pressed it sinks 1 px */
        LlRibbonBox(sw->x, sw->y + (pressed ? 1 : 0), sw->w, sw->h - 1, 7, 0, UI_RGB(255, 196, 90), UI_RGB(232, 112, 28),
                    v->ivView ? CTR_TR("EXP", "EXP", "EP") : "IV / EV");
    }
}

/* A move card of the moves page: a cream card with the type's colour as its left band, the name, the type, power,
 * accuracy and PP. An empty slot is a hollow. */
static void SMoveCard(const CtrSummaryUiView *v, int i)
{
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect r = SCard(i);
    int fy, bw;

    if (!d->moves[i]) {
        UiRoundFill(r.x, r.y, r.w, r.h - 2, 5, UI_RGB(20, 3, 4));
        UiRoundFill(r.x + 1, r.y + 1, r.w - 2, r.h - 4, 4, UI_RGB(54, 14, 14));
        return;
    }
    fy = EmButton(r.x, r.y, r.w, r.h, &kEmCream, 2, v->pressed == SUI_HIT_MOVE + i);
    UiRoundFill(r.x + 1, fy + 1, 7, r.h - 4, 3, UiTypeColor(d->moveTypeIds[i]));
    Em2Text(d->moveNames[i], r.x + 14, fy + 7, S_INK, EM_DARK_SH, r.x + r.w - 6);
    bw = UiTextWidth(d->moveTypes[i], 1) + 12;
    LlTypeBadge(r.x + r.w - 6 - bw, fy + 22, d->moveTypes[i], UiTypeColor(d->moveTypeIds[i]));
    {
        unsigned pp = SNumber(d->pp[i], 0), max = SNumber(d->pp[i], 1);

        UiText(CTR_TR("Pwr", "Pot", "St\xC3\xA4"), r.x + 14, fy + 25, S_LABEL, EM_DARK_SH, 1, r.x + 40);
        UiText(d->power[i], r.x + 40, fy + 25, S_INK, EM_DARK_SH, 1, r.x + r.w - 6 - bw - 2);
        UiText(CTR_TR("Acc", "Pre", "Gen"), r.x + 14, fy + 41, S_LABEL, EM_DARK_SH, 1, r.x + 40);
        UiText(d->accuracy[i], r.x + 40, fy + 41, S_INK, EM_DARK_SH, 1, r.x + 84);
        UiText("PP", r.x + 86, fy + 41, S_LABEL, EM_DARK_SH, 1, r.x + 104);
        UiText(d->pp[i], r.x + 104, fy + 41, SPpColor(pp, max), EM_DARK_SH, 1, r.x + r.w - 4);
    }
}

/* A row of the move details: cream with the type's colour as its left band; FireRed's cursor in flame with its select ring and
 * cursor triangle, the move being moved (a swap) ringed, CANCEL slate. */
static void SMoveRow(const CtrSummaryUiView *v, int i)
{
    const CtrSummaryView *d = &v->data;
    CtrPokeTouchRect r = SRow(i);
    int cursor = d->cursor == i, swap = d->swapping && d->swapCursor == i && i < 4;
    int pressed = v->pressed == (i < 4 ? SUI_HIT_MOVE + i : SUI_HIT_CANCEL);
    const EmPal *pal = LlPick(cursor, i == 4 ? &kEmSlate : &kEmCream);
    int fy;

    if (i == 4 && d->swapping)
        return;
    if (cursor || swap)
        LlSelectRing(r.x, r.y, r.w, r.h);
    fy = EmButton(r.x, r.y, r.w, r.h, pal, 2, pressed);
    if (i == 4) {
        if (cursor)
            LlCursor(r.x + 9, fy + (r.h - 2 - 10) / 2, EM_WHITE);
        EmLabel(CTR_TR("Cancel", "Cancelar", "Zur\xC3\xBC" "ck"), r.x, r.w, fy, r.h - 2, pal);
    } else if (d->moves[i]) {
        if (!cursor)
            UiRoundFill(r.x + 1, fy + 1, 5, r.h - 4, 2, UiTypeColor(d->moveTypeIds[i]));
        if (cursor || swap)
            LlCursor(r.x + 9, fy + (r.h - 2 - 10) / 2, cursor ? EM_WHITE : LL_CURSOR);
        Em2Text(d->moveNames[i], r.x + 20, fy + 5, pal->ink, pal->inkSh, r.x + 108);
        UiTextRight(d->pp[i], r.x + r.w - 7, fy + 5, cursor ? pal->ink : SPpColor(SNumber(d->pp[i], 0), SNumber(d->pp[i], 1)),
                    pal->inkSh, 1);
    } else
        UiText("-", r.x + 20, fy + 5, S_LABEL, EM_DARK_SH, 1, r.x + r.w);
}

static void SMoveDetail(const CtrSummaryUiView *v)
{
    const CtrSummaryView *d = &v->data;
    int i = d->cursor;
    const CtrPokeTouchRect *p = &sSDetail;

    LlWindow(p->x, p->y, p->w, p->h);
    if (i < 0 || i >= 4 || !d->moves[i]) {
        CtrPokeTouchRect in = {(int16_t)(p->x + 5), p->y, (int16_t)(p->w - 10), p->h};

        UiTextCentered(CTR_TR("Back to the moves", "Volver a los ataques", "Zur\xC3\xBC" "ck zu den Attacken"), &in, -1, S_LABEL, EM_DARK_SH, 1);
        return;
    }
    {
        int x = p->x + 10, right = p->x + p->w - 10;

        Em2Text(d->moveNames[i], x, p->y + 8, S_INK, EM_DARK_SH, right);
        SRule(x, p->y + 22, p->w - 20);
        LlTypeBadge(x, p->y + 27, d->moveTypes[i], UiTypeColor(d->moveTypeIds[i]));
        UiText(CTR_TR("Pwr", "Pot", "St\xC3\xA4"), x + 62, p->y + 28, S_LABEL, EM_DARK_SH, 1, right);
        UiTextRight(d->power[i], right, p->y + 28, S_INK, EM_DARK_SH, 1);
        UiText(CTR_TR("Acc", "Pre", "Gen"), x + 62, p->y + 42, S_LABEL, EM_DARK_SH, 1, right);
        UiTextRight(d->accuracy[i], right, p->y + 42, S_INK, EM_DARK_SH, 1);
        SRule(x, p->y + 57, p->w - 20);
        BDescribeN(d->moveDesc, x, p->y + 62, p->w - 20, 5, S_LABEL);
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
    const char *memo = CTR_TR("Trainer memo", "Notas entrenador", "Trainer-Memo");

    LlWindow(7, 76, 306, 130);
    snprintf(ribbons, sizeof(ribbons), "%u", d->ribbons);
    SRowText(19, 83, 282, "OT", d->ot, 1);
    SRowText(19, 98, 282, CTR_TR("ID No.", "N.\xC2\xBA ID", "ID-Nr."), d->id, 1);
    if (!d->egg) {
        SRowText(19, 113, 282, CTR_TR("Nature", "Naturaleza", "Wesen"), d->nature, 1);
        SRowText(19, 128, 282, CTR_TR("Ribbons", "Cintas", "B\xC3\xA4nder"), ribbons, 1);
    }
    LlRibbonBox(14, d->egg ? 113 : 144, UiTextWidth(memo, 1) + 18, 13, 7, 0, LL_RIB_TOP, LL_RIB_BOT, memo);
    BDescribeN(d->memo, 19, d->egg ? 129 : 160, 282, d->egg ? 2 : 3, S_INK);
    if (d->egg)
        BDescribeN(d->hatch, 19, 156, 282, 3, S_LABEL);
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
    return v->loading ? "" : v->data.page == SUI_PAGE_MOVES ? CTR_TR("Tap a move for its details.", "Toca un ataque para ver sus datos.", "Attacke antippen f\xC3\xBCr Details.")
         : v->data.page == SUI_PAGE_MOVES_INFO ? (v->data.swapping ? CTR_TR("Tap where to move it.", "Toca d\xC3\xB3nde moverlo.", "Neuen Platz antippen.") : CTR_TR("Tap the selected move to move it.", "Toca el ataque elegido para moverlo.", "Ausgew\xC3\xA4hlte Attacke antippen zum Tauschen."))
         : v->data.egg ? "" : CTR_TR("Up / down: another Pok\xC3\xA9mon", "Arriba / abajo: otro Pok\xC3\xA9mon", "Hoch / Runter: anderes Pok\xc3\xa9mon");
}

static void SFooter(const CtrSummaryUiView *v)
{
    const char *hint = SHint(v);
    int pressed = v->pressed == SUI_HIT_BACK, w = UiTextWidth(CTR_TR("Back", "Atr\xC3\xA1s", "ZurÃ¼" "ck"), 1), fy, x;

    TsBgRect(sSFooter.x, sSFooter.y, sSBack.x + sSBack.w - sSFooter.x, sSFooter.h);
    LlWindow(sSFooter.x, sSFooter.y, sSFooter.w, sSFooter.h);
    Em2Text(hint, sSFooter.x + 12, sSFooter.y + (sSFooter.h - UI_TEXT_H) / 2, S_LABEL, EM_DARK_SH, sSFooter.x + sSFooter.w - 10);
    fy = EmButton(sSBack.x, sSBack.y, sSBack.w, sSBack.h, &kEmSlate, 2, pressed);
    x = sSBack.x + (sSBack.w - w - 12) / 2;
    LlArrow(x, fy + (sSBack.h - 2 - 10) / 2, 6, 10, 2, EM_WHITE);
    UiText(CTR_TR("Back", "Atr\xC3\xA1s", "ZurÃ¼" "ck"), x + 12, fy + (sSBack.h - 2 - UI_TEXT_H) / 2, EM_WHITE, kEmSlate.inkSh, 1, sSBack.x + sSBack.w);
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

    int ia = a->pressed == SUI_HIT_IVTOGGLE, ib = b->pressed == SUI_HIT_IVTOGGLE;

    return a->loading != b->loading || a->icon != b->icon || a->trainer != b->trainer || pa != pb
        || a->ivView != b->ivView || ia != ib || memcmp(&a->data, &b->data, sizeof(a->data));
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
