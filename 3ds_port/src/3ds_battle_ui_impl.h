/* The battle screen (included at the end of 3ds_poketouch.c, whose drawing helpers and design system it uses).
 * The lower screen while FireRed's battle is on the upper one, in the shared design (3ds_ui_theme.h) wearing
 * PokeTouch's red chrome (sUiRed): the dark red backdrop, the strip of six Poke Balls on top, then the body: the MAIN
 * buttons in the chrome's reds (FIGHT the brightest), the move cards in their type's colour, the target cards (the
 * opposing side red, the player's blue), the Safari Zone's four, the battle Bag and the battle Party. Selection and
 * focus keep the blue and the white ring PokeTouch uses. Everything shown is what the controller reports; nothing
 * here moves or resizes a rectangle. */

#include "3ds_icons.h"

static const CtrPokeTouchRect sBStrip = {3, 3, 314, 31};
static const CtrPokeTouchRect sBBody = {3, 34, 314, 203};
static const CtrPokeTouchRect sBAction[4] = {      /* by the controller's action number */
    {8, 40, 304, 98},               /* FIGHT */
    {8, 144, 98, 84},               /* BAG */
    {214, 144, 98, 84},             /* POKeMON */
    {111, 144, 98, 84},             /* RUN */
};
static const CtrPokeTouchRect sBCard[4] = {{6, 40, 151, 68}, {163, 40, 151, 68}, {6, 114, 151, 69}, {163, 114, 151, 69}};
static const CtrPokeTouchRect sBCancel = {6, 190, 308, 30};
/* The Safari Zone's four actions in the controller's 2x2 order (BALL, BAIT, ROCK, RUN). */
static const CtrPokeTouchRect sBSafari[4] = {{8, 42, 150, 90}, {162, 42, 150, 90}, {8, 138, 150, 90}, {162, 138, 150, 90}};
/* The battle Bag: three tabs (the Bag's real pockets), the item list on the left, the detail on the right, CANCEL. */
static const CtrPokeTouchRect sBTab[3] = {{8, 36, 100, 26}, {111, 36, 100, 26}, {214, 36, 98, 26}};
static const int sBTabPocket[3] = {0, 2, 1};            /* the tabs show Items, Poke Balls, Key Items: FireRed's pockets 0, 2, 1 */
static const char *const sBTabLabel[3] = {"ITEMS", "BALLS", "KEY ITEMS"};
static const CtrPokeTouchRect sBList = {5, 66, 172, 134};
static const CtrPokeTouchRect sBDetail = {181, 66, 133, 134};
static const CtrPokeTouchRect sBBagCancel = {5, 204, 309, 27};
static const CtrPokeTouchRect sBUpArrow = {155, 68, 20, 14}, sBDownArrow = {155, 184, 20, 14};

static CtrPokeTouchRect BRow(int i)
{
    CtrPokeTouchRect r = {9, (int16_t)(70 + 25 * i), 150, 24};
    return r;
}

static CtrPokeTouchRect BMenuRow(int i)
{
    CtrPokeTouchRect r = {187, (int16_t)(104 + 30 * i), 121, 26};
    return r;
}
/* The battle Party: a header, the six Pokemon in two columns of three, CANCEL. */
static const CtrPokeTouchRect sBPHeader = {8, 36, 304, 23};
static const CtrPokeTouchRect sBPCancel = {8, 211, 304, 23};

static CtrPokeTouchRect BPanel(int i)
{
    CtrPokeTouchRect r = {(int16_t)(i & 1 ? 163 : 6), (int16_t)(62 + 49 * (i >> 1)), 151, 47};
    return r;
}

