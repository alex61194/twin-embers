/* A PC box in the viewport (3ds_poketouch.h CtrPokeTouchBox), included at the end of 3ds_poketouch.c after the naming
 * screen, whose FireRed text it shares, and the Pokedex, whose picture helpers it shares. A remote for FireRed's own
 * box: its name between the arrows on an amber band, its thirty places as light cells with the Pokemon's icons, PARTY
 * and CLOSE BOX on the right with the Pokemon in hand under them, the Party over the cells while the cursor is in it,
 * and over them the box's menu, YES/NO, box chooser or marking card. FireRed's cursor is amber. The box's message and the Pokemon
 * under the cursor (FireRed's lines about it and its picture) are in the note. Every word is FireRed's (CtrBottomStorage_
 * BoxFill) but for the buttons' labels (3ds_ui_strings.h). MOVE ITEMS is the same box: an amber tag on every Pokemon
 * that holds an item, the item in hand on the plate under the buttons, and in the note the item under the cursor. */
#include "3ds_storage_touch.h"

#define EM_SLATE_ARROW UI_RGB(76, 90, 140)
#define FX_SLOT_TOP UI_RGB(62, 14, 14)              /* the mockup slot: the dark sunk place of the cells and the hand */
#define FX_SLOT_BOT UI_RGB(42, 8, 9)
#define FX_SLOT_EDGE UI_RGB(20, 3, 4)

static const CtrPokeTouchRect sFXPrev = {4, 4, 26, 24}, sFXTitle = {32, 4, 120, 24}, sFXNext = {154, 4, 26, 24};
static const CtrPokeTouchRect sFXParty = {184, 32, 60, 38}, sFXClose = {184, 74, 60, 38};
static const CtrPokeTouchRect sFXHand = {184, 116, 60, 46};
static const CtrPokeTouchRect sFXGrid = {4, 32, 176, 132};
static const CtrPokeTouchRect sFXSlot0 = {10, 38, 70, 74}, sFXCancel = {10, 118, 70, 40};
static const CtrPokeTouchRect sFXYes = {20, 96, 70, 30}, sFXNo = {94, 96, 70, 30};
static const CtrPokeTouchRect sFXAsk = {12, 56, 160, 82};
static const CtrPokeTouchRect sFXChoosePrev = {22, 86, 30, 32}, sFXChooseNext = {132, 86, 30, 32};
static const CtrPokeTouchRect sFXChooseName = {56, 86, 72, 32};
static const CtrPokeTouchRect sFXMarkCard = {12, 46, 160, 110};
static const CtrPokeTouchRect sFXMarkOk = {20, 118, 70, 30}, sFXMarkCancel = {94, 118, 70, 30};

/* The marking card's four marks, in a row under the Pokemon's icon. */
static CtrPokeTouchRect FXMark(int i)
{
    CtrPokeTouchRect r = {(int16_t)(18 + 38 * i), 80, 34, 30};
    return r;
}

static CtrPokeTouchRect FXCell(int pos)
{
    CtrPokeTouchRect r = {(int16_t)(4 + 29 * (pos % 6)), (int16_t)(32 + 26 * (pos / 6)), 28, 25};
    return r;
}

static CtrPokeTouchRect FXPartyRow(int slot)
{
    CtrPokeTouchRect r = {84, (int16_t)(36 + 25 * (slot - 1)), 92, 24};
    return r;
}

/* The menu's rows: 24 pixels apart, closer for six or seven (the card then takes the frame's whole height). */
static int FXMenuPitch(const CtrPokeTouchBox *b)
{
    return b->menuCount > 5 ? 21 : 24;
}

static CtrPokeTouchRect FXMenuCard(const CtrPokeTouchBox *b)
{
    int h = 8 + FXMenuPitch(b) * b->menuCount, top = b->menuCount > 5 ? 4 + (160 - h) / 2 : sFXGrid.y + (sFXGrid.h - h) / 2;
    CtrPokeTouchRect r = {30, (int16_t)top, 124, (int16_t)h};
    return r;
}

static CtrPokeTouchRect FXMenuRow(const CtrPokeTouchBox *b, int i)
{
    CtrPokeTouchRect c = FXMenuCard(b), r = {38, (int16_t)(c.y + 5 + FXMenuPitch(b) * i), 108, (int16_t)(FXMenuPitch(b) - 2)};
    return r;
}

