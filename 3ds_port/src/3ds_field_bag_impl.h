/* The field Bag in the viewport (3ds_poketouch.h CtrPokeTouchBag), included at the end of 3ds_poketouch.c after the
 * battle screen, whose ember pieces, item icons and text wrapping it shares. A remote for FireRed's own Bag, in the Llama look
 * (llama2_mockup.html, panel 6): the pockets as tabs (the open one in flame), the list's rows as cream cards (the item's icon in a well,
 * its name, its count in a sunk pill; the chosen row in flame with FireRed's cursor and the select ring, CLOSE BAG in slate), a rail with a
 * flame thumb between the two arrow buttons, the item's description in FireRed's text window with its count in a red ribbon, the
 * "1-5 of 9" card beside it, and whatever the Bag has open over the list (its menu, the quantity box, its question or its message) on
 * FireRed's window. Every row, cursor and count is the Bag's own (3ds_bottom.c walks them with its keys); the touch rectangles are
 * unchanged. The TM Case and the Berry Pouch (bag.kind) are the same list under their title, with FireRed's CLOSE row; a TM/HM shows its
 * move's type; a mart shows prices, what the Bag holds and the player's money. FBCard / FBButton below are the earlier look, kept for the
 * screens that still use them (the Pokedex, the PC, the choice, the move to forget, the naming screen). */
#include "3ds_ui_strings.h"

static const CtrPokeTouchRect sFBTab[3] = {{4, 4, 78, 24}, {85, 4, 78, 24}, {166, 4, 78, 24}};
static const CtrUiString sFBTabLabel[3] = {UIS_BAG_ITEMS, UIS_BAG_KEY_ITEMS, UIS_BAG_BALLS};
/* The arrows' zones are their buttons only; the rail between them (222, 59, 22, 78) is a slider: a tap or a drag on it goes where the finger is. */
static const CtrPokeTouchRect sFBUp = {222, 34, 22, 25}, sFBDown = {222, 137, 22, 26}, sFBRail = {222, 59, 22, 78};
static const CtrPokeTouchRect sFBArea = {PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H};
static const CtrPokeTouchRect sFBTitle = {4, 4, 240, 24};
#define FB_DEPTH 2

/* ---- Poke Ball: the Bag, the TM Case, the Berry Pouch, a mart and the item PC on the whole lower screen (320 x 240) ----
 * Words for the pockets on top; five plain rows, the chosen one a white pill; a thread for the rail; a dark band at y 160 with the exit piece on it;
 * under the band, white: the item's note. The popups (menu, question, quantity, message) keep their own rectangles, drawn over the dimmed
 * screen on the canvas moved by (BL_POP_X, BL_POP_Y). The touch codes are the Bag's own. */
#define BL_BAND_Y 160
#define BL_POP_X 36
#define BL_POP_Y 20
static CtrPokeTouchRect BLTab(int i)
{
    CtrPokeTouchRect r = {(int16_t)(8 + 102 * i), 4, 100, 26};
    return r;
}
static CtrPokeTouchRect BLRow(int i)
{
    CtrPokeTouchRect r = {8, (int16_t)(34 + 25 * i), 232, 24};
    return r;
}
static const CtrPokeTouchRect sBLTitle = {8, 4, 304, 26};
static const CtrPokeTouchRect sBLUp = {244, 34, 36, 24}, sBLRail = {244, 58, 36, 64}, sBLDown = {244, 122, 36, 22};
static const CtrPokeTouchRect sBLExit = {238, 146, 76, 38};

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

static int FBBallListHit(const CtrPokeTouchBag *b, int x, int y)
{
    for (int i = 0; i < 3 && b->kind == PT_BAG_KIND_BAG; ++i) {
        CtrPokeTouchRect r = BLTab(i);

        if (UiIn(&r, x, y))
            return PT_BAG_HIT_TAB + i;
    }
    if (UiIn(&sBLExit, x, y))
        return PT_BAG_HIT_QTY_CANCEL;                   /* the exit piece: B, as CLOSE BAG's row did */
    if (b->count + 1 > PT_BAG_ROWS) {
        if (UiIn(&sBLUp, x, y))
            return b->first > 0 ? PT_BAG_HIT_UP : -1;
        if (UiIn(&sBLDown, x, y))
            return b->first + PT_BAG_ROWS <= b->count ? PT_BAG_HIT_DOWN : -1;
        if (UiIn(&sBLRail, x, y))
            return PT_BAG_HIT_RAIL;
    }
    for (int i = 0; i < PT_BAG_ROWS; ++i) {
        CtrPokeTouchRect r = BLRow(i);

        if (b->first + i < b->count && UiIn(&r, x, y))
            return PT_BAG_HIT_ROW + i;
    }
    return -1;
}

int CtrPokeTouch_BagHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchBag *b = &v->bag;

    if (sStyleBall && b->on && b->state == PT_BAG_STATE_LIST)
        return FBBallListHit(b, x, y);
    x -= sStyleBall ? BL_POP_X : MENU_SHIFT;            /* the Bag is drawn on the floor of HOME's layout: its rectangles are at (4, 4) */
    y -= sStyleBall ? BL_POP_Y : MENU_SHIFT;
    if (!b->on || x >= 248 || x < 0 || y < 0)
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
            if (UiIn(&sFBRail, x, y))
                return PT_BAG_HIT_RAIL;
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


