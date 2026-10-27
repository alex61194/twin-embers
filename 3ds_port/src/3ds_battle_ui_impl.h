/* The battle screen (included at the end of 3ds_poketouch.c, whose drawing helpers and design system it uses).
 * The lower screen while FireRed's battle is on the upper one, in PokeTouch's ember look (3ds_touch_skin_impl.h): the
 * ember backdrop, the party's six Poke Balls on a dark pill on top, raised pieces over it (amber for the main choice
 * and FireRed's cursor, cream for the others, violet for CANCEL): the action menu (FIGHT across the top; BAG, POKeMON
 * and RUN below), the move cards, the target cards, the Safari Zone's four, the battle Bag and the battle Party.
 * Everything shown is what the controller reports. */

#include "3ds_icons.h"
#include "3ds_ui_strings.h"
#include "3ds_type_chart.h"

/* The party's balls, on their pill. */
static const CtrPokeTouchRect sBStrip = {70, 3, 180, 28};
/* The action menu, FireRed's cursors 0-3: FIGHT across the top, BAG, POKeMON and RUN below (3ds_battle_nav.h walks the
 * D-pad over them the same way). */
static const CtrPokeTouchRect sBAct[4] = {{8, 38, 304, 90}, {8, 138, 98, 94}, {111, 138, 98, 94}, {214, 138, 98, 94}};
/* The move cards (and a double battle's target cards) in the controller's 2x2 order; under them the note and CANCEL. */
static const CtrPokeTouchRect sBCard[4] = {{8, 38, 150, 62}, {162, 38, 150, 62}, {8, 104, 150, 62}, {162, 104, 150, 62}};
static const CtrPokeTouchRect sBNote = {4, 176, 236, 60};
static const CtrPokeTouchRect sBCancel = {244, 180, 72, 52};
/* The Safari Zone's four actions in the controller's 2x2 order (BALL, BAIT, ROCK, RUN). */
static const CtrPokeTouchRect sBSafari[4] = {{8, 38, 150, 92}, {162, 38, 150, 92}, {8, 140, 150, 92}, {162, 140, 150, 92}};
/* The battle Bag: three tabs (the Bag's real pockets), the item list on the left with its scroll arrows, the selected
 * item's card on the right, the description's note and CANCEL. */
static const CtrPokeTouchRect sBTab[3] = {{6, 4, 100, 26}, {110, 4, 100, 26}, {214, 4, 100, 26}};
static const int sBTabPocket[3] = {0, 2, 1};            /* the tabs show Items, Poke Balls, Key Items: FireRed's pockets 0, 2, 1 */
static const char *const sBTabLabel[3] = {"ITEMS", "BALLS", "KEY ITEMS"};
static const CtrPokeTouchRect sBList = {6, 36, 190, 138};
static const CtrPokeTouchRect sBDetail = {200, 36, 114, 138};
static const CtrPokeTouchRect sBUse = {210, 136, 94, 30};
static const CtrPokeTouchRect sBUpArrow = {176, 36, 20, 66}, sBDownArrow = {176, 108, 20, 66};
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
/* The Party (in battle and on the field): the six Pokemon as raised cards in two columns of three on the embers, and
 * under them the note card with what the Party says and CANCEL. */
static const CtrPokeTouchRect sBPNote = {4, 176, 236, 60};
static const CtrPokeTouchRect sBPCancel = {244, 180, 72, 52};
/* Picking several Pokemon: CONFIRM over a shorter CANCEL. */
static const CtrPokeTouchRect sBPConfirm = {244, 180, 72, 26}, sBPCancelLow = {244, 208, 72, 24};
#define BP_DEPTH 3

/* A slot's card: two columns of three, the slots reading left to right and down, as the Party's D-pad walks them (the
 * patched UpdatePartySelectionGrid). */
static CtrPokeTouchRect BPanel(const CtrBattleParty *p, int i)
{
    int h = (164 - 4 * 2) / 3;     /* 4 apart: a card's glow never reaches its neighbour's redraw */
    CtrPokeTouchRect r = {(int16_t)(i & 1 ? 163 : 6), (int16_t)(6 + (h + 4) * (i >> 1)), 151, (int16_t)h};

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
    if (v->state == BUI_ACTION)
        for (int i = 0; i < 4; ++i)
            if (BIn(&sBAct[i], x, y))
                return i;
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

        if (p->panel == BUI_PTY_TEXT)
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
        if (BIn(&sBCancel, x, y))
            return BUI_BAG_CANCEL;
        for (int i = 0; i < 3; ++i)
            if (BIn(&sBTab[i], x, y))
                return BUI_BAG_TAB + i;
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
        || (a->showCursor && a->cursor == i) != (b->showCursor && b->cursor == i);
}

/* Appends `word` to `line` (a space first when it is not empty), within `size`. */
static void BAppend(char *line, size_t size, const char *word)
{
    size_t n = strlen(line);

    if (n && n + 1 < size)
        line[n++] = ' ';
    for (; *word && n + 1 < size; ++word)
        line[n++] = *word;
    line[n] = 0;
}