int CtrPokeTouch_BoxHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchBox *b = &v->box;

    if (!b->on)
        return ST_HIT_NONE;
    switch (b->state) {
    case PT_BOX_STATE_MESSAGE:
        return y < NOTE_Y + NOTE_H && x < 248 ? PT_BOX_HIT_MESSAGE : ST_HIT_NONE;
    case PT_BOX_STATE_MENU:
        for (int i = 0; i < b->menuCount; ++i) {
            CtrPokeTouchRect r = FXMenuRow(b, i);
            if (UiIn(&r, x, y))
                return ST_HIT_MENU_BASE + i;
        }
        return ST_HIT_NONE;
    case PT_BOX_STATE_YESNO:
        return UiIn(&sFXYes, x, y) ? ST_HIT_MENU_BASE : UiIn(&sFXNo, x, y) ? ST_HIT_MENU_BASE + 1 : ST_HIT_NONE;
    case PT_BOX_STATE_CHOOSE:
        return UiIn(&sFXChoosePrev, x, y) ? ST_HIT_PREV : UiIn(&sFXChooseNext, x, y) ? ST_HIT_NEXT
             : UiIn(&sFXChooseName, x, y) ? ST_HIT_TITLE : ST_HIT_NONE;
    case PT_BOX_STATE_MARKS:
        for (int i = 0; i < 4; ++i) {
            CtrPokeTouchRect r = FXMark(i);
            if (UiIn(&r, x, y))
                return ST_HIT_MENU_BASE + i;
        }
        return UiIn(&sFXMarkOk, x, y) ? ST_HIT_MENU_BASE + 4 : UiIn(&sFXMarkCancel, x, y) ? ST_HIT_MENU_BASE + 5
             : ST_HIT_NONE;
    case PT_BOX_STATE_BOX:
        break;
    default:
        return ST_HIT_NONE;
    }
    if (b->area == ST_AREA_PARTY) {
        if (UiIn(&sFXSlot0, x, y))
            return ST_HIT_PARTY_BASE;
        for (int i = 1; i < 6; ++i) {
            CtrPokeTouchRect r = FXPartyRow(i);
            if (UiIn(&r, x, y))
                return ST_HIT_PARTY_BASE + i;
        }
        return UiIn(&sFXCancel, x, y) ? ST_HIT_PARTY_BASE + 6 : ST_HIT_NONE;
    }
    if (UiIn(&sFXPrev, x, y))
        return ST_HIT_PREV;
    if (UiIn(&sFXNext, x, y))
        return ST_HIT_NEXT;
    if (UiIn(&sFXTitle, x, y))
        return ST_HIT_TITLE;
    if (UiIn(&sFXParty, x, y))
        return ST_HIT_BUTTON_PARTY;
    if (UiIn(&sFXClose, x, y))
        return ST_HIT_BUTTON_CLOSE;
    for (int i = 0; i < PT_BOX_SLOTS; ++i) {
        CtrPokeTouchRect r = FXCell(i);
        if (UiIn(&r, x, y))
            return i;
    }
    return ST_HIT_NONE;
}

/* The box's wallpaper, FireRed's own picture of it (160x144) in the lower screen's colours; decoded once per wallpaper.
 * Its body (the frame's inside) sits behind the cells and its header strip behind the box's name, so choosing another
 * wallpaper changes the box's whole look, as it does in the game. */
static uint16_t sFXWall[160 * 144];
static int sFXWallId = -1, sFXWallOk;

static int FXWallpaper(unsigned id)
{
    static unsigned short raw[160 * 144];

    if ((int)id != sFXWallId) {
        sFXWallId = (int)id;
        sFXWallOk = CtrPokeTouch_Wallpaper(id, raw);
        for (int i = 0; sFXWallOk && i < 160 * 144; ++i) {
            unsigned c = raw[i];
            sFXWall[i] = GbaColor(c);
        }
    }
    return sFXWallOk;
}

/* The wallpaper's pixel behind lower-screen point (x, y) of the grid (sFXGrid: the picture's body, 160x120 from its row
 * 24, stretched to the grid) or of the title (its header strip, 128x24 from column 16, stretched to the title). */
static uint16_t FXWallBody(int x, int y)
{
    int sx = (x - sFXGrid.x) * 160 / sFXGrid.w, sy = 24 + (y - sFXGrid.y) * 120 / sFXGrid.h;

    return sFXWall[sy * 160 + sx];
}

