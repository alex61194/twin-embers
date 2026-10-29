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
static const CtrPokeTouchRect sFDArea = {4, 174, 80, 62}, sFDCry = {88, 174, 80, 62};
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

/* ---- Drawing ---- */
static void FDLabel(const char *s, int x, int w, int y, const EmPal *p)
{
    UiText(s, x + (w - UiTextWidth(s, 1)) / 2, y, p->ink, p->inkSh, 1, x + w);
}

static void FDCounts(int x, int y, int w, const EmPal *p, unsigned seen, unsigned owned)
{
    char n[8];

    UiText(CtrUiText(UIS_DEX_SEEN), x + 10, y, p->ink, p->inkSh, 1, x + w);
    snprintf(n, sizeof(n), "%u", seen);
    UiTextRight(n, x + w - 10, y, EM_VALUE, p->inkSh, 1);
    UiText(CtrUiText(UIS_DEX_OWNED), x + 10, y + 13, p->ink, p->inkSh, 1, x + w);
    snprintf(n, sizeof(n), "%u", owned);
    UiTextRight(n, x + w - 10, y + 13, EM_VALUE, p->inkSh, 1);
}

/* A thin page arrow (up or down) in the ember well of the Bag's arrows. */
static void FDArrow(const CtrPokeTouchRect *r, int dir, int on)
{
    UiRoundFill(r->x, r->y, r->w, r->h, 5, UI_RGB(24, 8, 10));
    EmGradFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, 4, UI_RGB(72, 26, 26), UI_RGB(44, 14, 16));
    if (dir == -1 || dir == 1)
        UiArrow(r->x + (r->w - 5) / 2, r->y + r->h / 2 - 4, dir, on ? EM_GLOW : UI_RGB(110, 50, 44));
    else
        UiArrow(r->x + (r->w - 9) / 2, r->y + r->h / 2 - 3, dir, on ? EM_GLOW : UI_RGB(110, 50, 44));
}

static void FDMenu(const CtrPokeTouchDex *d, int pressed)
{
    int tab = FDTabShown(d), first, count, pages;

    for (int i = 0; i < 3; ++i)
        FBButton(&sFDTab[i], tab == i ? &kEmAmber : &kEmCream, CtrUiText(sFDTabLabel[i]), pressed == PT_DEX_HIT_TAB + i);
    if (tab == 0) {
        int n = FDSectionRows(d, 0, &first), cancel = FDCancelRow(d);

        for (int i = 0; i < n && i < 2; ++i) {
            CtrPokeTouchRect r = FDListCard(d, i, n);
            int row = first + i, sel = d->menuCursor == row, nat = d->menu[row].id == PT_DEX_ID_NATIONAL;
            const EmPal *p = sel ? &kEmAmber : &kEmCream;
            int fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, pressed == PT_DEX_HIT_ITEM + row);
            char label[24];

            FDIconDraw(d->menu[row].id, r.x + (r.w - 64) / 2, fy + 4, 1);
            FDCardLabel(d, row, label, sizeof(label));
            FDLabel(label, r.x, r.w, fy + 54, p);
            FDCounts(r.x, fy + 70, r.w, p, d->seen[nat], d->owned[nat]);
        }
        if (cancel >= 0)
            FBButton(&sFDClose, d->menuCursor == cancel ? &kEmAmber : &kEmViolet, d->menu[cancel].name,
                     pressed == PT_DEX_HIT_ITEM + cancel);
        return;
    }
    FDGridPage(d, tab, &first, &count, &pages);
    for (int i = 0; i < count; ++i) {
        CtrPokeTouchRect r = FDGridCard(i);
        int row = first + i, locked = d->menu[row].locked;
        const EmPal *p = d->menuCursor == row ? &kEmAmber : locked ? &kEmGrey : &kEmCream;
        int fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, pressed == PT_DEX_HIT_ITEM + row);
        char label[24];

        FDIconDraw(d->menu[row].id, r.x + (r.w - 64) / 2, fy + 1, !locked);
        FDCardLabel(d, row, label, sizeof(label));
        FDLabel(label, r.x, r.w, fy + 50, p);
    }
    if (pages > 1) {
        int start, at;

        FDSectionRows(d, tab, &start);
        at = (first - start) / FD_GRID_PAGE;
        FDArrow(&sFDGridUp, -2, at > 0);
        FDArrow(&sFDGridDown, 2, at < pages - 1);
    }
}

