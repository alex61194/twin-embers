/* The battle screen (included at the end of 3ds_poketouch.c, whose drawing helpers and design system it uses).
 * The lower screen while FireRed's battle is on the upper one, in the "Llama" look (the owner's mockup, llama2_mockup.html; the pieces are
 * 3ds_ember2_impl.h's Ll*): the dark ember backdrop, the party's six Poke Balls on a slanted slate tray on top, raised glass cards over
 * it (flame for FIGHT and for what is chosen, sky blue for the party, white for the moves, slate for CANCEL), FireRed's cream HP box
 * inside FIGHT, its text window for the notes, its red cursor and slanted red ribbons: the action menu (FIGHT across the top; BAG,
 * POKeMON and RUN below, and in a wild battle the quick ball between BAG and POKeMON), the move cards, the target cards, the
 * Safari Zone's four, the battle Bag and the battle Party.
 * Everything shown is what the controller reports. A card is drawn in a rectangle of its own plus the 2 px its select ring and shadow
 * use (the canvas redraws only what changed). */

#include "3ds_icons.h"
#include "3ds_text_case.h"
#include "3ds_ui_strings.h"
#include "3ds_type_chart.h"
#include "3ds_battle_nav.h"

/* The party's balls, on their tray (the mockup's .tray: 196 x 25 at (62, 5); a ball every 28 px, 14 across, its mini bar under it). */
#define BL_TRAY_X 62
#define BL_TRAY_Y 5
/* The action menu, FireRed's cursors 0-3: FIGHT across the top, BAG, POKeMON and RUN below; in a wild battle the quick
 * ball (CTR_ACTION_BALL) between BAG and POKeMON, the four a row of equal buttons (3ds_battle_nav.h walks the D-pad over
 * them the same way). The rectangles are the touch areas; a card is drawn in the top of its own (FIGHT 76 tall, the row's 111: the
 * mockup's 73 and 108 with the 3 px lip). */
#define BACT_ROW_Y 120
#define BACT_ROW_H 114
#define BL_FIGHT_H 76
#define BL_ROW_H 111
static const CtrPokeTouchRect sBFight = {8, 36, 304, 78};
static const CtrPokeTouchRect sBRow3[3] = {{8, BACT_ROW_Y, 98, BACT_ROW_H}, {111, BACT_ROW_Y, 98, BACT_ROW_H},
                                           {214, BACT_ROW_Y, 98, BACT_ROW_H}};
static const CtrPokeTouchRect sBRow4[4] = {{8, BACT_ROW_Y, 73, BACT_ROW_H}, {85, BACT_ROW_Y, 73, BACT_ROW_H},
                                           {162, BACT_ROW_Y, 73, BACT_ROW_H}, {239, BACT_ROW_Y, 73, BACT_ROW_H}};
/* The rows inside a row button, from the top of its face: the icon's disc, the name, the quick ball's dots, the caption pill
 * (or the quick ball's arrows and count). */
#define BACT_DISC_Y 30
#define BACT_DISC_R 24
#define BACT_NAME_Y 62
#define BACT_DOTS_Y 79
#define BACT_PILL_Y 88          /* the quick ball's arrows start two px above this (their touch area) */
#define BL_SUB_Y 85             /* the caption pill (.sub): 15 tall, 7 px in from each side */
#define BACT_ARROW_W 20
#define BACT_ARROW_H 19

/* Action `i` (0-3, or CTR_ACTION_BALL) on the screen `v` shows. */
static const CtrPokeTouchRect *BActRect(const CtrBattleView *v, int i)
{
    if (i == 0)
        return &sBFight;
    if (v->ball == CTR_QUICK_BALL_NONE)
        return &sBRow3[i == 1 ? 0 : i == 2 ? 1 : 2];
    return &sBRow4[i == 1 ? 0 : i == CTR_ACTION_BALL ? 1 : i == 2 ? 2 : 3];
}

/* The quick ball's arrows: the lower corners of its button, where a finger reaches them apart from the throw. */
static CtrPokeTouchRect BBallArrow(const CtrBattleView *v, int next)
{
    const CtrPokeTouchRect *r = BActRect(v, CTR_ACTION_BALL);
    CtrPokeTouchRect a = {(int16_t)(next ? r->x + r->w - 3 - BACT_ARROW_W : r->x + 3), (int16_t)(r->y + BACT_PILL_Y - 2),
                          BACT_ARROW_W, BACT_ARROW_H};
    return a;
}
/* The move cards (and a double battle's target cards) in the controller's 2x2 order, 150 x 53 (the mockup's 50 and the lip); under them
 * the text window and CANCEL. */
static const CtrPokeTouchRect sBCard[4] = {{8, 36, 150, 53}, {162, 36, 150, 53}, {8, 93, 150, 53}, {162, 93, 150, 53}};
#define BL_CARD_H 53
static const CtrPokeTouchRect sBMNote = {7, 153, 228, 80};
static const CtrPokeTouchRect sBCancel = {240, 154, 72, 78};
/* The Safari Zone's four actions in the controller's 2x2 order (BALL, BAIT, ROCK, RUN). */
static const CtrPokeTouchRect sBSafari[4] = {{8, 38, 150, 92}, {162, 38, 150, 92}, {8, 140, 150, 92}, {162, 140, 150, 92}};
/* The battle Bag: its title (the Items pocket, the only one it shows) and BERRIES for the Berry Pouch, the item list
 * on the left with its scroll arrows, the selected item's card on the right, the description's note and CANCEL. */
static const CtrPokeTouchRect sBTitle = {6, 4, 204, 26}, sBTitleWide = {6, 4, 308, 26}, sBBerries = {214, 4, 100, 26};
static const int sBTabPocket[3] = {0, 2, 1};            /* FireRed's pockets in the order the tabs once showed them */
static const CtrPokeTouchRect sBList = {6, 36, 190, 138};
static const CtrPokeTouchRect sBDetail = {200, 36, 114, 138};
static const CtrPokeTouchRect sBUse = {210, 136, 94, 30};
static const CtrPokeTouchRect sBUpArrow = {176, 36, 20, 66}, sBDownArrow = {176, 108, 20, 66};
/* The Bag's text window and CANCEL (the moves' are above). */
static const CtrPokeTouchRect sBNote = {4, 176, 236, 60};
static const CtrPokeTouchRect sBBagCancel = {244, 180, 72, 52};
#define B_DEPTH 3

static CtrPokeTouchRect BRow(int i)
{
    CtrPokeTouchRect r = {6, (int16_t)(36 + 28 * i), 166, 26};
    return r;
}

static CtrPokeTouchRect BMenuRow(int i)
{
    CtrPokeTouchRect r = {208, (int16_t)(84 + 30 * i), 98, 27};
    return r;
}
/* The Party (in battle and on the field): the six Pokemon as sky-blue cards in two columns of three on the embers (the mockup's 150 x 48
 * and the lip), and under them the text window with what the Party says and CANCEL. */
static const CtrPokeTouchRect sBPNote = {7, 169, 228, 65};
static const CtrPokeTouchRect sBPCancel = {240, 170, 72, 63};
/* Picking several Pokemon: CONFIRM over a shorter CANCEL. */
static const CtrPokeTouchRect sBPConfirm = {240, 170, 72, 30}, sBPCancelLow = {240, 203, 72, 30};
#define BP_DEPTH 3
#define BP_CARD_H 51

/* A slot's card: two columns of three, the slots reading left to right and down, as the Party's D-pad walks them (the
 * patched UpdatePartySelectionGrid). Vertically the cards are 2 px apart: the select ring of one fills the gap. */
static CtrPokeTouchRect BPanel(const CtrBattleParty *p, int i)
{
    CtrPokeTouchRect r = {(int16_t)(i & 1 ? 162 : 8), (int16_t)(6 + 53 * (i >> 1)), 150, BP_CARD_H};

    (void)p;
    return r;
}

/* A row of the Party's menu: one column of up to four, two columns beyond. */
static CtrPokeTouchRect BPMenuRow(int count, int i)
{
    int cols = count > 4 ? 2 : 1, rows = (count + cols - 1) / cols, col = cols == 2 ? i / rows : 0, row = cols == 2 ? i % rows : i;
    CtrPokeTouchRect r = {(int16_t)(cols == 2 ? 44 + 118 * col : 100), (int16_t)(88 - 15 * rows + 30 * row),
                          (int16_t)(cols == 2 ? 112 : 120), 27};
    return r;
}

static int BIn(const CtrPokeTouchRect *r, int x, int y)
{
    return UiIn(r, x, y);
}

int CtrBattleUi_TabPocket(int tab)
{
    return tab >= 0 && tab < 3 ? sBTabPocket[tab] : -1;
}


/* The row of the Bag's selected item when it is on screen (USE stands for it), else -1. */
static int BBagSelRow(const CtrBattleBag *b)
{
    int i = b->cursor - b->first;

    return i >= 0 && i < BUI_BAG_ROWS && b->row[i].item ? i : -1;
}

int CtrBattleUi_Hit(const CtrBattleView *v, int x, int y)
{
    if (v->state == BUI_ACTION) {
        for (int i = 0; i < 4; ++i)
            if (BIn(BActRect(v, i), x, y))
                return i == BUI_ACT_RUN && v->runBlocked ? -1 : i;
        if (v->ball != CTR_QUICK_BALL_NONE && BIn(BActRect(v, CTR_ACTION_BALL), x, y)) {
            CtrPokeTouchRect prev = BBallArrow(v, 0), next = BBallArrow(v, 1);

            /* The arrows answer a little round themselves too: the button is narrow. */
            prev = UiInset(&prev, -3);
            next = UiInset(&next, -3);
            if (v->ball == CTR_QUICK_BALL_READY && v->ballCount > 1 && BIn(&prev, x, y))
                return BUI_ACT_BALL_PREV;
            if (v->ball == CTR_QUICK_BALL_READY && v->ballCount > 1 && BIn(&next, x, y))
                return BUI_ACT_BALL_NEXT;
            return BUI_ACT_BALL;
        }
    }
    if (v->state == BUI_MOVE || v->state == BUI_TARGET) {
        for (int i = 0; i < 4; ++i)
            if ((v->state == BUI_MOVE ? v->move[i].present : v->target[i].present) && BIn(&sBCard[i], x, y))
                return i;
        if (BIn(&sBCancel, x, y))
            return BUI_CANCEL;
        return -1;
    }
    if (v->state == BUI_SAFARI) {
        for (int i = 0; i < 4; ++i)
            if (BIn(&sBSafari[i], x, y))
                return i;
        return -1;
    }
    if (v->state == BUI_PARTY) {
        const CtrBattleParty *p = &v->pty;

        if (p->panel == BUI_PTY_TEXT || p->panel == BUI_PTY_LEVELUP)
            return BUI_PTY_MESSAGE;                         /* anywhere: the message waits for A */
        if (p->panel == BUI_PTY_ACTIONS || p->panel == BUI_PTY_YESNO) {
            int n = p->menuCount > 8 ? 8 : p->menuCount;

            for (int i = 0; i < n; ++i) {
                CtrPokeTouchRect r = BPMenuRow(n, i);
                if (BIn(&r, x, y))
                    return BUI_PTY_MENU + i;
            }
            return -1;
        }
        if (p->mode == BUI_PTY_MULTI && BIn(&sBPConfirm, x, y))
            return BUI_PTY_CONFIRM;
        if (p->mode != BUI_PTY_FORCED && BIn(p->mode == BUI_PTY_MULTI ? &sBPCancelLow : &sBPCancel, x, y))
            return BUI_PTY_CANCEL;
        for (int i = 0; i < 6; ++i) {
            CtrPokeTouchRect r = BPanel(p, i);
            if (p->present[i] && BIn(&r, x, y))
                return BUI_PTY_SLOT + i;
        }
        return -1;
    }
    if (v->state == BUI_BAG) {
        const CtrBattleBag *b = &v->bag;

        if (b->message[0])
            return BUI_BAG_MESSAGE;                         /* anywhere: the message waits for A */
        if (BIn(&sBBagCancel, x, y))
            return BUI_BAG_CANCEL;
        if (b->berries && BIn(&sBBerries, x, y))
            return BUI_BAG_BERRIES;
        if (b->menuCount > 0 && BIn(&sBDetail, x, y)) {
            for (int i = 0; i < b->menuCount && i < 3; ++i) {
                CtrPokeTouchRect r = BMenuRow(i);
                if (BIn(&r, x, y))
                    return BUI_BAG_MENU + i;
            }
            return -1;
        }
        if (b->menuCount == 0 && BBagSelRow(b) >= 0 && BIn(&sBUse, x, y))
            return BUI_BAG_ROW + BBagSelRow(b);             /* USE: the selected row again, which uses it */
        if (b->count > BUI_BAG_ROWS && BIn(&sBUpArrow, x, y) && b->first > 0)
            return BUI_BAG_UP;
        if (b->count > BUI_BAG_ROWS && BIn(&sBDownArrow, x, y) && b->first + BUI_BAG_ROWS < b->count)
            return BUI_BAG_DOWN;
        for (int i = 0; i < BUI_BAG_ROWS; ++i) {
            CtrPokeTouchRect r = BRow(i);
            if (b->row[i].item && BIn(&r, x, y))
                return BUI_BAG_ROW + i;
        }
    }
    return -1;
}