static uint16_t FXWallHead(int x, int y)
{
    int sx = 16 + (x - sFXTitle.x) * 128 / sFXTitle.w, sy = (y - sFXTitle.y) * 24 / sFXTitle.h;

    return sFXWall[sy * 160 + sx];
}

/* A Pokemon's icon (FireRed's, its first frame), its centre at (cx, cy). */
static void FXIcon(const CtrPokeTouchBoxMon *m, int cx, int cy)
{
    static unsigned char tiles[1024];
    static unsigned short pal[16];
    static uint8_t px[32 * 32];
    static uint16_t pal565[16];

    if (!m->species || !CtrPokeTouch_MonIcon(m->species, m->personality, m->egg, tiles, pal))
        return;
    FDTiles(tiles, 32, 32, px);
    FDPal(pal, pal565);
    UiImage(px, pal565, 32, 32, 32, cx - 16, cy - 18, 1, 1);
}

/* MOVE ITEMS: the tag of a Pokemon that holds an item, centred on (cx, cy). */
static void FXItemTag(int cx, int cy)
{
    UiRoundFill(cx - 5, cy - 5, 11, 11, 3, LL_EDGE);
    EmGradFill(cx - 4, cy - 4, 9, 9, 2, kEmFlame.top, kEmFlame.bottom);
    UiRoundFill(cx - 1, cy - 2, 3, 5, 1, EM_WHITE);
}

static void FXText(const uint8_t *raw, int n, int x, int y, uint16_t ink, uint16_t sh, int right)
{
    char text[64];

    FNText(raw, n, text, (int)sizeof(text));
    UiText(text, x, y, ink, sh, 1, right);
}

static void FXButton(const CtrPokeTouchRect *r, const char *label, const EmPal *p, int pressed)
{
    int fy = EmButton(r->x, r->y, r->w, r->h, p, FB_DEPTH, pressed);

    EmLabel(label, r->x, r->w, fy, r->h - FB_DEPTH, p);
}

static void FXArrowButton(const CtrPokeTouchRect *r, int dir, int selected, int pressed)
{
    const EmPal *p = LlPick(selected, &kEmCream);
    int fy;

    if (selected)
        LlSelectRing(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, p, FB_DEPTH, pressed);
    UiArrow(r->x + r->w / 2 - 3, fy + (r->h - FB_DEPTH) / 2 - 4, dir, selected ? EM_WHITE : EM_SLATE_ARROW);
}

static void FXTitle(const CtrPokeTouchBox *b)
{
    int onTitle = b->state == PT_BOX_STATE_BOX && b->area == ST_AREA_TITLE;
    const CtrPokeTouchRect *t = &sFXTitle;
    CtrPokeTouchRect line = {t->x, 0, t->w, 13};
    char name[32];
    int fy;

    FXArrowButton(&sFXPrev, -1, 0, b->pressed == ST_HIT_PREV);
    FXArrowButton(&sFXNext, 1, 0, b->pressed == ST_HIT_NEXT);
    if (onTitle)
        LlSelectRing(t->x, t->y, t->w, t->h);
    fy = EmButton(t->x, t->y, t->w, t->h, LlPick(onTitle, &kEmCream), FB_DEPTH, b->pressed == ST_HIT_TITLE);
    FNText(b->boxName, (int)sizeof(b->boxName), name, (int)sizeof(name));
    if (FXWallpaper(b->wallpaper)) {
        /* The wallpaper's own header strip is the name's plate, its text FireRed's white on shadow. */
        for (int y = fy + 2; y < fy + t->h - FB_DEPTH - 2; ++y)
            for (int x = t->x + 3; x < t->x + t->w - 3; ++x)
                Px(x, y, FXWallHead(x, y));
        UiTextCentered(name, &line, fy + 6, EM_WHITE, UI_RGB(56, 56, 56), 1);
        return;
    }
    UiTextCentered(name, &line, fy + 6, onTitle ? EM_WHITE : EM_DARK, onTitle ? kEmFlame.inkSh : EM_DARK_SH, 1);
}