/* The selected Pokemon beside a list: its picture on a disc, name, number and types. */
static void FDSidePanel(const CtrPokeTouchDex *d)
{
    const CtrPokeTouchRect *r = &sFDSide;
    int fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, FB_DEPTH, 0), cx = r->x + r->w / 2;
    char no[12];

    UiDisc(cx, fy + 44, 41, UiMix(kEmCream.edge, EM_WHITE, 2));
    UiDisc(cx, fy + 44, 40, UI_RGB(255, 236, 214));
    if (d->sel.flags & 1)
        FDPicDrawBig(d->sel.species, cx - 32, fy + 12);
    FDLabel(d->sel.flags & 1 ? d->sel.name : "-----", r->x, r->w, fy + 88, &kEmCream);
    FDNumber(d->sel.number, no, sizeof(no));
    FDLabel(no, r->x, r->w, fy + 102, &kEmCream);
    if (d->type[0] >= 0) {
        CtrPokeTouchRect t0 = {(int16_t)(r->x + 12), (int16_t)(fy + 120), (int16_t)(r->w - 24), 15};

        if (d->type[1] >= 0) {
            t0.y = (int16_t)(fy + 118);
            UiTypeBadge(&t0, d->type[0], d->typeName[0]);
            t0.y = (int16_t)(fy + 136);
            UiTypeBadge(&t0, d->type[1], d->typeName[1]);
        } else
            UiTypeBadge(&t0, d->type[0], d->typeName[0]);
    }
}

static void FDList(const CtrPokeTouchDex *d, int pressed)
{
    int fy = EmButton(sFDListHead.x, sFDListHead.y, sFDListHead.w, sFDListHead.h, &kEmCream, FB_DEPTH, 0);
    static const CtrUiString kOrder[6] = {UIS_DEX_KANTO, UIS_DEX_ATOZ, UIS_DEX_TYPE, UIS_DEX_LIGHTEST,
                                          UIS_DEX_SMALLEST, UIS_DEX_NATIONAL};

    const char *title = CtrUiText(kOrder[d->order < 6 ? d->order : 0]);
    int titleEnd = sFDListHead.x + 8 + UiTextWidth(title, 1);

    UiText(title, sFDListHead.x + 8, fy + 6, EM_DARK, EM_DARK_SH, 1, sFDListHead.x + sFDListHead.w - 6);
    if (d->order == 0 || d->order == 5) {
        char counts[32], owned[8];
        int nat = d->order == 5, right = sFDListHead.x + sFDListHead.w - 6;

        /* SEEN n  OWNED n when it fits beside the title; with three-digit counts OWNED becomes the caught ball of
           the rows, so the counts never run into KANTO or NATIONAL. */
        snprintf(counts, sizeof(counts), "%s %u  %s %u", CtrUiText(UIS_DEX_SEEN), d->seen[nat], CtrUiText(UIS_DEX_OWNED),
                 d->owned[nat]);
        if (right - UiTextWidth(counts, 1) >= titleEnd + 6 || !FDCaughtLoad()) {
            UiTextRight(counts, right, fy + 6, EM_DIM, EM_DARK_SH, 1);
        } else {
            snprintf(owned, sizeof(owned), "%u", d->owned[nat]);
            snprintf(counts, sizeof(counts), "%s %u", CtrUiText(UIS_DEX_SEEN), d->seen[nat]);
            UiTextRight(owned, right, fy + 6, EM_DIM, EM_DARK_SH, 1);
            right -= UiTextWidth(owned, 1) + 10;
            FDCaughtDraw(right, fy + 7);
            UiTextRight(counts, right - 5, fy + 6, EM_DIM, EM_DARK_SH, 1);
        }
    }
    for (int i = 0; i < PT_DEX_ROWS; ++i) {
        CtrPokeTouchRect r = FDListRow(i);
        int index = d->listFirst + i;
        const CtrPokeTouchDexMon *m = &d->row[i];
        const EmPal *p;
        char no[8];
        int ry;

        if (index >= d->listCount)
            break;
        p = index == d->listCursor ? &kEmAmber : &kEmCream;
        ry = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, pressed == PT_DEX_HIT_ROW + i);
        if (m->flags & 2)
            FDCaughtDraw(r.x + 5, ry + 7);
        snprintf(no, sizeof(no), "%03u", m->number > 999 ? 999 : m->number);
        UiText(no, r.x + 18, ry + 7, p->ink, p->inkSh, 1, r.x + 44);
        UiText(m->flags & 1 ? m->name : "-----", r.x + 44, ry + 7, p->ink, p->inkSh, 1, r.x + r.w - 4);
    }
    if (d->listCount > PT_DEX_ROWS) {
        FDArrow(&sFDListUp, -2, d->listFirst > 0);
        FDArrow(&sFDListDown, 2, d->listFirst + PT_DEX_ROWS < d->listCount);
    }
    FDSidePanel(d);
}