/* The slider: the cursor index that puts the thumb, centred on y, where the finger is. */
int CtrPokeTouch_BagRailTarget(const CtrPokeTouchBag *b, int y)
{
    const CtrPokeTouchRect railBL = sBLRail, railFB = sFBRail;
    const CtrPokeTouchRect sFBRail = sStyleBall ? railBL : railFB;
    int total = b->count + 1, range = total - PT_BAG_ROWS, thumbH = sFBRail.h * PT_BAG_ROWS / total, first, t;

    y -= sStyleBall ? 0 : MENU_SHIFT;
    if (range <= 0)
        return -1;
    if (thumbH < 12)
        thumbH = 12;
    t = y - sFBRail.y - thumbH / 2;
    t = t < 0 ? 0 : t > sFBRail.h - thumbH ? sFBRail.h - thumbH : t;
    first = (t * range * 2 + (sFBRail.h - thumbH)) / (2 * (sFBRail.h - thumbH));
    first = first > range ? range : first;
    t = first + (y < sFBRail.y ? 0 : y >= sFBRail.y + sFBRail.h ? PT_BAG_ROWS - 1
              : (y - sFBRail.y) * (PT_BAG_ROWS - 1) / sFBRail.h);
    return t > b->count ? b->count : t;
}

/* ---- the Llama look (llama2_mockup.html, panel 6) ----
 * A card is a raised piece (EmButton): FB_FACE rows of face and FB_DEPTH of lip, in a rectangle FB_FACE + FB_DEPTH tall; the chosen one wears
 * the flame palette, FireRed's red cursor (white on flame) and the select ring, which lies in the 2 px gap between the rows (the rows are 26
 * apart) and so never reaches the next card. The touch rectangles are the Bag's own (above): a card is drawn inside its rectangle. */
#define FB_FACE 22
#define FB_QTY_INK UI_RGB(48, 104, 216)                 /* a count in its sunk pill: the mockup's .blu */
#define FB_QTY_SH UI_RGB(192, 208, 240)
#define FB_PILL_W 34                                    /* the pill of a count: 34 x 16, 4 from the card's right edge */
#define FB_PILL_H 16
#define FB_IDLE_ARROW UI_RGB(200, 192, 176)             /* a scroll arrow with nothing to scroll to */

/* The light card and the raised button the Pokedex, the PC, the choice, the move to forget and the naming screen still call (kept as they were
 * until those screens are converted; UI_FN so that a screen that stops using one does not leave it unused). */
UI_FN void FBCard(const CtrPokeTouchRect *r)
{
    UiRoundFill(r->x, r->y, r->w, r->h, 5, kEmCream.edge);
    EmGradFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, 4, kEmCream.top, kEmCream.bottom);
}

UI_FN void FBButton(const CtrPokeTouchRect *r, const EmPal *p, const char *label, int pressed)
{
    int fy = EmButton(r->x, r->y, r->w, r->h, p, FB_DEPTH, pressed);

    EmLabel(label, r->x, r->w, fy, r->h - FB_DEPTH, p);
}

/* A window over the dimmed list (the menu, the question, the quantity box, the message): FireRed's text window. */
static void FBWindow(const CtrPokeTouchRect *r)
{
    LlWindowPlain(r->x, r->y, r->w, r->h);
}

/* A card of the zone `r` (FB_DEPTH of it is the lip) with `label` centred on it; `chosen` adds the select ring and the cursor. */
static void FBLlButton(const CtrPokeTouchRect *r, const EmPal *p, const char *label, int pressed, int chosen)
{
    int face = r->h - FB_DEPTH, fy;

    if (chosen)
        LlSelectRing(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, p, FB_DEPTH, pressed);
    if (chosen)
        LlCursor(r->x + 9, fy + (face - 10) / 2, EM_WHITE);
    if (UiTextWidth(label, 1) > r->w - 8 && strchr(label, ' ')) {   /* a label that would touch both edges: two lines (CLOSE / BOX) */
        char first[40];
        const char *rest = strchr(label, ' ');
        int n = (int)(rest - label), top = fy + (face - (2 * UI_TEXT_H + 2)) / 2;

        if (n > (int)sizeof(first) - 1)
            n = (int)sizeof(first) - 1;
        memcpy(first, label, (size_t)n);
        first[n] = 0;
        EmLabel(first, r->x, r->w, top, UI_TEXT_H, p);
        EmLabel(rest + 1, r->x, r->w, top + UI_TEXT_H + 2, UI_TEXT_H, p);
        return;
    }
    EmLabel(label, r->x, r->w, fy, face, p);
}