/* Running text wrapped to `width` pixels, `maxLines` at most (FireRed's line breaks are spaces here). */
static void BDescribeN(const char *text, int x, int y, int width, int maxLines, uint16_t color)
{
    char word[24], line[80], joined[80];
    int lines = 0;
    const char *p = text;

    line[0] = 0;
    while (*p && lines < maxLines) {
        int w = 0;

        while (*p == ' ' || *p == '\n')
            ++p;
        while (*p && *p != ' ' && *p != '\n' && w < (int)sizeof(word) - 1)
            word[w++] = *p++;
        word[w] = 0;
        if (!w)
            break;
        memcpy(joined, line, sizeof(joined));
        BAppend(joined, sizeof(joined), word);
        if (line[0] && UiTextWidth(joined, 1) > width) {
            UiText(line, x, y + lines * UI_LINE_H, color, UIS_TEXT_SH, 1, x + width);
            line[0] = 0;
            if (++lines >= maxLines)
                return;
            BAppend(line, sizeof(line), word);
        } else
            memcpy(line, joined, sizeof(line));
    }
    if (line[0] && lines < maxLines)
        UiText(line, x, y + lines * UI_LINE_H, color, UIS_TEXT_SH, 1, x + width);
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
        sBPIcon[slot].pal[i] = RGB((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
    }
    return 1;
}

/* Text wrapped into `maxLines` lines of `width`, in `ink` over `shadow`. */
static void BPWrap(const char *text, int x, int y, int width, int maxLines, uint16_t ink, uint16_t shadow)
{
    char word[24], line[80], joined[80];
    int lines = 0;
    const char *p = text;

    line[0] = 0;
    while (*p && lines < maxLines) {
        int w = 0;

        while (*p == ' ' || *p == '\n')
            ++p;
        while (*p && *p != ' ' && *p != '\n' && w < (int)sizeof(word) - 1)
            word[w++] = *p++;
        word[w] = 0;
        if (!w)
            break;
        memcpy(joined, line, sizeof(joined));
        BAppend(joined, sizeof(joined), word);
        if (line[0] && UiTextWidth(joined, 1) > width) {
            UiText(line, x, y + lines * 16, ink, shadow, 1, x + width);
            line[0] = 0;
            if (++lines >= maxLines)
                return;
            BAppend(line, sizeof(line), word);
        } else
            memcpy(line, joined, sizeof(line));
    }
    if (line[0] && lines < maxLines)
        UiText(line, x, y + lines * 16, ink, shadow, 1, x + width);
}

/* The selected slot's icon hops with its frames, as FireRed's Party does: up on the first, down on the second. */
static int BPartyBounce(const CtrBattleParty *p, int i)
{
    return p->cursor == i ? (p->iconFrame[i] & 2 ? 1 : -3) : 0;
}

/* The card's verdict on the item or move being taught, on a pill where its HP numbers would be. */
static int BPartyTagPill(const CtrBattleParty *p, int i, const CtrPokeTouchRect *r, int fy, int y, int h)
{
    static const char *const text[5] = {"", "ABLE", "NOT ABLE", "LEARNED", "NO USE"};
    static const uint16_t tint[5] = {0, UI_RGB(70, 168, 84), UI_RGB(112, 112, 128), UI_RGB(70, 132, 214), UI_RGB(112, 112, 128)};
    int tag = p->tag[i], w;
    CtrPokeTouchRect pill;

    if (tag <= BUI_PTY_TAG_NONE || tag > BUI_PTY_TAG_NO_USE || p->egg[i] || p->mode == BUI_PTY_MULTI)
        return 0;
    w = UiTextWidth(text[tag], 1) + 12;
    pill = (CtrPokeTouchRect){(int16_t)(r->x + r->w - 6 - w), (int16_t)(fy + y), (int16_t)w, (int16_t)h};
    UiBadge(&pill, tint[tag], text[tag]);
    return 1;
}

/* A Party card: a raised card (amber and glowing for the Party's selection, grey for one that fainted) with the icon
 * on its round plate, the name, the level or a status chip, the HP bar and numbers. An empty slot is a hollow. */
static void BPartyPanel(int i, const CtrBattleView *v)
{
    static const char *const code[7] = {"", "PSN", "PAR", "SLP", "FRZ", "BRN", "PKRS"};
    static const uint16_t tint[7] = {0, UI_RGB(170, 80, 190), UI_RGB(214, 170, 30), UI_RGB(120, 128, 150),
                                     UI_RGB(80, 170, 220), UI_RGB(226, 96, 48), UI_RGB(200, 90, 160)};
    const CtrBattleParty *p = &v->pty;
    CtrPokeTouchRect r = BPanel(p, i);
    int fainted = p->present[i] && !p->egg[i] && p->hp[i] == 0, selected = p->cursor == i;
    int press = v->pressed == BUI_PTY_SLOT + i;
    int barred = (p->mode == BUI_PTY_MULTI && p->entry[i] == BUI_PTY_NOT_ABLE)
              || p->tag[i] == BUI_PTY_TAG_NOT_ABLE || p->tag[i] == BUI_PTY_TAG_NO_USE;
    const EmPal *pal = selected ? &kEmAmber : fainted || barred ? &kEmGrey : &kEmCream;
    int fy, fh = r.h - BP_DEPTH;

    TsBgRect(r.x - 2, r.y - 2, r.w + 4, r.h + 4);
    if (!p->present[i]) {
        UiRoundFill(r.x, r.y, r.w, fh, 5, UI_RGB(140, 70, 60));
        UiRoundFill(r.x + 1, r.y + 1, r.w - 2, fh - 2, 4, UI_RGB(70, 18, 16));
        return;
    }
    if (selected)
        EmGlowRing(r.x, r.y, r.w, r.h);
    fy = EmButton(r.x, r.y, r.w, r.h, pal, BP_DEPTH, press);
    if (r.h < 44) {
        /* A right-hand card, shorter: the icon on a smaller plate, the name and level above, the bar and HP below. */
        int cy = fy + fh / 2;

        UiDisc(r.x + 17, cy, 12, UiMix(pal->bottom, pal->edge, 2));
        UiDisc(r.x + 17, cy, 11, UiMix(pal->top, pal->bottom, 6));
        if (BPartyIcon(i, p->icon[i]))
            for (int y = 2; y < 30; ++y)
                for (int x = 0; x < 32; ++x) {
                    uint8_t index = sBPIcon[i].pixels[(y + (p->iconFrame[i] & 1) * 32) * 32 + x];
                    int py = cy - 18 + y + BPartyBounce(p, i);
                    if (index && py >= fy && py < fy + fh)
                        Px(r.x + 1 + x, py, fainted ? Tone(sBPIcon[i].pal[index], 0) : sBPIcon[i].pal[index]);
                }
        UiText(p->name[i], r.x + 36, fy + 2, pal->ink, pal->inkSh, 1, r.x + r.w - 32);
        if (p->egg[i])
            return;
        {
            char text[24];
            int ly = fy + fh - 12;
            CtrPokeTouchRect bar = {(int16_t)(r.x + 36), (int16_t)(ly + 3), 48, 5};

            if (fainted || (p->status[i] >= 1 && p->status[i] <= 6)) {
                CtrPokeTouchRect badge = {(int16_t)(r.x + r.w - 33), (int16_t)(fy + 2), 28, 11};

                UiBadge(&badge, fainted ? UI_RGB(90, 80, 90) : tint[p->status[i]], fainted ? "FNT" : code[p->status[i]]);
            } else {
                snprintf(text, sizeof(text), "Lv%u", p->level[i]);
                UiText(text, r.x + r.w - 6 - UiTextWidth(text, 1), fy + 2, pal->ink, pal->inkSh, 1, r.x + r.w);
            }
            UiRoundFill(bar.x - 1, bar.y - 1, bar.w + 2, bar.h + 2, 2, UI_RGB(64, 72, 104));
            UiHpBar(&bar, p->hp[i], p->maxHp[i]);
            snprintf(text, sizeof(text), "%u/%u", p->hp[i], p->maxHp[i]);
            if (!BPartyTagPill(p, i, &r, fy, fh - 12, 11))
                UiText(text, r.x + r.w - 6 - UiTextWidth(text, 1), ly, pal->ink, pal->inkSh, 1, r.x + r.w);
        }
        return;
    }
    UiDisc(r.x + 21, fy + 21, 15, UiMix(pal->bottom, pal->edge, 2));
    UiDisc(r.x + 21, fy + 21, 14, UiMix(pal->top, pal->bottom, 6));
    if (BPartyIcon(i, p->icon[i]))
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                uint8_t index = sBPIcon[i].pixels[(y + (p->iconFrame[i] & 1) * 32) * 32 + x];
                if (index)
                    Px(r.x + 5 + x, fy + 3 + y + BPartyBounce(p, i), fainted ? Tone(sBPIcon[i].pal[index], 0) : sBPIcon[i].pal[index]);
            }
    UiText(p->name[i], r.x + 41, fy + 4, pal->ink, pal->inkSh, 1, r.x + r.w - 4);
    if (p->egg[i])
        return;
    {
        char text[24];
        CtrPokeTouchRect bar = {(int16_t)(r.x + 41), (int16_t)(fy + 34), 52, 6};

        if (fainted || (p->status[i] >= 1 && p->status[i] <= 6)) {
            CtrPokeTouchRect badge = {(int16_t)(r.x + 41), (int16_t)(fy + 17), 30, 13};

            UiBadge(&badge, fainted ? UI_RGB(90, 80, 90) : tint[p->status[i]], fainted ? "FNT" : code[p->status[i]]);
        } else {
            snprintf(text, sizeof(text), "Lv%u", p->level[i]);
            UiText(text, r.x + 41, fy + 18, pal->ink, pal->inkSh, 1, r.x + r.w - 4);
        }
        UiRoundFill(bar.x - 1, bar.y - 1, bar.w + 2, bar.h + 2, 2, UI_RGB(64, 72, 104));
        UiHpBar(&bar, p->hp[i], p->maxHp[i]);
        snprintf(text, sizeof(text), "%u/%u", p->hp[i], p->maxHp[i]);
        if (!BPartyTagPill(p, i, &r, fy, 28, 13))
            UiText(text, r.x + r.w - 6 - UiTextWidth(text, 1), fy + 30, pal->ink, pal->inkSh, 1, r.x + r.w);
    }
}

