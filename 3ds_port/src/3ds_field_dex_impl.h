/* The Pokedex in the viewport (3ds_poketouch.h CtrPokeTouchDex), included at the end of 3ds_poketouch.c after the
 * field Bag, whose ember pieces it shares. A remote for FireRed's own Pokedex: the top menu's sections as tabs and its
 * rows as cards with the Pokedex's own icons, an ordered list as rows beside the selected Pokemon's picture, a habitat
 * page as its Pokemon, and a Pokemon's page with its picture, data and entry. Every cursor is the Pokedex's own
 * (3ds_bottom.c walks them with its keys); every picture and word is FireRed's (CtrPokeTouch_Dex*, the bridge's text),
 * but for the port's few labels (3ds_ui_strings.h). */

static const CtrPokeTouchRect sFDTab[3] = {{4, 4, 78, 24}, {85, 4, 78, 24}, {166, 4, 78, 24}};
static const CtrUiString sFDTabLabel[3] = {UIS_DEX_LISTS, UIS_DEX_HABITATS, UIS_DEX_SEARCH};
static const CtrPokeTouchRect sFDClose = {4, 140, 240, 24};
static const CtrPokeTouchRect sFDGridUp = {230, 34, 14, 63}, sFDGridDown = {230, 101, 14, 63};
static const CtrPokeTouchRect sFDListHead = {4, 4, 150, 24};
static const CtrPokeTouchRect sFDListUp = {135, 34, 20, 63}, sFDListDown = {135, 101, 20, 63};
static const CtrPokeTouchRect sFDSide = {158, 4, 86, 160};
static const CtrPokeTouchRect sFDPrev = {220, 4, 24, 80}, sFDNext = {220, 86, 24, 80};
static const CtrPokeTouchRect sFDArea = {156, 124, 38, 18}, sFDCry = {200, 124, 38, 18};   /* on the card, lower right: face + depth */
#define FD_GRID_PAGE 6

/* ---- Pictures (decoded once, kept by species or row id) ---- */
#define FD_PICS 6
static struct {
    uint16_t species;
    uint8_t ok;
    uint8_t px[64 * 64];
    uint16_t pal[16];
    uint8_t hasBig;                 /* HeartGold's picture whole (CtrPokeTouch_DexMonPic80) */
    uint8_t big[80 * 80];
} sFDPic[FD_PICS];
static int sFDPicNext;
static struct {
    uint8_t ready, ok;
    uint8_t px[64 * 48];
    uint16_t pal[16];
} sFDIcon[24];                          /* by id + 3: the header, CANCEL, the habitats and the lists */
static struct {
    uint8_t ready, ok;
    uint8_t px[64];
    uint16_t pal[16];
} sFDCaught;

static void FDTiles(const unsigned char *tiles, int w, int h, uint8_t *px)
{
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            unsigned byte = tiles[((y / 8) * (w / 8) + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2];
            px[y * w + x] = (uint8_t)((byte >> ((x & 1) * 4)) & 15);
        }
}

static void FDPal(const unsigned short *in, uint16_t *out)
{
    for (int i = 0; i < 16; ++i) {
        unsigned c = in[i];
        out[i] = GbaColor(c);
    }
}

static int FDPicSlot(unsigned species)
{
    static unsigned char tiles[0x800];
    static unsigned short pal[16];
    int slot;

    if (species == 0)
        return -1;
    for (int i = 0; i < FD_PICS; ++i)
        if (sFDPic[i].species == species)
            return sFDPic[i].ok ? i : -1;
    slot = sFDPicNext;
    sFDPicNext = (sFDPicNext + 1) % FD_PICS;
    sFDPic[slot].species = (uint16_t)species;
    sFDPic[slot].ok = (uint8_t)(CtrPokeTouch_DexMonPic(species, tiles, pal) != 0);
    if (!sFDPic[slot].ok)
        return -1;
    FDTiles(tiles, 64, 64, sFDPic[slot].px);
    FDPal(pal, sFDPic[slot].pal);
    {
        static unsigned char linear[80 * 80 / 2];
        unsigned short big[16];

        sFDPic[slot].hasBig = (uint8_t)(CtrPokeTouch_DexMonPic80(species, linear, big) != 0);
        for (int i = 0; sFDPic[slot].hasBig && i < 80 * 80; ++i)
            sFDPic[slot].big[i] = (uint8_t)((linear[i / 2] >> ((i & 1) * 4)) & 15);
    }
    return slot;
}