static void FDCategory(const CtrPokeTouchDex *d, int pressed)
{
    for (int i = 0; i < d->slots && i < 4; ++i) {
        CtrPokeTouchRect r = FDSlot(i);
        const CtrPokeTouchDexMon *m = &d->mon[i];
        const EmPal *p = i == d->slot ? &kEmAmber : &kEmCream;
        int fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, pressed == PT_DEX_HIT_SLOT + i);
        char label[24];

        /* The picture whole (80x80) on a disc filling the card, its number and name on a pill over its foot. */
        UiDisc(r.x + r.w / 2, fy + 38, 37, i == d->slot ? UiMix(p->top, EM_WHITE, 2) : UI_RGB(255, 236, 214));
        if (m->flags & 1)
            FDPicDrawBig(m->species, r.x + (r.w - 64) / 2, fy + 6);
        snprintf(label, sizeof(label), "%03u %s", m->number > 999 ? 999 : m->number, m->flags & 1 ? m->name : "-----");
        BPill(r.x + (r.w - BPillW(label)) / 2, fy + 62, label, p->lip, EM_WHITE, p->edge);
        if (m->flags & 2)
            FDCaughtDraw(r.x + 6, fy + 6);
    }
    FDArrow(&sFDPrev, -1, d->canPrev);
    FDArrow(&sFDNext, 1, d->canNext);
}

static void FDPage(const CtrPokeTouchDex *d)
{
    int fy = EmButton(4, 4, 96, 110, &kEmCream, FB_DEPTH, 0);
    char no[12], text[32];

    UiDisc(52, fy + 46, 40, UiMix(kEmCream.edge, EM_WHITE, 2));
    UiDisc(52, fy + 46, 39, UI_RGB(255, 236, 214));
    FDPicDrawBig(d->sel.species, 20, fy + 14);
    FDNumber(d->sel.number, no, sizeof(no));
    BPill(52 - BPillW(no) / 2, fy + 90, no, kEmAmber.lip, EM_WHITE, kEmAmber.edge);
    fy = EmButton(104, 4, 140, 110, &kEmCream, FB_DEPTH, 0);
    UiText(d->sel.name, 114, fy + 8, EM_DARK, EM_DARK_SH, 1, 240);
    UiText(d->sel.flags & 2 ? d->kind : "?????", 114, fy + 22, EM_DIM, EM_DARK_SH, 1, 240);
    Fill(112, fy + 35, 124, 1, UI_RGB(214, 204, 192));
    UiText(CtrUiText(UIS_DEX_HT), 114, fy + 42, EM_DARK, EM_DARK_SH, 1, 140);
    FDHeight(d, text, sizeof(text));
    UiTextRight(text, 232, fy + 42, EM_VALUE, EM_DARK_SH, 1);
    UiText(CtrUiText(UIS_DEX_WT), 114, fy + 56, EM_DARK, EM_DARK_SH, 1, 140);
    FDWeight(d, text, sizeof(text));
    UiTextRight(text, 232, fy + 56, EM_VALUE, EM_DARK_SH, 1);
    Fill(112, fy + 69, 124, 1, UI_RGB(214, 204, 192));
    if (d->type[0] >= 0) {
        CtrPokeTouchRect t = {114, (int16_t)(fy + 78), 46, 15};

        UiTypeBadge(&t, d->type[0], d->typeName[0]);
        if (d->type[1] >= 0) {
            t.x = 164;
            UiTypeBadge(&t, d->type[1], d->typeName[1]);
        }
    }
    FDFootprint(d->sel.species, 218, fy + 77);
    fy = EmButton(4, 118, 240, 46, &kEmCream, FB_DEPTH, 0);
    if (d->desc[0])
        BDescribeN(d->desc, 12, fy + 5, 226, 3, EM_DARK);
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

/* The note under the Pokedex: what the selection is (a Pokemon's page has its AREA and CRY buttons there). */
static void FDexNote(const CtrPokeTouchView *v, int w)
{
    const CtrPokeTouchDex *d = &v->dex;
    char title[40], body[96];

    EmSurface();
    if (d->state == PT_DEX_STATE_PAGE && !d->added) {
        FBButton(&sFDArea, &kEmCream, CtrUiText(UIS_DEX_AREA), d->pressed == PT_DEX_HIT_AREA);
        FBButton(&sFDCry, &kEmCream, CtrUiText(UIS_DEX_CRY), d->pressed == PT_DEX_HIT_CRY);
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
    EmNote(NOTE_X, NOTE_Y, w, NOTE_H);
    UiText(title, NOTE_X + 12, NOTE_Y + 6, EM_DARK, EM_DARK_SH, 1, NOTE_X + w - 4);
    BDescribeN(body, NOTE_X + 12, NOTE_Y + 21, w - 18, 3, EM_DIM);
    sUiS = &sUiNavy;
}