/* A count (or a price) in the sunk pill the mockup gives it, blue; (x, y) its left and top, `w` its width. */
static void FBPillOn(int x, int y, int w, const char *text, int onFlame)
{
    LlWell(x, y, w, FB_PILL_H);
    if (onFlame)                                        /* on the chosen (orange) row the well's light row under it reads as a gap in the outline: a dark one closes it */
        Fill(x + 3, y + FB_PILL_H, w - 6, 1, UI_RGB(150, 60, 20));
    UiText(text, x + (w - UiTextWidth(text, 1)) / 2, y + (FB_PILL_H - UI_TEXT_H) / 2, FB_QTY_INK, FB_QTY_SH, 1, x + w - 1);
}

static void FBPill(int x, int y, int w, const char *text)
{
    FBPillOn(x, y, w, text, 0);
}

static void FBTabs(const CtrPokeTouchBag *b)
{
    if (b->kind != PT_BAG_KIND_BAG) {                   /* a case: its icon and name on the flame card the open pocket is */
        int w = 28 + UiTextWidth(b->title, 1), x = sFBTitle.x + (sFBTitle.w - w) / 2, fy;

        fy = EmButton(sFBTitle.x, sFBTitle.y, sFBTitle.w, sFBTitle.h, &kEmFlame, FB_DEPTH, 0);
        if (b->kind == PT_BAG_KIND_SHOP) {              /* a mart: its BUY, and the player's money in a pill on the right */
            char money[16];
            int mw;

            snprintf(money, sizeof(money), "\xc2\xa5%u", (unsigned)b->money);
            mw = UiTextWidth(money, 1) + 14;
            if (mw < FB_PILL_W)
                mw = FB_PILL_W;
            FBPill(sFBTitle.x + sFBTitle.w - 6 - mw, fy + 3, mw, money);
            Em2Text(b->title, sFBTitle.x + 12, fy + (FB_FACE - UI_TEXT_H) / 2, kEmFlame.ink, kEmFlame.inkSh, sFBTitle.x + sFBTitle.w - 12 - mw);
            return;
        }
        if (b->titleItem)
            BItemDraw(b->titleItem, x, fy - 1, 1);
        Em2Text(b->title, x + 28, fy + (FB_FACE - UI_TEXT_H) / 2, kEmFlame.ink, kEmFlame.inkSh, sFBTitle.x + sFBTitle.w - 6);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        const CtrPokeTouchRect *r = &sFBTab[i];
        const EmPal *p = LlPick(b->pocket == i, &kEmCream);
        int fy = EmButton(r->x, r->y, r->w, r->h, p, FB_DEPTH, b->pressed == PT_BAG_HIT_TAB + i);

        EmLabel(CtrUiText(sFBTabLabel[i]), r->x, r->w, fy, r->h - FB_DEPTH, p);
    }
}

/* A row: an item (icon in its well, name, count in its pill but for key items) or CLOSE BAG; the chosen one in flame with the cursor. */
static void FBRowDraw(const CtrPokeTouchBag *b, int i)
{
    CtrPokeTouchRect r = FBRow(i);
    int index = b->first + i, selected = index == b->cursor, press = b->pressed == PT_BAG_HIT_ROW + i, fy, nameX, nameRight = r.x + r.w - 8;
    const EmPal *p;

    if (index > b->count)
        return;
    if (index == b->count) {                            /* CLOSE BAG (a case's own CLOSE): slate, with BACK's arrow */
        p = LlPick(selected, &kEmSlate);
        if (selected)
            LlSelectRing(r.x, r.y, r.w, FB_FACE + FB_DEPTH);
        fy = EmButton(r.x, r.y, r.w, FB_FACE + FB_DEPTH, p, FB_DEPTH, press);
        LlArrow(r.x + 10, fy + (FB_FACE - 10) / 2, 6, 10, 2, p->ink);
        EmLabel(b->kind != PT_BAG_KIND_BAG ? b->closeName : CtrUiText(UIS_BAG_CLOSE), r.x, r.w, fy, FB_FACE, p);
        return;
    }
    p = LlPick(selected, &kEmCream);
    if (selected)
        LlSelectRing(r.x, r.y, r.w, FB_FACE + FB_DEPTH);
    fy = EmButton(r.x, r.y, r.w, FB_FACE + FB_DEPTH, p, FB_DEPTH, press);
    LlWell(r.x + 2, fy + 2, 26, 18);
    BItemDraw(b->row[i].item, r.x + 3, fy - 1, 1);
    if (selected) {
        LlCursor(r.x + 31, fy + (FB_FACE - 10) / 2, EM_WHITE);
        nameX = r.x + 41;
    } else
        nameX = r.x + 33;
    if (b->kind == PT_BAG_KIND_SHOP) {                  /* a mart's row: its price, in a pill that fits it */
        char price[12];
        int pw;

        snprintf(price, sizeof(price), "\xc2\xa5%u", (unsigned)b->row[i].price);
        pw = UiTextWidth(price, 1) + 14;
        if (pw < FB_PILL_W)
            pw = FB_PILL_W;
        FBPill(r.x + r.w - 4 - pw, fy + 3, pw, price);
        nameRight = r.x + r.w - 4 - pw - 4;
    } else if (b->kind == PT_BAG_KIND_BAG && b->pocket == 1 && b->row[i].item
               && (b->row[i].item == b->regX || b->row[i].item == b->regY)) {
        const char *key = b->row[i].item == b->regX ? "X" : "Y";            /* its button: the blue key tab of the shortcuts */
        int kw = UiTextWidth(key, 1) + 14;

        LlRibbonBox(r.x + r.w - 6 - kw, fy + (FB_FACE - 11) / 2, kw, 11, 7, LL_TL, LL_BLUE_TOP, LL_BLUE_BOT, key);
        nameRight = r.x + r.w - 6 - kw - 4;
    } else if (b->kind != PT_BAG_KIND_BAG ? b->row[i].qty != 0 : b->pocket != 1) {   /* key items and HMs: no count */
        char qty[8];

        snprintf(qty, sizeof(qty), "x%u", b->row[i].qty > 999 ? 999 : b->row[i].qty);
        FBPillOn(r.x + r.w - 4 - FB_PILL_W, fy + 3, FB_PILL_W, qty, selected);
        nameRight = r.x + r.w - 4 - FB_PILL_W - 4;
    }
    Em2Text(b->row[i].name, nameX, fy + (FB_FACE - UI_TEXT_H) / 2, p->ink, p->inkSh, nameRight);
}