static void FXGrid(const CtrPokeTouchBox *b)
{
    int wall = FXWallpaper(b->wallpaper);

    if (wall) {
        /* The wallpaper's picture behind the cells, in a frame of its own border colour. */
        UiRoundFill(sFXGrid.x - 1, sFXGrid.y - 1, sFXGrid.w + 2, sFXGrid.h + 2, 4, sFXWall[70 * 160 + 4]);
        for (int y = sFXGrid.y; y < sFXGrid.y + sFXGrid.h; ++y)
            for (int x = sFXGrid.x; x < sFXGrid.x + sFXGrid.w; ++x)
                Px(x, y, FXWallBody(x, y));
    }
    else {
        /* No wallpaper: the mockup's dark sunk place, its cells a lighter shade (an empty one darker). */
        UiRoundFill(sFXGrid.x, sFXGrid.y, sFXGrid.w, sFXGrid.h, 5, FX_SLOT_EDGE);
        EmGradFill(sFXGrid.x + 1, sFXGrid.y + 1, sFXGrid.w - 2, sFXGrid.h - 2, 4, FX_SLOT_TOP, FX_SLOT_BOT);
        for (int i = 0; i < PT_BOX_SLOTS; ++i) {
            CtrPokeTouchRect r = FXCell(i);

            UiRoundFill(r.x, r.y, r.w, r.h, 4, b->slot[i].species ? UI_RGB(104, 52, 48) : UI_RGB(52, 14, 14));
        }
    }
    for (int i = 0; i < PT_BOX_SLOTS; ++i) {
        CtrPokeTouchRect r = FXCell(i);
        FXIcon(&b->slot[i], r.x + r.w / 2, r.y + r.h / 2 + 2);
        if (b->items && b->slot[i].species && b->slotItem[i])
            FXItemTag(r.x + r.w - 6, r.y + r.h - 6);
    }
    for (int i = 0; i < PT_BOX_SLOTS; ++i) {
        CtrPokeTouchRect r = FXCell(i);

        if (b->area == ST_AREA_BOX && b->pos == i && b->state != PT_BOX_STATE_BUSY)
            LlSelectRing(r.x, r.y, r.w, r.h);
    }
}

static void FXSide(const CtrPokeTouchBox *b)
{
    int buttons = b->state == PT_BOX_STATE_BOX && b->area == ST_AREA_BUTTONS;
    const CtrPokeTouchRect *h = &sFXHand;

    FBLlButton(&sFXParty, LlPick(buttons && b->pos == 0, &kEmGold), CtrUiText(UIS_BOX_PARTY), b->pressed == ST_HIT_BUTTON_PARTY,
               buttons && b->pos == 0);
    FBLlButton(&sFXClose, LlPick(buttons && b->pos == 1, &kEmSlate), CtrUiText(UIS_BOX_CLOSE), b->pressed == ST_HIT_BUTTON_CLOSE,
               buttons && b->pos == 1);
    /* The Pokemon in hand, on a sunk plate; MOVE ITEMS: the item in hand. */
    UiRoundFill(h->x, h->y, h->w, h->h, 5, FX_SLOT_EDGE);
    EmGradFill(h->x + 1, h->y + 1, h->w - 2, h->h - 2, 4, FX_SLOT_TOP, FX_SLOT_BOT);
    if (b->items && b->handItem) {
        CtrPokeTouchRect line = {h->x, 0, h->w, 13};

        ItemArt(ICON_BOX_HAND, b->handItem, h->x + h->w / 2 - 10, h->y + 8, 1);
        UiTextCentered(CtrUiText(UIS_BOX_IN_HAND), &line, h->y + h->h - 15, UI_RGB(240, 200, 170), UI_RGB(30, 8, 8), 1);
    } else if (b->holding)
        FXIcon(&b->held, h->x + h->w / 2, h->y + h->h / 2 + 2);
}