/* A species' front picture, its top-left corner at (x, y), its lowest `cut` rows left out. */
static void FDPicDraw(unsigned species, int x, int y, int cut)
{
    int slot = FDPicSlot(species);

    if (slot >= 0)
        UiImage(sFDPic[slot].px, sFDPic[slot].pal, 64, 64, 64 - cut, x, y, 1, 1);
}

/* The same, whole when HeartGold's 80x80 picture is there: on the 64x64 box's centre, 8 pixels past each side. */
static void FDPicDrawBig(unsigned species, int x, int y)
{
    int slot = FDPicSlot(species);

    if (slot >= 0 && sFDPic[slot].hasBig)
        UiImage(sFDPic[slot].big, sFDPic[slot].pal, 80, 80, 80, x - 8, y - 8, 1, 1);
    else if (slot >= 0)
        UiImage(sFDPic[slot].px, sFDPic[slot].pal, 64, 64, 64, x, y, 1, 1);
}

static void FDIconDraw(int id, int x, int y, int on)
{
    static unsigned char tiles[64 * 48 / 2];
    static unsigned short pal[16];
    int k = id + 3;

    if (k < 0 || k >= (int)(sizeof(sFDIcon) / sizeof(sFDIcon[0])))
        return;
    if (!sFDIcon[k].ready) {
        sFDIcon[k].ready = 1;
        sFDIcon[k].ok = (uint8_t)(CtrPokeTouch_DexMenuIcon(id, tiles, pal) != 0);
        if (sFDIcon[k].ok) {
            FDTiles(tiles, 64, 48, sFDIcon[k].px);
            FDPal(pal, sFDIcon[k].pal);
        }
    }
    if (sFDIcon[k].ok)
        UiImage(sFDIcon[k].px, sFDIcon[k].pal, 64, 64, 48, x, y, 1, on);
}

static int FDCaughtLoad(void)
{
    if (!sFDCaught.ready) {
        unsigned char tiles[32];
        unsigned short pal[16];

        sFDCaught.ready = 1;
        sFDCaught.ok = (uint8_t)(CtrPokeTouch_DexCaughtMarker(tiles, pal) != 0);
        if (sFDCaught.ok) {
            FDTiles(tiles, 8, 8, sFDCaught.px);
            FDPal(pal, sFDCaught.pal);
        }
    }
    return sFDCaught.ok;
}

static void FDCaughtDraw(int x, int y)
{
    if (FDCaughtLoad())
        UiImage(sFDCaught.px, sFDCaught.pal, 8, 8, 8, x, y, 1, 1);
}

static void FDFootprint(unsigned species, int x, int y)
{
    unsigned char bits[32];

    if (!CtrPokeTouch_DexFootprint(species, bits))
        return;
    for (int t = 0; t < 4; ++t)
        for (int row = 0; row < 8; ++row)
            for (int col = 0; col < 8; ++col)
                if (bits[t * 8 + row] & (1 << col))
                    Px(x + (t & 1) * 8 + col, y + (t >> 1) * 8 + row, UI_RGB(64, 72, 104));
}

/* ---- Text ---- */
/* `s` without `tail` at its end (FireRed's " POKeMON", " MODE"), into out. */
static void FDStrip(const char *s, const char *tail, char *out, int size)
{
    int n = (int)strlen(s), t = (int)strlen(tail);

    if (n > t && !strcmp(s + n - t, tail))
        n -= t;
    if (n >= size)
        n = size - 1;
    memcpy(out, s, (size_t)n);
    out[n] = 0;
}

/* A top menu row's label on its card: KANTO / NATIONAL for the lists, FireRed's own words otherwise. */
static void FDCardLabel(const CtrPokeTouchDex *d, int i, char *out, int size)
{
    int id = d->menu[i].id;

    if (id == PT_DEX_ID_KANTO || id == PT_DEX_ID_NATIONAL)
        snprintf(out, (size_t)size, "%s", CtrUiText(id == PT_DEX_ID_KANTO ? UIS_DEX_KANTO : UIS_DEX_NATIONAL));
    else if (id >= 0 && id < 9)
        FDStrip(d->menu[i].name, " POK\xC3\xA9MON", out, size);
    else
        FDStrip(d->menu[i].name, " MODE", out, size);
}

static void FDNumber(unsigned n, char *out, int size)
{
    snprintf(out, (size_t)size, "No.%03u", n > 999 ? 999 : n);
}