/* The rail beside a list longer than its rows: an arrow button at each end (the zones above), between them the rail and its flame thumb. */
static void FBRail(const CtrPokeTouchBag *b)
{
    enum { RAIL_Y = 59, RAIL_H = 78, DOWN_Y = 140 };
    int total = b->count + 1, range = total - PT_BAG_ROWS, thumbH = RAIL_H * PT_BAG_ROWS / total, thumbY;
    int upOn = b->first > 0, downOn = b->first + PT_BAG_ROWS <= b->count, fy;

    if (thumbH < 12)
        thumbH = 12;
    thumbY = range > 0 ? (RAIL_H - thumbH) * (b->first < range ? b->first : range) / range : 0;
    fy = EmButton(sFBUp.x, sFBUp.y, sFBUp.w, FB_FACE, &kEmCream, FB_DEPTH, b->pressed == PT_BAG_HIT_UP);
    LlArrow(sFBUp.x + 7, fy + 7, 8, 5, 0, upOn ? LL_CURSOR : FB_IDLE_ARROW);
    LlScrollRail(sFBUp.x + 6, RAIL_Y, 10, RAIL_H, thumbY, thumbH);
    fy = EmButton(sFBUp.x, DOWN_Y, sFBUp.w, FB_FACE, &kEmCream, FB_DEPTH, b->pressed == PT_BAG_HIT_DOWN);
    LlArrow(sFBUp.x + 7, fy + 8, 8, 5, 1, downOn ? LL_CURSOR : FB_IDLE_ARROW);
}

static void FBList(const CtrPokeTouchBag *b)
{
    FBTabs(b);
    for (int pass = 0; pass < 2; ++pass)                /* the chosen row last: its ring lies in the gaps its neighbours' cards leave */
        for (int i = 0; i < PT_BAG_ROWS; ++i)
            if ((pass == 1) == (b->first + i == b->cursor))
                FBRowDraw(b, i);
    if (b->count + 1 > PT_BAG_ROWS)
        FBRail(b);
}

/* The item's menu on FireRed's window: the item under its icon, USE / GIVE / TOSS ... as cards, CANCEL (the last) in slate. */
static void FBMenu(const CtrPokeTouchBag *b)
{
    CtrPokeTouchRect c = FBMenuCard(b);
    int n = b->menuCount > PT_BAG_MENU_MAX ? PT_BAG_MENU_MAX : b->menuCount;

    FBWindow(&c);
    if (b->selItem) {
        BItemDraw(b->selItem, c.x + 8, c.y + 3, 1);
        Em2Text(b->selName, c.x + 36, c.y + 11, EM_DARK, EM_DARK_SH, c.x + c.w - 10);
    }
    for (int i = 0; i < n; ++i) {
        CtrPokeTouchRect r = FBMenuRow(b, i);
        int chosen = b->menuCursor == i;

        FBLlButton(&r, LlPick(chosen, i == n - 1 ? &kEmSlate : &kEmCream), b->menuName[i], b->pressed == PT_BAG_HIT_MENU + i, chosen);
    }
}

static void FBYesNo(const CtrPokeTouchBag *b)
{
    FBWindow(&sFBAsk);
    BPWrap(b->prompt, sFBAsk.x + 14, sFBAsk.y + 12, sFBAsk.w - 24, 2, EM_DARK, EM_DARK_SH);
    if (b->selItem) {                                   /* what the question is about: its icon, name and count */
        char what[32];
        int w, x;

        snprintf(what, sizeof(what), "%s x%u", b->selName, b->tossCount);
        w = 28 + UiTextWidth(what, 1);
        x = sFBAsk.x + (sFBAsk.w - w) / 2;
        BItemDraw(b->selItem, x, sFBAsk.y + 38, 1);
        Em2Text(what, x + 28, sFBAsk.y + 46, EM_DIM, EM_DARK_SH, sFBAsk.x + sFBAsk.w - 8);
    }
    FBLlButton(&sFBYes, LlPick(b->menuCursor == 0, &kEmCream), CtrUiText(UIS_YES), b->pressed == PT_BAG_HIT_MENU, b->menuCursor == 0);
    FBLlButton(&sFBNo, LlPick(b->menuCursor == 1, &kEmSlate), CtrUiText(UIS_NO), b->pressed == PT_BAG_HIT_MENU + 1, b->menuCursor == 1);
}