static void FXParty(const CtrPokeTouchBox *b)
{
    CtrPokeTouchRect dim = sFXGrid;

    UiDim(&dim);
    LlWindowPlain(sFXGrid.x, sFXGrid.y, sFXGrid.w, sFXGrid.h);
    for (int i = 0; i < 6; ++i) {
        CtrPokeTouchRect r = i == 0 ? sFXSlot0 : FXPartyRow(i);
        int cursor = b->pos == i;
        const EmPal *p = LlPick(cursor, &kEmCream);
        int fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, b->pressed == ST_HIT_PARTY_BASE + i);
        char level[8];

        if (!b->party[i].species)
            continue;
        if (i == 0) {
            FXIcon(&b->party[0], r.x + r.w / 2, fy + 24);
            if (b->items && b->partyItem[0])
                FXItemTag(r.x + r.w - 10, fy + 32);
            FXText(b->partyName[0], (int)sizeof(b->partyName[0]), r.x + 6, fy + 44, p->ink, p->inkSh, r.x + r.w - 4);
            if (!b->party[0].egg) {
                snprintf(level, sizeof(level), "Lv%u", (unsigned)b->partyLevel[0]);
                UiText(level, r.x + 6, fy + 57, p->ink, p->inkSh, 1, r.x + r.w - 4);
            }
            continue;
        }
        FXIcon(&b->party[i], r.x + 14, fy + 11);
        if (b->items && b->partyItem[i])
            FXItemTag(r.x + 24, fy + 15);
        FXText(b->partyName[i], (int)sizeof(b->partyName[i]), r.x + 30, fy + 6, p->ink, p->inkSh, r.x + r.w - 4);
    }
    FXButton(&sFXCancel, CtrUiText(UIS_CANCEL), LlPick(b->pos == 6, &kEmSlate), b->pressed == ST_HIT_PARTY_BASE + 6);
    /* The chosen piece's ring, after the others so that no neighbour covers it. */
    if (b->pos >= 0 && b->pos <= 6) {
        CtrPokeTouchRect r = b->pos == 0 ? sFXSlot0 : b->pos == 6 ? sFXCancel : FXPartyRow(b->pos);

        LlSelectRing(r.x, r.y, r.w, r.h);
    }
}

static void FXMenu(const CtrPokeTouchBox *b)
{
    CtrPokeTouchRect card = FXMenuCard(b), dim = {0, 0, 182, 168};
    char text[40];

    UiDim(&dim);
    LlWindowPlain(card.x, card.y, card.w, card.h);
    for (int i = 0; i < b->menuCount; ++i) {
        CtrPokeTouchRect r = FXMenuRow(b, i);
        const EmPal *p = LlPick(b->menuCursor == i, i == b->menuCount - 1 ? &kEmSlate : &kEmCream);

        FNText(b->menu[i], (int)sizeof(b->menu[i]), text, (int)sizeof(text));
        FXButton(&r, text, p, b->pressed == ST_HIT_MENU_BASE + i);
    }
    if (b->menuCursor >= 0 && b->menuCursor < b->menuCount) {
        CtrPokeTouchRect r = FXMenuRow(b, b->menuCursor);

        LlSelectRing(r.x, r.y, r.w, r.h);
    }
}

static void FXYesNo(const CtrPokeTouchBox *b)
{
    CtrPokeTouchRect dim = sFXGrid;

    UiDim(&dim);
    LlWindowPlain(sFXAsk.x, sFXAsk.y, sFXAsk.w, sFXAsk.h);
    FXIcon(&b->mon, sFXAsk.x + sFXAsk.w / 2, sFXAsk.y + 22);
    FBLlButton(&sFXYes, LlPick(b->menuCursor == 0, &kEmCream), CtrUiText(UIS_YES), b->pressed == ST_HIT_MENU_BASE, b->menuCursor == 0);
    FBLlButton(&sFXNo, LlPick(b->menuCursor == 1, &kEmSlate), CtrUiText(UIS_NO), b->pressed == ST_HIT_MENU_BASE + 1, b->menuCursor == 1);
}

static void FXChoose(const CtrPokeTouchBox *b)
{
    CtrPokeTouchRect dim = sFXGrid, line = {sFXChooseName.x, 0, sFXChooseName.w, 13};
    char name[32], count[12];
    int fy;

    UiDim(&dim);
    LlWindowPlain(sFXAsk.x, sFXAsk.y, sFXAsk.w, sFXAsk.h);
    FXArrowButton(&sFXChoosePrev, -1, 0, b->pressed == ST_HIT_PREV);
    FXArrowButton(&sFXChooseNext, 1, 0, b->pressed == ST_HIT_NEXT);
    LlSelectRing(sFXChooseName.x, sFXChooseName.y, sFXChooseName.w, sFXChooseName.h);
    fy = EmButton(sFXChooseName.x, sFXChooseName.y, sFXChooseName.w, sFXChooseName.h, &kEmFlame, FB_DEPTH,
                  b->pressed == ST_HIT_TITLE);
    FNText(b->chooseName, (int)sizeof(b->chooseName), name, (int)sizeof(name));
    UiTextCentered(name, &line, fy + 4, kEmFlame.ink, kEmFlame.inkSh, 1);
    snprintf(count, sizeof(count), "%u/%u", (unsigned)b->chooseCount, (unsigned)PT_BOX_SLOTS);
    UiTextCentered(count, &line, fy + 16, kEmFlame.ink, kEmFlame.inkSh, 1);
}