/* FireRed's own conversions: feet and inches, pounds to the tenth. */
static void FDHeight(const CtrPokeTouchDex *d, char *out, int size)
{
    unsigned inches, feet;

    if (!(d->sel.flags & 2)) {
        snprintf(out, (size_t)size, "??" "'??\"");
        return;
    }
    inches = 10000u * d->height / 254u;
    if (inches % 10 >= 5)
        inches += 10;
    feet = inches / 120;
    inches = (inches - feet * 120) / 10;
    snprintf(out, (size_t)size, "%u'%02u\"", feet, inches);
}

static void FDWeight(const CtrPokeTouchDex *d, char *out, int size)
{
    unsigned lbs;

    if (!(d->sel.flags & 2)) {
        snprintf(out, (size_t)size, "????.? %s", CtrUiText(UIS_DEX_LBS));
        return;
    }
    lbs = (unsigned)d->weight * 100000u / 4536u;
    if (lbs % 10 >= 5)
        lbs += 10;
    snprintf(out, (size_t)size, "%u.%u %s", lbs / 100, lbs % 100 / 10, CtrUiText(UIS_DEX_LBS));
}

/* ---- The top menu: sections (the rows after each header), the one the cursor is in, its cards ---- */
static int FDSection(const CtrPokeTouchDex *d, int index)
{
    int section = -1;

    for (int i = 0; i <= index && i < d->menuCount; ++i)
        if (d->menu[i].id == PT_DEX_ID_HEADER)
            ++section;
    return section;
}

/* The rows of section `k` (first, count). */
static int FDSectionRows(const CtrPokeTouchDex *d, int k, int *first)
{
    int section = -1, n = 0;

    *first = -1;
    for (int i = 0; i < d->menuCount; ++i) {
        if (d->menu[i].id == PT_DEX_ID_HEADER) {
            ++section;
            continue;
        }
        if (section == k) {
            if (*first < 0)
                *first = i;
            ++n;
        }
    }
    return n;
}

static int FDCancelRow(const CtrPokeTouchDex *d)
{
    for (int i = 0; i < d->menuCount; ++i)
        if (d->menu[i].id == PT_DEX_ID_CANCEL)
            return i;
    return -1;
}

/* The tab shown: the cursor's section (CLOSE belongs to the lists). */
static int FDTabShown(const CtrPokeTouchDex *d)
{
    int k = FDSection(d, d->menuCursor);

    return k < 0 || k > 2 ? 0 : k;
}

/* A grid section's page (six cards) and the rows it shows. */
static int FDGridPage(const CtrPokeTouchDex *d, int k, int *first, int *count, int *pages)
{
    int start, n = FDSectionRows(d, k, &start), at = d->menuCursor - start, page;

    page = at >= 0 && at < n ? at / FD_GRID_PAGE : 0;
    *pages = (n + FD_GRID_PAGE - 1) / FD_GRID_PAGE;
    *first = start + page * FD_GRID_PAGE;
    *count = n - page * FD_GRID_PAGE > FD_GRID_PAGE ? FD_GRID_PAGE : n - page * FD_GRID_PAGE;
    return page;
}

static CtrPokeTouchRect FDGridCard(int i)
{
    CtrPokeTouchRect r = {(int16_t)(4 + (i % 3) * 76), (int16_t)(34 + (i / 3) * 66), 74, 64};
    return r;
}

static CtrPokeTouchRect FDListCard(const CtrPokeTouchDex *d, int i, int n)
{
    CtrPokeTouchRect r = {4, 34, 240, 102};

    (void)d;
    if (n > 1) {
        r.x = (int16_t)(i == 0 ? 4 : 126);
        r.w = 118;
    }
    return r;
}

static CtrPokeTouchRect FDListRow(int i)
{
    CtrPokeTouchRect r = {4, (int16_t)(34 + 26 * i), 128, 25};
    return r;
}

static CtrPokeTouchRect FDSlot(int i)
{
    CtrPokeTouchRect r = {(int16_t)(4 + (i % 2) * 108), (int16_t)(4 + (i / 2) * 82), 104, 80};
    return r;
}

int CtrPokeTouch_DexTabRow(const CtrPokeTouchView *v, int tab)
{
    int first;

    return FDSectionRows(&v->dex, tab, &first) > 0 ? first : -1;
}

int CtrPokeTouch_DexGridPageRow(const CtrPokeTouchView *v, int dir)
{
    const CtrPokeTouchDex *d = &v->dex;
    int tab = FDTabShown(d), start, n, first, count, pages, page;

    if (tab == 0)
        return -1;
    n = FDSectionRows(d, tab, &start);
    page = FDGridPage(d, tab, &first, &count, &pages) + dir;
    return page >= 0 && page < pages && start + page * FD_GRID_PAGE < start + n ? start + page * FD_GRID_PAGE : -1;
}

