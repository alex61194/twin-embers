/* The field Bag in the viewport (3ds_poketouch.h CtrPokeTouchBag), included at the end of 3ds_poketouch.c after the
 * battle screen, whose ember pieces, item icons and text wrapping it shares. A remote for FireRed's own Bag: the pockets
 * as tabs, the list's rows as raised buttons (the Bag's selection amber, CLOSE BAG violet), the item's description in
 * the note, and whatever the Bag has open over the list: its menu, the quantity box, its question or its message.
 * Every row, cursor and count is the Bag's own (3ds_bottom.c walks them with its keys). The TM Case and the Berry Pouch
 * (bag.kind) are the same list under their title, with FireRed's CLOSE row; a TM/HM shows its move's type. */
#include "3ds_ui_strings.h"

static const CtrPokeTouchRect sFBTab[3] = {{4, 4, 78, 24}, {85, 4, 78, 24}, {166, 4, 78, 24}};
static const CtrUiString sFBTabLabel[3] = {UIS_BAG_ITEMS, UIS_BAG_KEY_ITEMS, UIS_BAG_BALLS};
static const CtrPokeTouchRect sFBUp = {222, 34, 22, 63}, sFBDown = {222, 101, 22, 62};
static const CtrPokeTouchRect sFBArea = {PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H};
static const CtrPokeTouchRect sFBTitle = {4, 4, 240, 24};
#define FB_DEPTH 2

static CtrPokeTouchRect FBRow(int i)
{
    CtrPokeTouchRect r = {4, (int16_t)(34 + 26 * i), 214, 25};
    return r;
}

/* The cards over the list: the menu (its rows under the item), the question, the quantity box, the message. */
static CtrPokeTouchRect FBMenuCard(const CtrPokeTouchBag *b)
{
    int n = b->menuCount > PT_BAG_MENU_MAX ? PT_BAG_MENU_MAX : b->menuCount, h = 34 + 26 * n;
    CtrPokeTouchRect r = {46, (int16_t)(PT_VIEW_Y + (PT_VIEW_H - h) / 2), 156, (int16_t)h};
    return r;
}

static CtrPokeTouchRect FBMenuRow(const CtrPokeTouchBag *b, int i)
{
    CtrPokeTouchRect c = FBMenuCard(b), r = {(int16_t)(c.x + 8), (int16_t)(c.y + 30 + 26 * i), (int16_t)(c.w - 16), 24};
    return r;
}

static const CtrPokeTouchRect sFBAsk = {14, 30, 220, 108};
static const CtrPokeTouchRect sFBYes = {30, 100, 90, 30}, sFBNo = {128, 100, 90, 30};
static const CtrPokeTouchRect sFBQtyDown = {30, 64, 40, 32}, sFBQtyUp = {178, 64, 40, 32};
static const CtrPokeTouchRect sFBMessage = {14, 40, 220, 88};

int CtrPokeTouch_BagHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchBag *b = &v->bag;

    if (!b->on || x >= 248)
        return -1;
    switch (b->state) {
    case PT_BAG_STATE_MESSAGE:
        return y < PT_VIEW_Y + PT_VIEW_H + 4 ? PT_BAG_HIT_MESSAGE : -1;   /* anywhere above the note: it waits for A */
    case PT_BAG_STATE_ACTIONS:
        for (int i = 0; i < b->menuCount && i < PT_BAG_MENU_MAX; ++i) {
            CtrPokeTouchRect r = FBMenuRow(b, i);
            if (UiIn(&r, x, y))
                return PT_BAG_HIT_MENU + i;
        }
        {
            CtrPokeTouchRect c = FBMenuCard(b);
            return UiIn(&c, x, y) || y >= PT_VIEW_Y + PT_VIEW_H ? -1 : PT_BAG_HIT_QTY_CANCEL;   /* off the card: B */
        }
    case PT_BAG_STATE_YESNO:
        return UiIn(&sFBYes, x, y) ? PT_BAG_HIT_MENU : UiIn(&sFBNo, x, y) ? PT_BAG_HIT_MENU + 1 : -1;
    case PT_BAG_STATE_QUANTITY:
        return UiIn(&sFBQtyUp, x, y) ? PT_BAG_HIT_QTY_UP : UiIn(&sFBQtyDown, x, y) ? PT_BAG_HIT_QTY_DOWN
             : UiIn(&sFBYes, x, y) ? PT_BAG_HIT_QTY_OK : UiIn(&sFBNo, x, y) ? PT_BAG_HIT_QTY_CANCEL : -1;
    case PT_BAG_STATE_LIST:
        for (int i = 0; i < 3 && b->kind == PT_BAG_KIND_BAG; ++i)
            if (UiIn(&sFBTab[i], x, y))
                return PT_BAG_HIT_TAB + i;
        if (b->count + 1 > PT_BAG_ROWS) {
            if (UiIn(&sFBUp, x, y))
                return b->first > 0 ? PT_BAG_HIT_UP : -1;
            if (UiIn(&sFBDown, x, y))
                return b->first + PT_BAG_ROWS <= b->count ? PT_BAG_HIT_DOWN : -1;
        }
        for (int i = 0; i < PT_BAG_ROWS; ++i) {
            CtrPokeTouchRect r = FBRow(i);
            if (b->first + i <= b->count && UiIn(&r, x, y))
                return PT_BAG_HIT_ROW + i;
        }
        return -1;
    }
    return -1;
}