/* Picking several Pokemon: a slot's place in the order (1-3) on a disc at the foot of its icon plate. */
static void BPartyEntry(int i, const CtrBattleView *v)
{
    const CtrBattleParty *p = &v->pty;
    CtrPokeTouchRect r = BPanel(p, i);
    int fy = r.y + (v->pressed == BUI_PTY_SLOT + i ? BP_DEPTH : 0), place = p->entry[i] - BUI_PTY_FIRST;
    int cx = r.x + (r.h < 44 ? 9 : 10), cy = fy + r.h - BP_DEPTH - 9;
    char digit[2] = {(char)('1' + place), 0};

    if (p->mode != BUI_PTY_MULTI || !p->present[i] || p->entry[i] < BUI_PTY_FIRST)
        return;
    UiDisc(cx, cy, 7, kEmAmber.lip);
    UiDisc(cx, cy, 6, kEmAmber.top);
    UiText(digit, cx - UiTextWidth(digit, 1) / 2, cy - 5, kEmAmber.ink, kEmAmber.inkSh, 1, cx + 8);
}

/* CANCEL: violet, as BACK. Picking several, the shorter one under CONFIRM, both lit while the Party's cursor is on
 * them. */
static void BPCancelButton(const CtrBattleView *v)
{
    int multi = v->pty.mode == BUI_PTY_MULTI;
    const CtrPokeTouchRect *r = multi ? &sBPCancelLow : &sBPCancel;
    const EmPal *pal = multi && v->pty.button == 1 ? &kEmAmber : &kEmViolet;
    int fy;

    TsBgRect(sBPCancel.x - 2, sBPCancel.y - 2, sBPCancel.w + 4, sBPCancel.h + 4);
    if (multi) {
        const EmPal *cp = v->pty.button == 0 ? &kEmAmber : &kEmCream;

        if (v->pty.button == 0)
            EmGlowRing(sBPConfirm.x, sBPConfirm.y, sBPConfirm.w, sBPConfirm.h);
        fy = EmButton(sBPConfirm.x, sBPConfirm.y, sBPConfirm.w, sBPConfirm.h, cp, BP_DEPTH, v->pressed == BUI_PTY_CONFIRM);
        EmLabel(CtrUiText(UIS_PTY_CONFIRM), sBPConfirm.x, sBPConfirm.w, fy, sBPConfirm.h - BP_DEPTH, cp);
        if (v->pty.button == 1)
            EmGlowRing(r->x, r->y, r->w, r->h);
    }
    fy = EmButton(r->x, r->y, r->w, r->h, pal, BP_DEPTH, v->pressed == BUI_PTY_CANCEL);
    EmLabel("CANCEL", r->x, r->w, fy, r->h - BP_DEPTH, pal);
}