int CtrPokeTouch_DexHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchDex *d = &v->dex;

    if (!d->on || x >= 248)
        return -1;
    switch (d->state) {
    case PT_DEX_STATE_MENU: {
        int tab = FDTabShown(d), first, count, pages;

        for (int i = 0; i < 3; ++i)
            if (UiIn(&sFDTab[i], x, y))
                return PT_DEX_HIT_TAB + i;
        if (tab == 0) {
            int n = FDSectionRows(d, 0, &first), cancel = FDCancelRow(d);

            for (int i = 0; i < n && i < 2; ++i) {
                CtrPokeTouchRect r = FDListCard(d, i, n);
                if (UiIn(&r, x, y))
                    return PT_DEX_HIT_ITEM + first + i;
            }
            if (cancel >= 0 && UiIn(&sFDClose, x, y))
                return PT_DEX_HIT_ITEM + cancel;
            return -1;
        }
        FDGridPage(d, tab, &first, &count, &pages);
        if (pages > 1 && UiIn(&sFDGridUp, x, y))
            return PT_DEX_HIT_UP;
        if (pages > 1 && UiIn(&sFDGridDown, x, y))
            return PT_DEX_HIT_DOWN;
        for (int i = 0; i < count; ++i) {
            CtrPokeTouchRect r = FDGridCard(i);
            if (UiIn(&r, x, y))
                return PT_DEX_HIT_ITEM + first + i;
        }
        return -1;
    }
    case PT_DEX_STATE_LIST:
        if (UiIn(&sFDListUp, x, y))
            return d->listFirst > 0 ? PT_DEX_HIT_UP : -1;
        if (UiIn(&sFDListDown, x, y))
            return d->listFirst + PT_DEX_ROWS < d->listCount ? PT_DEX_HIT_DOWN : -1;
        for (int i = 0; i < PT_DEX_ROWS; ++i) {
            CtrPokeTouchRect r = FDListRow(i);
            if (d->listFirst + i < d->listCount && UiIn(&r, x, y))
                return PT_DEX_HIT_ROW + i;
        }
        return -1;
    case PT_DEX_STATE_CATEGORY:
        if (UiIn(&sFDPrev, x, y))
            return d->canPrev ? PT_DEX_HIT_PREV : -1;
        if (UiIn(&sFDNext, x, y))
            return d->canNext ? PT_DEX_HIT_NEXT : -1;
        for (int i = 0; i < d->slots && i < 4; ++i) {
            CtrPokeTouchRect r = FDSlot(i);
            if (UiIn(&r, x, y))
                return PT_DEX_HIT_SLOT + i;
        }
        return -1;
    case PT_DEX_STATE_PAGE:
        if (d->added) {
            /* The page a catch added: anywhere on it or its note goes on with the battle (its A). */
            CtrPokeTouchRect note = {NOTE_X, NOTE_Y, NOTE_W_SIDE, NOTE_H};

            return UiIn(&sFBArea, x, y) || UiIn(&note, x, y) ? PT_DEX_HIT_CONTINUE : -1;
        }
        if (UiIn(&sFDArea, x, y))
            return PT_DEX_HIT_AREA;
        if (UiIn(&sFDCry, x, y))
            return PT_DEX_HIT_CRY;
        return -1;
    }
    return -1;
}

/* ---- Drawing (the Llama look: cream cards, flame for the chosen with its 2 px ring, FireRed's window for panels and notes) ---- */
static void FDLabel(const char *s, int x, int w, int y, const EmPal *p)
{
    char cut[40];

    Em2Fit(s, w - 4, cut, sizeof(cut));
    UiText(cut, x + (w - UiTextWidth(cut, 1)) / 2, y, p->ink, p->inkSh, 1, x + w);
}

/* A card of the zone r: ringed and flame when chosen; returns the face's top. */
static int FDCard(const CtrPokeTouchRect *r, const EmPal *p, int chosen, int pressed)
{
    if (chosen)
        LlSelectRing(r->x, r->y, r->w, r->h);
    return EmButton(r->x, r->y, r->w, r->h, p, FB_DEPTH, pressed);
}