static CtrPokeTouchRect BPMenuRow(int count, int i)
{
    CtrPokeTouchRect r = {104, (int16_t)(124 - 15 * count + 30 * i), 112, 26};
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

int CtrBattleUi_Hit(const CtrBattleView *v, int x, int y)
{
    if (v->state == BUI_ACTION)
        for (int i = 0; i < 4; ++i)
            if (BIn(&sBAction[i], x, y))
                return i;
    if (v->state == BUI_MOVE) {
        for (int i = 0; i < 4; ++i)
            if (v->move[i].present && BIn(&sBCard[i], x, y))
                return i;
        if (BIn(&sBCancel, x, y))
            return BUI_CANCEL;
    }
    if (v->state == BUI_TARGET) {
        for (int i = 0; i < 4; ++i)
            if (v->target[i].present && BIn(&sBCard[i], x, y))
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
            int n = p->menuCount > 6 ? 6 : p->menuCount;

            for (int i = 0; i < n; ++i) {
                CtrPokeTouchRect r = BPMenuRow(n, i);
                if (BIn(&r, x, y))
                    return BUI_PTY_MENU + i;
            }
            return -1;
        }
        if (p->mode != BUI_PTY_FORCED && BIn(&sBPCancel, x, y))
            return BUI_PTY_CANCEL;
        for (int i = 0; i < 6; ++i) {
            CtrPokeTouchRect r = BPanel(i);
            if (p->present[i] && BIn(&r, x, y))
                return BUI_PTY_SLOT + i;
        }
        return -1;
    }
    if (v->state == BUI_BAG) {
        const CtrBattleBag *b = &v->bag;

        if (BIn(&sBBagCancel, x, y))
            return BUI_BAG_CANCEL;
        for (int i = 0; i < 3; ++i)
            if (BIn(&sBTab[i], x, y))
                return BUI_BAG_TAB + i;
        if (b->message[0] && BIn(&sBDetail, x, y))
            return BUI_BAG_MESSAGE;
        if (b->menuCount > 0 && BIn(&sBDetail, x, y)) {
            for (int i = 0; i < b->menuCount && i < 3; ++i) {
                CtrPokeTouchRect r = BMenuRow(i);
                if (BIn(&r, x, y))
                    return BUI_BAG_MENU + i;
            }
            return -1;
        }
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

/* A Poke Ball of the party strip: red and white for a Pokemon that can battle, grey for one that fainted, an empty
 * socket for no Pokemon. */
static void BallDraw(int cx, int cy, int kind)
{
    const uint16_t sTop[3] = {UIS_PANEL_LO, UI_RGB(226, 62, 52), UI_RGB(120, 128, 146)};

    for (int dy = -10; dy < 10; ++dy)
        for (int dx = -10; dx < 10; ++dx) {
            int d2 = (2 * dx + 1) * (2 * dx + 1) + (2 * dy + 1) * (2 * dy + 1);    /* in half pixels, radius 10 */
            uint16_t c;

            if (d2 > 20 * 20)
                continue;
            if (kind == 0)
                c = d2 > 17 * 17 ? UIS_BORDER : UIS_PANEL_LO;
            else if (d2 > 17 * 17 || (dy >= -1 && dy < 1))
                c = UIS_OUTLINE;
            else if (d2 < 6 * 6)
                c = d2 < 3 * 3 ? UI_TEXT : UIS_OUTLINE;
            else if (dy < 0)
                c = dx < -2 && dy < -4 && kind == 1 ? UI_RGB(255, 140, 120) : sTop[kind];
            else
                c = kind == 2 ? UI_RGB(170, 176, 190) : UI_RGB(236, 240, 246);
            Px(cx + dx, cy + dy, c);
        }
}

/* Exactly six slots, one for each of the player's party places, on a dark strip. */
static void BStrip(const CtrBattleView *v)
{
    static const int xs[6] = {50, 94, 138, 182, 226, 270};

    /* The chrome panel of PokeTouch: a light line under the top edge, a dark one over the bottom. */
    UiPanel(&sBStrip, UIS_PANEL, UIS_BORDER);
    Fill(sBStrip.x + 3, sBStrip.y + 1, sBStrip.w - 6, 1, UIS_BORDER_LIT);
    Fill(sBStrip.x + 3, sBStrip.y + sBStrip.h - 2, sBStrip.w - 6, 1, UIS_PANEL_LO);
    for (int i = 0; i < 6; ++i)
        BallDraw(xs[i], 18, v->party[i]);
}

/* PokeTouch's side buttons carry a coloured tick to tell them apart on the one red; the battle's wear it on their top
 * edge, centred (the left edge is where the longest label, POKeMON, reaches): FIGHT amber, BAG green, POKeMON blue,
 * RUN yellow. */
static void BAccent(const CtrPokeTouchRect *r, int dy, uint16_t color)
{
    int x = r->x + (r->w - 28) / 2;

    UiRoundFill(x, r->y + 6 + dy, 28, 6, 2, UIS_OUTLINE);
    UiRoundFill(x + 1, r->y + 7 + dy, 26, 4, 1, color);
    Fill(x + 3, r->y + 7 + dy, 22, 1, UiMix(color, UI_TEXT, 3));
}

static void BActionButton(int i, const CtrBattleView *v)
{
    static const uint16_t sAccent[4] = {UI_RGB(255, 190, 80), UI_RGB(72, 206, 104), UI_RGB(70, 172, 255), UI_RGB(246, 214, 80)};
    const UiButtonStyle *sStyle[4] = {&sUiS->fight, &sUiS->bag, &sUiS->pokemon, &sUiS->run};
    static const char *const label[4] = {"FIGHT", "BAG", "POK\xC3\xA9MON", "RUN"};
    static const int sIcon[4] = {BI_BURST, BI_BAG, BI_POKEBALL, BI_SHOE};
    const CtrPokeTouchRect *r = &sBAction[i];
    int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);

    BIconsPrepare();
    if (i == BUI_ACT_FIGHT) {
        int top = UiButtonLabelTop(r, state, 3), w = UiTextWidth(label[i], 3), x = r->x + (r->w - w + 34) / 2;

        UiButton(r, sStyle[i], state);
        BAccent(r, (state & UI_PRESSED) ? 1 : 0, sAccent[i]);
        BIconAt(BI_BURST, x - 34, top + 2);
        UiText(label[i], x, top, UI_TEXT, sStyle[i]->shade, 3, r->x + r->w);
    } else {
        UiIconButton(r, sStyle[i], state, &sBImg[sIcon[i]].image, sBImg[sIcon[i]].scale, label[i], 2);
        BAccent(r, (state & UI_PRESSED) ? 1 : 0, sAccent[i]);
    }
}

static void BAction(const CtrBattleView *v)
{
    for (int i = 0; i < 4; ++i)
        BActionButton(i, v);
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

/* A card of the battle (a move, a target): raised, its header band in `color` with `title`, the body a dark tint of
 * the same colour. Returns the vertical offset of its content (1 while pressed). An empty slot is a dark socket. */
static int BCardFrame(const CtrPokeTouchRect *r, uint16_t color, const char *title, int state)
{
    int pressed = (state & UI_PRESSED) != 0, dy = pressed ? 1 : 0;
    uint16_t head = pressed ? UiMix(color, UIS_OUTLINE, 2) : color;
    int bottom = r->h - 2 - (pressed ? 1 : UI_DEPTH);

    UiRoundRect(r, UI_RADIUS_BUTTON, UIS_OUTLINE);
    if (!title) {
        UiRoundFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, UI_RADIUS_BUTTON - 1, UIS_PANEL_LO);
        UiTextCentered("-", r, -1, UI_TEXT_OFF, UIS_TEXT_SH, 1);
        return 0;
    }
    if (!pressed)
        UiRoundFill(r->x + 1, r->y + 1, r->w - 2, r->h - 2, UI_RADIUS_BUTTON - 1, UiMix(color, UIS_OUTLINE, 5));
    UiRoundFill(r->x + 1, r->y + 1 + dy, r->w - 2, bottom, UI_RADIUS_BUTTON - 1, UiMix(UIS_PANEL, color, pressed ? 1 : 2));
    UiRoundFill(r->x + 1, r->y + 1 + dy, r->w - 2, 20, UI_RADIUS_BUTTON - 1, head);
    Fill(r->x + 1, r->y + 18 + dy, r->w - 2, 3, head);
    Fill(r->x + 3, r->y + 2 + dy, r->w - 6, 8, UiMix(head, UI_TEXT, 2));
    Fill(r->x + 1, r->y + 21 + dy, r->w - 2, 1, UiMix(color, UIS_OUTLINE, 4));
    {
        uint16_t ink = UiInkOn(head);

        UiText(title, r->x + 8, r->y + 7 + dy, ink, ink == UI_TEXT ? UiMix(color, UIS_OUTLINE, 6) : UiMix(head, UI_TEXT, 3), 1,
               r->x + r->w - 6);
    }
    if (state & UI_FOCUSED)
        UiFocus(r);
    return dy;
}

/* The PP colour FireRed's move list uses: the usual ink, then yellow at half, orange at a quarter, red at none. */
static uint16_t BPpColor(unsigned pp, unsigned max)
{
    return pp == 0 ? UI_HP_LOW : pp * 4 <= max ? UI_WARN : pp * 2 <= max ? UI_HP_MID : UI_TEXT;
}

/* A move card: the name on the type's colour, the type badge and the power, the PP left (number and bar). */
static void BCard(int i, const CtrBattleView *v)
{
    const CtrPokeTouchRect *r = &sBCard[i];
    const CtrBattleMove *m = &v->move[i];
    int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);
    int dy;

    if (!m->present) {
        BCardFrame(r, 0, NULL, 0);
        return;
    }
    dy = BCardFrame(r, UiTypeColor(m->type), m->name, state);
    {
        CtrPokeTouchRect badge = {(int16_t)(r->x + 8), (int16_t)(r->y + 28 + dy), 52, 14};
        CtrPokeTouchRect bar = {(int16_t)(r->x + 70), (int16_t)(r->y + 51 + dy), (int16_t)(r->w - 78), 5};
        char pp[12];

        UiTypeBadge(&badge, m->type, m->typeName);
        UiText("PWR", r->x + r->w - 50, r->y + 31 + dy, UIS_TEXT_DIM, UIS_TEXT_SH, 1, r->x + r->w);
        UiTextRight(m->power, r->x + r->w - 8, r->y + 31 + dy, UI_TEXT, UIS_TEXT_SH, 1);
        UiText("PP", r->x + 8, r->y + 48 + dy, UIS_TEXT_DIM, UIS_TEXT_SH, 1, r->x + 30);
        snprintf(pp, sizeof(pp), "%u/%u", m->pp, m->maxPp);
        UiText(pp, r->x + 24, r->y + 48 + dy, BPpColor(m->pp, m->maxPp), UIS_TEXT_SH, 1, r->x + 68);
        UiProgressBar(&bar, m->pp, m->maxPp, m->pp * 4 <= m->maxPp ? UI_WARN : UI_FOCUS);
    }
}