/* An arrow button of the quantity box: a cream card with the red triangle, like the list's. */
static void FBQtyButton(const CtrPokeTouchRect *r, int dir, int pressed)
{
    int fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, FB_DEPTH, pressed);

    LlArrow(r->x + (r->w - 12) / 2, fy + (r->h - FB_DEPTH - 7) / 2, 12, 7, dir, LL_CURSOR);
}

/* The quantity box: how many to toss, the count in its sunk well between DOWN and UP, OK and CANCEL under it. */
static void FBQuantity(const CtrPokeTouchBag *b)
{
    char count[12];
    CtrPokeTouchRect name = {sFBAsk.x, 0, sFBAsk.w, 13};

    FBWindow(&sFBAsk);
    UiTextCentered(CtrUiText(b->kind == PT_BAG_KIND_SHOP ? UIS_BAG_BUY_HOW_MANY
                             : b->kind == PT_BAG_KIND_ITEMPC ? UIS_BAG_WITHDRAW_HOW_MANY : b->use == PT_BAG_USE_SELL
                             ? UIS_BAG_SELL_HOW_MANY : b->use == PT_BAG_USE_DEPOSIT ? UIS_BAG_DEPOSIT_HOW_MANY
                             : UIS_BAG_TOSS_HOW_MANY), &name, sFBAsk.y + 8, EM_DARK, EM_DARK_SH, 1);
    if (b->use == PT_BAG_USE_SELL || b->kind == PT_BAG_KIND_SHOP) {   /* the item, and what that many come to */
        char line[40];

        snprintf(line, sizeof(line), "%s  \xc2\xa5%u", b->selName, (unsigned)b->sale);
        UiTextCentered(line, &name, sFBAsk.y + 22, EM_VALUE, EM_DARK_SH, 1);
    } else
        UiTextCentered(b->selName, &name, sFBAsk.y + 22, EM_DIM, EM_DARK_SH, 1);
    snprintf(count, sizeof(count), "x%u", b->tossCount);
    LlWell(sFBQtyDown.x + sFBQtyDown.w + 6, sFBQtyDown.y + 1, sFBQtyUp.x - sFBQtyDown.x - sFBQtyDown.w - 12, sFBQtyDown.h - 4);
    UiText(count, 124 - UiTextWidth(count, 2) / 2, sFBQtyDown.y + 7, FB_QTY_INK, FB_QTY_SH, 2, 170);
    FBQtyButton(&sFBQtyDown, 1, b->pressed == PT_BAG_HIT_QTY_DOWN);
    FBQtyButton(&sFBQtyUp, 0, b->pressed == PT_BAG_HIT_QTY_UP);
    FBLlButton(&sFBYes, &kEmFlame, CtrUiText(UIS_OK), b->pressed == PT_BAG_HIT_QTY_OK, 0);
    FBLlButton(&sFBNo, &kEmSlate, CtrUiText(UIS_CANCEL), b->pressed == PT_BAG_HIT_QTY_CANCEL, 0);
}

static void FBMessage(const CtrPokeTouchBag *b)
{
    const CtrPokeTouchRect *m = &sFBMessage;
    const char *tap = CtrUiText(UIS_TAP_TO_CONTINUE);

    FBWindow(m);
    BPWrap(b->prompt, m->x + 14, m->y + 10, m->w - 24, 3, EM_DARK, EM_DARK_SH);
    UiText(tap, m->x + m->w - 14 - UiTextWidth(tap, 1) - 9, m->y + m->h - 17, EM_DIM, EM_DARK_SH, 1, m->x + m->w - 8);
    LlArrow(m->x + m->w - 19, m->y + m->h - 16, 8, 5, 1, LL_CURSOR);
}

static int FBSelectedCount(const CtrPokeTouchBag *b);
static void FBTag(int *right, int base, int nameW, int room, int y, const char *text, uint16_t top, uint16_t bottom, uint16_t type);