/* A light card over the dimmed list. */
static void FBCard(const CtrPokeTouchRect *r)
{
    UiRoundFill(r->x, r->y, r->w, r->h, 5, kEmCream.edge);
    EmGradFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, 4, kEmCream.top, kEmCream.bottom);
}

static void FBButton(const CtrPokeTouchRect *r, const EmPal *p, const char *label, int pressed)
{
    int fy = EmButton(r->x, r->y, r->w, r->h, p, FB_DEPTH, pressed);

    EmLabel(label, r->x, r->w, fy, r->h - FB_DEPTH, p);
}

static void FBTabs(const CtrPokeTouchBag *b)
{
    if (b->kind != PT_BAG_KIND_BAG) {                   /* a case: its icon and name on a plain card */
        int w = 28 + UiTextWidth(b->title, 1), x = sFBTitle.x + (sFBTitle.w - w) / 2;

        FBCard(&sFBTitle);
        if (b->kind == PT_BAG_KIND_SHOP) {              /* a mart: its BUY, and the player's money on the right */
            char money[16];

            snprintf(money, sizeof(money), "\xc2\xa5%u", (unsigned)b->money);
            UiText(b->title, sFBTitle.x + 12, sFBTitle.y + 6, EM_DARK, EM_DARK_SH, 1, sFBTitle.x + 120);
            UiTextRight(money, sFBTitle.x + sFBTitle.w - 12, sFBTitle.y + 6, EM_VALUE, EM_DARK_SH, 1);
            return;
        }
        if (b->titleItem)
            BItemDraw(b->titleItem, x, sFBTitle.y - 2, 1);
        UiText(b->title, x + 28, sFBTitle.y + 6, EM_DARK, EM_DARK_SH, 1, sFBTitle.x + sFBTitle.w - 4);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        const CtrPokeTouchRect *r = &sFBTab[i];

        FBButton(r, b->pocket == i ? &kEmAmber : &kEmCream, CtrUiText(sFBTabLabel[i]), b->pressed == PT_BAG_HIT_TAB + i);
    }
}

/* A row: an item (icon, name, quantity but for key items) or CLOSE BAG; the Bag's selection amber. */
static void FBRowDraw(const CtrPokeTouchBag *b, int i)
{
    CtrPokeTouchRect r = FBRow(i);
    int index = b->first + i, selected = index == b->cursor, press = b->pressed == PT_BAG_HIT_ROW + i, fy;
    const EmPal *p;

    if (index > b->count)
        return;
    if (index == b->count) {
        FBButton(&r, selected ? &kEmAmber : &kEmViolet, b->kind != PT_BAG_KIND_BAG ? b->closeName : CtrUiText(UIS_BAG_CLOSE),
                 press);
        return;
    }
    p = selected ? &kEmAmber : &kEmCream;
    fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, press);
    BItemDraw(b->row[i].item, r.x + 3, fy - 1, 1);
    if (b->kind == PT_BAG_KIND_SHOP) {                  /* a mart's row: its price */
        char price[12];

        snprintf(price, sizeof(price), "\xc2\xa5%u", (unsigned)b->row[i].price);
        UiText(b->row[i].name, r.x + 30, fy + 7, p->ink, p->inkSh, 1, r.x + r.w - 12 - UiTextWidth(price, 1));
        UiTextRight(price, r.x + r.w - 8, fy + 7, p->ink, p->inkSh, 1);
        return;
    }
    UiText(b->row[i].name, r.x + 30, fy + 7, p->ink, p->inkSh, 1, r.x + r.w - 40);
    if (b->kind == PT_BAG_KIND_BAG && b->pocket == 1 && b->row[i].item
        && (b->row[i].item == b->regX || b->row[i].item == b->regY)) {
        UiTextRight(b->row[i].item == b->regX ? "X" : "Y", r.x + r.w - 8, fy + 7, p->ink, p->inkSh, 1);   /* its button */
    } else if (b->kind != PT_BAG_KIND_BAG ? b->row[i].qty != 0 : b->pocket != 1) {   /* key items and HMs: no count */
        char qty[8];

        snprintf(qty, sizeof(qty), "x%u", b->row[i].qty > 999 ? 999 : b->row[i].qty);
        UiTextRight(qty, r.x + r.w - 8, fy + 7, p->ink, p->inkSh, 1);
    }
}