static void BCancel(const CtrPokeTouchRect *r, int pressed)
{
    UiLabelButton(r, &sUiS->cancel, pressed ? UI_PRESSED : 0, "CANCEL", r->h >= 28 ? 2 : 1);
}

static void BMove(const CtrBattleView *v)
{
    for (int i = 0; i < 4; ++i)
        BCard(i, v);
    BCancel(&sBCancel, v->pressed == BUI_CANCEL);
}

/* Aiming a move in a double battle: a card for each battler FireRed lets it be aimed at, the opposing ones on top in
 * FIGHT's red, the player's below in POKeMON's blue: the name, the level, the side and the HP bar (the numbers too for
 * the player's own). */
static void BTargetCard(int i, const CtrBattleView *v)
{
    static const UiButtonStyle sFoe = UI_STYLE_FIGHT, sAlly = UI_STYLE_POKEMON;
    const CtrPokeTouchRect *r = &sBCard[i];
    const CtrBattleTarget *t = &v->target[i];
    int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);
    int dy;

    if (!t->present) {
        BCardFrame(r, 0, NULL, 0);
        return;
    }
    dy = BCardFrame(r, t->side ? sAlly.body : sFoe.body, t->name, state);
    {
        char level[12], hp[20];
        CtrPokeTouchRect tag = {(int16_t)(r->x + r->w - 46), (int16_t)(r->y + 27 + dy), 38, 14};
        CtrPokeTouchRect bar = {(int16_t)(r->x + 8), (int16_t)(r->y + 46 + dy), (int16_t)(r->w - 16), 6};

        snprintf(level, sizeof(level), "Lv%u", t->level);
        UiText(level, r->x + 8, r->y + 30 + dy, UI_TEXT, UIS_TEXT_SH, 1, r->x + 60);
        UiBadge(&tag, t->side ? sAlly.shade : sFoe.shade, t->side ? "ALLY" : "FOE");
        UiHpBar(&bar, t->hp, t->maxHp);
        if (t->side) {
            snprintf(hp, sizeof(hp), "%u/%u", t->hp, t->maxHp);
            UiTextRight(hp, r->x + r->w - 8, r->y + 55 + dy, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
        }
    }
}