/* Poke Ball: the pockets are words, the open one white and underlined; a case or a mart shows its title and, at a mart, the money in plain text. */
static void FBBallTabs(const CtrPokeTouchBag *b)
{
    if (b->kind != PT_BAG_KIND_BAG) {
        UiText(b->title, sBLTitle.x + 8, sBLTitle.y + 9, EM_WHITE, BL_DEEP, 1, sBLTitle.x + sBLTitle.w - 80);
        if (b->kind == PT_BAG_KIND_SHOP) {
            char money[16];

            snprintf(money, sizeof(money), "\xc2\xa5%u", (unsigned)b->money);
            UiText(money, sBLTitle.x + sBLTitle.w - 8 - UiTextWidth(money, 1), sBLTitle.y + 9, BL_PINK, BL_DEEP, 1, sBLTitle.x + sBLTitle.w);
        }
        Fill(sBLTitle.x, sBLTitle.y + sBLTitle.h, sBLTitle.w, 1, BL_HAIR);
        return;
    }
    for (int i = 0; i < 3; ++i) {
        CtrPokeTouchRect r = BLTab(i);
        const char *label = CtrUiText(sFBTabLabel[i]);
        int on = b->pocket == i, tw = UiTextWidth(label, 1), x = r.x + (r.w - tw) / 2;

        UiText(label, x, r.y + 8, on ? EM_WHITE : BL_PINK, BL_DEEP, 1, r.x + r.w);
        if (on)
            UiRoundFill(x, r.y + r.h - 3, tw, 3, 1, EM_WHITE);
    }
    Fill(8, 30, 304, 1, BL_HAIR);
}

/* Poke Ball: a row is its icon (on a white disc), name and count or price in plain text; the chosen one a white pill with a red dot. The row
 * after the last item (CLOSE) is the exit piece on the band. */
static void FBBallRow(const CtrPokeTouchBag *b, int i)
{
    CtrPokeTouchRect r = BLRow(i);
    int index = b->first + i, selected = index == b->cursor, nameX, nameRight = r.x + r.w - 10;
    char text[12];

    if (index >= b->count)
        return;
    if (selected)
        UiRoundFill(r.x, r.y, r.w, r.h, 11, BL_WHITE);
    else if (i)
        Fill(r.x + 8, r.y - 1, r.w - 16, 1, BL_HAIR);
    if (!selected)
        TsDisc(r.x + 16, r.y + 12, 11, BL_WHITE);
    BItemDraw(b->row[i].item, r.x + 4, r.y - 1, 1);
    if (selected) {
        TsDisc(r.x + 34, r.y + 12, 3, BL_REDTXT);
        nameX = r.x + 44;
    } else
        nameX = r.x + 34;
    if (b->kind == PT_BAG_KIND_SHOP) {
        snprintf(text, sizeof(text), "\xc2\xa5%u", (unsigned)b->row[i].price);
        UiText(text, nameRight - UiTextWidth(text, 1), r.y + 8, selected ? BL_REDTXT : EM_WHITE, selected ? BL_WHITE : BL_DEEP, 1, nameRight + 2);
        nameRight -= UiTextWidth(text, 1) + 10;
    } else if (b->kind == PT_BAG_KIND_BAG && b->pocket == 1 && b->row[i].item && (b->row[i].item == b->regX || b->row[i].item == b->regY)) {
        const char *key = b->row[i].item == b->regX ? "X" : "Y";

        TsDisc(nameRight - 6, r.y + 12, 8, selected ? BL_REDTXT : BL_WHITE);
        UiText(key, nameRight - 6 - UiTextWidth(key, 1) / 2, r.y + 8, selected ? BL_WHITE : BL_REDTXT, selected ? BL_REDTXT : BL_WHITE, 1, nameRight + 4);
        nameRight -= 20;
    } else if (b->kind != PT_BAG_KIND_BAG ? b->row[i].qty != 0 : b->pocket != 1) {
        snprintf(text, sizeof(text), "x%u", b->row[i].qty > 999 ? 999 : b->row[i].qty);
        UiText(text, nameRight - UiTextWidth(text, 1), r.y + 8, selected ? UI_RGB(150, 140, 140) : BL_PINK, selected ? BL_WHITE : BL_DEEP, 1, nameRight + 2);
        nameRight -= UiTextWidth(text, 1) + 10;
    }
    UiText(b->row[i].name, nameX, r.y + 8, selected ? BL_REDTXT : EM_WHITE, selected ? BL_WHITE : BL_DEEP, 1, nameRight);
}

/* Poke Ball: the rail is a thread with a white thumb; the two arrows (their zones above) are small triangles at its ends. */
static void FBBallRail(const CtrPokeTouchBag *b)
{
    int total = b->count + 1, range = total - PT_BAG_ROWS, railY = sBLRail.y, railH = sBLRail.h, thumbH = railH * PT_BAG_ROWS / total, thumbY, cx = sBLRail.x + sBLRail.w / 2;
    int upOn = b->first > 0, downOn = b->first + PT_BAG_ROWS <= b->count;

    if (thumbH < 14)
        thumbH = 14;
    thumbY = range > 0 ? (railH - thumbH) * (b->first < range ? b->first : range) / range : 0;
    UiRoundFill(cx - 1, railY, 3, railH, 1, BL_DEEP);
    UiRoundFill(cx - 1, railY + thumbY, 3, thumbH, 1, EM_WHITE);
    for (int k = 0; k < 6; ++k) {
        UiRoundFill(cx - k, sBLUp.y + 4 + k, 2 * k + 1, 1, 0, upOn ? EM_WHITE : BL_HAIR);
        UiRoundFill(cx - (5 - k), sBLDown.y + 14 + k, 2 * (5 - k) + 1, 1, 0, downOn ? EM_WHITE : BL_HAIR);
    }
}