static void FBList(const CtrPokeTouchBag *b)
{
    FBTabs(b);
    for (int i = 0; i < PT_BAG_ROWS; ++i)
        FBRowDraw(b, i);
    if (b->count + 1 > PT_BAG_ROWS) {
        BBagArrow(&sFBUp, -2, b->first > 0);
        BBagArrow(&sFBDown, 2, b->first + PT_BAG_ROWS <= b->count);
    }
}

static void FBMenu(const CtrPokeTouchBag *b)
{
    CtrPokeTouchRect c = FBMenuCard(b);
    int n = b->menuCount > PT_BAG_MENU_MAX ? PT_BAG_MENU_MAX : b->menuCount;

    FBCard(&c);
    if (b->selItem) {
        BItemDraw(b->selItem, c.x + 6, c.y + 3, 1);
        UiText(b->selName, c.x + 34, c.y + 10, EM_DARK, EM_DARK_SH, 1, c.x + c.w - 6);
    }
    for (int i = 0; i < n; ++i) {
        CtrPokeTouchRect r = FBMenuRow(b, i);
        const EmPal *p = b->menuCursor == i ? &kEmAmber : i == n - 1 ? &kEmViolet : &kEmCream;

        FBButton(&r, p, b->menuName[i], b->pressed == PT_BAG_HIT_MENU + i);
    }
}

static void FBYesNo(const CtrPokeTouchBag *b)
{
    FBCard(&sFBAsk);
    BPWrap(b->prompt, sFBAsk.x + 14, sFBAsk.y + 12, sFBAsk.w - 24, 2, EM_DARK, EM_DARK_SH);
    if (b->selItem) {                                   /* what the question is about: its icon, name and count */
        char what[32];
        int w, x;

        snprintf(what, sizeof(what), "%s x%u", b->selName, b->tossCount);
        w = 28 + UiTextWidth(what, 1);
        x = sFBAsk.x + (sFBAsk.w - w) / 2;
        BItemDraw(b->selItem, x, sFBAsk.y + 38, 1);
        UiText(what, x + 28, sFBAsk.y + 46, EM_DIM, EM_DARK_SH, 1, sFBAsk.x + sFBAsk.w - 8);
    }
    FBButton(&sFBYes, b->menuCursor == 0 ? &kEmAmber : &kEmCream, CtrUiText(UIS_YES), b->pressed == PT_BAG_HIT_MENU);
    FBButton(&sFBNo, b->menuCursor == 1 ? &kEmAmber : &kEmViolet, CtrUiText(UIS_NO), b->pressed == PT_BAG_HIT_MENU + 1);
}

/* The quantity box: how many to toss, FireRed's UP and DOWN on either side, OK and CANCEL under it. */
static void FBQuantity(const CtrPokeTouchBag *b)
{
    char count[12];
    CtrPokeTouchRect name = {sFBAsk.x, 0, sFBAsk.w, 13};
    int fy;

    FBCard(&sFBAsk);
    UiTextCentered(CtrUiText(b->kind == PT_BAG_KIND_SHOP ? UIS_BAG_BUY_HOW_MANY : b->use == PT_BAG_USE_SELL
                             ? UIS_BAG_SELL_HOW_MANY : b->use == PT_BAG_USE_DEPOSIT ? UIS_BAG_DEPOSIT_HOW_MANY
                             : UIS_BAG_TOSS_HOW_MANY), &name, sFBAsk.y + 8, EM_DARK, EM_DARK_SH, 1);
    if (b->use == PT_BAG_USE_SELL || b->kind == PT_BAG_KIND_SHOP) {   /* the item, and what that many come to */
        char line[40];

        snprintf(line, sizeof(line), "%s  \xc2\xa5%u", b->selName, (unsigned)b->sale);
        UiTextCentered(line, &name, sFBAsk.y + 22, EM_VALUE, EM_DARK_SH, 1);
    } else
        UiTextCentered(b->selName, &name, sFBAsk.y + 22, EM_DIM, EM_DARK_SH, 1);
    snprintf(count, sizeof(count), "x%u", b->tossCount);
    UiText(count, 124 - UiTextWidth(count, 2) / 2, sFBQtyDown.y + 3, EM_DARK, EM_DARK_SH, 2, 170);
    fy = EmButton(sFBQtyDown.x, sFBQtyDown.y, sFBQtyDown.w, sFBQtyDown.h, &kEmCream, FB_DEPTH, b->pressed == PT_BAG_HIT_QTY_DOWN);
    UiArrow(sFBQtyDown.x + sFBQtyDown.w / 2 - 4, fy + (sFBQtyDown.h - FB_DEPTH) / 2 - 2, 2, EM_STRIPE);
    fy = EmButton(sFBQtyUp.x, sFBQtyUp.y, sFBQtyUp.w, sFBQtyUp.h, &kEmCream, FB_DEPTH, b->pressed == PT_BAG_HIT_QTY_UP);
    UiArrow(sFBQtyUp.x + sFBQtyUp.w / 2 - 4, fy + (sFBQtyUp.h - FB_DEPTH) / 2 - 2, -2, EM_STRIPE);
    FBButton(&sFBYes, &kEmAmber, CtrUiText(UIS_OK), b->pressed == PT_BAG_HIT_QTY_OK);
    FBButton(&sFBNo, &kEmViolet, CtrUiText(UIS_CANCEL), b->pressed == PT_BAG_HIT_QTY_CANCEL);
}