/* SEEN n / OWNED n: the words dim, the seen count dark, the owned count blue. */
static void FDCounts(int x, int y, int w, unsigned seen, unsigned owned)
{
    char n[8];

    Em2Text(CtrUiText(UIS_DEX_SEEN), x + 10, y, EM_DIM, EM_DARK_SH, x + w - 40);
    snprintf(n, sizeof(n), "%u", seen);
    UiTextRight(n, x + w - 10, y, EM_DARK, EM_DARK_SH, 1);
    Em2Text(CtrUiText(UIS_DEX_OWNED), x + 10, y + 13, EM_DIM, EM_DARK_SH, x + w - 40);
    snprintf(n, sizeof(n), "%u", owned);
    UiTextRight(n, x + w - 10, y + 13, EM2_COUNT_INK, EM2_COUNT_SH, 1);
}

/* A page arrow (dir: 0 up, 1 down, 2 left, 3 right) on a cream card, red when there is somewhere to go. */
static void FDArrow(const CtrPokeTouchRect *r, int dir, int on)
{
    int fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, FB_DEPTH, 0), vert = dir < 2, aw = vert ? 8 : 6, ah = vert ? 5 : 10;

    LlArrow(r->x + (r->w - aw) / 2, fy + (r->h - FB_DEPTH - ah) / 2, aw, ah, dir, on ? LL_CURSOR : FB_IDLE_ARROW);
}

static void FDMenu(const CtrPokeTouchDex *d, int pressed)
{
    int tab = FDTabShown(d), first, count, pages;

    for (int i = 0; i < 3; ++i) {
        const EmPal *p = LlPick(tab == i, &kEmCream);
        int fy = EmButton(sFDTab[i].x, sFDTab[i].y, sFDTab[i].w, sFDTab[i].h, p, FB_DEPTH, pressed == PT_DEX_HIT_TAB + i);

        EmLabel(CtrUiText(sFDTabLabel[i]), sFDTab[i].x, sFDTab[i].w, fy, sFDTab[i].h - FB_DEPTH, p);
    }
    if (tab == 0) {
        int n = FDSectionRows(d, 0, &first), cancel = FDCancelRow(d);

        for (int i = 0; i < n && i < 2; ++i) {
            CtrPokeTouchRect r = FDListCard(d, i, n);
            int row = first + i, sel = d->menuCursor == row, nat = d->menu[row].id == PT_DEX_ID_NATIONAL;
            int fy = FDCard(&r, &kEmCream, sel, pressed == PT_DEX_HIT_ITEM + row), rw;
            char label[24];

            FDCardLabel(d, row, label, sizeof(label));
            rw = UiTextWidth(label, 1) + 18;
            LlRibbonBox(r.x + 1, fy + 1, rw < r.w - 2 ? rw : r.w - 2, 13, 7, LL_TL, LL_RIB_TOP, LL_RIB_BOT, label);
            FDIconDraw(d->menu[row].id, r.x + (r.w - 64) / 2, fy + 18, 1);
            FDCounts(r.x, fy + 70, r.w, d->seen[nat], d->owned[nat]);
        }
        if (cancel >= 0) {
            int sel = d->menuCursor == cancel;
            const EmPal *p = LlPick(sel, &kEmSlate);
            int fy = FDCard(&sFDClose, p, sel, pressed == PT_DEX_HIT_ITEM + cancel);

            LlArrow(sFDClose.x + 10, fy + (sFDClose.h - FB_DEPTH - 10) / 2, 6, 10, 2, EM_WHITE);
            EmLabel(d->menu[cancel].name, sFDClose.x, sFDClose.w, fy, sFDClose.h - FB_DEPTH, p);
        }
        return;
    }
    FDGridPage(d, tab, &first, &count, &pages);
    for (int i = 0; i < count; ++i) {
        CtrPokeTouchRect r = FDGridCard(i);
        int row = first + i, locked = d->menu[row].locked, sel = d->menuCursor == row;
        const EmPal *p = sel ? &kEmFlame : locked ? &kEmGrey : &kEmCream;
        int fy = FDCard(&r, p, sel, pressed == PT_DEX_HIT_ITEM + row);
        char label[24];

        FDIconDraw(d->menu[row].id, r.x + (r.w - 64) / 2, fy + 1, !locked);
        FDCardLabel(d, row, label, sizeof(label));
        FDLabel(label, r.x, r.w, fy + 50, p);
    }
    if (pages > 1) {
        int start, at;

        FDSectionRows(d, tab, &start);
        at = (first - start) / FD_GRID_PAGE;
        FDArrow(&sFDGridUp, 0, at > 0);
        FDArrow(&sFDGridDown, 1, at < pages - 1);
    }
}