/* FireRed's own mark (its 8x8 shape, twice the size) centred at (cx, cy): white while on, slate while off. */
static void FXMarkShape(int mark, int on, int cx, int cy)
{
    unsigned char px[64];
    uint16_t ink = on ? EM_WHITE : UI_RGB(112, 116, 136), edge = on ? UI_RGB(255, 226, 170) : UI_RGB(170, 168, 178);

    if (!CtrPokeTouch_MarkShape(mark, px))
        return;
    for (int i = 0; i < 64; ++i)
        if (px[i] == 3 || px[i] == 4)
            Fill(cx - 8 + 2 * (i % 8), cy - 8 + 2 * (i / 8), 2, 2, px[i] == 3 ? ink : edge);
}

/* The marking card: the Pokemon's icon, its four marks as switches (amber while on) and OK and CANCEL. FireRed's cursor
 * glows round its piece. */
static void FXMarks(const CtrPokeTouchBox *b)
{
    CtrPokeTouchRect dim = sFXGrid;

    UiDim(&dim);
    LlWindowPlain(sFXMarkCard.x, sFXMarkCard.y, sFXMarkCard.w, sFXMarkCard.h);
    FXIcon(&b->mon, sFXMarkCard.x + sFXMarkCard.w / 2, sFXMarkCard.y + 18);
    for (int i = 0; i < 4; ++i) {
        CtrPokeTouchRect r = FXMark(i);
        int on = (b->marks >> i) & 1;
        int fy = EmButton(r.x, r.y, r.w, r.h, LlPick(on, &kEmCream), FB_DEPTH, b->pressed == ST_HIT_MENU_BASE + i);

        FXMarkShape(i, on, r.x + r.w / 2, fy + (r.h - FB_DEPTH) / 2);
    }
    FXButton(&sFXMarkOk, CtrUiText(UIS_OK), &kEmFlame, b->pressed == ST_HIT_MENU_BASE + 4);
    FXButton(&sFXMarkCancel, CtrUiText(UIS_CANCEL), &kEmSlate, b->pressed == ST_HIT_MENU_BASE + 5);
    /* FireRed's cursor: the select ring, after every piece so that none covers it. */
    if (b->menuCursor >= 0 && b->menuCursor < 6) {
        CtrPokeTouchRect r = b->menuCursor < 4 ? FXMark(b->menuCursor) : b->menuCursor == 4 ? sFXMarkOk : sFXMarkCancel;

        LlSelectRing(r.x, r.y, r.w, r.h);
    }
}

static void FBoxFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchBox *b = &v->box;

    EmSurface();
    TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    FXTitle(b);
    FXGrid(b);
    FXSide(b);
    if (b->area == ST_AREA_PARTY && b->state != PT_BOX_STATE_MENU && b->state != PT_BOX_STATE_YESNO
        && b->state != PT_BOX_STATE_MARKS)
        FXParty(b);
    if (b->state == PT_BOX_STATE_MENU)
        FXMenu(b);
    else if (b->state == PT_BOX_STATE_YESNO)
        FXYesNo(b);
    else if (b->state == PT_BOX_STATE_CHOOSE)
        FXChoose(b);
    else if (b->state == PT_BOX_STATE_MARKS)
        FXMarks(b);
    sUiS = &sUiNavy;
}

/* The note's gender and level line ("<gender> Lv5"): FireRed colours the sign, so it is blue or red here too. */
static void FXGenderLevel(const uint8_t *raw, int n, int x, int y, int right)
{
    char text[32], sign[3];
    const char *rest = text;
    int g = 0;

    FNText(raw, n, text, (int)sizeof(text));
    if (text[0] == 0x01 && (text[1] == (char)0xb5 || text[1] == (char)0xb6)) {
        g = text[1] == (char)0xb6 ? 2 : 1;
        rest += 2;
    }
    while (*rest == ' ')
        ++rest;
    if (g) {
        FNChar(g == 2 ? 0xb6 : 0xb5, sign);
        UiText(sign, x, y, g == 2 ? UI_RGB(225, 70, 80) : UI_RGB(60, 120, 230), EM_DARK_SH, 1, right);
        x += UiTextWidth(sign, 1) + 4;
    }
    UiText(rest, x, y, EM_DIM, EM_DARK_SH, 1, right);
}