/* Whether button or card `i` looks different between two views of the same screen: pressed, or focused. */
static int BLooksChanged(const CtrBattleView *a, const CtrBattleView *b, int i)
{
    return (a->pressed == i) != (b->pressed == i)
        || (a->showCursor && a->cursor == i) != (b->showCursor && b->cursor == i)
        || (b->state == BUI_ACTION && i == 0 && a->activeFrame != b->activeFrame);   /* the lead's icon bounces */
}

/* Running text wrapped to `width` pixels, `maxLines` at most (FireRed's line breaks are spaces here): the last line ends in "..." when
 * there is more (Em2Wrap). */
static void BDescribeSh(const char *text, int x, int y, int width, int maxLines, uint16_t color, uint16_t shadow)
{
    Em2Wrap(text, x, y, width, maxLines, UI_LINE_H, color, shadow);
}

static void BDescribeN(const char *text, int x, int y, int width, int maxLines, uint16_t color)
{
    BDescribeSh(text, x, y, width, maxLines, color, UIS_TEXT_SH);
}


/* The PP colour FireRed's move list uses: the usual ink, then yellow at half, orange at a quarter, red at none. */
static uint16_t BPpColor(unsigned pp, unsigned max)
{
    return pp == 0 ? UI_HP_LOW : pp * 4 <= max ? UI_WARN : pp * 2 <= max ? UI_HP_MID : UI_TEXT;
}


/* The battle Party: a header, the six Pokemon in two columns of three, CANCEL. */
extern int CtrBottomParty_Icon(int slot, unsigned char *tilesOut, unsigned short *palOut);
/* Party icons: FireRed's 32x32 icon of a slot with both its frames (the second under the first), decoded once per
 * species and slot. */
static struct {
    unsigned icon;
    int ok;
    uint8_t pixels[32 * 64];
    uint16_t pal[16];
} sBPIcon[6];

static int BPartyIcon(int slot, unsigned icon)
{
    static unsigned char tiles[1024];
    static unsigned short pal[16];

    if (sBPIcon[slot].icon == icon + 1)
        return sBPIcon[slot].ok;
    memset(tiles, 0, sizeof(tiles));
    sBPIcon[slot].icon = icon + 1;
    sBPIcon[slot].ok = CtrBottomParty_Icon(slot, tiles, pal);
    if (!sBPIcon[slot].ok)
        return 0;
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 32; ++x) {
            unsigned byte = tiles[(y / 8 * 4 + x / 8) * 32 + y % 8 * 4 + x % 8 / 2];
            sBPIcon[slot].pixels[y * 32 + x] = (uint8_t)((byte >> (x & 1) * 4) & 15);
        }
    for (int i = 0; i < 16; ++i) {
        unsigned c = pal[i];
        sBPIcon[slot].pal[i] = GbaColor(c);
    }
    return 1;
}

/* Text wrapped into `maxLines` lines of `width`, in `ink` over `shadow`, 16 apart; the last ends in "..." when there is more. */
static void BPWrap(const char *text, int x, int y, int width, int maxLines, uint16_t ink, uint16_t shadow)
{
    Em2Wrap(text, x, y, width, maxLines, 16, ink, shadow);
}

/* The selected slot's icon hops with its frames, as FireRed's Party does: up on the first, down on the second. */
static int BPartyBounce(const CtrBattleParty *p, int i)
{
    return p->cursor == i ? (p->iconFrame[i] & 2 ? 1 : -3) : 0;
}

/* ---- the Llama pieces the battle screens share ---- */

/* A raised card of the battle: the backdrop back over its touch rectangle and 2 px round it, then its select ring while the controller's
 * cursor is on it (the mockup's .c.sel) or else, when `shadow`, the shade under its lip; `h` is the height drawn (the 3 px lip included, at
 * most the rectangle's). Returns where the face starts: a pressed card sinks onto its lip. */
static int BLCard(const CtrPokeTouchRect *r, int h, const EmPal *p, int focused, int pressed, int shadow)
{
    TsBgRect(r->x - 2, r->y - 2, r->w + 4, r->h + 4);
    if (focused)
        LlSelectRing(r->x, r->y, r->w, h);
    else if (shadow && !pressed)
        Em2CardShadow(r->x, r->y, r->w, h);
    return EmButton(r->x, r->y, r->w, h, p, B_DEPTH, pressed);
}

/* The faint ring of light behind a card's art (the mockup's border of 8 or 10 px in white at 10 to 14 percent, a circle that the card clips): over
 * the card's face (x, y, w, h, outline included) the pixels whose centre is between rIn and rOut from (cx, cy), `a`/255 of white. */
static void BRingDecor(int x, int y, int w, int h, int cx, int cy, int rIn, int rOut, int a)
{
    int x0 = cx - rOut > x + 1 ? cx - rOut : x + 1, x1 = cx + rOut < x + w - 1 ? cx + rOut : x + w - 1;
    int y0 = cy - rOut > y + 1 ? cy - rOut : y + 1, y1 = cy + rOut < y + h - 1 ? cy + rOut : y + h - 1;
    int in2 = 4 * rIn * rIn, out2 = 4 * rOut * rOut;

    for (int py = y0; py < y1; ++py) {
        int dy = 2 * py + 1 - 2 * cy, row = py - y, cutY = LlCut(row, h);

        for (int px = x0; px < x1; ++px) {
            int dx = 2 * px + 1 - 2 * cx, d2 = dx * dx + dy * dy, col = px - x;

            if (d2 < in2 || d2 >= out2 || col < 1 + cutY || col > w - 2 - cutY)
                continue;
            Px(px, py, Em2Lerp(TsGet(px, py), EM_WHITE, a, 255));
        }
    }
}

/* A slot with nothing in it: a hollow in the backdrop (the mockup's rgba(20,0,0,.35) with a pale hairline and the shade of its top edge), `word`
 * dim in the middle when there is one. */
static void BHollow(const CtrPokeTouchRect *r, int h, const char *word)
{
    TsBgRect(r->x - 2, r->y - 2, r->w + 4, r->h + 4);
    for (int row = 0; row < h; ++row) {
        int cut = LlCut(row, h);

        for (int col = cut; col < r->w - cut; ++col) {
            int cx = r->x + col, cy = r->y + row;
            uint16_t c = UiMix(TsGet(cx, cy), UI_RGB(20, 0, 0), 3);

            if (row < 2)
                c = UiMix(c, UI_RGB(0, 0, 0), 2);
            if (row == 0 || row == h - 1 || col == cut || col == r->w - cut - 1)
                c = UiMix(c, UI_RGB(255, 200, 180), 1);
            Px(cx, cy, c);
        }
    }
    if (word)
        UiText(word, r->x + (r->w - UiTextWidth(word, 1)) / 2, r->y + (h - UI_TEXT_H) / 2, UI_RGB(176, 128, 120), UI_RGB(52, 12, 12), 1, r->x + r->w - 4);
}

static const char *const sBPartyTagText[5] = {"", CTR_TR("Able", "Puede", "Kann"), CTR_TR("Not able", "No puede", "Nein"), CTR_TR("Learned", "Aprendido", "Kennt Att."), CTR_TR("No use", "No sirve", "Nutzlos")};

/* The width of the card's verdict badge (0: none), so the status badge beside it leaves it room. */
static int BPartyTagWidth(const CtrBattleParty *p, int i)
{
    int tag = p->tag[i];

    if (tag <= BUI_PTY_TAG_NONE || tag > BUI_PTY_TAG_NO_USE || p->egg[i] || p->mode == BUI_PTY_MULTI)
        return 0;
    return UiTextWidth(sBPartyTagText[tag], 1) + 12;
}

/* The card's verdict on the item or move being taught, as a badge at the right of the row where its HP numbers would be (cut with "..." where it
 * would run into the status badge, which ends at `left`). Returns whether there is one. */
static int BPartyTagBadge(const CtrBattleParty *p, int i, const CtrPokeTouchRect *r, int y, int left)
{
    static const uint16_t tint[5] = {0, UI_RGB(56, 168, 88), UI_RGB(112, 112, 128), UI_RGB(70, 132, 214), UI_RGB(112, 112, 128)};
    char text[24];
    int tag = p->tag[i], w = BPartyTagWidth(p, i), room = r->x + 144 - left;

    if (w == 0)
        return 0;
    Em2Fit(sBPartyTagText[tag], (room < w ? room : w) - 12, text, sizeof(text));
    w = UiTextWidth(text, 1) + 12;
    LlTypeBadge(r->x + 144 - w + 1, y, text, tint[tag]);
    return 1;
}

/* Whether the card holds a Pokemon that cannot be chosen now (a link battle's pick that is not able, or the item or move's verdict). */
static int BPartyBarred(const CtrBattleParty *p, int i)
{
    return (p->mode == BUI_PTY_MULTI && p->entry[i] == BUI_PTY_NOT_ABLE) || p->tag[i] == BUI_PTY_TAG_NOT_ABLE || p->tag[i] == BUI_PTY_TAG_NO_USE;
}

/* A Party card: a sky-blue card (red when the Pokemon fainted, slate when it cannot be chosen) with the icon on its ringed plate, name and level,
 * the HP capsule, a status badge and the numbers, a "1st" ribbon on the first and FireRed's ring of light behind the plate. An empty slot is a hollow. */