/* The selected Pokemon beside a list: FireRed's window with its number in a ribbon, its picture on a plate, name and types. */
static void FDSidePanel(const CtrPokeTouchDex *d)
{
    const CtrPokeTouchRect *r = &sFDSide;
    int cx = r->x + r->w / 2, py = r->y + 26;
    char no[12], cut[40];

    LlWindow(r->x, r->y, r->w, r->h);
    FDNumber(d->sel.number, no, sizeof(no));
    LlRibbon(r->x + 1, r->y + 1, no, LL_RIB_TOP, LL_RIB_BOT);
    LlPlate(cx - 30, py, 60, EM2_SLATE, 0, 0);
    if (d->sel.flags & 1)
        FDPicDrawBig(d->sel.species, cx - 32, py - 2);
    Em2Fit(d->sel.flags & 1 ? d->sel.name : "-----", r->w - 12, cut, sizeof(cut));
    Em2Text(cut, cx - UiTextWidth(cut, 1) / 2, r->y + 96, EM_DARK, EM_DARK_SH, r->x + r->w - 4);
    if (d->type[0] >= 0) {
        int two = d->type[1] >= 0, y = r->y + (two ? 113 : 120);
        int w0 = UiTextWidth(d->typeName[0], 1) + 12;

        LlTypeBadge(cx - w0 / 2, y, d->typeName[0], UiTypeColor(d->type[0]));
        if (two)
            LlTypeBadge(cx - (UiTextWidth(d->typeName[1], 1) + 12) / 2, y + 17, d->typeName[1], UiTypeColor(d->type[1]));
    }
}

static void FDList(const CtrPokeTouchDex *d, int pressed)
{
    static const CtrUiString kOrder[6] = {UIS_DEX_KANTO, UIS_DEX_ATOZ, UIS_DEX_TYPE, UIS_DEX_LIGHTEST,
                                          UIS_DEX_SMALLEST, UIS_DEX_NATIONAL};
    const char *title = CtrUiText(kOrder[d->order < 6 ? d->order : 0]);
    int titleEnd = sFDListHead.x + 8 + UiTextWidth(title, 1), ty = sFDListHead.y + (sFDListHead.h - UI_TEXT_H) / 2;

    LlWindowPlain(sFDListHead.x, sFDListHead.y, sFDListHead.w, sFDListHead.h);
    UiText(title, sFDListHead.x + 8, ty, EM_DARK, EM_DARK_SH, 1, sFDListHead.x + sFDListHead.w - 6);
    if (d->order == 0 || d->order == 5) {
        char counts[32], owned[8];
        int nat = d->order == 5, right = sFDListHead.x + sFDListHead.w - 6;

        /* SEEN n  OWNED n when it fits beside the title; with three-digit counts OWNED becomes the caught ball of
           the rows, so the counts never run into KANTO or NATIONAL. */
        snprintf(counts, sizeof(counts), "%s %u  %s %u", CtrUiText(UIS_DEX_SEEN), d->seen[nat], CtrUiText(UIS_DEX_OWNED),
                 d->owned[nat]);
        if (right - UiTextWidth(counts, 1) >= titleEnd + 6) {
            UiTextRight(counts, right, ty, EM_DIM, EM_DARK_SH, 1);
        } else if (!FDCaughtLoad()) {
            snprintf(counts, sizeof(counts), "%u/%u", d->seen[nat], d->owned[nat]);
            UiTextRight(counts, right, ty, EM_DIM, EM_DARK_SH, 1);
        } else {
            snprintf(owned, sizeof(owned), "%u", d->owned[nat]);
            snprintf(counts, sizeof(counts), "%s %u", CtrUiText(UIS_DEX_SEEN), d->seen[nat]);
            UiTextRight(owned, right, ty, EM_DIM, EM_DARK_SH, 1);
            right -= UiTextWidth(owned, 1) + 10;
            FDCaughtDraw(right, ty + 1);
            UiTextRight(counts, right - 5, ty, EM_DIM, EM_DARK_SH, 1);
        }
    }
    for (int i = 0; i < PT_DEX_ROWS; ++i) {
        CtrPokeTouchRect r = FDListRow(i);
        int index = d->listFirst + i, sel = index == d->listCursor;
        const CtrPokeTouchDexMon *m = &d->row[i];
        const EmPal *p = LlPick(sel, &kEmCream);
        char no[8];
        int ry, ty2, nx;

        if (index >= d->listCount)
            break;
        ry = FDCard(&r, p, sel, pressed == PT_DEX_HIT_ROW + i);
        ty2 = ry + (r.h - FB_DEPTH - UI_TEXT_H) / 2;
        LlPlate(r.x + 3, ry + 2, 19, EM2_SLATE, 0, 0);
        if (m->flags & 2)
            FDCaughtDraw(r.x + 8, ry + 7);
        if (sel)
            LlCursor(r.x + 25, ry + (r.h - FB_DEPTH - 10) / 2, EM_WHITE);
        nx = r.x + (sel ? 35 : 27);
        snprintf(no, sizeof(no), "%03u", m->number > 999 ? 999 : m->number);
        UiText(no, nx, ty2, sel ? UI_RGB(255, 233, 168) : EM_DIM, p->inkSh, 1, nx + 24);
        Em2Text(m->flags & 1 ? m->name : "-----", r.x + (sel ? 58 : 50), ty2, p->ink, p->inkSh, r.x + r.w - 4);
    }
    if (d->listCount > PT_DEX_ROWS) {
        FDArrow(&sFDListUp, 0, d->listFirst > 0);
        FDArrow(&sFDListDown, 1, d->listFirst + PT_DEX_ROWS < d->listCount);
    }
    FDSidePanel(d);
}