/* The note card: what the Party says (its question or hint), or, with no CANCEL, that one must be sent out. */
static void BPNoteCard(const CtrBattleView *v)
{
    const CtrPokeTouchRect *n = &sBPNote;
    const char *text = v->pty.caption[0] ? v->pty.caption
                     : v->pty.mode == BUI_PTY_FORCED ? "A POK\xC3\xA9MON must be sent out."
                     : v->pty.mode == BUI_PTY_TARGET ? "Use on which one?" : "Choose a POK\xC3\xA9MON.";

    TsBgRect(n->x, n->y, n->w, n->h);
    EmNote(n->x, n->y, n->w, n->h);
    BPWrap(text, n->x + 12, n->y + 14, n->w - 18, 2, EM_DARK, EM_DARK_SH);
}

/* What the Party has open over its slots, over the dimmed Party: its menu (the selected row amber, the last violet),
 * or the message it waits on in a card. */
static void BPartyOverlay(const CtrBattleView *v)
{
    const CtrBattleParty *p = &v->pty;
    CtrPokeTouchRect all = {0, 0, W, H};

    if (p->panel == BUI_PTY_TEXT) {
        CtrPokeTouchRect box = {20, 70, 280, 76};

        UiDim(&all);
        EmNote(box.x, box.y, box.w, box.h);
        BPWrap(p->message, box.x + 14, box.y + 10, box.w - 24, 3, EM_DARK, EM_DARK_SH);
        UiText("Tap to continue", box.x + box.w - 14 - UiTextWidth("Tap to continue", 1) - 9, box.y + box.h - 16,
               EM_DIM, EM_DARK_SH, 1, box.x + box.w);
        UiArrow(box.x + box.w - 18, box.y + box.h - 16, 1, EM_STRIPE);
    } else if (p->panel == BUI_PTY_ACTIONS || p->panel == BUI_PTY_YESNO) {
        int n = p->menuCount > 8 ? 8 : p->menuCount;

        UiDim(&all);
        for (int i = 0; i < n; ++i) {
            CtrPokeTouchRect r = BPMenuRow(n, i);
            const EmPal *pal = p->menuCursor == i ? &kEmAmber : i == n - 1 ? &kEmViolet : &kEmCream;
            int fy;

            if (p->menuCursor == i)
                EmGlowRing(r.x, r.y, r.w, r.h);
            fy = EmButton(r.x, r.y, r.w, r.h, pal, BP_DEPTH, v->pressed == BUI_PTY_MENU + i);
            EmLabel(p->menuName[i], r.x, r.w, fy, r.h - BP_DEPTH, pal);
        }
    }
}