static void BTarget(const CtrBattleView *v)
{
    for (int i = 0; i < 4; ++i)
        BTargetCard(i, v);
    BCancel(&sBCancel, v->pressed == BUI_CANCEL);
}

/* The Safari Zone's menu: BALL, BAIT, ROCK, RUN (the words of FireRed's gText_SafariZoneMenu), as MAIN's buttons. */
static void BSafari(const CtrBattleView *v)
{
    const UiButtonStyle *sStyle[4] = {&sUiS->fight, &sUiS->bag, &sUiS->pokemon, &sUiS->run};
    static const char *const label[4] = {"BALL", "BAIT", "ROCK", "RUN"};
    static const int sSafariIcon[4] = {BI_POKEBALL, BI_BAIT, BI_ROCK, BI_SHOE};
    static const uint16_t sSafariAccent[4] = {UI_RGB(70, 172, 255), UI_RGB(236, 112, 140), UI_RGB(190, 182, 170), UI_RGB(72, 206, 104)};

    BIconsPrepare();
    for (int i = 0; i < 4; ++i) {
        const CtrPokeTouchRect *r = &sBSafari[i];
        int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);
        int dy = (state & UI_PRESSED) ? 1 : 0, cx = r->x + r->w / 2, cy = r->y + 30 + dy;

        UiButton(r, sStyle[i], state);
        BAccent(r, dy, sSafariAccent[i]);
        BIconAt(sSafariIcon[i], cx - 12, cy - 12 + 4);
        UiText(label[i], cx - UiTextWidth(label[i], 2) / 2, r->y + 52 + dy, UI_TEXT, sStyle[i]->shade, 2, r->x + r->w);
    }
}