static void FDCategory(const CtrPokeTouchDex *d, int pressed)
{
    for (int i = 0; i < d->slots && i < 4; ++i) {
        CtrPokeTouchRect r = FDSlot(i);
        const CtrPokeTouchDexMon *m = &d->mon[i];
        int sel = i == d->slot, fy = FDCard(&r, LlPick(sel, &kEmCream), sel, pressed == PT_DEX_HIT_SLOT + i);
        char label[40], cut[40];
        int lw;

        /* The picture on its plate filling the card, its number and name in a small window over its foot. */
        LlPlate(r.x + (r.w - 66) / 2, fy + 4, 66, EM2_SLATE, 0, 0);
        if (m->flags & 1)
            FDPicDrawBig(m->species, r.x + (r.w - 64) / 2, fy + 5);
        snprintf(label, sizeof(label), "%03u %s", m->number > 999 ? 999 : m->number, m->flags & 1 ? m->name : "-----");
        Em2Fit(label, r.w - 14, cut, sizeof(cut));
        lw = UiTextWidth(cut, 1) + 14;   /* the window's frame takes 4 px a side: 3 more of air round the text */
        LlWindowPlain(r.x + (r.w - lw) / 2, fy + 58, lw, 19);
        Em2Text(cut, r.x + (r.w - lw) / 2 + 7, fy + 58 + (19 - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, r.x + r.w - 6);
        if (m->flags & 2)
            FDCaughtDraw(r.x + 6, fy + 6);
    }
    FDArrow(&sFDPrev, 2, d->canPrev);
    FDArrow(&sFDNext, 3, d->canNext);
}

static void FDPage(const CtrPokeTouchDex *d)
{
    char no[12], text[32];
    int x = 88;

    LlWindow(4, 4, 240, 146);
    FDNumber(d->sel.number, no, sizeof(no));
    LlRibbon(5, 5, no, LL_RIB_TOP, LL_RIB_BOT);
    LlPlate(14, 26, 64, EM2_SLATE, 0, 0);
    FDPicDrawBig(d->sel.species, 14, 26);
    Em2Text(d->sel.name, x, 12, EM_DARK, EM_DARK_SH, 212);
    Em2Text(d->sel.flags & 2 ? d->kind : "?????", x, 26, EM_DIM, EM_DARK_SH, 212);
    FDFootprint(d->sel.species, 218, 10);
    if (d->type[0] >= 0) {
        int tx = x + LlTypeBadge(x, 48, d->typeName[0], UiTypeColor(d->type[0])) + 4;

        if (d->type[1] >= 0)
            LlTypeBadge(tx, 48, d->typeName[1], UiTypeColor(d->type[1]));
    }
    LlRule(14, 98, 224);
    Em2Text(CtrUiText(UIS_DEX_HT), 14, 106, EM_DIM, EM_DARK_SH, 60);
    FDHeight(d, text, sizeof(text));
    UiTextRight(text, 140, 106, EM_DARK, EM_DARK_SH, 1);
    Em2Text(CtrUiText(UIS_DEX_WT), 14, 124, EM_DIM, EM_DARK_SH, 60);
    FDWeight(d, text, sizeof(text));
    UiTextRight(text, 140, 124, EM_DARK, EM_DARK_SH, 1);
    if (!d->added) {
        const EmPal *pa = &kEmFlame, *pc = &kEmCream;
        int fa = EmButton(sFDArea.x, sFDArea.y, sFDArea.w, sFDArea.h, pa, FB_DEPTH, d->pressed == PT_DEX_HIT_AREA);
        int fc = EmButton(sFDCry.x, sFDCry.y, sFDCry.w, sFDCry.h, pc, FB_DEPTH, d->pressed == PT_DEX_HIT_CRY);

        EmLabel(CtrUiText(UIS_DEX_AREA), sFDArea.x, sFDArea.w, fa, sFDArea.h - FB_DEPTH, pa);
        EmLabel(CtrUiText(UIS_DEX_CRY), sFDCry.x, sFDCry.w, fc, sFDCry.h - FB_DEPTH, pc);
    }
}

/* The viewport's area: the screen the Pokedex shows. */
static void FDexFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchDex *d = &v->dex;

    EmSurface();
    TsBgRect(sFBArea.x - 4, sFBArea.y - 4, sFBArea.w + 8, sFBArea.h + 8);
    if (d->state == PT_DEX_STATE_MENU)
        FDMenu(d, d->pressed);
    else if (d->state == PT_DEX_STATE_LIST)
        FDList(d, d->pressed);
    else if (d->state == PT_DEX_STATE_CATEGORY)
        FDCategory(d, d->pressed);
    else if (d->state == PT_DEX_STATE_PAGE)
        FDPage(d);
    sUiS = &sUiNavy;
}