/* The note: the box's message while it shows one, else the Pokemon under the cursor and its picture. */
static void FBoxNote(const CtrPokeTouchView *v, int w)
{
    const CtrPokeTouchBox *b = &v->box;
    char text[100];

    EmSurface();
    LlWindow(NOTE_X, NOTE_Y, w, NOTE_H);
    if (b->message[0] != 0xff) {
        FNText(b->message, (int)sizeof(b->message), text, (int)sizeof(text));
        BPWrap(text, NOTE_X + 12, NOTE_Y + 8, w - 20, b->state == PT_BOX_STATE_MESSAGE ? 2 : 3, EM_DARK, EM_DARK_SH);
        if (b->state == PT_BOX_STATE_MESSAGE) {
            const char *tap = CtrUiText(UIS_TAP_TO_CONTINUE);

            UiText(tap, NOTE_X + w - 14 - UiTextWidth(tap, 1) - 9, NOTE_Y + NOTE_H - 16, EM_DIM, EM_DARK_SH, 1, NOTE_X + w);
            UiArrow(NOTE_X + w - 18, NOTE_Y + NOTE_H - 16, 1, LL_CURSOR);
        }
    } else if (b->items && b->mon.species) {
        /* MOVE ITEMS: the item the Pokemon holds, its name by its icon, the Pokemon's on the right, its description. */
        char name[32], desc[200];   /* two bytes a FireRed character */

        FNText(b->monText[0], (int)sizeof(b->monText[0]), name, (int)sizeof(name));
        if (b->monItem) {
            int nameX = NOTE_X + w - 10 - UiTextWidth(name, 1);

            /* Three lines of description: FireRed's are up to three (the note beside BACK is narrow). */
            ItemArt(ICON_BOX_NOTE, b->monItem, NOTE_X + 10, NOTE_Y + 1, 1);
            FXText(b->itemName, (int)sizeof(b->itemName), NOTE_X + 36, NOTE_Y + 5, EM_DARK, EM_DARK_SH, nameX - 6);
            UiText(name, nameX, NOTE_Y + 5, EM_DIM, EM_DARK_SH, 1, NOTE_X + w);
            FNText(b->itemDesc, (int)sizeof(b->itemDesc), desc, (int)sizeof(desc));
            BPWrap(desc, NOTE_X + 12, NOTE_Y + 21, w - 22, 3, EM_DIM, EM_DARK_SH);
        } else {
            UiText(name, NOTE_X + 12, NOTE_Y + 8, EM_DARK, EM_DARK_SH, 1, NOTE_X + w - 8);
            UiText(CtrUiText(UIS_BOX_NO_ITEM), NOTE_X + 12, NOTE_Y + 28, EM_DIM, EM_DARK_SH, 1, NOTE_X + w - 8);
        }
    } else if (b->mon.species) {
        int right = NOTE_X + w - (b->mon.egg ? 8 : 72);

        FXText(b->monText[0], (int)sizeof(b->monText[0]), NOTE_X + 12, NOTE_Y + 6, EM_DARK, EM_DARK_SH, right);
        FXGenderLevel(b->monText[2], (int)sizeof(b->monText[2]), NOTE_X + 12, NOTE_Y + 21, right);
        FXText(b->monText[1], (int)sizeof(b->monText[1]), NOTE_X + 12, NOTE_Y + 36, EM_DIM, EM_DARK_SH, right);
        FXText(b->monText[3], (int)sizeof(b->monText[3]), NOTE_X + 12, NOTE_Y + 49, EM_VALUE, EM_DARK_SH, right);
        if (!b->mon.egg)
            FDPicDraw(b->mon.species, NOTE_X + w - 68, NOTE_Y, 0);
    } else {
        FXText(b->boxName, (int)sizeof(b->boxName), NOTE_X + 12, NOTE_Y + (NOTE_H - UI_TEXT_H) / 2, EM_DIM, EM_DARK_SH,
               NOTE_X + w - 4);
    }
    sUiS = &sUiNavy;
}