static void BPartyPanel(int i, const CtrBattleView *v)
{
    static const char *const code[7] = {"", CTR_TR("Psn", "Env", "Gft"), CTR_TR("Par", "Par", "Par"), CTR_TR("Slp", "Dor", "Slf"), CTR_TR("Frz", "Con", "Efr"), CTR_TR("Brn", "Que", "Brn"), CTR_TR("PKRS", "PKRS", "PKRS")};
    static const uint16_t tint[7] = {0, UI_RGB(170, 80, 190), UI_RGB(216, 168, 24), UI_RGB(120, 128, 150),
                                     UI_RGB(80, 170, 220), UI_RGB(226, 96, 48), UI_RGB(200, 90, 160)};
    const CtrBattleParty *p = &v->pty;
    CtrPokeTouchRect r = BPanel(p, i);
    int fainted = p->present[i] && !p->egg[i] && p->hp[i] == 0, selected = p->cursor == i;
    const EmPal *pal = fainted ? &kEmPartyRed : BPartyBarred(p, i) ? &kEmSlate : &kEmPartyBlue;
    int fy, fh = BP_CARD_H - B_DEPTH, y0, right = r.x + 144, levelLeft = right;
    char text[24];

    if (!p->present[i]) {
        BHollow(&r, BP_CARD_H, CTR_TR("- empty -", "- vac\xC3\xADo -", "- leer -"));
        return;
    }
    fy = BLCard(&r, BP_CARD_H, pal, selected, v->pressed == BUI_PTY_SLOT + i, 0);
    BRingDecor(r.x, fy, r.w, fh, r.x + 20, fy + 18, 30, 38, 36);
    LlPlate(r.x + 5, fy + 4, 40, pal->inkSh, UI_RGB(232, 248, 255), 1);
    y0 = fy + 7 + BPartyBounce(p, i);
    if (BPartyIcon(i, p->icon[i]))
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                uint8_t index = sBPIcon[i].pixels[(y + (p->iconFrame[i] & 1) * 32) * 32 + x];

                if (index)
                    Px(r.x + 9 + x, y0 + y, fainted ? Tone(sBPIcon[i].pal[index], 0) : sBPIcon[i].pal[index]);
            }
    if (i == 0 && p->mode != BUI_PTY_MULTI)                   /* the first, as FireRed's Party draws it big: its ribbon over the plate's foot */
        LlRibbonBox(r.x + 1, fy + 36, UiTextWidth(CTR_TR("1st", "1\xC2\xBA", "1."), 1) + 18, 11, 7, LL_BL, LL_RIB_TOP, LL_RIB_BOT, CTR_TR("1st", "1\xC2\xBA", "1."));
    if (!p->egg[i]) {
        snprintf(text, sizeof(text), CTR_TR("Lv%u", "Nv%u", "Lv%u"), p->level[i]);
        levelLeft = right - UiTextWidth(text, 1);
        UiTextRight(text, right, fy + 5, EM_WHITE, pal->inkSh, 1);
    }
    Em2Text(p->name[i], r.x + 52, fy + 5, EM_WHITE, pal->inkSh, levelLeft - 4);
    if (p->egg[i])
        return;
    LlHpBar(r.x + 52, fy + 19, 92, p->hp[i], p->maxHp[i]);
    {
        int badgeRight = r.x + 52;

        if (fainted || (p->status[i] >= 1 && p->status[i] <= 6)) {
            const char *name = fainted ? CTR_TR("Fnt", "Deb", "Bes") : code[p->status[i]];

            /* The verdict badge is the more important of the two: the condition's badge makes way where both would not fit. */
            if (52 + UiTextWidth(name, 1) + 12 + 4 + BPartyTagWidth(p, i) <= 145 || !BPartyTagWidth(p, i))
                badgeRight += LlTypeBadge(r.x + 52, fy + 32, name, fainted ? UI_RGB(192, 48, 40) : tint[p->status[i]]);
        }
        if (!BPartyTagBadge(p, i, &r, fy + 32, badgeRight + 4)) {
            snprintf(text, sizeof(text), "%u/%u", p->hp[i], p->maxHp[i]);
            UiTextRight(text, right, fy + 34, EM_WHITE, pal->inkSh, 1);
        }
    }
}

/* The select ring of the card under the Party's cursor, drawn again after another card's own redraw: the cards are only 2 px apart down the
 * columns, so the backdrop a card restores round itself takes the neighbour's ring with it. */
static void BPartyKeepRing(const CtrBattleView *v, int i)
{
    const CtrBattleParty *p = &v->pty;

    if (p->cursor >= 0 && p->cursor < 6 && p->cursor != i && p->present[p->cursor]) {
        CtrPokeTouchRect r = BPanel(p, p->cursor);

        LlSelectRing(r.x, r.y, r.w, BP_CARD_H);
    }
}

/* Picking several Pokemon: a slot's place in the order (1-3) on a flame disc at the foot of its icon plate. */
static void BPartyEntry(int i, const CtrBattleView *v)
{
    const CtrBattleParty *p = &v->pty;
    CtrPokeTouchRect r = BPanel(p, i);
    int fy = r.y + (v->pressed == BUI_PTY_SLOT + i ? BP_DEPTH : 0), place = p->entry[i] - BUI_PTY_FIRST;
    int cx = r.x + 11, cy = fy + 37;
    char digit[2] = {(char)('1' + place), 0};

    if (p->mode != BUI_PTY_MULTI || !p->present[i] || p->entry[i] < BUI_PTY_FIRST)
        return;
    UiDisc(cx, cy, 8, kEmFlame.edge);
    UiDisc(cx, cy, 7, kEmFlame.lip);
    UiDisc(cx, cy - 1, 6, kEmFlame.top);
    UiText(digit, cx - UiTextWidth(digit, 1) / 2, cy - 5, EM_WHITE, kEmFlame.inkSh, 1, cx + 8);
}

/* The round back arrow, a word under it: the label of a tall CANCEL (a slate button whose face starts at `fy`: the disc `discY` and the word
 * `wordY` below it). */
static void BBackLabel(const CtrPokeTouchRect *r, int fy, int discY, int wordY)
{
    LlBackDisc(r->x + (r->w - 24) / 2, fy + discY);
    EmLabel(CTR_TR("Cancel", "Cancelar", "Zur\xC3\xBC" "ck"), r->x, r->w, fy + wordY, UI_TEXT_H, &kEmSlate);
}

/* CANCEL: slate with its back disc, as BACK. Picking several, the shorter one under CONFIRM (flame), the select ring on the one the Party's
 * cursor is on. */
static void BPCancelButton(const CtrBattleView *v)
{
    int multi = v->pty.mode == BUI_PTY_MULTI, fy;
    const CtrPokeTouchRect *r = multi ? &sBPCancelLow : &sBPCancel;

    TsBgRect(sBPCancel.x - 2, sBPCancel.y - 2, sBPCancel.w + 4, sBPCancel.h + 4);
    if (multi) {
        if (v->pty.button == 0)
            LlSelectRing(sBPConfirm.x, sBPConfirm.y, sBPConfirm.w, sBPConfirm.h);
        if (v->pty.button == 1)
            LlSelectRing(r->x, r->y, r->w, r->h);
        fy = EmButton(sBPConfirm.x, sBPConfirm.y, sBPConfirm.w, sBPConfirm.h, &kEmFlame, BP_DEPTH, v->pressed == BUI_PTY_CONFIRM);
        EmLabel(CtrUiText(UIS_PTY_CONFIRM), sBPConfirm.x, sBPConfirm.w, fy, sBPConfirm.h - BP_DEPTH, &kEmFlame);
        fy = EmButton(r->x, r->y, r->w, r->h, &kEmSlate, BP_DEPTH, v->pressed == BUI_PTY_CANCEL);
        EmLabel(CTR_TR("Cancel", "Cancelar", "Zur\xC3\xBC" "ck"), r->x, r->w, fy, r->h - BP_DEPTH, &kEmSlate);
        return;
    }
    Em2CardShadow(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, &kEmSlate, BP_DEPTH, v->pressed == BUI_PTY_CANCEL);
    BBackLabel(r, fy, 7, 37);
}

/* The text window of the mockup under the cards: what the Party says (its question or hint), or, with no CANCEL, that one must be sent out, a
 * hairline and, dim, how to answer. */
static void BPNoteCard(const CtrBattleView *v)
{
    const CtrPokeTouchRect *n = &sBPNote;
    const char *text = v->pty.caption[0] ? v->pty.caption
                     : v->pty.mode == BUI_PTY_FORCED ? CTR_TR("A Pok\xC3\xA9mon must be sent out.", "Tienes que sacar un Pok\xC3\xA9mon.", "Ein Pok\xC3\xA9mon muss antreten.")
                     : v->pty.mode == BUI_PTY_TARGET ? CTR_TR("Use on which one?", "\xC2\xBF" "En cu\xC3\xA1l lo usas?", "Auf welches anwenden?") : CTR_TR("Choose a Pok\xC3\xA9mon.", "Elige un Pok\xC3\xA9mon.", "W\xC3\xA4hle ein Pok\xC3\xA9mon.");

    TsBgRect(n->x - 2, n->y - 2, n->w + 4, n->h + 4);
    LlWindow(n->x, n->y, n->w, n->h);
    Em2Text(text, n->x + 13, n->y + 11, EM_DARK, EM_DARK_SH, n->x + n->w - 14);
    LlRule(n->x + 13, n->y + 26, n->w - 26);
    Em2Wrap(CTR_TR("Tap a card or use the D-pad and A.", "Toca una tarjeta o usa la cruceta y A.", "Tippe eine Karte an oder nutze Kreuz und A."),
            n->x + 13, n->y + 31, n->w - 26, 2, 14, EM_DIM, EM_DARK_SH);
}

/* A message in FireRed's text window over the dimmed screen, with "tap to continue" and its arrow (the Party's and the Bag's). */
static void BMessageWindow(const char *message)
{
    CtrPokeTouchRect all = {0, 0, W, H}, box = {20, 70, 280, 76};
    const char *tap = CTR_TR("Tap to continue", "Toca para seguir", "Tippen zum Fortfahren");

    UiDim(&all);
    LlWindow(box.x, box.y, box.w, box.h);
    Em2Wrap(message, box.x + 14, box.y + 11, box.w - 28, 3, 14, EM_DARK, EM_DARK_SH);
    UiText(tap, box.x + box.w - 30 - UiTextWidth(tap, 1), box.y + box.h - 19, EM_DIM, EM_DARK_SH, 1, box.x + box.w - 18);
    LlArrow(box.x + box.w - 26, box.y + box.h - 18, 8, 5, 1, LL_CURSOR);
}

/* What the Party has open over its slots, over the dimmed Party: its menu (the selected row flame with the select ring, the last slate), or
 * the message it waits on in the text window. */
static void BPartyOverlay(const CtrBattleView *v)
{
    const CtrBattleParty *p = &v->pty;
    CtrPokeTouchRect all = {0, 0, W, H};

    if (p->panel == BUI_PTY_TEXT) {
        BMessageWindow(p->message);
    } else if (p->panel == BUI_PTY_ACTIONS || p->panel == BUI_PTY_YESNO) {
        int n = p->menuCount > 8 ? 8 : p->menuCount;

        UiDim(&all);
        for (int i = 0; i < n; ++i) {
            CtrPokeTouchRect r = BPMenuRow(n, i);
            const EmPal *pal = p->menuCursor == i ? &kEmFlame : i == n - 1 ? &kEmSlate : &kEmCream;
            int fy;

            if (p->menuCursor == i)
                LlSelectRing(r.x, r.y, r.w, r.h);
            fy = EmButton(r.x, r.y, r.w, r.h, pal, BP_DEPTH, v->pressed == BUI_PTY_MENU + i);
            EmLabel(p->menuName[i], r.x, r.w, fy, r.h - BP_DEPTH, pal);
        }
    }
}

/* A Rare Candy's level up (BUI_PTY_LEVELUP): over the slots FireRed's text window, its flame header with the Pokemon's icon, name and
 * "LvN > LvN+1", and the six stats in sunk wells, the new value and its gain in green; the message in the text window and OK, flame, in
 * CANCEL's place. Anywhere on the screen goes on (A). */