/* Poke Ball: the exit piece, straddling the band; white with a dark outline, the one with the cursor on it ringed in red when the Bag's cursor is on CLOSE. */
static void FBBallExit(const CtrPokeTouchBag *b)
{
    const char *label = CTR_TR("EXIT", "SALIR", "ZURÃ" "CK");     /* short: the piece is 76 px wide */
    int on = b->cursor >= b->count && b->state == PT_BAG_STATE_LIST, tw = UiTextWidth(label, 1);

    UiRoundFill(sBLExit.x, sBLExit.y, sBLExit.w, sBLExit.h, 5, BL_BAND);
    UiRoundFill(sBLExit.x + 3, sBLExit.y + 3, sBLExit.w - 6, sBLExit.h - 6, 4, on ? BL_REDTXT : BL_WHITE);
    if (tw > sBLExit.w - 10)
        tw = sBLExit.w - 10;
    UiText(label, sBLExit.x + (sBLExit.w - tw) / 2, sBLExit.y + (sBLExit.h - UI_TEXT_H) / 2, on ? EM_WHITE : BL_BAND, on ? BL_REDTXT : BL_WHITE, 1, sBLExit.x + sBLExit.w - 4);
}

/* The note on the white under the band: the item, its tags (type, owned, money, count) and its description. */
static void FBBallNote(const CtrPokeTouchBag *b)
{
    int left = 14, ty = BL_BAND_Y + 14;

    if (b->selItem) {
        int base = 226, right = base, room = 200, nameW = UiTextWidth(b->selName, 1), count = FBSelectedCount(b), before;
        char tag[32];

        if (b->moveType >= 0)
            FBTag(&right, base, nameW, room, ty, b->typeName, 0, 0, UiTypeColor(b->moveType));
        if (b->showOwned) {
            before = right;
            snprintf(tag, sizeof(tag), "%s %u", CtrUiText(UIS_BAG_IN_BAG), (unsigned)b->owned);
            FBTag(&right, base, nameW, room, ty, tag, LL_BLUE_TOP, LL_BLUE_BOT, 0);
            if (right == before) {
                snprintf(tag, sizeof(tag), "x%u", (unsigned)b->owned);
                FBTag(&right, base, nameW, room, ty, tag, LL_BLUE_TOP, LL_BLUE_BOT, 0);
            }
        }
        if (b->use == PT_BAG_USE_SELL) {
            snprintf(tag, sizeof(tag), "\xc2\xa5%u", (unsigned)b->money);
            FBTag(&right, base, nameW, room, ty, tag, LL_RIB_TOP, LL_RIB_BOT, 0);
        }
        if (count >= 0) {
            snprintf(tag, sizeof(tag), "x%d", count);
            FBTag(&right, base, nameW, room, ty, tag, LL_RIB_TOP, LL_RIB_BOT, 0);
        }
        UiText(b->selName, left, ty, BL_BAND, BL_WHITE, 1, right);
        MenuWrap(b->selDesc, left, ty + 20, 212, 3, 12, UI_RGB(132, 128, 130), BL_WHITE);
    } else
        UiText(b->kind != PT_BAG_KIND_BAG ? b->closeName : CtrUiText(b->count ? UIS_BAG_CLOSE : UIS_BAG_EMPTY), left, ty, UI_RGB(132, 128, 130), BL_WHITE, 1, 226);
}

/* The whole Poke Ball Bag: the red ground and the white under the band, the words, rows and thread, the exit piece, the note, and over the
 * dimmed screen whatever the Bag has open (its popups are centred by moving the canvas). */