/* The battle Party: a header, the six Pokemon in two columns of three, CANCEL. */
extern int CtrBottomParty_Icon(int slot, unsigned char *tilesOut, unsigned short *palOut);
/* Party icons: FireRed's 32x32 icon of a slot, decoded once per species and slot. */
static struct {
    unsigned icon;
    int ok;
    uint8_t pixels[32 * 32];
    uint16_t pal[16];
} sBPIcon[6];

static int BPartyIcon(int slot, unsigned icon)
{
    static unsigned char tiles[512];
    static unsigned short pal[16];

    if (sBPIcon[slot].icon == icon + 1)
        return sBPIcon[slot].ok;
    memset(tiles, 0, sizeof(tiles));
    sBPIcon[slot].icon = icon + 1;
    sBPIcon[slot].ok = CtrBottomParty_Icon(slot, tiles, pal);
    if (!sBPIcon[slot].ok)
        return 0;
    for (int y = 0; y < 32; ++y)
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

/* A Party card: the icon on its plate, the name and level, the HP bar and numbers, a status badge. The Party's
 * selection is the blue of a selection with a bright ring; a Pokemon that fainted is tinted red. */
static void BPartyPanel(int i, const CtrBattleView *v)
{
    static const char *const code[7] = {"", "PSN", "PAR", "SLP", "FRZ", "BRN", "PKRS"};
    static const uint16_t tint[7] = {0, UI_RGB(170, 80, 190), UI_RGB(214, 170, 30), UI_RGB(120, 128, 150),
                                     UI_RGB(80, 170, 220), UI_RGB(226, 96, 48), UI_RGB(200, 90, 160)};
    const CtrBattleParty *p = &v->pty;
    CtrPokeTouchRect r = BPanel(i);
    int fainted = p->present[i] && !p->egg[i] && p->hp[i] == 0, selected = p->cursor == i;
    int press = v->pressed == BUI_PTY_SLOT + i;
    uint16_t body = press ? UI_SELECT_HI : selected ? UI_SELECT : fainted ? UiMix(UIS_PANEL, UIS_OUTLINE, 4) : UIS_PANEL;

    if (!p->present[i]) {
        UiRoundRect(&r, UI_RADIUS_PANEL, UIS_SEPARATOR);
        UiRoundFill(r.x + 1, r.y + 1, r.w - 2, r.h - 2, UI_RADIUS_PANEL - 1, UIS_BG);
        return;
    }
    UiPanel(&r, body, selected ? UI_FOCUS : fainted ? UiMix(UIS_BORDER, UI_DANGER, 4) : UIS_BORDER);
    if (selected) {
        CtrPokeTouchRect ring = UiInset(&r, 1);

        UiRing(&ring, UI_RADIUS_PANEL - 1, 1, UI_FOCUS_LIT);
    }
    UiRoundFill(r.x + 5, r.y + 5, 37, 37, 3, UiMix(body, UIS_OUTLINE, 4));
    if (BPartyIcon(i, p->icon[i]))
        for (int y = 0; y < 32; ++y)
            for (int x = 0; x < 32; ++x) {
                uint8_t index = sBPIcon[i].pixels[y * 32 + x];
                if (index)
                    Px(r.x + 7 + x, r.y + 6 + y, fainted ? Tone(sBPIcon[i].pal[index], 0) : sBPIcon[i].pal[index]);
            }
    UiText(p->name[i], r.x + 47, r.y + 6, UI_TEXT, UIS_TEXT_SH, 1, r.x + r.w - 30);
    if (p->egg[i])
        return;
    {
        char text[24];
        CtrPokeTouchRect bar = {(int16_t)(r.x + 47), (int16_t)(r.y + 20), (int16_t)(r.w - 54), 6};

        snprintf(text, sizeof(text), "Lv%u", p->level[i]);
        UiTextRight(text, r.x + r.w - 6, r.y + 6, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
        UiHpBar(&bar, p->hp[i], p->maxHp[i]);
        if (fainted || (p->status[i] >= 1 && p->status[i] <= 6)) {
            CtrPokeTouchRect badge = {(int16_t)(r.x + 47), (int16_t)(r.y + 30), 30, 13};

            UiBadge(&badge, fainted ? UI_DANGER : tint[p->status[i]], fainted ? "FNT" : code[p->status[i]]);
        }
        snprintf(text, sizeof(text), "%u/%u", p->hp[i], p->maxHp[i]);
        UiTextRight(text, r.x + r.w - 6, r.y + 32, UI_TEXT, UIS_TEXT_SH, 1);
    }
}

/* What the Party has open over its slots, as a modal over the dimmed Party: its menu, or the message it waits on. */
static void BPartyOverlay(const CtrBattleView *v)
{
    const UiButtonStyle *sFirst = &sUiS->primary, *sMiddle = &sUiS->neutral, *sLast = &sUiS->cancel;
    const CtrBattleParty *p = &v->pty;

    if (p->panel == BUI_PTY_TEXT) {
        CtrPokeTouchRect box = {20, 92, 280, 68};

        UiModal(&sBBody, &box);
        BDescribeN(p->message, box.x + 10, box.y + 10, box.w - 20, 3, UI_TEXT);
        UiText("Tap to continue", box.x + box.w - 10 - UiTextWidth("Tap to continue", 1) - 9, box.y + box.h - 16,
               UIS_TEXT_DIM, UIS_TEXT_SH, 1, box.x + box.w);
        UiArrow(box.x + box.w - 16, box.y + box.h - 16, 1, UI_FOCUS);
    } else if (p->panel == BUI_PTY_ACTIONS || p->panel == BUI_PTY_YESNO) {
        int n = p->menuCount > 6 ? 6 : p->menuCount;
        CtrPokeTouchRect frame = {98, (int16_t)(124 - 15 * n - 8), 124, (int16_t)(30 * n + 12)};

        UiModal(&sBBody, &frame);
        for (int i = 0; i < n; ++i) {
            CtrPokeTouchRect r = BPMenuRow(n, i);
            int state = (v->pressed == BUI_PTY_MENU + i ? UI_PRESSED : 0) | (p->menuCursor == i ? UI_FOCUSED : 0);

            UiLabelButton(&r, i == 0 ? sFirst : i == n - 1 ? sLast : sMiddle, state, p->menuName[i], 1);
        }
    }
}

static void BParty(const CtrBattleView *v)
{
    static const char *const sCaption[3] = {"Choose a POK\xC3\xA9MON.", "Use on which one?", "Send one out."};

    UiHeader(&sBPHeader, "POK\xC3\xA9MON", sCaption[v->pty.mode < 3 ? v->pty.mode : 0]);
    for (int i = 0; i < 6; ++i)
        BPartyPanel(i, v);
    if (v->pty.mode != BUI_PTY_FORCED)
        BCancel(&sBPCancel, v->pressed == BUI_PTY_CANCEL);
    else
        UiFooter(&sBPCancel, "A POK\xC3\xA9MON must be sent out.");
    BPartyOverlay(v);
}

/* Item icons: FireRed's 24x24 icon of an item, decoded once and kept for the few that are on screen. */
#define BICONS 12
static struct {
    unsigned item;
    int ok;
    uint8_t pixels[24 * 24];
    uint16_t pal[16];
} sBIcon[BICONS];

static int BItemIcon(unsigned item)
{
    static unsigned char tiles[0x200];
    static unsigned short pal[16];
    int slot = (int)(item % BICONS);

    if (item == 0)
        return -1;
    if (sBIcon[slot].item == item)
        return sBIcon[slot].ok ? slot : -1;
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

static void BTabs(const CtrBattleView *v)
{
    for (int i = 0; i < 3; ++i)
        UiTab(&sBTab[i], sBTabLabel[i], (v->bag.pocket == sBTabPocket[i] ? UI_SELECTED : 0)
                                        | (v->pressed == BUI_BAG_TAB + i ? UI_PRESSED : 0));
}

/* A list row: the icon, the name and (but for key items) the quantity; the Bag's selection is the blue of a selection
 * with a bright edge, the row under a touch the pressed blue. */
static void BBagRow(int i, const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;
    CtrPokeTouchRect r = BRow(i);
    int selected = b->first + i == b->cursor, press = v->pressed == BUI_BAG_ROW + i;

    Fill(r.x, r.y, r.w, r.h, UIS_PANEL_LO);
    if (!b->row[i].item)
        return;
    if (selected || press) {
        UiRoundFill(r.x, r.y, r.w, r.h - 1, UI_RADIUS_PANEL, press ? UI_SELECT_HI : UI_SELECT);
        Fill(r.x + 1, r.y + 4, 2, r.h - 9, UI_FOCUS_LIT);
    } else
        UiSeparator(r.x + 4, r.y + r.h - 1, r.w - 8);
    BItemDraw(b->row[i].item, r.x + 5, r.y, 1);
    UiText(b->row[i].name, r.x + 32, r.y + 8, UI_TEXT, UIS_TEXT_SH, 1, r.x + r.w - 30);
    if (b->pocket != 1) {                       /* key items are one of each: no count */
        char qty[8];

        snprintf(qty, sizeof(qty), "x%u", b->row[i].qty > 999 ? 999 : b->row[i].qty);
        UiTextRight(qty, r.x + r.w - 6, r.y + 8, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
    }
}

static void BBagList(const CtrBattleView *v)
{
    const CtrBattleBag *b = &v->bag;

    UiPanel(&sBList, UIS_PANEL_LO, UIS_BORDER);
    for (int i = 0; i < BUI_BAG_ROWS; ++i)
        BBagRow(i, v);
    if (b->count > BUI_BAG_ROWS) {
        if (b->first > 0)
            UiArrow(sBUpArrow.x + 7, sBUpArrow.y + 4, -2, UI_FOCUS);
        if (b->first + BUI_BAG_ROWS < b->count)
            UiArrow(sBDownArrow.x + 7, sBDownArrow.y + 4, 2, UI_FOCUS);
    }
}

/* The right panel: the selected item (icon on its plate, name, description); over it the open menu or the Bag's
 * message. */
static void BBagDetail(const CtrBattleView *v)
{
    const UiButtonStyle *sFirst = &sUiS->primary, *sOther = &sUiS->cancel;
    const CtrBattleBag *b = &v->bag;
    CtrPokeTouchRect plate = {218, 74, 58, 56}, name = {185, 136, 125, 12};

    UiPanel(&sBDetail, UIS_PANEL, UIS_BORDER);
    if (b->message[0]) {
        BDescribeN(b->message, 189, 76, 117, 7, UI_TEXT);
        UiText("Tap", 260, 180, UIS_TEXT_DIM, UIS_TEXT_SH, 1, 300);
        UiArrow(282, 180, 1, UI_FOCUS);
        return;
    }
    if (b->menuCount > 0) {
        if (b->selItem) {
            BItemDraw(b->selItem, 189, 74, 1);
            UiText(b->selName, 217, 82, UI_TEXT, UIS_TEXT_SH, 1, 310);
        }
        for (int i = 0; i < b->menuCount && i < 3; ++i) {
            CtrPokeTouchRect r = BMenuRow(i);
            int state = (v->pressed == BUI_BAG_MENU + i ? UI_PRESSED : 0) | (b->menuCursor == i ? UI_FOCUSED : 0);

            UiLabelButton(&r, i == 0 ? sFirst : sOther, state, b->menuName[i], 1);
        }
        return;
    }
    UiRoundFill(plate.x, plate.y, plate.w, plate.h, UI_RADIUS_BUTTON, UIS_PANEL_LO);
    if (b->selItem) {
        BItemDraw(b->selItem, plate.x + 5, plate.y + 4, 2);
        UiTextCentered(b->selName, &name, name.y, UI_TEXT, UIS_TEXT_SH, 1);
        UiSeparator(189, 151, 117);
        BDescribeN(b->selDesc, 189, 157, 117, 3, UIS_TEXT_DIM);
    } else
        UiTextCentered("Close the Bag", &name, name.y, UIS_TEXT_DIM, UIS_TEXT_SH, 1);
}

static void BBag(const CtrBattleView *v)
{
    BTabs(v);
    BBagList(v);
    BBagDetail(v);
    BCancel(&sBBagCancel, v->pressed == BUI_BAG_CANCEL);
}

/* The body: the navy backdrop and the screen for the controller's state (nothing at all while it is busy). */
static void BBody(const CtrBattleView *v)
{
    Fill(sBBody.x, sBBody.y, sBBody.w, sBBody.h, UIS_BG);
    if (v->state == BUI_ACTION)
        BAction(v);
    else if (v->state == BUI_MOVE)
        BMove(v);
    else if (v->state == BUI_BAG)
        BBag(v);
    else if (v->state == BUI_PARTY)
        BParty(v);
    else if (v->state == BUI_TARGET)
        BTarget(v);
    else if (v->state == BUI_SAFARI)
        BSafari(v);
}

void CtrBattleUi_Draw(uint16_t *canvas, const CtrBattleView *v)
{
    sCanvas = canvas;
    sUiS = &sUiRed;
    Fill(0, 0, W, H, UIS_BG);
    Fill(0, 0, W, 2, UIS_BG_TOP);
    BStrip(v);
    BBody(v);
    sUiS = &sUiNavy;
}

/* Each changed battle element is presented through the shared dirty-rectangle path. */
static void BMark(CtrDirtyList *dirty, const CtrPokeTouchRect *r)
{
    CtrDirty_Add(dirty, r->x, r->y, r->x + r->w, r->y + r->h);
}

void CtrBattleUi_Update(uint16_t *canvas, const CtrBattleView *old, const CtrBattleView *v, CtrDirtyList *dirty)
{
    sCanvas = canvas;
    sUiS = &sUiRed;
    CtrDirty_Clear(dirty);
    if (memcmp(old->party, v->party, sizeof(v->party))) {
        BStrip(v);
        BMark(dirty, &sBStrip);
    }
    if (old->state != v->state || (v->state == BUI_MOVE && memcmp(old->move, v->move, sizeof(v->move)))
        || (v->state == BUI_BAG && memcmp(&old->bag, &v->bag, sizeof(v->bag)))
        || (v->state == BUI_BAG && old->pressed != v->pressed)
        || (v->state == BUI_PARTY && (memcmp(&old->pty, &v->pty, sizeof(v->pty)) || old->pressed != v->pressed))
        || ((v->state == BUI_TARGET || v->state == BUI_SAFARI)
            && (memcmp(old->target, v->target, sizeof(v->target)) || old->pressed != v->pressed
                || old->cursor != v->cursor || old->showCursor != v->showCursor))) {
        BBody(v);
        BMark(dirty, &sBBody);
    } else if (v->state == BUI_MOVE && (old->pressed != v->pressed || old->cursor != v->cursor || old->showCursor != v->showCursor)) {
        /* The cards and CANCEL do not overlap and each draws only inside its rectangle: only the ones that look
         * different (pressed or focused, or no longer) are redrawn and presented. */
        for (int i = 0; i < 4; ++i)
            if (BLooksChanged(old, v, i)) {
                Fill(sBCard[i].x, sBCard[i].y, sBCard[i].w, sBCard[i].h, UIS_BG);
                BCard(i, v);
                BMark(dirty, &sBCard[i]);
            }
        if ((old->pressed == BUI_CANCEL) != (v->pressed == BUI_CANCEL)) {
            Fill(sBCancel.x, sBCancel.y, sBCancel.w, sBCancel.h, UIS_BG);
            BCancel(&sBCancel, v->pressed == BUI_CANCEL);
            BMark(dirty, &sBCancel);
        }
    } else if (v->state == BUI_ACTION
               && (old->pressed != v->pressed || old->cursor != v->cursor || old->showCursor != v->showCursor)) {
        /* The buttons do not overlap: only the ones that look different, over their own rectangles. */
        for (int i = 0; i < 4; ++i)
            if (BLooksChanged(old, v, i)) {
                Fill(sBAction[i].x, sBAction[i].y, sBAction[i].w, sBAction[i].h, UIS_BG);
                BActionButton(i, v);
                BMark(dirty, &sBAction[i]);
            }
    }
    sUiS = &sUiNavy;
}