static void FBMessage(const CtrPokeTouchBag *b)
{
    const CtrPokeTouchRect *m = &sFBMessage;
    const char *tap = CtrUiText(UIS_TAP_TO_CONTINUE);

    EmNote(m->x, m->y, m->w, m->h);
    BPWrap(b->prompt, m->x + 14, m->y + 10, m->w - 24, 3, EM_DARK, EM_DARK_SH);
    UiText(tap, m->x + m->w - 14 - UiTextWidth(tap, 1) - 9, m->y + m->h - 16, EM_DIM, EM_DARK_SH, 1, m->x + m->w);
    UiArrow(m->x + m->w - 18, m->y + m->h - 16, 1, EM_STRIPE);
}

/* The viewport's area: the list, and over the dimmed list what the Bag has open. */
static void FBagFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchBag *b = &v->bag;

    EmSurface();
    TsBgRect(sFBArea.x - 4, sFBArea.y - 4, sFBArea.w + 8, sFBArea.h + 8);
    FBList(b);
    if (b->state == PT_BAG_STATE_ACTIONS || b->state == PT_BAG_STATE_YESNO || b->state == PT_BAG_STATE_QUANTITY
        || b->state == PT_BAG_STATE_MESSAGE) {
        CtrPokeTouchRect dim = {0, 0, 248, 168};

        UiDim(&dim);
        if (b->state == PT_BAG_STATE_ACTIONS)
            FBMenu(b);
        else if (b->state == PT_BAG_STATE_YESNO)
            FBYesNo(b);
        else if (b->state == PT_BAG_STATE_QUANTITY)
            FBQuantity(b);
        else
            FBMessage(b);
    }
    sUiS = &sUiNavy;
}

/* The note under the Bag: the selected item and its description (FireRed's), or what the pocket holds. */
static void FBagNote(const CtrPokeTouchView *v, int w)
{
    const CtrPokeTouchBag *b = &v->bag;

    EmSurface();
    EmNote(NOTE_X, NOTE_Y, w, NOTE_H);
    if (b->selItem) {
        int right = NOTE_X + w - 4;

        if (b->moveType >= 0) {                         /* a TM/HM: its move's type beside its name */
            CtrPokeTouchRect t = {(int16_t)(NOTE_X + w - 54), NOTE_Y + 5, 46, 15};

            UiTypeBadge(&t, b->moveType, b->typeName);
            right = t.x - 4;
        }
        if (b->use == PT_BAG_USE_SELL) {                /* selling: the player's money on the right */
            char money[16];

            snprintf(money, sizeof(money), "\xc2\xa5%u", (unsigned)b->money);
            UiTextRight(money, right - 4, NOTE_Y + 6, EM_VALUE, EM_DARK_SH, 1);
            right -= UiTextWidth(money, 1) + 10;
        }
        UiText(b->selName, NOTE_X + 12, NOTE_Y + 6, EM_DARK, EM_DARK_SH, 1, right);
        BDescribeN(b->selDesc, NOTE_X + 12, NOTE_Y + 21, w - 18, 3, EM_DIM);
    } else
        UiText(b->kind != PT_BAG_KIND_BAG ? b->closeName : CtrUiText(b->count ? UIS_BAG_CLOSE : UIS_BAG_EMPTY), NOTE_X + 12, NOTE_Y + (NOTE_H - UI_TEXT_H) / 2,
               EM_DIM, EM_DARK_SH, 1, NOTE_X + w - 4);
    sUiS = &sUiNavy;
}