static void BallBagScreen(const CtrPokeTouchView *v)
{
    const CtrPokeTouchBag *b = &v->bag;
    uint16_t *base = sCanvas;

    sBallExitR = sBLExit;
    EmSurface();
    TsBallGround(BL_BAND_Y);
    FBBallTabs(b);
    for (int i = 0; i < PT_BAG_ROWS; ++i)
        FBBallRow(b, i);
    if (b->count + 1 > PT_BAG_ROWS)
        FBBallRail(b);
    FBBallNote(b);
    FBBallExit(b);
    if (b->state == PT_BAG_STATE_ACTIONS || b->state == PT_BAG_STATE_YESNO || b->state == PT_BAG_STATE_QUANTITY || b->state == PT_BAG_STATE_MESSAGE) {
        CtrPokeTouchRect dim = {0, 0, W, H};

        UiDim(&dim);
        sCanvas = base + (size_t)BL_POP_X * H - BL_POP_Y;
        if (b->state == PT_BAG_STATE_ACTIONS)
            FBMenu(b);
        else if (b->state == PT_BAG_STATE_YESNO)
            FBYesNo(b);
        else if (b->state == PT_BAG_STATE_QUANTITY)
            FBQuantity(b);
        else
            FBMessage(b);
        sCanvas = base;
    }
    sUiS = &sUiNavy;
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
        CtrPokeTouchRect dim = {PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H};

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

/* The count of the selected row, or -1 when its row shows none (key items and HMs have no count; a mart's rows show a price). */
static int FBSelectedCount(const CtrPokeTouchBag *b)
{
    int i = b->cursor - b->first;

    if (!b->selItem || i < 0 || i >= PT_BAG_ROWS || b->kind == PT_BAG_KIND_SHOP)
        return -1;
    if (b->kind != PT_BAG_KIND_BAG ? b->row[i].qty == 0 : b->pocket == 1)
        return -1;
    return b->row[i].qty > 999 ? 999 : b->row[i].qty;
}

/* "1-5 of 9": the items the rows show, over the list's length, on the status plate's place in the top band (absolute coordinates). */
static void FBPositionTop(const CtrPokeTouchBag *b)
{
    const CtrPokeTouchRect *r = &sHomeStatus;
    char text[24];
    int first = b->first + 1, last = b->first + PT_BAG_ROWS, fy;

    if (b->count <= 0)
        return;
    if (last > b->count)
        last = b->count;
    if (first > last)
        first = last;
    snprintf(text, sizeof(text), CTR_TR("%d-%d of %d", "%d-%d de %d", "%d-%d von %d"), first, last, b->count);
    if (UiTextWidth(text, 1) > r->w - 10)               /* "54-58 von 58" would be cut: the short form */
        snprintf(text, sizeof(text), "%d-%d/%d", first, last, b->count);
    Em2CardShadow(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, FB_DEPTH, 0);
    EmLabel(text, r->x, r->w, fy, r->h - FB_DEPTH, &kEmCream);
}

/* A tag on the note's title line, from the right: a ribbon of `top`..`bottom` (a type badge when `top` is 0) if the name is still whole beside it. */
static void FBTag(int *right, int base, int nameW, int room, int y, const char *text, uint16_t top, uint16_t bottom, uint16_t type)
{
    int tw = (top ? UiTextWidth(text, 1) + 18 : UiTextWidth(text, 1) + 12);

    if (nameW + base - *right + tw + 4 > room)
        return;
    if (top)
        LlRibbon(*right - tw, y, text, top, bottom);
    else
        LlTypeBadge(*right - tw, y, text, type);
    *right -= tw + 4;
}

/* The note under the floor, FireRed's text window: the selected item (its count in a red ribbon, a TM/HM its move's type, a mart its owned
 * count, a sale the player's money; each only while the name stays whole beside it) and its description in two small lines, or what the
 * pocket holds. */
static void FBagNote(const CtrPokeTouchView *v, int w)
{
    const CtrPokeTouchBag *b = &v->bag;

    (void)w;
    EmSurface();
    NoteWindow();
    if (b->selItem) {
        int base = HN_X + HN_W - 12, right = base, room = HN_W - 30, nameW = UiTextWidth(b->selName, 1), count = FBSelectedCount(b), before;
        int ty = HN_Y + 2;
        char tag[32];

        if (b->moveType >= 0)                           /* a TM/HM: its move's type, the rectangular badge */
            FBTag(&right, base, nameW, room, ty, b->typeName, 0, 0, UiTypeColor(b->moveType));
        if (b->showOwned) {                             /* buying: how many the Bag already holds, in blue (just the count when the name is long) */
            before = right;
            snprintf(tag, sizeof(tag), "%s %u", CtrUiText(UIS_BAG_IN_BAG), (unsigned)b->owned);
            FBTag(&right, base, nameW, room, ty, tag, LL_BLUE_TOP, LL_BLUE_BOT, 0);
            if (right == before) {
                snprintf(tag, sizeof(tag), "x%u", (unsigned)b->owned);
                FBTag(&right, base, nameW, room, ty, tag, LL_BLUE_TOP, LL_BLUE_BOT, 0);
            }
        }
        if (b->use == PT_BAG_USE_SELL) {                /* selling: the player's money */
            snprintf(tag, sizeof(tag), "\xc2\xa5%u", (unsigned)b->money);
            FBTag(&right, base, nameW, room, ty, tag, LL_RIB_TOP, LL_RIB_BOT, 0);
        }
        if (count >= 0) {                               /* the quantity: the ribbon of the mockup */
            snprintf(tag, sizeof(tag), "x%d", count);
            FBTag(&right, base, nameW, room, ty, tag, LL_RIB_TOP, LL_RIB_BOT, 0);
        }
        Em2Text(b->selName, HN_X + 12, ty, EM_DARK, EM_DARK_SH, right);
        MenuWrap(b->selDesc, HN_X + 12, HN_Y + 11, HN_W - 24, 2, 9, EM_DIM, EM_DARK_SH);
    } else
        Em2Text(b->kind != PT_BAG_KIND_BAG ? b->closeName : CtrUiText(b->count ? UIS_BAG_CLOSE : UIS_BAG_EMPTY), HN_X + 12,
                HN_Y + (HN_H - UI_TEXT_H) / 2, EM_DIM, EM_DARK_SH, HN_X + HN_W - 12);
    sUiS = &sUiNavy;
}