/* The note under the Pokedex, FireRed's window: what the selection is (a Pokemon's page has its AREA and CRY buttons there). */
static void FDexNote(const CtrPokeTouchView *v, int w)
{
    const CtrPokeTouchDex *d = &v->dex;
    char title[40], body[96];

    EmSurface();
    if (d->state == PT_DEX_STATE_PAGE && !d->added) {
        /* The entry in FireRed's text window (the buttons are on the card); BDescribeN ends it with "..." if it runs on. */
        LlWindow(NOTE_X, NOTE_Y, w, NOTE_H);
        BDescribeN(d->desc[0] ? d->desc : "", NOTE_X + 13, NOTE_Y + 9, w - 26, 3, EM_DIM);
        sUiS = &sUiNavy;
        return;
    }
    title[0] = body[0] = 0;
    if (d->state == PT_DEX_STATE_MENU && d->menuCursor >= 0 && d->menuCursor < d->menuCount) {
        int id = d->menu[d->menuCursor].id;

        snprintf(title, sizeof(title), "%s", d->menu[d->menuCursor].name);
        snprintf(body, sizeof(body), "%s", CtrUiText(id == PT_DEX_ID_CANCEL ? UIS_DEX_NOTE_CLOSE
                                                   : id == PT_DEX_ID_KANTO || id == PT_DEX_ID_NATIONAL ? UIS_DEX_NOTE_LIST
                                                   : id >= 0 && id < 9 ? (d->menu[d->menuCursor].locked ? UIS_DEX_NOTE_LOCKED
                                                                                                       : UIS_DEX_NOTE_HABITAT)
                                                   : UIS_DEX_NOTE_SEARCH));
    } else if (d->state == PT_DEX_STATE_PAGE) {
        snprintf(title, sizeof(title), "%s", d->sel.name);
        snprintf(body, sizeof(body), "%s", CtrUiText(UIS_DEX_NOTE_ADDED));
    } else if (d->state == PT_DEX_STATE_LIST || d->state == PT_DEX_STATE_CATEGORY) {
        snprintf(title, sizeof(title), "%s", d->sel.flags & 1 ? d->sel.name : "-----");
        if (d->state == PT_DEX_STATE_CATEGORY)
            snprintf(body, sizeof(body), "%s %u/%u", CtrUiText(UIS_DEX_PAGE), d->page, d->pages);
        else
            snprintf(body, sizeof(body), "%s", CtrUiText(d->sel.flags & 1 ? UIS_DEX_NOTE_OPEN : UIS_DEX_NOTE_UNSEEN));
    }
    LlWindow(NOTE_X, NOTE_Y, w, NOTE_H);
    Em2Text(title, NOTE_X + 13, NOTE_Y + 8, EM_DARK, EM_DARK_SH, NOTE_X + w - 13);
    LlRule(NOTE_X + 13, NOTE_Y + 21, w - 26);
    BDescribeN(body, NOTE_X + 13, NOTE_Y + 25, w - 26, 3, EM_DIM);
    sUiS = &sUiNavy;
}