static void BPLevelUp(const CtrBattleView *v)
{
    static const char *const sLabels[6] = {CTR_TR("Max HP", "PS m\xC3\xA1x.", "Max. KP"), CTR_TR("Attack", "Ataque", "Angriff"), CTR_TR("Defense", "Defensa", "Vert."), CTR_TR("Sp. atk", "At. esp.", "Sp. Ang."), CTR_TR("Sp. def", "Def. esp.", "Sp. Ver."), CTR_TR("Speed", "Velocidad", "Init.")};
    const CtrBattleParty *p = &v->pty;
    const EmPal *flame = &kEmFlame;
    const CtrPokeTouchRect *n = &sBPNote, *ok = &sBPCancel;
    int slot = p->lvSlot < 6 ? p->lvSlot : 0, fy, x;
    char now[8], before[8];

    LlWindow(6, 6, 308, 160);
    fy = EmButton(12, 12, 296, 36, flame, BP_DEPTH, 0);
    if (BPartyIcon(slot, p->icon[slot]))
        for (int y = 2; y < 31; ++y)
            for (int ix = 0; ix < 32; ++ix) {
                uint8_t index = sBPIcon[slot].pixels[y * 32 + ix];
                if (index)
                    Px(18 + ix, fy - 1 + y, sBPIcon[slot].pal[index]);
            }
    UiText(p->name[slot], 56, fy + (33 - UI_TEXT_H) / 2, flame->ink, flame->inkSh, 1, 170);
    snprintf(now, sizeof(now), CTR_TR("Lv%u", "Nv%u", "Lv%u"), p->lvLevel);
    snprintf(before, sizeof(before), CTR_TR("Lv%u", "Nv%u", "Lv%u"), p->lvLevel > 1 ? p->lvLevel - 1 : 1);
    x = 298 - UiTextWidth(now, 1);
    UiText(now, x, fy + (33 - UI_TEXT_H) / 2, flame->ink, flame->inkSh, 1, 304);
    LlArrow(x - 12, fy + 12, 6, 10, 3, flame->ink);
    x -= 20 + UiTextWidth(before, 1);
    UiText(before, x, fy + (33 - UI_TEXT_H) / 2, UiMix(flame->ink, flame->bottom, 3), flame->inkSh, 1, x + 40);
    for (int i = 0; i < 6; ++i) {
        int cx = 14 + (i & 1) * 150, cy = 54 + (i >> 1) * 36;
        char value[8], gain[8];

        LlWell(cx, cy, 142, 30);
        UiText(sLabels[i], cx + 8, cy + (30 - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, 1, cx + 80);
        snprintf(value, sizeof(value), "%u", p->lvStat[i]);
        UiTextRight(value, cx + 102, cy + (30 - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, 1);
        snprintf(gain, sizeof(gain), "%+d", p->lvGain[i]);
        UiTextRight(gain, cx + 134, cy + (30 - UI_TEXT_H) / 2, UI_RGB(32, 144, 72), UI_RGB(196, 236, 204), 1);
    }
    LlWindow(n->x, n->y, n->w, n->h);
    Em2Wrap(p->message, n->x + 13, n->y + 10, n->w - 26, 3, 14, EM_DARK, EM_DARK_SH);
    Em2CardShadow(ok->x, ok->y, ok->w, ok->h);
    fy = EmButton(ok->x, ok->y, ok->w, ok->h, flame, BP_DEPTH, v->pressed == BUI_PTY_MESSAGE);
    EmLabel("OK", ok->x, ok->w, fy, ok->h - BP_DEPTH, flame);
}

static void BParty(const CtrBattleView *v)
{
    TsBackdrop();
    if (v->pty.panel == BUI_PTY_LEVELUP) {
        BPLevelUp(v);
        return;
    }
    for (int i = 0; i < 6; ++i) {
        BPartyPanel(i, v);
        BPartyEntry(i, v);
        BPartyKeepRing(v, i);
    }
    BPNoteCard(v);
    if (v->pty.mode != BUI_PTY_FORCED)
        BPCancelButton(v);
    BPartyOverlay(v);
}

/* Item icons: FireRed's 24x24 icon of an item, decoded once into a collision-free 12-slot FIFO cache. */
#define BICONS 12
static struct {
    unsigned item;
    int ok;
    uint8_t pixels[24 * 24];
    uint16_t pal[16];
} sBIcon[BICONS];
static int sBIconNext;

static int BItemIcon(unsigned item)
{
    static unsigned char tiles[0x200];
    static unsigned short pal[16];
    int slot = -1;

    if (item == 0)
        return -1;
    for (int i = 0; i < BICONS; ++i)
        if (sBIcon[i].item == item)
            return sBIcon[i].ok ? i : -1;
    for (int i = 0; i < BICONS; ++i)
        if (sBIcon[i].item == 0) {
            slot = i;
            break;
        }
    if (slot < 0) {
        slot = sBIconNext;
        sBIconNext = (sBIconNext + 1) % BICONS;
    }
    memset(tiles, 0, sizeof(tiles));
    sBIcon[slot].item = item;
    sBIcon[slot].ok = CtrPokeTouch_ItemIcon(item, tiles, pal);
    if (!sBIcon[slot].ok)
        return -1;
    for (int y = 0; y < 24; ++y)
        for (int x = 0; x < 24; ++x) {
            unsigned byte = tiles[(y / 8 * 3 + x / 8) * 32 + y % 8 * 4 + x % 8 / 2];
            sBIcon[slot].pixels[y * 24 + x] = (uint8_t)((byte >> (x & 1) * 4) & 15);
        }
    for (int i = 0; i < 16; ++i) {
        unsigned c = pal[i];
        sBIcon[slot].pal[i] = GbaColor(c);
    }
    return slot;
}

/* An item's icon at `scale` times its size, its top-left corner at (x, y). */
static void BItemDraw(unsigned item, int x, int y, int scale)
{
    int slot = BItemIcon(item);

    if (slot >= 0)
        UiImage(sBIcon[slot].pixels, sBIcon[slot].pal, 24, 24, 24, x, y, scale, 1);
}


/* The ember surface: PokeTouch's red theme with the shadows and outlines of the light pieces (the Summary's too). */
static UiSurface sUiEmber;

static void EmSurface(void)
{
    sUiEmber = sUiRed;
    sUiEmber.textSh = EM_DARK_SH;
    sUiEmber.textDim = EM_DIM;
    sUiEmber.outline = UI_RGB(64, 72, 104);
    sUiEmber.track = UI_RGB(64, 72, 104);
    sUiS = &sUiEmber;
}

/* The action menu's other colours (BAG blue, POKeMON green, RUN purple, the quick ball's gold) are the skin's. */

/* A pill with `text` on it, 15 pixels tall; BPillW is its width, x its left edge. */
UI_FN int BPillW(const char *text)
{
    return UiTextWidth(text, 1) + 14;
}

/* A sunk pill (the mockup's ipill): outline, a flat fill, a shade under its top edge and a light line under it. */
UI_FN void BPill(int x, int y, const char *text, uint16_t bg, uint16_t ink, uint16_t shadow)
{
    int w = BPillW(text);

    UiRoundFill(x, y, w, 15, 5, shadow);
    UiRoundFill(x + 1, y + 1, w - 2, 13, 4, bg);
    Fill(x + 3, y + 1, w - 6, 1, UiMix(bg, UI_RGB(0, 0, 0), 3));
    for (int cx = x + 4; cx < x + w - 4; ++cx)
        EmBlend(cx, y + 15, EM_WHITE, 56);
    UiText(text, x + 7, y + 3, ink, shadow, 1, x + w);
}

/* The menu's icons: FIGHT, the Poke Ball and the shoe drawn flat by tools/make_battle_icons.py, the game's own bag
 * from the user's pack (reduced to fit its disc), BAIT and ROCK the port's art (3ds_icons.h). */
#include "battle_flat_icons.h"
enum { BX_FIGHT, BX_BAG, BX_BALL, BX_SHOE, BX_BAIT, BX_ROCK };
#define BBAG_MAX 48
static struct {
    int ready, ok, w, h;
    uint16_t rgb[BBAG_MAX * BBAG_MAX];
    uint8_t a[BBAG_MAX * BBAG_MAX];
} sBBag;

/* FireRed's bag (the ITEMS frame of graphics/interface/bag_male) at most `maxH` tall, its edge blended. */
static void BBagPrepare(int maxH)
{
    static uint8_t tiles[64 * 256 / 2];
    uint8_t colors[32], px[64 * 64];
    uint16_t pal[16];
    uint32_t len = 0;
    uint8_t *packed;
    int n = 0, x0 = 64, y0 = 64, x1 = -1, y1 = -1, bw, bh, tw, th;
    float step;

    if (sBBag.ready)
        return;
    sBBag.ready = 1;
    packed = CtrData_Load("graphics/interface/bag_male.4bpp.lz", &len);
    if (packed) {
        n = PokeTouch_Lz77(packed, len, tiles, sizeof(tiles));
        free(packed);
    }
    if (n < 64 * 64 / 2 || PokeTouch_LoadLz("graphics/interface/bag.gbapal.lz", colors, sizeof(colors)) != 32)
        return;
    for (int i = 1; i < 16; ++i) {
        unsigned c = (unsigned)colors[i * 2] | (unsigned)colors[i * 2 + 1] << 8;
        pal[i] = GbaColor(c);
    }
    for (int y = 0; y < 64; ++y)
        for (int x = 0; x < 64; ++x) {
            uint8_t b = tiles[(y / 8 * 8 + x / 8) * 32 + y % 8 * 4 + x % 8 / 2], v = (x & 1) ? b >> 4 : b & 15;

            px[y * 64 + x] = v;
            if (v) {
                x0 = x < x0 ? x : x0; x1 = x > x1 ? x : x1;
                y0 = y < y0 ? y : y0; y1 = y > y1 ? y : y1;
            }
        }
    if (x1 < 0)
        return;
    bw = x1 - x0 + 1;
    bh = y1 - y0 + 1;
    step = bh > maxH ? (float)bh / maxH : 1.0f;
    tw = (int)(bw / step + 0.5f);
    th = (int)(bh / step + 0.5f);
    if (tw > BBAG_MAX || th > BBAG_MAX)
        return;
    /* Area-averaged: each pixel the colours of the source pixels it covers (only the drawn ones), its cover as alpha. */
    for (int oy = 0; oy < th; ++oy)
        for (int ox = 0; ox < tw; ++ox) {
            float fx0 = ox * step, fx1 = fx0 + step, fy0 = oy * step, fy1 = fy0 + step, wsum = 0, wop = 0, r = 0, g = 0, b = 0;

            for (int sy = (int)fy0; sy < bh && sy < fy1; ++sy)
                for (int sx = (int)fx0; sx < bw && sx < fx1; ++sx) {
                    float wx = (sx + 1 < fx1 ? sx + 1 : fx1) - (sx > fx0 ? sx : fx0);
                    float wy = (sy + 1 < fy1 ? sy + 1 : fy1) - (sy > fy0 ? sy : fy0);
                    float w = wx * wy;
                    uint8_t v = px[(y0 + sy) * 64 + x0 + sx];

                    wsum += w;
                    if (v) {
                        wop += w;
                        r += w * (pal[v] >> 11);
                        g += w * ((pal[v] >> 5) & 63);
                        b += w * (pal[v] & 31);
                    }
                }
            sBBag.a[oy * tw + ox] = wop > 0 && wsum > 0 ? (uint8_t)(wop / wsum * 255.0f + 0.5f) : 0;
            sBBag.rgb[oy * tw + ox] = wop > 0 ? (uint16_t)((int)(r / wop + 0.5f) << 11 | (int)(g / wop + 0.5f) << 5
                                                           | (int)(b / wop + 0.5f)) : 0;
        }
    sBBag.w = tw;
    sBBag.h = th;
    sBBag.ok = 1;
}

static void BFlat(const uint16_t *px, int w, int h, int x, int y)
{
    for (int v = 0; v < h; ++v)
        for (int u = 0; u < w; ++u)
            if (px[v * w + u])
                Px(x + u, y + v, px[v * w + u]);
}

/* A light disc of the piece's colour, `r` its radius, centred at (cx, cy). */
static void BDisc(int cx, int cy, int r, const EmPal *p)
{
    /* The mockup's disc: a light rim of the piece's colour, a lighter middle and a white glint at the top (a plate, warmer, on the flame). */
    uint16_t rim = p == &kEmFlame ? UI_RGB(248, 184, 96) : p->top, mid = p == &kEmFlame ? UI_RGB(255, 216, 144) : p->g1;

    UiDisc(cx, cy, r + 1, p == &kEmFlame ? UI_RGB(184, 90, 20) : p->lip);
    UiDisc(cx, cy, r, rim);
    UiDisc(cx, cy - r / 8, r * 7 / 10 + 1, mid);
    UiDisc(cx, cy - r / 4, r / 2, p == &kEmFlame ? UI_RGB(255, 246, 220) : UiMix(p->g1, EM_WHITE, 4));
}

/* Icon `icon` (BX_*) centred at (cx, cy) on a light disc of the piece's colour, `r` its radius. */
static void BDiscIcon(int icon, int cx, int cy, int r, const EmPal *p)
{
    BDisc(cx, cy, r, p);
    if (icon == BX_FIGHT)
        BFlat(kFlatFight, FLATFIGHT_W, FLATFIGHT_H, cx - FLATFIGHT_W / 2, cy - FLATFIGHT_H / 2);
    else if (icon == BX_BALL)
        BFlat(kFlatBall, FLATBALL_W, FLATBALL_H, cx - FLATBALL_W / 2, cy - FLATBALL_H / 2);
    else if (icon == BX_SHOE)
        BFlat(kFlatShoe, FLATSHOE_W, FLATSHOE_H, cx - FLATSHOE_W / 2, cy - FLATSHOE_H / 2);
    else if (icon == BX_BAG && (BBagPrepare(2 * r - 2), sBBag.ok)) {
        for (int v = 0; v < sBBag.h; ++v)
            for (int u = 0; u < sBBag.w; ++u)
                EmBlend(cx - sBBag.w / 2 + u, cy - sBBag.h / 2 + v, sBBag.rgb[v * sBBag.w + u], sBBag.a[v * sBBag.w + u]);
    } else {
        int art = icon == BX_BAG ? BI_BAG : icon == BX_BAIT ? BI_BAIT : BI_ROCK, w, h;

        BIconsPrepare();
        w = sBImg[art].image.w * sBImg[art].scale;
        h = sBImg[art].image.h * sBImg[art].scale;
        BIconAt(art, cx - w / 2, cy - h / 2);
    }
}

/* CANCEL of the move cards, the targets and the Bag: slate (the mockup's .sl) with its round back arrow and the word under it; a tall one has the
 * arrow and the word as the mockup draws them, a short one only the word. */
static void BEmCancel(const CtrPokeTouchRect *r, int pressed)
{
    int fy;

    TsBgRect(r->x - 2, r->y - 2, r->w + 4, r->h + 4);
    if (!pressed)
        Em2CardShadow(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, &kEmSlate, B_DEPTH, pressed);
    if (r->h >= 40)
        BBackLabel(r, fy, r->h >= 70 ? 13 : 6, r->h >= 70 ? 47 : 34);
    else
        EmLabel(CTR_TR("Cancel", "Cancelar", "Zur\xC3\xBC" "ck"), r->x, r->w, fy, r->h - B_DEPTH, &kEmSlate);
}

/* A Poke Ball of the party strip, 15 pixels across: red and white for a Pokemon that can fight, grey for one that
 * fainted (kind 2), a hollow ring for no Pokemon (kind 0); an amber dot for a status condition. */
/* The colour of a condition's dot on its ball: an if-chain, not a table (the clean-release audit counts tables as game data). */
static uint16_t BStatusColor(int status)
{
    if (status == 1)
        return UI_RGB(176, 96, 208);        /* PSN */
    if (status == 2)
        return UI_RGB(248, 216, 48);        /* PAR */
    if (status == 3)
        return UI_RGB(170, 180, 200);       /* SLP */
    if (status == 4)
        return UI_RGB(120, 220, 240);       /* FRZ */
    if (status == 5)
        return UI_RGB(240, 96, 40);         /* BRN */
    if (status == 6)
        return UI_RGB(248, 140, 190);       /* PKRS */
    return UI_RGB(255, 200, 60);
}

static void BBall(int cx, int cy, int kind, int status)
{
    for (int dy = -7; dy <= 7; ++dy)
        for (int dx = -7; dx <= 7; ++dx) {
            int d2 = dx * dx + dy * dy;
            uint16_t c;

            if (d2 > 52)
                continue;
            if (kind == 0) {
                if (d2 >= 36)
                    Px(cx + dx, cy + dy, UI_RGB(120, 56, 52));
                continue;
            }
            if (d2 >= 40 || dy == 0)
                c = UI_RGB(40, 24, 28);
            else if (d2 <= 5)
                c = d2 <= 2 ? EM_WHITE : UI_RGB(40, 24, 28);
            else if (dy < 0)
                c = kind == 2 ? UI_RGB(150, 146, 156) : dx < -1 && dy < -3 ? UI_RGB(255, 140, 120) : UI_RGB(230, 64, 52);
            else
                c = kind == 2 ? UI_RGB(196, 192, 200) : UI_RGB(244, 242, 248);
            Px(cx + dx, cy + dy, c);
        }
    if (kind == 2)
        for (int k = -5; k <= 5; ++k) {      /* fainted: a cross over the grey ball, two pixels thick */
            Px(cx + k, cy + k, UI_RGB(52, 40, 52));
            Px(cx + k + 1, cy + k, UI_RGB(52, 40, 52));
            Px(cx + k, cy - k, UI_RGB(52, 40, 52));
            Px(cx + k + 1, cy - k, UI_RGB(52, 40, 52));
        }
    if (kind && kind != 2 && status) {
        UiDisc(cx + 6, cy - 6, 3, UI_RGB(110, 40, 6));
        UiDisc(cx + 6, cy - 6, 2, BStatusColor(status));
    }
}

/* The party's six balls on their slanted tray (the mockup's .tray), a mini HP bar under each, the lead in its gold frame, an empty slot a ringed disc. */
static void BStrip(const CtrBattleView *v)
{
    LlBallTray(BL_TRAY_X, BL_TRAY_Y, 196, 25);
    for (int i = 0; i < 6; ++i) {
        int kind = v->party[i], left = BL_TRAY_X + 17 + 28 * i;

        if (kind && i == v->activeSlot)                      /* the Pokemon in the lead: a gold frame round its slot */
            LlLeadFrame(BL_TRAY_X + 13 + 28 * i, BL_TRAY_Y + 1, 22, 23);
        if (!kind) {
            LlTraySlot(left, BL_TRAY_Y + 2);
            continue;
        }
        BBall(left + 7, BL_TRAY_Y + 9, kind, kind == 3 ? (v->partyStatus[i] ? v->partyStatus[i] : 7) : v->partyStatus[i]);
        LlTrayBar(BL_TRAY_X + 15 + 28 * i, BL_TRAY_Y + 18, 18, kind == 2 ? 0 : v->partyHp[i], 100);
    }
}

/* ---- The action menu ---- */
/* A row button's name, centred on its line (the same line on every button of the row). */
static void BRowLabel(const char *s, const CtrPokeTouchRect *r, int fy, const EmPal *p)
{
    UiText(s, r->x + (r->w - UiTextWidth(s, 1)) / 2, fy + BACT_NAME_Y, p->ink, p->inkSh, 1, r->x + r->w);
}

/* The caption under a row button's name (the mockup's .sub): a sunk pill that darkens the card, the caption in white in the middle of it. */
static void BSubPill(const CtrPokeTouchRect *r, int fy, const char *text, const EmPal *p)
{
    char fit[32];

    LlSubPill(r->x + 7, fy + BL_SUB_Y, r->w - 14, 15);
    Em2Fit(text, r->w - 14 - 6, fit, sizeof(fit));
    UiText(fit, r->x + (r->w - UiTextWidth(fit, 1)) / 2, fy + BL_SUB_Y + 3, EM_WHITE, p->inkSh, 1, r->x + r->w - 8);
}

static void BBallCard(const CtrBattleView *v);

/* A hit code of the quick ball: the button or one of its arrows. */
static int BBallPart(int code)
{
    return code >= BUI_ACT_BALL && code <= BUI_ACT_BALL_NEXT;
}

/* FIGHT's face (the mockup's .fl card): the ring of light at its right, the lead on its ringed plate with its shadow, the big word, the lead's type
 * badges and FireRed's cream HP box with its name, level, HP capsule and numbers. `fy` is where the face starts. */
static void BFightFace(const CtrPokeTouchRect *r, int fy, const CtrBattleView *v, const EmPal *p)
{
    const char *word = CTR_TR("Fight", "Luchar", "Kampf");
    int left = r->x + 88, bx = r->x + 82, by = fy + 32, right = r->x + r->w - 10, x;
    char text[32];

    BRingDecor(r->x, fy, r->w, BL_FIGHT_H - B_DEPTH, r->x + 270, fy + 36, 38, 48, 26);
    LlPlate(r->x + 10, fy + 5, 63, p->lip, UI_RGB(255, 216, 144), 2);
    for (int dy = 0; dy < 6; ++dy)                          /* the Pokemon's shadow on the plate: an ellipse of warm brown at 30 percent */
        for (int dx = 0; dx < 42; ++dx) {
            int ex = 2 * dx + 1 - 42, ey = 2 * dy + 1 - 6;

            if (ex * ex * 36 + ey * ey * 1764 <= 63504)
                Px(r->x + 21 + dx, fy + 57 + dy, Em2Lerp(TsGet(r->x + 21 + dx, fy + 57 + dy), UI_RGB(140, 80, 20), 77, 255));
        }
    if (v->activeIcon && BPartyIcon(v->activeSlot, v->activeIcon)) {       /* the Pokemon's own party icon at twice its size, bouncing */
        int x0 = r->x + 10, y0 = fy + 8 - 2 * v->activeFrame;

        for (int y = 0; y < 32; ++y)
            for (int px = 0; px < 32; ++px) {
                uint8_t index = sBPIcon[v->activeSlot].pixels[y * 32 + px];

                if (index)
                    Fill(x0 + 2 * px, y0 + 2 * y, 2, 2, sBPIcon[v->activeSlot].pal[index]);
            }
    } else
        BDiscIcon(BX_FIGHT, r->x + 41, fy + 36, 24, p);
    UiText(word, left, fy + 7, EM_WHITE, p->inkSh, 2, r->x + r->w - 8);
    x = left + UiTextWidth(word, 2) + 12;
    for (int k = 0; k < 2; ++k)
        if (v->activeType[k] >= 0 && v->activeTypeName[k][0]) {
            int w = UiTextWidth(v->activeTypeName[k], 1) + 12;

            if (x + w > right)
                break;
            LlTypeBadge(x, fy + 9, v->activeTypeName[k], UiTypeColor(v->activeType[k]));
            x += w + 4;
        }
    if (!v->activeName[0])
        return;
    LlHpBox(bx, by, 216, 36);
    snprintf(text, sizeof(text), CTR_TR("Lv%u", "Nv%u", "Lv%u"), (unsigned)v->activeLevel);
    UiTextRight(text, bx + 192, by + 4, EM_DARK, EM_DARK_SH, 1);
    Em2Text(v->activeName, bx + 16, by + 4, EM_DARK, EM_DARK_SH, bx + 192 - UiTextWidth(text, 1) - 6);
    LlHpBar(bx + 13, by + 20, 136, v->activeHp, v->activeMaxHp);
    if (v->activeMaxHp) {
        snprintf(text, sizeof(text), "%u/%u", (unsigned)v->activeHp, (unsigned)v->activeMaxHp);
        UiTextRight(text, bx + 196, by + 18, EM_DARK, EM_DARK_SH, 1);
    }
}

/* Grey and still: the quick ball with nothing to throw, and RUN when there is no running. */
static const EmPal kEmBallOff = EM_PAL(UI_RGB(216, 212, 222), UI_RGB(200, 196, 206), UI_RGB(176, 172, 184), UI_RGB(150, 146, 160), UI_RGB(104, 100, 116),
                                     LL_EDGE, UI_RGB(232, 230, 238), UI_RGB(120, 116, 134), EM_WHITE, UI_RGB(60, 58, 72));

static void BActCard(int i, const CtrBattleView *v)
{
    static const EmPal *const pal[4] = {&kEmFlame, &kEmBlue, &kEmGreen, &kEmPurple};
    static const int icon[4] = {BX_FIGHT, BX_BAG, BX_BALL, BX_SHOE};
    static const char *const label[4] = {CTR_TR("Fight", "Luchar", "Kampf"), CTR_TR("Bag", "Mochila", "Beutel"), CTR_TR("Pok\xC3\xA9mon", "Pok\xC3\xA9mon", "Pok\xC3\xA9mon"), CTR_TR("Run", "Huir", "Flucht")};
    static const char *const sub[4] = {"", CTR_TR("Items", "Objetos", "Items"), CTR_TR("Party", "Equipo", "Team"), CTR_TR("Escape", "Escapar", "Fliehen")};
    const CtrPokeTouchRect *r = BActRect(v, i);
    int off = i == BUI_ACT_RUN && v->runBlocked;
    const EmPal *p = off ? &kEmBallOff : pal[i];
    int fy = BLCard(r, i == 0 ? BL_FIGHT_H : BL_ROW_H, p, v->showCursor && v->cursor == i, v->pressed == i && !off, 1);

    if (i == 0) {
        BFightFace(r, fy, v, p);
        return;
    }
    BDiscIcon(icon[i], r->x + r->w / 2, fy + BACT_DISC_Y, BACT_DISC_R, p);
    BRowLabel(label[i], r, fy, p);
    if (i == 2) {    /* POKEMON: how many are still able, not the word the card already says */
        char able[16];
        unsigned here = 0, left = 0;

        for (int k = 0; k < 6; ++k) {
            here += v->party[k] != 0;
            left += v->party[k] == 1 || v->party[k] == 3;
        }
        snprintf(able, sizeof(able), CTR_TR("%u/%u able", "%u/%u vivos", "%u/%u fit"), left, here);
        BSubPill(r, fy, able, p);
        return;
    }
    BSubPill(r, fy, off ? CTR_TR("No escape", "Sin huida", "Kein Weg") : sub[i], p);
}

/* The quick ball: grey and still when there is no ball to throw (none left, or the boxes full). */
/* Its colour: gold (kEmGold), apart from FIGHT's flame and the cursor's ring. */

/* An arrow of the quick ball: a white triangle at the end of its caption pill (the mockup's two arrows round the count), its shade under it;
 * held, it sinks a pixel and lights. The touch area is the whole corner of the button (BBallArrow). */
static void BBallArrowDraw(const CtrBattleView *v, int next, const EmPal *p, int face)
{
    const CtrPokeTouchRect *r = BActRect(v, CTR_ACTION_BALL);
    int pressed = v->pressed == (next ? BUI_ACT_BALL_NEXT : BUI_ACT_BALL_PREV);
    int x = next ? r->x + r->w - 7 - 5 - 6 : r->x + 7 + 5, y = face + BL_SUB_Y + 3 + pressed;

    LlArrow(x + 1, y + 1, 6, 10, next ? 3 : 2, p->inkSh);
    LlArrow(x, y, 6, 10, next ? 3 : 2, pressed ? p->hl : EM_WHITE);
}

/* The quick ball (a wild battle's fourth button): the ball's own icon at twice its size, its name, a dot for each of
 * the Bag's balls (the one shown lit), and in its caption pill the count between two arrows. Without a ball to throw: grey,
 * a plain Poke Ball and why. */
static void BBallCard(const CtrBattleView *v)
{
    const CtrPokeTouchRect *r = BActRect(v, CTR_ACTION_BALL);
    int ready = v->ball == CTR_QUICK_BALL_READY;
    const EmPal *p = ready ? &kEmGold : &kEmBallOff;
    int fy = BLCard(r, BL_ROW_H, p, v->showCursor && v->cursor == CTR_ACTION_BALL, ready && v->pressed == BUI_ACT_BALL, 1);
    int cx = r->x + r->w / 2;
    char qty[8];

    if (!ready) {
        const char *why = v->ball == CTR_QUICK_BALL_FULL ? CTR_TR("Box full", "Caja llena", "Box voll")
                        : v->ball == CTR_QUICK_BALL_AREA ? CTR_TR("Area used", "Zona usada", "Gebiet belegt") : CTR_TR("No balls", "Sin balls", "Kein Ball");

        BDiscIcon(BX_BALL, cx, fy + BACT_DISC_Y, BACT_DISC_R, p);
        BRowLabel(why, r, fy, p);
        /* The pill on the row's line as on the others: none left, or how many of the ball there would be. */
        snprintf(qty, sizeof(qty), "x%u", v->ball == CTR_QUICK_BALL_FULL || v->ball == CTR_QUICK_BALL_AREA ? (unsigned)v->ballQty : 0u);
        BSubPill(r, fy, qty, p);
        return;
    }
    BDisc(cx, fy + BACT_DISC_Y, BACT_DISC_R, p);
    BItemDraw(v->ballItem, cx - 24, fy + BACT_DISC_Y - 24, 2);
    {
        /* The name; " BALL" left off when the whole of it is wider than the button. */
        char name[BUI_ITEM_LEN];
        size_t n;

        snprintf(name, sizeof(name), "%s", v->ballName);
        n = strlen(name);
        /* "BALL" left off too when it is joined to the name, as in German (MEISTERBALL). */
        if (UiTextWidth(name, 1) > r->w - 8 && n > 6 && CtrTextCase_Eq(name + n - 4, "BALL"))
            name[n - (name[n - 5] == ' ' ? 5 : 4)] = 0;
        BRowLabel(name, r, fy, p);
    }
    if (v->ballCount > 1) {
        int count = v->ballCount, gap = (r->w - 16) / count > 8 ? 8 : (r->w - 16) / count;
        int x0 = cx - gap * (count - 1) / 2;

        for (int k = 0; k < count; ++k)
            UiDisc(x0 + gap * k, fy + BACT_DOTS_Y, gap >= 6 ? 2 : 1,
                   k == v->ballPlace ? EM_WHITE : UiMix(p->f1, p->lip, 4));
    }
    snprintf(qty, sizeof(qty), "x%u", (unsigned)v->ballQty);
    BSubPill(r, fy, qty, p);
    if (v->ballCount > 1) {
        BBallArrowDraw(v, 0, p, fy);
        BBallArrowDraw(v, 1, p, fy);
    }
}

/* ---- The moves ---- */
/* What the move does to the foe, on a badge: green when it is super effective, orange when not very, grey otherwise (the mockup's .ty colours). */
static const uint16_t kBFxColor[] = {0, UI_RGB(110, 100, 110), UI_RGB(232, 120, 44), UI_RGB(214, 112, 30),
                                     UI_RGB(56, 168, 88), UI_RGB(30, 130, 64), UI_RGB(100, 106, 132)};

/* The mark before the badge's words: two arrows up for super effective, one down for not very, a red
 * cross for no effect; a plain hit has none. 7 pixels tall, `x` its left edge; returns its width. */
enum { BFX_MARK_NONE, BFX_MARK_UP, BFX_MARK_DOWN, BFX_MARK_CROSS };

static int BFxMarkW(int mark)
{
    return mark == BFX_MARK_UP ? 11 : mark == BFX_MARK_DOWN ? 7 : mark == BFX_MARK_CROSS ? 7 : 0;
}

static void BFxMark(int mark, int x, int y, uint16_t ink, uint16_t shadow)
{
    for (int pass = 0; pass < 2; ++pass) {
        uint16_t c = pass ? ink : shadow;
        int sx = x + !pass, sy = y + !pass;      /* the shadow first, a pixel down and right, as the font's */

        if (mark == BFX_MARK_UP)
            for (int k = 0; k < 2; ++k)
                for (int row = 0; row < 6; ++row)
                    Fill(sx + 6 * k + 2 - row / 2, sy + 1 + row, 1 + row / 2 * 2, 1, c);
        else if (mark == BFX_MARK_DOWN)
            for (int row = 0; row < 7; ++row)
                Fill(sx + (row + 1) / 2, sy + row, 7 - (row + 1) / 2 * 2, 1, c);
        else if (mark == BFX_MARK_CROSS) {
            if (pass)
                c = UI_RGB(240, 72, 64);
            for (int k = 0; k < 6; ++k) {
                Fill(sx + k, sy + k, 2, 1, c);
                Fill(sx + 5 - k, sy + k, 2, 1, c);
            }
        }
    }
}

/* The effectiveness badge (the mockup's "x2 EFICAZ": LlTypeBadge's look with the mark before its words): at (x, y) left, or with `right` its right
 * edge at x; the words cut with "..." and the mark left out where it would be wider than `maxW`. Returns its width (0: a hit that needs none). */
static int BFxBadge(uint8_t matchup, int x, int y, int maxW, int right)
{
    static const char *const kText[] = {"", CTR_TR("No effect", "No afecta", "Wirkungslos"), CTR_TR("Not very", "Poco efic.", "Nicht sehr"), CTR_TR("Not very", "Poco efic.", "Nicht sehr"), CTR_TR("Super", "S\xC3\xBAper", "Sehr"), CTR_TR("Super x4", "S\xC3\xBAper x4", "Sehr x4"), CTR_TR("Effective", "Eficaz", "Effektiv")};
    static const uint8_t kMark[] = {BFX_MARK_NONE, BFX_MARK_CROSS, BFX_MARK_DOWN, BFX_MARK_DOWN, BFX_MARK_UP, BFX_MARK_UP, BFX_MARK_NONE};
    int mark, markW, w;
    uint16_t c, shadow;
    char text[24];

    if (matchup == CTR_FX_NONE || matchup > CTR_FX_NEUTRAL)
        return 0;
    mark = kMark[matchup];
    markW = BFxMarkW(mark);
    if (UiTextWidth(kText[matchup], 1) + 12 + (markW ? markW + 2 : 0) > maxW)
        markW = 0;                                                   /* no room for the mark */
    Em2Fit(kText[matchup], maxW - 12 - (markW ? markW + 2 : 0), text, sizeof(text));
    w = UiTextWidth(text, 1) + 12 + (markW ? markW + 2 : 0);
    if (right)
        x -= w;
    c = kBFxColor[matchup];
    shadow = Em2Lerp(c, UI_RGB(0, 0, 0), 115, 255);
    UiRoundFill(x, y, w, 13, 4, Em2Lerp(c, UI_RGB(30, 20, 40), 160, 255));
    UiRoundFill(x + 1, y + 1, w - 2, 11, 3, c);
    Fill(x + 3, y + 1, w - 6, 1, Em2Lerp(c, EM_WHITE, 115, 255));
    if (markW)
        BFxMark(mark, x + 6, y + 3, EM_WHITE, shadow);
    UiText(text, x + 6 + (markW ? markW + 2 : 0), y + 2, EM_WHITE, shadow, 1, x + w - 1);
    return w;
}

/* The band down the left of a card (the mockup's 7 px: its colour lighter at the top, darker at the bottom, the card's own corners). */
static void BColorBand(int x, int y, int h, uint16_t c)
{
    LlGradSlab(x, y, 6, h, 0, 0, LL_TL | LL_BL, Em2Lerp(c, EM_WHITE, 72, 255), Em2Lerp(c, UI_RGB(0, 0, 0), 52, 255));
}

static void BMoveCard(int i, const CtrBattleView *v)
{
    const CtrPokeTouchRect *r = &sBCard[i];
    const CtrBattleMove *m = &v->move[i];
    int focused = v->showCursor && v->cursor == i, fy, badgeW, power, accNumber;
    uint16_t c;
    char text[16];

    if (!m->present) {
        BHollow(r, BL_CARD_H - B_DEPTH, NULL);
        return;
    }
    fy = BLCard(r, BL_CARD_H, &kEmCream, focused, v->pressed == i, 1);
    c = UiTypeColor(m->type);
    badgeW = UiTextWidth(m->typeName, 1) + 12;
    power = m->power[0] && m->power[0] != '-';
    accNumber = m->accuracy[0] && m->accuracy[0] != '-';
    BColorBand(r->x + 1, fy + 1, BL_CARD_H - B_DEPTH - 2, c);
    if (focused)
        LlCursor(r->x + 11, fy + 6, LL_CURSOR);
    Em2Text(m->name, r->x + (focused ? 21 : 13), fy + 6, EM_DARK, EM_DARK_SH, r->x + r->w - 5 - badgeW - 4);
    LlTypeBadge(r->x + r->w - 5 - badgeW, fy + 5, m->typeName, c);
    if (power) {
        UiText(CTR_TR("Pow", "Pot", "St\xC3\xA4"), r->x + 13, fy + 22, EM_DIM, EM_DARK_SH, 1, r->x + 40);
        UiText(m->power, r->x + 35, fy + 22, EM_DARK, EM_DARK_SH, 1, r->x + 64);
    } else
        UiText(CTR_TR("Status", "Estado", "Status"), r->x + 13, fy + 22, EM_DIM, EM_DARK_SH, 1, r->x + 64);
    if (accNumber) {
        UiText(CTR_TR("Acc", "Pre", "Gen"), r->x + 66, fy + 22, EM_DIM, EM_DARK_SH, 1, r->x + 88);
        UiText(m->accuracy, r->x + 88, fy + 22, EM_DARK, EM_DARK_SH, 1, r->x + 120);
    }
    BFxBadge(m->matchup, r->x + 13, fy + 34, 70, 0);
    UiText("PP", r->x + 88, fy + 36, EM_DIM, EM_DARK_SH, 1, r->x + 104);
    snprintf(text, sizeof(text), "%u/%u", m->pp, m->maxPp);
    if (m->pp * 4 <= m->maxPp)       /* a quarter or less left, or none: the game's red */
        UiText(text, r->x + 104, fy + 36, UI_RGB(224, 56, 40), UI_RGB(248, 200, 184), 1, r->x + r->w - 4);
    else
        UiText(text, r->x + 104, fy + 36, EM_DARK, EM_DARK_SH, 1, r->x + r->w - 4);
}

/* The text window under the cards (and CANCEL beside it): the move under FireRed's cursor, its name, "STAB" on a slanted ribbon when it gets the
 * bonus, the chance of its extra effect, a hairline and its description. */
static void BNoteWindow(const CtrPokeTouchRect *n, const char *title, int stab, const char *chance, const char *text)
{
    int right = n->x + 215;

    TsBgRect(n->x - 2, n->y - 2, n->w + 4, n->h + 4);
    LlWindow(n->x, n->y, n->w, n->h);
    if (stab) {
        LlRibbon(n->x + 161, n->y + 9, CTR_TR("STAB", "STAB", "STAB"), LL_RIB_TOP, LL_RIB_BOT);
        right = n->x + 161 - 6;
    }
    if (chance && chance[0]) {
        UiTextRight(chance, right, n->y + 11, EM_DIM, EM_DARK_SH, 1);
        right -= UiTextWidth(chance, 1) + 8;
    }
    Em2Text(title, n->x + 13, n->y + 11, EM_DARK, EM_DARK_SH, right);
    LlRule(n->x + 13, n->y + 26, 202);
    Em2Wrap(text, n->x + 13, n->y + 33, 202, 3, 14, EM_DIM, EM_DARK_SH);
}

static void BMoveNote(const CtrBattleView *v)
{
    const CtrPokeTouchRect *n = &sBMNote;
    int sel = v->cursor >= 0 && v->cursor < 4 && v->move[v->cursor].present ? v->cursor : 0;
    const CtrBattleMove *m = &v->move[sel];
    char chance[24] = "";

    if (!m->present) {
        TsBgRect(n->x - 2, n->y - 2, n->w + 4, n->h + 4);
        LlWindow(n->x, n->y, n->w, n->h);
        return;
    }
    if (m->effectChance)
        snprintf(chance, sizeof(chance), "%s %u%%", CTR_TR("Effect", "Efecto", "Effekt"), m->effectChance);
    BNoteWindow(n, m->name, m->stab, chance,
                m->desc[0] ? m->desc : m->type < 10 ? CTR_TR("Physical move", "Ataque f\xC3\xADsico", "Physischer Angriff") : CTR_TR("Special move", "Ataque especial", "Spezialangriff"));
}

/* ---- A double battle's targets: the opposing battlers on top, the player's below, as the move cards ---- */
static void BTargetCard(int i, const CtrBattleView *v)
{
    const CtrPokeTouchRect *r = &sBCard[i];
    const CtrBattleTarget *t = &v->target[i];
    int focused = v->showCursor && v->cursor == i, fy, badgeW;
    uint16_t side = t->side ? UI_RGB(60, 120, 230) : UI_RGB(220, 60, 50);
    const char *sideWord = t->side ? CTR_TR("Ally", "Aliado", "Partner") : CTR_TR("Foe", "Rival", "Gegner");
    char text[20];

    if (!t->present) {
        BHollow(r, BL_CARD_H - B_DEPTH, NULL);
        return;
    }
    fy = BLCard(r, BL_CARD_H, &kEmCream, focused, v->pressed == i, 1);
    badgeW = UiTextWidth(sideWord, 1) + 12;
    BColorBand(r->x + 1, fy + 1, BL_CARD_H - B_DEPTH - 2, side);
    if (focused)
        LlCursor(r->x + 11, fy + 6, LL_CURSOR);
    Em2Text(t->name, r->x + (focused ? 21 : 13), fy + 6, EM_DARK, EM_DARK_SH, r->x + r->w - 5 - badgeW - 4);
    LlTypeBadge(r->x + r->w - 5 - badgeW, fy + 5, sideWord, side);
    snprintf(text, sizeof(text), CTR_TR("Lv%u", "Nv%u", "Lv%u"), t->level);
    UiText(text, r->x + 13, fy + 22, EM_DARK, EM_DARK_SH, 1, r->x + 64);
    BFxBadge(t->matchup, r->x + r->w - 5, fy + 18, 80, 1);
    LlHpBar(r->x + 13, fy + 35, 72, t->hp, t->maxHp);
    if (t->side) {
        snprintf(text, sizeof(text), "%u/%u", t->hp, t->maxHp);
        UiTextRight(text, r->x + r->w - 8, fy + 35, EM_DARK, EM_DARK_SH, 1);
    }
}

static void BTargetNote(void)
{
    BNoteWindow(&sBMNote, CTR_TR("Target", "Objetivo", "ZIEL"), 0, NULL,
                CTR_TR("Which Pok\xC3\xA9mon should the move hit?", "\xC2\xBF" "A qu\xC3\xA9 Pok\xC3\xA9mon ataca?", "Welches Pok\xC3\xA9mon angreifen?"));
}

/* ---- The Safari Zone: BALL, BAIT, ROCK, RUN (the words of FireRed's gText_SafariZoneMenu) ---- */
static void BSafariCard(int i, const CtrBattleView *v)
{
    static const EmPal *const pal[4] = {&kEmFlame, &kEmGreen, &kEmBlue, &kEmPurple};
    static const int icon[4] = {BX_BALL, BX_BAIT, BX_ROCK, BX_SHOE};
    static const char *const label[4] = {CTR_TR("Ball", "Ball", "Ball"), CTR_TR("Bait", "Cebo", "K\xC3\xB6" "der"), CTR_TR("Rock", "Roca", "Stein"), CTR_TR("Run", "Huir", "Flucht")};
    const CtrPokeTouchRect *r = &sBSafari[i];
    const EmPal *p = pal[i];
    int fy = BLCard(r, r->h, p, v->showCursor && v->cursor == i, v->pressed == i, 1);

    BDiscIcon(icon[i], r->x + r->w / 2, fy + 34, 25, p);
    UiText(label[i], r->x + (r->w - UiTextWidth(label[i], 1)) / 2, fy + 69, p->ink, p->inkSh, 1, r->x + r->w);
}

/* ---- The battle Bag ---- */
/* Its title (the items: the balls have their button on the action screen) in flame and, with the Berry Pouch, BERRIES. */
static void BTabs(const CtrBattleView *v)
{
    const CtrPokeTouchRect *t = v->bag.berries ? &sBTitle : &sBTitleWide;
    int fy = EmButton(t->x, t->y, t->w, t->h, &kEmFlame, 2, 0);

    EmLabel(CTR_TR("Items", "Objetos", "Items"), t->x, t->w, fy, t->h - 2, &kEmFlame);
    if (v->bag.berries) {
        const CtrPokeTouchRect *r = &sBBerries;

        fy = EmButton(r->x, r->y, r->w, r->h, &kEmCream, 2, v->pressed == BUI_BAG_BERRIES);
        EmLabel(CTR_TR("Berries", "Bayas", "Beeren"), r->x, r->w, fy, r->h - 2, &kEmCream);
    }
}

/* A list row: the icon, the name and (but for key items) the quantity in a sunk pill, in blue as FireRed's "x12"; the Bag's selection flame with
 * FireRed's cursor and the select ring (the rows are 2 px apart: the ring fills the gap). */
static void BBagRow(int i, const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;
    CtrPokeTouchRect r = BRow(i);
    int chosen = b->first + i == b->cursor;
    const EmPal *p = chosen ? &kEmFlame : &kEmCream;
    int fy;

    if (!b->row[i].item)
        return;
    if (chosen)
        LlSelectRing(r.x, r.y, r.w, r.h);
    fy = EmButton(r.x, r.y, r.w, r.h, p, 2, v->pressed == BUI_BAG_ROW + i);
    BItemDraw(b->row[i].item, r.x + 3, fy, 1);
    if (chosen)
        LlCursor(r.x + 29, fy + 7, EM_WHITE);
    if (b->pocket != 1) {                       /* key items are one of each: no count */
        char qty[8];

        snprintf(qty, sizeof(qty), "x%u", b->row[i].qty > 999 ? 999 : b->row[i].qty);
        LlWell(r.x + r.w - 42, fy + 4, 34, 15);
        UiText(qty, r.x + r.w - 42 + (34 - UiTextWidth(qty, 1)) / 2, fy + 7, EM2_COUNT_INK, EM2_COUNT_SH, 1, r.x + r.w - 8);
    }
    Em2Text(b->row[i].name, r.x + (chosen ? 39 : 30), fy + 8, p->ink, p->inkSh, r.x + r.w - (b->pocket != 1 ? 46 : 8));
}

/* A scroll arrow: a small cream button with a red triangle (FireRed's), the triangle grey while the list cannot go that way. The button sits at the
 * top of the arrow's area going up, at its foot going down. */
static void BBagArrow(const CtrPokeTouchRect *r, int down, int on)
{
    int fy = EmButton(r->x, down ? r->y + r->h - 20 : r->y, 20, 20, &kEmCream, 2, 0);

    LlArrow(r->x + 6, fy + (down ? 7 : 6), 8, 5, down ? 1 : 0, on ? LL_CURSOR : UI_RGB(184, 176, 168));
}

static void BBagList(const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;

    for (int i = 0; i < BUI_BAG_ROWS; ++i)
        BBagRow(i, v);
    if (b->count > BUI_BAG_ROWS) {
        int railY = sBUpArrow.y + 22, railH = sBDownArrow.y + sBDownArrow.h - 22 - railY, left = b->count - BUI_BAG_ROWS;
        int thumbH = railH * BUI_BAG_ROWS / b->count, thumbY;

        if (thumbH < 14)
            thumbH = 14;
        thumbY = (railH - thumbH) * (b->first < 0 ? 0 : b->first > left ? left : b->first) / left;
        BBagArrow(&sBUpArrow, 0, b->first > 0);
        BBagArrow(&sBDownArrow, 1, b->first + BUI_BAG_ROWS < b->count);
        LlScrollRail(sBUpArrow.x + 5, railY, 10, railH, thumbY, thumbH);
    }
}

/* The selected item's window: its icon on a plate, name, quantity and USE; with FireRed's menu open, the menu. */
static void BBagDetail(const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;
    const CtrPokeTouchRect *d = &sBDetail;
    CtrPokeTouchRect name = {(int16_t)(d->x + 7), 0, (int16_t)(d->w - 14), 13};
    char fit[BUI_ITEM_LEN + 4];

    LlWindow(d->x, d->y, d->w, d->h);
    if (b->menuCount > 0) {
        if (b->selItem) {
            BItemDraw(b->selItem, d->x + 9, d->y + 8, 1);
            Em2Text(b->selName, d->x + 37, d->y + 14, EM_DARK, EM_DARK_SH, d->x + d->w - 8);
        }
        for (int i = 0; i < b->menuCount && i < 3; ++i) {
            CtrPokeTouchRect r = BMenuRow(i);
            const EmPal *p = b->menuCursor == i ? &kEmFlame : i == b->menuCount - 1 ? &kEmSlate : &kEmCream;
            int fy;

            if (b->menuCursor == i)
                LlSelectRing(r.x, r.y, r.w, r.h);
            fy = EmButton(r.x, r.y, r.w, r.h, p, B_DEPTH, v->pressed == BUI_BAG_MENU + i);
            EmLabel(b->menuName[i], r.x, r.w, fy, r.h - B_DEPTH, p);
        }
        return;
    }
    if (!b->selItem) {
        Em2Fit(CTR_TR("Close the Bag", "Cerrar la mochila", "Beutel schlie\xC3\x9F" "en"), name.w, fit, sizeof(fit));
        UiTextCentered(fit, &name, d->y + 60, EM_DIM, EM_DARK_SH, 1);
        return;
    }
    LlPlate(d->x + d->w / 2 - 27, d->y + 7, 54, LL_SLATE, 0, 0);
    BItemDraw(b->selItem, d->x + d->w / 2 - 24, d->y + 10, 2);
    Em2Fit(b->selName, name.w, fit, sizeof(fit));
    UiTextCentered(fit, &name, d->y + 66, EM_DARK, EM_DARK_SH, 1);
    if (b->pocket != 1) {
        char qty[8];

        snprintf(qty, sizeof(qty), "x%u", b->selItem && BBagSelRow(b) >= 0 ? b->row[BBagSelRow(b)].qty : 0);
        if (BBagSelRow(b) >= 0)
            UiTextCentered(qty, &name, d->y + 81, EM2_COUNT_INK, EM2_COUNT_SH, 1);
    }
    if (BBagSelRow(b) >= 0) {
        int fy = EmButton(sBUse.x, sBUse.y, sBUse.w, sBUse.h, &kEmFlame, B_DEPTH, v->pressed == BUI_BAG_ROW + BBagSelRow(b));

        EmLabel(CTR_TR("Use", "Usar", "Benutzen"), sBUse.x, sBUse.w, fy, sBUse.h - B_DEPTH, &kEmFlame);
    }
}

/* The Bag's message over the dimmed Bag, in the text window (A, or a tap anywhere, goes on). */
static void BBagMessage(const CtrBattleView *v)
{
    BMessageWindow(v->bag.message);
}

static void BBag(const CtrBattleView *v)
{
    const CtrPokeTouchRect *n = &sBNote;

    TsBackdrop();
    BTabs(v);
    BBagList(v);
    BBagDetail(v);
    LlWindow(n->x, n->y, n->w, n->h);
    if (v->bag.selItem && v->bag.menuCount == 0)
        Em2Wrap(v->bag.selDesc, n->x + 13, n->y + 11, n->w - 26, 4, UI_LINE_H, EM_DIM, EM_DARK_SH);
    BEmCancel(&sBBagCancel, v->pressed == BUI_BAG_CANCEL);
    if (v->bag.message[0])
        BBagMessage(v);
}

/* A screen with the party's balls on top. */
static void BScreen(const CtrBattleView *v)
{
    TsBackdrop();
    BStrip(v);
    if (v->state == BUI_ACTION) {
        for (int i = 0; i < 4; ++i)
            BActCard(i, v);
        if (v->ball != CTR_QUICK_BALL_NONE)
            BBallCard(v);
    }
    else if (v->state == BUI_MOVE) {
        for (int i = 0; i < 4; ++i)
            BMoveCard(i, v);
        BMoveNote(v);
        BEmCancel(&sBCancel, v->pressed == BUI_CANCEL);
    } else if (v->state == BUI_TARGET) {
        for (int i = 0; i < 4; ++i)
            BTargetCard(i, v);
        BTargetNote();
        BEmCancel(&sBCancel, v->pressed == BUI_CANCEL);
    } else if (v->state == BUI_SAFARI)
        for (int i = 0; i < 4; ++i)
            BSafariCard(i, v);
}

void CtrBattleUi_Draw(uint16_t *canvas, const CtrBattleView *v)
{
    sCanvas = canvas;
    EmSurface();
    if (v->state == BUI_PARTY)
        BParty(v);
    else if (v->state == BUI_BAG)
        BBag(v);
    else
        BScreen(v);
    sUiS = &sUiNavy;
}

/* Each changed battle element is presented through the shared dirty-rectangle path. */
static void BMark(CtrDirtyList *dirty, const CtrPokeTouchRect *r)
{
    CtrDirty_Add(dirty, r->x, r->y, r->x + r->w, r->y + r->h);
}

/* Whether two Party views differ only by `cursor` (with no modal open), so only the affected slot panels need redraw. */
static int BPartySlotsOnly(const CtrBattleParty *a, const CtrBattleParty *b)
{
    CtrBattleParty copy = *a;

    if (a->panel != BUI_PTY_NONE || b->panel != BUI_PTY_NONE)
        return 0;
    copy.cursor = b->cursor;
    /* A card's icon changes frames by itself (FireRed's icon animation): its own card is drawn again, not all of them. */
    memcpy(copy.iconFrame, b->iconFrame, sizeof(copy.iconFrame));
    return memcmp(&copy, b, sizeof(copy)) == 0;
}

/* Whether two Party views with the same menu or YES / NO open differ only by its cursor row and the row pressed. */
static int BPartyMenuOnly(const CtrBattleView *a, const CtrBattleView *b)
{
    CtrBattleView copy = *a;

    if (a->state != BUI_PARTY || b->state != BUI_PARTY || a->pty.panel != b->pty.panel
        || (b->pty.panel != BUI_PTY_ACTIONS && b->pty.panel != BUI_PTY_YESNO))
        return 0;
    copy.pty.menuCursor = b->pty.menuCursor;
    copy.pressed = b->pressed;
    memcpy(copy.pty.iconFrame, b->pty.iconFrame, sizeof(copy.pty.iconFrame));
    return memcmp(&copy, b, sizeof(copy)) == 0;
}

/* Whether a screen with the balls must be drawn again whole: another screen, other balls, other cards. */
static int BScreenChanged(const CtrBattleView *a, const CtrBattleView *b)
{
    if (a->state != b->state || memcmp(a->party, b->party, sizeof(b->party)) || memcmp(a->partyHp, b->partyHp, sizeof(b->partyHp))
        || memcmp(a->partyStatus, b->partyStatus, sizeof(b->partyStatus)))
        return 1;
    if (b->state == BUI_ACTION)
        return strcmp(a->activeName, b->activeName) || a->activeLevel != b->activeLevel || a->activeIcon != b->activeIcon
            || memcmp(a->activeType, b->activeType, sizeof(b->activeType)) || memcmp(a->activeTypeName, b->activeTypeName, sizeof(b->activeTypeName))
            || a->activeHp != b->activeHp || a->activeMaxHp != b->activeMaxHp
            || a->activeSlot != b->activeSlot || a->ball != b->ball || a->ballItem != b->ballItem
            || a->ballQty != b->ballQty || a->runBlocked != b->runBlocked || a->ballPlace != b->ballPlace || a->ballCount != b->ballCount
            || strcmp(a->ballName, b->ballName);
    if (b->state == BUI_MOVE)
        return memcmp(a->move, b->move, sizeof(b->move)) != 0;
    if (b->state == BUI_TARGET)
        return memcmp(a->target, b->target, sizeof(b->target)) != 0;
    return 0;
}

void CtrBattleUi_Update(uint16_t *canvas, const CtrBattleView *old, const CtrBattleView *v, CtrDirtyList *dirty)
{
    CtrPokeTouchRect all = {0, 0, W, H};

    sCanvas = canvas;
    EmSurface();
    CtrDirty_Clear(dirty);
    if (v->state == BUI_PARTY || old->state == BUI_PARTY) {
        /* The Party: a move of the selection or a press redraws its cards, anything else all of it. */
        CtrBattleView still = *old;

        memcpy(still.pty.iconFrame, v->pty.iconFrame, sizeof(still.pty.iconFrame));
        if (v->state == BUI_PARTY && old->state == BUI_PARTY && v->pty.panel != BUI_PTY_NONE
            && memcmp(&still, v, sizeof(still)) == 0) {
            /* Only the icons' frames: with a menu or a message over the cards they stay as they were (a full compose,
             * ~35 ms, every few frames for a hop nobody is looking at). */
        } else if (BPartyMenuOnly(old, v)) {
            /* A menu's cursor or press: the picture is composed in the canvas, but only the rows that changed are presented
             * (the whole screen written at once to the shown buffer tears). */
            int n = v->pty.menuCount > 8 ? 8 : v->pty.menuCount;

            CtrBattleUi_Draw(canvas, v);
            EmSurface();
            for (int i = 0; i < n; ++i)
                if ((old->pty.menuCursor == i) != (v->pty.menuCursor == i)
                    || (old->pressed == BUI_PTY_MENU + i) != (v->pressed == BUI_PTY_MENU + i)) {
                    CtrPokeTouchRect r = BPMenuRow(n, i);

                    r = UiInset(&r, -3);
                    BMark(dirty, &r);
                }
        } else if (old->state != v->state || !BPartySlotsOnly(&old->pty, &v->pty) || old->pty.lead != v->pty.lead
            || ((old->pressed >= BUI_PTY_MENU && old->pressed < BUI_PTY_MESSAGE) != (v->pressed >= BUI_PTY_MENU && v->pressed < BUI_PTY_MESSAGE))
            || (v->pty.panel != BUI_PTY_NONE && old->pressed != v->pressed)) {
            CtrBattleUi_Draw(canvas, v);
            EmSurface();
            BMark(dirty, &all);
        } else {
            for (int i = 0; i < 6; ++i)
                if ((old->pty.cursor == i) != (v->pty.cursor == i)
                    || old->pty.iconFrame[i] != v->pty.iconFrame[i]
                    || (old->pressed == BUI_PTY_SLOT + i) != (v->pressed == BUI_PTY_SLOT + i)) {
                    CtrPokeTouchRect r = BPanel(&v->pty, i);

                    r = UiInset(&r, -2);
                    BPartyPanel(i, v);
                    BPartyEntry(i, v);
                    BPartyKeepRing(v, i);
                    BMark(dirty, &r);
                }
            if (v->pty.mode != BUI_PTY_FORCED && ((old->pressed == BUI_PTY_CANCEL) != (v->pressed == BUI_PTY_CANCEL)
                                                 || (old->pressed == BUI_PTY_CONFIRM) != (v->pressed == BUI_PTY_CONFIRM))) {
                CtrPokeTouchRect r = UiInset(&sBPCancel, -2);

                BPCancelButton(v);
                BMark(dirty, &r);
            }
        }
    } else if (v->state == BUI_BAG || old->state == BUI_BAG) {
        if (old->state != v->state || memcmp(&old->bag, &v->bag, sizeof(v->bag)) || old->pressed != v->pressed) {
            CtrBattleUi_Draw(canvas, v);
            EmSurface();
            BMark(dirty, &all);
        }
    } else if (BScreenChanged(old, v)) {
        CtrBattleUi_Draw(canvas, v);
        EmSurface();
        BMark(dirty, &all);
    } else if (v->state != BUI_IDLE) {
        /* Only what looks different: a card pressed or under the cursor, the note that follows the cursor, CANCEL. */
        for (int i = 0; i < 4; ++i)
            if (BLooksChanged(old, v, i)) {
                const CtrPokeTouchRect *c = v->state == BUI_ACTION ? BActRect(v, i) : v->state == BUI_SAFARI ? &sBSafari[i] : &sBCard[i];
                CtrPokeTouchRect g = UiInset(c, -2);

                if (v->state == BUI_ACTION)
                    BActCard(i, v);
                else if (v->state == BUI_SAFARI)
                    BSafariCard(i, v);
                else if (v->state == BUI_MOVE)
                    BMoveCard(i, v);
                else
                    BTargetCard(i, v);
                BMark(dirty, &g);
            }
        if (v->state == BUI_MOVE && old->cursor != v->cursor) {
            CtrPokeTouchRect g = UiInset(&sBMNote, -2);

            BMoveNote(v);
            BMark(dirty, &g);
        }
        /* The quick ball: pressed (the button or an arrow), or the cursor on it or off it. */
        if (v->state == BUI_ACTION && v->ball != CTR_QUICK_BALL_NONE
            && ((old->pressed != v->pressed && (BBallPart(old->pressed) || BBallPart(v->pressed)))
                || (old->showCursor && old->cursor == CTR_ACTION_BALL) != (v->showCursor && v->cursor == CTR_ACTION_BALL))) {
            CtrPokeTouchRect g = UiInset(BActRect(v, CTR_ACTION_BALL), -2);

            BBallCard(v);
            BMark(dirty, &g);
        }
        if ((v->state == BUI_MOVE || v->state == BUI_TARGET) && (old->pressed == BUI_CANCEL) != (v->pressed == BUI_CANCEL)) {
            CtrPokeTouchRect g = UiInset(&sBCancel, -2);

            BEmCancel(&sBCancel, v->pressed == BUI_CANCEL);
            BMark(dirty, &g);
        }
    }
    sUiS = &sUiNavy;
}