static void BParty(const CtrBattleView *v)
{
    TsBackdrop();
    for (int i = 0; i < 6; ++i) {
        BPartyPanel(i, v);
        BPartyEntry(i, v);
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
        sBIcon[slot].pal[i] = RGB((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
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

/* The action menu's other colours: BAG blue, POKeMON green, RUN purple. */
static const EmPal kEmBlue = {UI_RGB(130, 188, 255), UI_RGB(44, 112, 224), UI_RGB(26, 70, 160), UI_RGB(14, 36, 96),
                              EM_WHITE, UI_RGB(14, 36, 96)};
static const EmPal kEmGreen = {UI_RGB(140, 224, 124), UI_RGB(44, 160, 72), UI_RGB(26, 110, 46), UI_RGB(12, 64, 24),
                               EM_WHITE, UI_RGB(12, 64, 24)};
static const EmPal kEmPurple = {UI_RGB(196, 168, 255), UI_RGB(122, 88, 222), UI_RGB(82, 56, 160), UI_RGB(44, 28, 96),
                                EM_WHITE, UI_RGB(44, 28, 96)};

/* A pill with `text` on it, 15 pixels tall; BPillW is its width, x its left edge. */
static int BPillW(const char *text)
{
    return UiTextWidth(text, 1) + 14;
}

static void BPill(int x, int y, const char *text, uint16_t bg, uint16_t ink, uint16_t shadow)
{
    int w = BPillW(text);

    UiRoundFill(x, y, w, 15, 5, UiMix(bg, UI_RGB(0, 0, 0), 3));
    UiRoundFill(x + 1, y + 1, w - 2, 13, 4, bg);
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
        pal[i] = RGB((c & 31) << 3, ((c >> 5) & 31) << 3, ((c >> 10) & 31) << 3);
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

/* Icon `icon` (BX_*) centred at (cx, cy) on a light disc of the piece's colour, `r` its radius. */
static void BDiscIcon(int icon, int cx, int cy, int r, const EmPal *p)
{
    UiDisc(cx, cy, r + 1, UiMix(p->bottom, p->lip, 3));
    UiDisc(cx, cy, r, UiMix(p->top, EM_WHITE, 4));
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

/* A raised card of the battle: the backdrop back round it, the glow while FireRed's cursor is on it. */
static int BEmCard(const CtrPokeTouchRect *r, const EmPal *p, int focused, int pressed)
{
    TsBgRect(r->x - 2, r->y - 2, r->w + 4, r->h + 4);
    if (focused)
        EmGlowRing(r->x, r->y, r->w, r->h);
    return EmButton(r->x, r->y, r->w, r->h, p, B_DEPTH, pressed);
}

/* An empty slot: a hollow in the backdrop. */
static void BHollow(const CtrPokeTouchRect *r)
{
    TsBgRect(r->x - 2, r->y - 2, r->w + 4, r->h + 4);
    UiRoundFill(r->x, r->y, r->w, r->h - B_DEPTH, 5, UI_RGB(140, 70, 60));
    UiRoundFill(r->x + 1, r->y + 1, r->w - 2, r->h - B_DEPTH - 2, 4, UI_RGB(70, 18, 16));
}

/* CANCEL: violet, as BACK and the Party's. */
static void BEmCancel(const CtrPokeTouchRect *r, int pressed)
{
    int fy;

    TsBgRect(r->x, r->y, r->w, r->h);
    fy = EmButton(r->x, r->y, r->w, r->h, &kEmViolet, B_DEPTH, pressed);
    EmLabel("CANCEL", r->x, r->w, fy, r->h - B_DEPTH, &kEmViolet);
}

/* A Poke Ball of the party strip, 15 pixels across: red and white for a Pokemon that can fight, grey for one that
 * fainted (kind 2), a hollow ring for no Pokemon (kind 0); an amber dot for a status condition. */
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
    if (kind && kind != 2 && status) {
        UiDisc(cx + 6, cy - 6, 3, UI_RGB(110, 40, 6));
        UiDisc(cx + 6, cy - 6, 2, UI_RGB(255, 200, 60));
    }
}

/* The party's six balls on a dark pill, an HP bar under each. */
static void BStrip(const CtrBattleView *v)
{
    const CtrPokeTouchRect *s = &sBStrip;

    TsBgRect(s->x, s->y, s->w, s->h);
    UiRoundFill(s->x, s->y, s->w, s->h, 5, UI_RGB(24, 8, 10));
    EmGradFill(s->x + 1, s->y + 1, s->w - 2, s->h - 2, 4, UI_RGB(72, 26, 26), UI_RGB(44, 14, 16));
    for (int i = 0; i < 6; ++i) {
        int cx = s->x + 15 + 30 * i, kind = v->party[i];

        BBall(cx, s->y + 10, kind, kind == 3 || (v->partyStatus[i] >= 1 && v->partyStatus[i] <= 6));
        if (kind) {
            unsigned hp = kind == 2 ? 0 : v->partyHp[i];
            int fill = (int)(hp * 18 / 100);

            if (hp && fill < 1)
                fill = 1;
            UiRoundFill(cx - 10, s->y + 20, 20, 5, 2, UI_RGB(24, 8, 10));
            Fill(cx - 9, s->y + 21, fill, 3, hp > 50 ? UI_RGB(72, 200, 96) : hp > 20 ? UI_RGB(240, 200, 40) : UI_RGB(230, 60, 40));
        }
    }
}

/* ---- The action menu ---- */
static void BActCard(int i, const CtrBattleView *v)
{
    static const EmPal *const pal[4] = {&kEmAmber, &kEmBlue, &kEmGreen, &kEmPurple};
    static const int icon[4] = {BX_FIGHT, BX_BAG, BX_BALL, BX_SHOE};
    static const char *const label[4] = {"FIGHT", "BAG", "POK\xC3\xA9MON", "RUN"};
    static const char *const sub[4] = {"", "ITEMS", "PARTY", "ESCAPE"};
    const CtrPokeTouchRect *r = &sBAct[i];
    const EmPal *p = pal[i];
    int fy = BEmCard(r, p, v->showCursor && v->cursor == i, v->pressed == i), fh = r->h - B_DEPTH;

    if (i == 0) {
        int left = r->x + 100, w = r->w - 108, tw = UiTextWidth(label[0], 2);
        char text[32];

        BDiscIcon(icon[0], r->x + 52, fy + fh / 2, 33, p);
        UiText(label[0], left + (w - tw) / 2, fy + 14, p->ink, p->inkSh, 2, r->x + r->w);
        if (v->activeName[0]) {
            snprintf(text, sizeof(text), "%s Lv%u", v->activeName, (unsigned)v->activeLevel);
            BPill(left + (w - BPillW(text)) / 2, fy + 52, text, p->lip, EM_WHITE, p->edge);
        }
        return;
    }
    BDiscIcon(icon[i], r->x + r->w / 2, fy + 26, 22, p);
    UiText(label[i], r->x + (r->w - UiTextWidth(label[i], 1)) / 2, fy + 54, p->ink, p->inkSh, 1, r->x + r->w);
    BPill(r->x + (r->w - BPillW(sub[i])) / 2, fy + 69, sub[i], p->lip, EM_WHITE, p->edge);
}

/* ---- The moves ---- */
/* The PP count's ink: red at 0, orange and amber when low. On the amber card the orange and amber vanish into it, so
 * there only 0 is red and the pips tell the rest. */
static uint16_t BEmPpInk(unsigned pp, unsigned max, const EmPal *p)
{
    if (pp == 0)
        return EM_VALUE;
    if (p == &kEmAmber)
        return p->ink;
    return pp * 4 <= max ? UI_RGB(222, 96, 24) : pp * 2 <= max ? UI_RGB(196, 128, 0) : p->ink;
}

/* What the move does to the foe, on a pill: green when it is super effective, orange when not very, grey otherwise. */
static const uint16_t kBFxColor[] = {0, UI_RGB(110, 100, 110), UI_RGB(204, 84, 24), UI_RGB(214, 112, 30),
                                     UI_RGB(40, 150, 70), UI_RGB(30, 170, 60), UI_RGB(100, 106, 132)};

/* The mark before the pill's words, as in the mockup: two arrows up for super effective, one down for not very, a red
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

static void BMatchupPill(uint8_t matchup, int right, int y)
{
    static const char *const kText[] = {"", "NO EFFECT", "NOT VERY", "NOT VERY", "SUPER", "SUPER x4", "EFFECTIVE"};
    static const uint8_t kMark[] = {BFX_MARK_NONE, BFX_MARK_CROSS, BFX_MARK_DOWN, BFX_MARK_DOWN, BFX_MARK_UP, BFX_MARK_UP,
                                    BFX_MARK_NONE};
    const uint16_t *kColor = kBFxColor;
    uint16_t shadow;
    int markW, w, x;

    if (matchup == CTR_FX_NONE || matchup > CTR_FX_NEUTRAL)
        return;
    markW = BFxMarkW(kMark[matchup]);
    w = BPillW(kText[matchup]) + (markW ? markW + 2 : 0);
    x = right - w;
    shadow = UiMix(kColor[matchup], UI_RGB(0, 0, 0), 4);
    UiRoundFill(x, y, w, 15, 5, UiMix(kColor[matchup], UI_RGB(0, 0, 0), 3));
    UiRoundFill(x + 1, y + 1, w - 2, 13, 4, kColor[matchup]);
    if (markW)
        BFxMark(kMark[matchup], x + 6, y + 4, EM_WHITE, shadow);
    UiText(kText[matchup], x + 7 + (markW ? markW + 2 : 0), y + 3, EM_WHITE, shadow, 1, x + w);
}

static void BMoveCard(int i, const CtrBattleView *v)
{
    const CtrPokeTouchRect *r = &sBCard[i];
    const CtrBattleMove *m = &v->move[i];
    int focused = v->showCursor && v->cursor == i;
    const EmPal *p = focused ? &kEmAmber : &kEmCream;
    uint16_t label = focused ? UI_RGB(255, 236, 200) : EM_DIM;
    int fy, fh = r->h - B_DEPTH;

    if (!m->present) {
        BHollow(r);
        return;
    }
    fy = BEmCard(r, p, focused, v->pressed == i);
    Fill(r->x + 3, fy + 4, 4, fh - 8, UiTypeColor(m->type));
    UiText(m->name, r->x + 12, fy + 7, p->ink, p->inkSh, 1, r->x + r->w - 58);
    BPill(r->x + r->w - 6 - BPillW(m->typeName), fy + 4, m->typeName, UiTypeColor(m->type), EM_WHITE,
          UiMix(UiTypeColor(m->type), UI_RGB(0, 0, 0), 4));
    UiText("POW", r->x + 12, fy + 25, label, p->inkSh, 1, r->x + 40);
    UiText(m->power[0] ? m->power : "-", r->x + 40, fy + 25, p->ink, p->inkSh, 1, r->x + 72);
    if (m->matchup != CTR_FX_NONE)
        BMatchupPill(m->matchup, r->x + r->w - 6, fy + 22);
    else if (!m->power[0] || m->power[0] == '-')
        UiTextRight("STATUS", r->x + r->w - 8, fy + 25, label, p->inkSh, 1);
    {
        int lit = m->maxPp ? ((int)m->pp * 10 + m->maxPp - 1) / m->maxPp : 0;
        uint16_t on = m->pp * 4 <= m->maxPp ? UI_RGB(232, 110, 30) : m->pp * 2 <= m->maxPp ? UI_RGB(236, 190, 30)
                    : UI_RGB(72, 190, 96);
        /* On the amber card the unlit pips are its dark brown: its orange looked like lit pips (a move at 0 PP read full). */
        uint16_t off = focused ? UiMix(p->lip, p->edge, 4) : UiMix(p->bottom, p->edge, 3);
        char pp[16];

        for (int k = 0; k < 10; ++k)
            UiRoundFill(r->x + 12 + 6 * k, fy + 45, 5, 5, 2, k < lit ? on : off);
        snprintf(pp, sizeof(pp), "%u/%u", m->pp, m->maxPp);
        UiTextRight(pp, r->x + r->w - 8, fy + 43, BEmPpInk(m->pp, m->maxPp, p), p->inkSh, 1);
        UiTextRight("PP", r->x + r->w - 13 - UiTextWidth(pp, 1), fy + 43, label, p->inkSh, 1);
    }
}

/* The note under the moves: accuracy, effect chance and STAB of the move under FireRed's cursor, and its description. */
static void BMoveNote(const CtrBattleView *v)
{
    const CtrPokeTouchRect *n = &sBNote;
    int sel = v->cursor >= 0 && v->cursor < 4 && v->move[v->cursor].present ? v->cursor : 0;
    const CtrBattleMove *m = &v->move[sel];

    TsBgRect(n->x, n->y, n->w, n->h);
    EmNote(n->x, n->y, n->w, n->h);
    if (!m->present)
        return;
    {
        const char *acc = m->accuracy[0] && m->accuracy[0] != '-' ? m->accuracy : "--";
        char val[16];
        int x = n->x + 12;

        UiText("ACC", x, n->y + 7, EM_DIM, EM_DARK_SH, 1, n->x + n->w);
        snprintf(val, sizeof(val), "%s%s", acc, acc[0] == '-' ? "" : "%");
        UiText(val, x + 26, n->y + 7, EM_DARK, EM_DARK_SH, 1, n->x + n->w);
        if (m->effectChance) {
            UiText("EFFECT", x + 70, n->y + 7, EM_DIM, EM_DARK_SH, 1, n->x + n->w);
            snprintf(val, sizeof(val), "%u%%", m->effectChance);
            UiText(val, x + 116, n->y + 7, EM_DARK, EM_DARK_SH, 1, n->x + n->w);
        }
        if (m->stab)
            BPill(n->x + n->w - 8 - BPillW("STAB"), n->y + 4, "STAB", EM_STRIPE, EM_WHITE, UI_RGB(110, 40, 6));
    }
    BDescribeN(m->desc[0] ? m->desc : m->type < 10 ? "Physical move" : "Special move", n->x + 12, n->y + 24,
               n->w - 18, 3, EM_DARK);
}

/* ---- A double battle's targets: the opposing battlers on top, the player's below, as the move cards ---- */
static void BTargetCard(int i, const CtrBattleView *v)
{
    const CtrPokeTouchRect *r = &sBCard[i];
    const CtrBattleTarget *t = &v->target[i];
    int focused = v->showCursor && v->cursor == i;
    const EmPal *p = focused ? &kEmAmber : &kEmCream;
    uint16_t side = t->side ? UI_RGB(60, 120, 230) : UI_RGB(220, 60, 50);
    int fy, fh = r->h - B_DEPTH;
    char text[20];

    if (!t->present) {
        BHollow(r);
        return;
    }
    fy = BEmCard(r, p, focused, v->pressed == i);
    Fill(r->x + 3, fy + 4, 4, fh - 8, side);
    UiText(t->name, r->x + 12, fy + 7, p->ink, p->inkSh, 1, r->x + r->w - 48);
    BPill(r->x + r->w - 6 - BPillW(t->side ? "ALLY" : "FOE"), fy + 4, t->side ? "ALLY" : "FOE", side, EM_WHITE,
          UiMix(side, UI_RGB(0, 0, 0), 4));
    snprintf(text, sizeof(text), "Lv%u", t->level);
    UiText(text, r->x + 12, fy + 25, p->ink, p->inkSh, 1, r->x + 60);
    BMatchupPill(t->matchup, r->x + r->w - 6, fy + 22);
    {
        CtrPokeTouchRect bar = {(int16_t)(r->x + 12), (int16_t)(fy + 45), 70, 6};

        UiRoundFill(bar.x - 1, bar.y - 1, bar.w + 2, bar.h + 2, 2, UI_RGB(64, 72, 104));
        UiHpBar(&bar, t->hp, t->maxHp);
        if (t->side) {
            snprintf(text, sizeof(text), "%u/%u", t->hp, t->maxHp);
            UiTextRight(text, r->x + r->w - 8, fy + 44, p->ink, p->inkSh, 1);
        }
    }
}

static void BTargetNote(void)
{
    const CtrPokeTouchRect *n = &sBNote;

    TsBgRect(n->x, n->y, n->w, n->h);
    EmNote(n->x, n->y, n->w, n->h);
    BPWrap("Which POK\xC3\xA9MON should the move hit?", n->x + 12, n->y + 25, n->w - 18, 2, EM_DARK, EM_DARK_SH);
}

/* ---- The Safari Zone: BALL, BAIT, ROCK, RUN (the words of FireRed's gText_SafariZoneMenu) ---- */
static void BSafariCard(int i, const CtrBattleView *v)
{
    static const EmPal *const pal[4] = {&kEmAmber, &kEmGreen, &kEmBlue, &kEmPurple};
    static const int icon[4] = {BX_BALL, BX_BAIT, BX_ROCK, BX_SHOE};
    static const char *const label[4] = {"BALL", "BAIT", "ROCK", "RUN"};
    const CtrPokeTouchRect *r = &sBSafari[i];
    const EmPal *p = pal[i];
    int fy = BEmCard(r, p, v->showCursor && v->cursor == i, v->pressed == i);

    BDiscIcon(icon[i], r->x + r->w / 2, fy + 34, 25, p);
    UiText(label[i], r->x + (r->w - UiTextWidth(label[i], 1)) / 2, fy + 69, p->ink, p->inkSh, 1, r->x + r->w);
}

/* ---- The battle Bag ---- */
static void BTabs(const CtrBattleView *v)
{
    for (int i = 0; i < 3; ++i) {
        const CtrPokeTouchRect *r = &sBTab[i];
        const EmPal *p = v->bag.pocket == sBTabPocket[i] ? &kEmAmber : &kEmCream;
        int fy = EmButton(r->x, r->y, r->w, r->h, p, 2, v->pressed == BUI_BAG_TAB + i);

        EmLabel(sBTabLabel[i], r->x, r->w, fy, r->h - 2, p);
    }
}

/* A list row: the icon, the name and (but for key items) the quantity; the Bag's selection amber. */
static void BBagRow(int i, const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;
    CtrPokeTouchRect r = BRow(i);
    const EmPal *p = b->first + i == b->cursor ? &kEmAmber : &kEmCream;
    int fy;

    if (!b->row[i].item)
        return;
    fy = EmButton(r.x, r.y, r.w, r.h, p, 2, v->pressed == BUI_BAG_ROW + i);
    BItemDraw(b->row[i].item, r.x + 3, fy, 1);
    UiText(b->row[i].name, r.x + 30, fy + 8, p->ink, p->inkSh, 1, r.x + r.w - 34);
    if (b->pocket != 1) {                       /* key items are one of each: no count */
        char qty[8];

        snprintf(qty, sizeof(qty), "x%u", b->row[i].qty > 999 ? 999 : b->row[i].qty);
        UiTextRight(qty, r.x + r.w - 8, fy + 8, p->ink, p->inkSh, 1);
    }
}

/* A scroll arrow: a dark pill, its arrow lit while the list can go that way. */
static void BBagArrow(const CtrPokeTouchRect *r, int dir, int on)
{
    UiRoundFill(r->x, r->y, r->w, r->h, 5, UI_RGB(24, 8, 10));
    EmGradFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, 4, UI_RGB(72, 26, 26), UI_RGB(44, 14, 16));
    UiArrow(r->x + 7, r->y + r->h / 2 - 3, dir, on ? EM_GLOW : UI_RGB(110, 50, 44));
}

static void BBagList(const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;

    for (int i = 0; i < BUI_BAG_ROWS; ++i)
        BBagRow(i, v);
    if (b->count > BUI_BAG_ROWS) {
        BBagArrow(&sBUpArrow, -2, b->first > 0);
        BBagArrow(&sBDownArrow, 2, b->first + BUI_BAG_ROWS < b->count);
    }
}

/* The selected item's card: its icon on a plate, name, quantity and USE; with FireRed's menu open, the menu. */
static void BBagDetail(const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;
    const CtrPokeTouchRect *d = &sBDetail;
    CtrPokeTouchRect name = {d->x, 0, d->w, 13};

    UiRoundFill(d->x, d->y, d->w, d->h, 5, kEmCream.edge);
    EmGradFill(d->x + 1, d->y + 1, d->w - 2, d->h - 2, 4, kEmCream.top, kEmCream.bottom);
    if (b->menuCount > 0) {
        if (b->selItem) {
            BItemDraw(b->selItem, d->x + 6, d->y + 8, 1);
            UiText(b->selName, d->x + 34, d->y + 14, EM_DARK, EM_DARK_SH, 1, d->x + d->w - 4);
        }
        for (int i = 0; i < b->menuCount && i < 3; ++i) {
            CtrPokeTouchRect r = BMenuRow(i);
            const EmPal *p = b->menuCursor == i ? &kEmAmber : i == b->menuCount - 1 ? &kEmViolet : &kEmCream;
            int fy = EmButton(r.x, r.y, r.w, r.h, p, B_DEPTH, v->pressed == BUI_BAG_MENU + i);

            EmLabel(b->menuName[i], r.x, r.w, fy, r.h - B_DEPTH, p);
        }
        return;
    }
    if (!b->selItem) {
        UiTextCentered("Close the Bag", &name, d->y + 60, EM_DIM, EM_DARK_SH, 1);
        return;
    }
    UiDisc(d->x + d->w / 2, d->y + 34, 27, UiMix(kEmCream.bottom, kEmCream.edge, 2));
    UiDisc(d->x + d->w / 2, d->y + 34, 26, UI_RGB(255, 226, 196));
    BItemDraw(b->selItem, d->x + d->w / 2 - 24, d->y + 10, 2);
    UiTextCentered(b->selName, &name, d->y + 66, EM_DARK, EM_DARK_SH, 1);
    if (b->pocket != 1) {
        char qty[8];

        snprintf(qty, sizeof(qty), "x%u", b->selItem && BBagSelRow(b) >= 0 ? b->row[BBagSelRow(b)].qty : 0);
        if (BBagSelRow(b) >= 0)
            UiTextCentered(qty, &name, d->y + 81, UI_RGB(52, 104, 220), EM_DARK_SH, 1);
    }
    if (BBagSelRow(b) >= 0) {
        int fy = EmButton(sBUse.x, sBUse.y, sBUse.w, sBUse.h, &kEmAmber, B_DEPTH, v->pressed == BUI_BAG_ROW + BBagSelRow(b));

        EmLabel("USE", sBUse.x, sBUse.w, fy, sBUse.h - B_DEPTH, &kEmAmber);
    }
}

/* The Bag's message over the dimmed Bag, in a card (A, or a tap anywhere, goes on). */
static void BBagMessage(const CtrBattleView *v)
{
    CtrPokeTouchRect all = {0, 0, W, H}, box = {20, 70, 280, 76};

    UiDim(&all);
    EmNote(box.x, box.y, box.w, box.h);
    BPWrap(v->bag.message, box.x + 14, box.y + 10, box.w - 24, 3, EM_DARK, EM_DARK_SH);
    UiText("Tap to continue", box.x + box.w - 14 - UiTextWidth("Tap to continue", 1) - 9, box.y + box.h - 16,
           EM_DIM, EM_DARK_SH, 1, box.x + box.w);
    UiArrow(box.x + box.w - 18, box.y + box.h - 16, 1, EM_STRIPE);
}

static void BBag(const CtrBattleView *v)
{
    const CtrPokeTouchRect *n = &sBNote;

    TsBackdrop();
    BTabs(v);
    BBagList(v);
    BBagDetail(v);
    EmNote(n->x, n->y, n->w, n->h);
    if (v->bag.selItem && v->bag.menuCount == 0)
        BDescribeN(v->bag.selDesc, n->x + 12, n->y + 9, n->w - 18, 4, EM_DARK);
    BEmCancel(&sBCancel, v->pressed == BUI_BAG_CANCEL);
    if (v->bag.message[0])
        BBagMessage(v);
}

/* A screen with the party's balls on top. */
static void BScreen(const CtrBattleView *v)
{
    TsBackdrop();
    BStrip(v);
    if (v->state == BUI_ACTION)
        for (int i = 0; i < 4; ++i)
            BActCard(i, v);
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
    return memcmp(&copy, b, sizeof(copy)) == 0;
}

/* Whether a screen with the balls must be drawn again whole: another screen, other balls, other cards. */
static int BScreenChanged(const CtrBattleView *a, const CtrBattleView *b)
{
    if (a->state != b->state || memcmp(a->party, b->party, sizeof(b->party)) || memcmp(a->partyHp, b->partyHp, sizeof(b->partyHp))
        || memcmp(a->partyStatus, b->partyStatus, sizeof(b->partyStatus)))
        return 1;
    if (b->state == BUI_ACTION)
        return strcmp(a->activeName, b->activeName) || a->activeLevel != b->activeLevel;
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
        if (BPartyMenuOnly(old, v)) {
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
                    || (old->pressed == BUI_PTY_SLOT + i) != (v->pressed == BUI_PTY_SLOT + i)) {
                    CtrPokeTouchRect r = BPanel(&v->pty, i);

                    r = UiInset(&r, -2);
                    BPartyPanel(i, v);
                    BPartyEntry(i, v);
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
                const CtrPokeTouchRect *c = v->state == BUI_ACTION ? &sBAct[i] : v->state == BUI_SAFARI ? &sBSafari[i] : &sBCard[i];
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
            BMoveNote(v);
            BMark(dirty, &sBNote);
        }
        if ((v->state == BUI_MOVE || v->state == BUI_TARGET) && (old->pressed == BUI_CANCEL) != (v->pressed == BUI_CANCEL)) {
            BEmCancel(&sBCancel, v->pressed == BUI_CANCEL);
            BMark(dirty, &sBCancel);
        }
    }
    sUiS = &sUiNavy;
}
