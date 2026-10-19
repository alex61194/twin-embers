/* The battle screen (included at the end of 3ds_poketouch.c, whose drawing helpers and design system it uses).
 * The lower screen while FireRed's battle is on the upper one, in the shared design (3ds_ui_theme.h) wearing
 * PokeTouch's red chrome (sUiRed): the dark red backdrop, the strip of six Poke Balls on top, then the body: the MAIN
 * buttons in the chrome's reds (FIGHT the brightest), the move cards in their type's colour, the target cards (the
 * opposing side red, the player's blue), the Safari Zone's four, the battle Bag and the battle Party. Selection and
 * focus keep the blue and the white ring PokeTouch uses. Everything shown is what the controller reports; nothing
 * here moves or resizes a rectangle. */

#include "3ds_icons.h"
#include "3ds_type_chart.h"

static const CtrPokeTouchRect sBStrip = {3, 3, 314, 31};
static const CtrPokeTouchRect sBBody = {3, 34, 314, 203};
static const CtrPokeTouchRect sBAction[4] = {      /* by the controller's action number */
    {8, 40, 304, 104},              /* FIGHT: the priority, a little taller than the row */
    {8, 150, 98, 78},               /* BAG */
    {214, 150, 98, 78},             /* POKeMON */
    {111, 150, 98, 78},             /* RUN */
};
static const CtrPokeTouchRect sBCard[4] = {{6, 40, 151, 68}, {163, 40, 151, 68}, {6, 114, 151, 69}, {163, 114, 151, 69}};
static const CtrPokeTouchRect sBMoveFooter = {6, 188, 218, 44};
static const CtrPokeTouchRect sBMoveCancel = {228, 188, 86, 44};
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
        if (BIn(&sBMoveCancel, x, y))
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

/* A Poke Ball of the party strip: red and white for a healthy Pokemon, amber-gold for one with a status condition
 * (kind == 3), grey for one that fainted (kind == 2), an empty socket for no Pokemon (kind == 0). */
static void BallDraw(int cx, int cy, int kind)
{
    const uint16_t sTop[4] = {UIS_PANEL_LO, UI_RGB(226, 62, 52), UI_RGB(120, 128, 146), UI_RGB(236, 174, 36)};
    int k = kind >= 0 && kind <= 3 ? kind : 0;

    for (int dy = -10; dy < 10; ++dy)
        for (int dx = -10; dx < 10; ++dx) {
            int d2 = (2 * dx + 1) * (2 * dx + 1) + (2 * dy + 1) * (2 * dy + 1);    /* in half pixels, radius 10 */
            uint16_t c;

            if (d2 > 20 * 20)
                continue;
            if (k == 0)
                c = d2 > 17 * 17 ? UIS_BORDER : UIS_PANEL_LO;
            else if (d2 > 17 * 17 || (dy >= -1 && dy < 1))
                c = UIS_OUTLINE;
            else if (d2 < 6 * 6)
                c = d2 < 3 * 3 ? UI_TEXT : UIS_OUTLINE;
            else if (dy < 0)
                c = dx < -2 && dy < -4 && k == 1 ? UI_RGB(255, 140, 120)
                  : dx < -2 && dy < -4 && k == 3 ? UI_RGB(255, 228, 120) : sTop[k];
            else
                c = k == 2 ? UI_RGB(170, 176, 190) : UI_RGB(236, 240, 246);
            Px(cx + dx, cy + dy, c);
        }
}

/* Subtle circular watermark Pokéball, strictly clipped within `clip` */
static void BWatermarkBallIn(const CtrPokeTouchRect *clip, int cx, int cy, int rad, uint16_t color)
{
    int r2_out = rad * rad;
    int r2_in = (rad - 1) * (rad - 1);
    int r_c = rad / 4;
    int r2_c_out = (r_c + 1) * (r_c + 1);
    int r2_c_in = (r_c - 1) * (r_c - 1);
    int r2_dot = (r_c / 2) * (r_c / 2);

    for (int dy = -rad; dy <= rad; ++dy) {
        int py = cy + dy;
        if (clip && (py < clip->y || py >= clip->y + clip->h)) continue;
        if (py < 0 || py >= 240) continue;
        for (int dx = -rad; dx <= rad; ++dx) {
            int px = cx + dx;
            if (clip && (px < clip->x || px >= clip->x + clip->w)) continue;
            if (px < 0 || px >= 320) continue;
            int d2 = dx * dx + dy * dy;
            if (d2 <= r2_out && d2 >= r2_in) {
                Px(px, py, color);
            } else if (d2 <= r2_out && (dy == 0 || dy == -1) && (dx < -r_c || dx > r_c)) {
                Px(px, py, color);
            } else if (d2 <= r2_c_out && d2 >= r2_c_in) {
                Px(px, py, color);
            } else if (d2 <= r2_dot) {
                Px(px, py, color);
            }
        }
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

    /* Metallic corner rivets */
    Px(sBStrip.x + 3, sBStrip.y + 3, UIS_PANEL_LO); Px(sBStrip.x + 4, sBStrip.y + 4, UIS_BORDER_LIT);
    Px(sBStrip.x + sBStrip.w - 4, sBStrip.y + 3, UIS_PANEL_LO); Px(sBStrip.x + sBStrip.w - 5, sBStrip.y + 4, UIS_BORDER_LIT);
    Px(sBStrip.x + 3, sBStrip.y + sBStrip.h - 4, UIS_PANEL_LO); Px(sBStrip.x + 4, sBStrip.y + sBStrip.h - 5, UIS_BORDER_LIT);
    Px(sBStrip.x + sBStrip.w - 4, sBStrip.y + sBStrip.h - 4, UIS_PANEL_LO); Px(sBStrip.x + sBStrip.w - 5, sBStrip.y + sBStrip.h - 5, UIS_BORDER_LIT);

    /* Decorative watermark Pokéballs on the far left and right edges */
    BWatermarkBallIn(&sBStrip, sBStrip.x + 15, 18, 9, UI_RGB(68, 14, 18));
    BWatermarkBallIn(&sBStrip, sBStrip.x + sBStrip.w - 15, 18, 9, UI_RGB(68, 14, 18));

    for (int i = 0; i < 6; ++i) {
        if (v->party[i] && i == 0) {
            /* Active Pokémon golden aura ring */
            for (int dy = -13; dy <= 13; ++dy)
                for (int dx = -13; dx <= 13; ++dx) {
                    int d2 = dx * dx + dy * dy;
                    if (d2 >= 11 * 11 && d2 <= 13 * 13)
                        Px(xs[i] + dx, 18 + dy, d2 <= 12 * 12 ? UI_RGB(255, 230, 90) : UI_RGB(200, 140, 20));
                }
        }
        BallDraw(xs[i], 18, v->party[i]);
        if (v->party[i] == 3) {
            /* Yellow [PAR] status condition pill badge */
            int px = xs[i] + 1, py = 18 + 2;
            CtrPokeTouchRect pill = {(int16_t)px, (int16_t)py, 20, 8};
            UiRoundFill(pill.x, pill.y, pill.w, pill.h, 2, UI_RGB(240, 180, 20));
            UiRoundRect(&pill, 2, UI_RGB(255, 235, 90));
            UiText("PAR", px + 2, py + 1, UI_RGB(60, 35, 0), 0, 1, px + 20);
        }
    }
}

/* Golden glowing corner brackets and inner amber contour for focused card/button. */
static void BFocusGlowBrackets(const CtrPokeTouchRect *r)
{
    uint16_t gold = UI_RGB(255, 224, 80);
    uint16_t bright = UI_RGB(255, 255, 210);
    uint16_t halo = UI_RGB(200, 140, 20);
    int arm = 7;
    CtrPokeTouchRect rim = UiInset(r, 1);

    UiRing(&rim, UI_RADIUS_BUTTON - 1, 1, halo);

    /* Top-Left Bracket */
    Fill(r->x + 1, r->y + 1, arm, 2, gold);
    Fill(r->x + 1, r->y + 1, 2, arm, gold);
    Px(r->x + 1, r->y + 1, bright);
    Px(r->x + 2, r->y + 2, bright);

    /* Top-Right Bracket */
    Fill(r->x + r->w - 1 - arm, r->y + 1, arm, 2, gold);
    Fill(r->x + r->w - 3, r->y + 1, 2, arm, gold);
    Px(r->x + r->w - 2, r->y + 1, bright);
    Px(r->x + r->w - 3, r->y + 2, bright);

    /* Bottom-Left Bracket */
    Fill(r->x + 1, r->y + r->h - 3, arm, 2, gold);
    Fill(r->x + 1, r->y + r->h - 1 - arm, 2, arm, gold);
    Px(r->x + 1, r->y + r->h - 2, bright);
    Px(r->x + 2, r->y + r->h - 3, bright);

    /* Bottom-Right Bracket */
    Fill(r->x + r->w - 1 - arm, r->y + r->h - 3, arm, 2, gold);
    Fill(r->x + r->w - 3, r->y + r->h - 1 - arm, 2, arm, gold);
    Px(r->x + r->w - 2, r->y + r->h - 2, bright);
    Px(r->x + r->w - 3, r->y + r->h - 3, bright);
}

/* 8x8 silhouettes for FireRed's 18 types (its type numbers 0-17: Normal, Fighting, Flying, Poison, Ground, Rock, Bug,
 * Ghost, Steel, ???, Fire, Water, Grass, Electric, Psychic, Ice, Dragon, Dark), one byte a row, the high bit the left
 * pixel. Hand-drawn for this port. */
static void BTypeIcon(int x, int y, uint8_t type, uint16_t ink)
{
    static const uint8_t sIcon[18][8] = {
        {0x3C, 0x7E, 0xC3, 0xC3, 0xC3, 0xC3, 0x7E, 0x3C},   /* Normal: a ring */
        {0x78, 0x7C, 0x7F, 0x7F, 0x7E, 0x3E, 0x3C, 0x3C},   /* Fighting: a fist */
        {0x03, 0x0F, 0x3F, 0x7F, 0xFF, 0x66, 0x24, 0x00},   /* Flying: a wing */
        {0x38, 0x7C, 0x6C, 0x7C, 0x38, 0x28, 0x10, 0x00},   /* Poison: a skull */
        {0x00, 0x08, 0x1C, 0x3E, 0x7F, 0xFF, 0xFF, 0xFF},   /* Ground: a mountain */
        {0x00, 0x3C, 0x7E, 0xFF, 0xFF, 0xFF, 0x7E, 0x3C},   /* Rock: a boulder */
        {0x99, 0x5A, 0x3C, 0x7E, 0x7E, 0x3C, 0x5A, 0x99},   /* Bug: a beetle */
        {0x3C, 0x7E, 0xDB, 0xFF, 0xFF, 0xFF, 0xFF, 0xAA},   /* Ghost: a sheet with eyes */
        {0x18, 0x5A, 0x3C, 0xE7, 0xE7, 0x3C, 0x5A, 0x18},   /* Steel: a cog */
        {0x3C, 0x66, 0x06, 0x0C, 0x18, 0x00, 0x18, 0x00},   /* ???: a question mark */
        {0x10, 0x18, 0x38, 0x3C, 0x7E, 0x7F, 0x7F, 0x3E},   /* Fire: a flame */
        {0x10, 0x10, 0x38, 0x38, 0x7C, 0x7C, 0x7C, 0x38},   /* Water: a drop */
        {0x03, 0x0F, 0x3F, 0x7F, 0x7E, 0xBC, 0x30, 0x40},   /* Grass: a leaf */
        {0x0C, 0x18, 0x30, 0x7E, 0x0C, 0x18, 0x30, 0x20},   /* Electric: a bolt */
        {0x3C, 0x42, 0x99, 0xA5, 0xA5, 0x99, 0x42, 0x3C},   /* Psychic: a spiral */
        {0x10, 0x92, 0x54, 0x38, 0xFE, 0x38, 0x54, 0x92},   /* Ice: a snowflake */
        {0x40, 0x60, 0x70, 0x78, 0x7C, 0x3C, 0x18, 0x08},   /* Dragon: a fang */
        {0x3C, 0x78, 0xF0, 0xF0, 0xF0, 0xF0, 0x78, 0x3C},   /* Dark: a crescent */
    };

    if (type > 17)
        return;
    for (int r = 0; r < 8; ++r)
        for (int c = 0; c < 8; ++c)
            if (sIcon[type][r] & (0x80u >> c))
                Px(x + c, y + r, ink);
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
    /* FIGHT keeps the chrome's bright crimson; the three below wear colours of their own (BAG a leather brown,
     * POKeMON a deep wine, RUN the slate) so that only FIGHT is the saturated red. */
    UiButtonStyle sStyle[4];
    static const char *const label[4] = {"FIGHT", "BAG", "POK\xC3\xA9MON", "RUN"};
    static const int sIcon[4] = {BI_BURST, BI_BAG, BI_POKEBALL, BI_SHOE};
    const CtrPokeTouchRect *r = &sBAction[i];
    int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);
    int dy = (state & UI_PRESSED) ? 1 : 0;
    static const uint16_t sLight[4] = {UI_RGB(255, 205, 55), UI_RGB(65, 225, 110), UI_RGB(60, 190, 255), UI_RGB(255, 225, 60)};
    static const uint16_t sSpec[4]  = {UI_RGB(255, 255, 215), UI_RGB(200, 255, 215), UI_RGB(210, 245, 255), UI_RGB(255, 255, 210)};

    sStyle[0] = sUiS->fight;
    sStyle[1] = sUiS->bag;
    sStyle[2] = sUiS->pokemon;
    sStyle[3] = sUiS->run;
    if (sUiS == &sUiRed) {
        sStyle[1] = (UiButtonStyle){UI_RGB(138, 74, 28), UI_RGB(186, 112, 52), UI_RGB(86, 42, 14)};
        sStyle[2] = (UiButtonStyle){UI_RGB(110, 22, 48), UI_RGB(160, 52, 84), UI_RGB(68, 12, 30)};
    }
    BIconsPrepare();
    if (i == BUI_ACT_FIGHT) {
        int top = 24, mid = 49;     /* where the two flat bands of the face start, from the button's top */

        UiButton(r, &sStyle[i], state);
        if (!dy) {
            Fill(r->x + 2, r->y + top, r->w - 4, mid - top, UI_RGB(200, 28, 22));
            Fill(r->x + 2, r->y + mid, r->w - 4, r->h - mid - 5, UI_RGB(165, 20, 18));
        }
        /* Fine diagonal pinstripes, over the upper band only: one pixel in six, a touch lighter than the face. */
        for (int sy = r->y + 10; sy < r->y + mid; ++sy) {
            for (int sx = r->x + 4; sx < r->x + r->w - 4; ++sx) {
                if ((sx + sy) % 6 == 0)
                    Px(sx, sy + dy, UI_RGB(207, 56, 51));
            }
        }
        /* Giant watermark Pokeball on the right half */
        BWatermarkBallIn(r, r->x + 215, r->y + r->h / 2 + dy, 36, UI_RGB(192, 24, 22));

        /* Golden top neon bar across FIGHT, clear of the corner brackets */
        UiRoundFill(r->x + 18, r->y + 5 + dy, r->w - 36, 4, 2, sLight[0]);
        Fill(r->x + 24, r->y + 6 + dy, r->w - 48, 1, sSpec[0]);

        if (state & UI_FOCUSED) {
            CtrPokeTouchRect inside = UiInset(r, 2);     /* inside the golden frame, so all four corners show */

            BFocusGlowBrackets(&inside);
        }

        /* Fiery starburst icon */
        int burstX = r->x + 36, burstY = r->y + (r->h - 46) / 2 + dy;
        BIconAt(BI_BURST, burstX, burstY);

        /* Huge bold white arcade FIGHT label with a solid black drop shadow (two pixels at 3x) */
        int tx = r->x + 115, ty = r->y + (r->h - UI_TEXT_H * 3) / 2 + dy;
        UiText("FIGHT", tx + 2, ty + 2, UIS_OUTLINE, UIS_OUTLINE, 3, r->x + r->w);
        UiText("FIGHT", tx + 1, ty + 1, UIS_OUTLINE, UIS_OUTLINE, 3, r->x + r->w);
        UiText("FIGHT", tx, ty, UI_TEXT, UIS_OUTLINE, 3, r->x + r->w);
    } else {
        UiButton(r, &sStyle[i], state);

        /* Watermark Pokeball behind icon */
        uint16_t wm = i == BUI_ACT_RUN ? UI_RGB(56, 66, 82) : i == BUI_ACT_BAG ? UI_RGB(118, 60, 22) : UI_RGB(92, 16, 40);
        BWatermarkBallIn(r, r->x + r->w / 2, r->y + 32 + dy, 20, wm);

        /* Colored top neon lightbar */
        UiRoundFill(r->x + 8, r->y + 5 + dy, r->w - 16, 4, 2, sLight[i]);
        Fill(r->x + 14, r->y + 6 + dy, r->w - 28, 1, sSpec[i]);

        if (state & UI_FOCUSED)
            BFocusGlowBrackets(r);

        /* Centered high-resolution icons, in the 40 pixels between the bar and the label */
        int iconW = sBImg[sIcon[i]].image.w, iconH = sBImg[sIcon[i]].image.h;
        int ix = r->x + (r->w - iconW) / 2;
        int iy = r->y + 12 + (40 - iconH) / 2 + dy;
        BIconAt(sIcon[i], ix, iy);

        /* Bold white arcade label with a one-pixel drop shadow (two pixels would swallow a 2x letter) */
        int lw = UiTextWidth(label[i], 2);
        int lx = r->x + (r->w - lw) / 2;
        int ly = r->y + r->h - UI_TEXT_H * 2 - 7 + dy;
        UiText(label[i], lx + 1, ly + 1, UIS_OUTLINE, UIS_OUTLINE, 2, r->x + r->w);
        UiText(label[i], lx, ly, UI_TEXT, UIS_OUTLINE, 2, r->x + r->w);
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

/* Gen 3 Physical (type < 10: 9x9 starburst) vs Special (type >= 10: 9x9 concentric rings) icon in the move header. */
static void BMoveCategoryIcon(int x, int y, uint8_t type, uint16_t ink, uint16_t sh)
{
    static const uint16_t sPhys[9] = {
        0x020, 0x0A8, 0x070, 0x0FA, 0x1FF, 0x0FA, 0x070, 0x0A8, 0x020
    };
    static const uint16_t sSpec[9] = {
        0x070, 0x088, 0x124, 0x154, 0x154, 0x154, 0x124, 0x088, 0x070
    };
    const uint16_t *rows = type < 10 ? sPhys : sSpec;

    for (int ry = 0; ry < 9; ++ry)
        for (int rx = 0; rx < 9; ++rx)
            if (rows[ry] & (1u << (8 - rx))) {
                Px(x + rx + 1, y + ry + 1, sh);
                Px(x + rx, y + ry, ink);
            }
}

/* Deep saturated elemental background tone for the card body */
static uint16_t BCardBodyColor(int type)
{
    switch (type) {
    case 10: return UI_RGB(58, 8, 14);   /* Fire: deep saturated crimson */
    case 4:  return UI_RGB(50, 34, 14);  /* Ground: deep earthy gold-brown */
    case 5:  return UI_RGB(50, 42, 18);  /* Rock: deep mineral bronze */
    case 11: return UI_RGB(12, 30, 68);  /* Water: deep ocean indigo */
    case 12: return UI_RGB(14, 48, 18);  /* Grass: deep jungle green */
    case 13: return UI_RGB(54, 46, 10);  /* Electric: deep amber gold */
    case 14: return UI_RGB(54, 12, 38);  /* Psychic: deep magenta */
    case 15: return UI_RGB(16, 48, 58);  /* Ice: deep glacier cyan */
    case 16: return UI_RGB(34, 16, 68);  /* Dragon: deep royal violet */
    case 17: return UI_RGB(28, 24, 28);  /* Dark: deep slate charcoal */
    default: return UI_RGB(36, 32, 40);  /* Normal/other: dark neutral slate */
    }
}

/* A card of the battle (a move, a target): raised, its header band in `color` with `title`, the body a dark tint of
 * the same colour. Returns the vertical offset of its content (1 while pressed). An empty slot is a dark socket. */
static int BCardFrame(const CtrPokeTouchRect *r, uint16_t color, int type, const char *title, int state)
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

    /* Card body with deep saturated elemental tone */
    {
        int bodyY = r->y + 21 + dy;
        int bodyH = (r->y + 1 + dy + bottom) - bodyY;
        if (bodyH > 0) {
            uint16_t base = type >= 0 ? BCardBodyColor(type) : UIS_PANEL;
            uint16_t c_top = UiMix(base, color, pressed ? 2 : 3);
            uint16_t c_mid = base;
            uint16_t c_bot = UiMix(base, UI_RGB(6, 4, 8), 2);
            int seam = r->y + 45 + dy - bodyY;     /* between the badge row (to y+42) and the PP row (from y+48) */

            if (seam < 1 || seam > bodyH - 3)
                seam = bodyH * 2 / 3;
            UiRoundBand(r->x + 1, bodyY, r->w - 2, bodyH, UI_RADIUS_BUTTON - 1, 0, 1, c_bot);
            Fill(r->x + 1, bodyY, r->w - 2, seam, c_top);
            Fill(r->x + 1, bodyY + seam, r->w - 2, bodyH - seam - 2, c_mid);
            Fill(r->x + 1, bodyY, 1, bodyH - 2, UiMix(c_mid, UI_TEXT, 2));
            Fill(r->x + 1, bodyY, r->w - 2, 1, UIS_OUTLINE);
        }
    }

    /* Glossy Header Band */
    UiRoundBand(r->x + 1, r->y + 1 + dy, r->w - 2, 21, UI_RADIUS_BUTTON - 1, 1, 0, head);
    Fill(r->x + 3, r->y + 1 + dy, r->w - 6, 1, UiMix(head, UI_TEXT, 5));
    Fill(r->x + 2, r->y + 2 + dy, r->w - 4, 1, UiMix(head, UI_TEXT, 3));
    Fill(r->x + 2, r->y + 3 + dy, r->w - 4, 5, UiMix(head, UI_TEXT, 2));
    Fill(r->x + 1, r->y + 19 + dy, r->w - 2, 1, UiMix(head, UIS_OUTLINE, 3));
    Fill(r->x + 1, r->y + 20 + dy, r->w - 2, 1, UIS_OUTLINE);

    /* Diagonal 45° pinstripes across the header band */
    for (int hy = 2; hy < 20; ++hy) {
        int py = r->y + hy + dy;
        uint16_t stripe = UiMix(head, UI_RGB(255, 255, 255), 2);
        for (int hx = 2; hx < r->w - 2; ++hx) {
            if ((hx + hy) % 7 < 2)
                Px(r->x + hx, py, stripe);
        }
    }

    /* Left mini emblem box for elemental icon */
    if (type >= 0) {
        int ibX = r->x + 5, ibY = r->y + 4 + dy;
        CtrPokeTouchRect ibox = {(int16_t)ibX, (int16_t)ibY, 14, 13};
        UiRoundFill(ibox.x, ibox.y, ibox.w, ibox.h, 2, UI_RGB(20, 6, 8));
        UiRoundRect(&ibox, 2, UI_RGB(64, 20, 24));
        BTypeIcon(ibox.x + 3, ibox.y + 2, (uint8_t)type, UI_TEXT);
    }

    /* Move Title in BOLD WHITE arcade font with solid 1px black outline */
    {
        int tx = type >= 0 ? r->x + 23 : r->x + 8;
        int ty = r->y + 7 + dy;
        int limit = r->x + r->w - 18;

        UiText(title, tx - 1, ty, UIS_OUTLINE, 0, 1, limit);
        UiText(title, tx + 1, ty, UIS_OUTLINE, 0, 1, limit);
        UiText(title, tx, ty - 1, UIS_OUTLINE, 0, 1, limit);
        UiText(title, tx, ty + 1, UIS_OUTLINE, 0, 1, limit);
        UiText(title, tx + 1, ty + 1, UIS_OUTLINE, 0, 1, limit);
        UiText(title, tx, ty, UI_TEXT, 0, 1, limit);
    }

    if (state & UI_FOCUSED)
        BFocusGlowBrackets(r);

    return dy;
}

/* The PP colour FireRed's move list uses: the usual ink, then yellow at half, orange at a quarter, red at none. */
static uint16_t BPpColor(unsigned pp, unsigned max)
{
    return pp == 0 ? UI_HP_LOW : pp * 4 <= max ? UI_WARN : pp * 2 <= max ? UI_HP_MID : UI_TEXT;
}

/* The matchup tag beside the type badge: what the move does to the foe's types. x2 green, x4 lime with a gold rim,
 * x1/2 and x1/4 orange, x0 red; an arrow or a cross says it again without colour. A neutral hit has no tag. 40x14: the
 * 5-pixel glyph at +4, the text to end 4 from the right edge (x1/2 is the widest, 24 wide, so they never meet). */
#define BTAG_W 40
static void BMatchupTag(uint8_t matchup, int x, int y)
{
    static const char *const kText[] = {"", "x0", "x1/4", "x1/2", "x2", "x4"};
    static const uint16_t kColor[] = {0, UI_RGB(190, 40, 40), UI_RGB(168, 62, 26), UI_RGB(204, 84, 24),
                                      UI_RGB(40, 150, 70), UI_RGB(110, 245, 30)};
    static const uint8_t kUp[5] = {0x04, 0x0E, 0x1F, 0x0E, 0x0E};          /* x2, x4 */
    static const uint8_t kDown[5] = {0x0E, 0x0E, 0x1F, 0x0E, 0x04};        /* x1/2, x1/4 */
    static const uint8_t kCross[5] = {0x11, 0x0A, 0x04, 0x0A, 0x11};       /* x0 */
    CtrPokeTouchRect tag = {(int16_t)x, (int16_t)y, BTAG_W, 14};
    const uint8_t *glyph;
    uint16_t ink;

    if (matchup == CTR_FX_NONE || matchup > CTR_FX_QUADRUPLE)
        return;
    ink = matchup == CTR_FX_QUADRUPLE ? UI_RGB(15, 35, 5) : UI_TEXT;
    UiRoundFill(tag.x, tag.y, tag.w, tag.h, 3, matchup == CTR_FX_QUADRUPLE ? UI_RGB(255, 224, 102) : UIS_OUTLINE);
    UiRoundFill(tag.x + 1, tag.y + 1, tag.w - 2, tag.h - 2, 2, kColor[matchup]);
    Fill(tag.x + 2, tag.y + 1, tag.w - 4, 1, UiMix(kColor[matchup], UI_TEXT, 3));

    glyph = matchup == CTR_FX_IMMUNE ? kCross : (matchup == CTR_FX_HALF || matchup == CTR_FX_QUARTER) ? kDown : kUp;
    for (int r = 0; r < 5; ++r)
        for (int c = 0; c < 5; ++c)
            if (glyph[r] & (1u << (4 - c)))
                Px(tag.x + 4 + c, tag.y + 4 + r, ink);
    UiTextRight(kText[matchup], tag.x + tag.w - 4, tag.y + (tag.h - UI_TEXT_H) / 2 + 1, ink,
                matchup == CTR_FX_QUADRUPLE ? 0 : UIS_TEXT_SH, 1);
}

/* A move card: the name and Physical/Special icon on the type's colour, the type badge, the matchup, power (and
 * accuracy when populated), and the PP left (number and bar). */
static void BCard(int i, const CtrBattleView *v)
{
    const CtrPokeTouchRect *r = &sBCard[i];
    const CtrBattleMove *m = &v->move[i];
    int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);
    uint16_t color;
    int dy;

    if (!m->present) {
        BCardFrame(r, 0, -1, NULL, 0);
        return;
    }
    color = UiTypeColor(m->type);
    dy = BCardFrame(r, color, m->type, m->name, state);

    /* Category icon on right corner of header */
    BMoveCategoryIcon(r->x + r->w - 15, r->y + 6 + dy, m->type, UI_TEXT, UIS_OUTLINE);

    {
        int pill = UiTextWidth(m->typeName, 1) + 8;
        CtrPokeTouchRect badge = {(int16_t)(r->x + 8), (int16_t)(r->y + 28 + dy), (int16_t)(pill < 34 ? 34 : pill > 46 ? 46 : pill), 14};
        char pp[12];

        UiTypeBadge(&badge, m->type, m->typeName);
        BMatchupTag(m->matchup, badge.x + badge.w + 3, r->y + 28 + dy);   /* at most to x+97: PWR stays at w-52 */

        /* PWR in light blue-grey (#a0b4cc) and value in bold white */
        UiText("PWR", r->x + r->w - 52, r->y + 28 + dy, UI_RGB(160, 180, 204), UIS_TEXT_SH, 1, r->x + r->w);
        UiTextRight(m->power, r->x + r->w - 8, r->y + 28 + dy, UI_TEXT, UIS_TEXT_SH, 1);

        /* PP in #a0b4cc and count in bold white */
        UiText("PP", r->x + 8, r->y + 48 + dy, UI_RGB(160, 180, 204), UIS_TEXT_SH, 1, r->x + 30);
        snprintf(pp, sizeof(pp), "%u/%u", m->pp, m->maxPp);
        UiText(pp, r->x + 24, r->y + 48 + dy, UI_TEXT, UIS_TEXT_SH, 1, r->x + 68);

        /* Segmented neon capsule: ten segments of six pixels and a one-pixel gap, lit by the share of PP left (rounded up,
         * so one PP left still shows one). Cyan, then amber at half, orange at a quarter, a dim red at none. */
        {
            CtrPokeTouchRect bar = {(int16_t)(r->x + r->w - 8 - 71), (int16_t)(r->y + 50 + dy), 71, 6};
            int lit = m->maxPp ? (int)(((unsigned)m->pp * 10 + m->maxPp - 1) / m->maxPp) : 0;
            uint16_t neon = m->pp == 0 ? UI_RGB(160, 30, 20)
                          : m->pp * 4 <= m->maxPp ? UI_RGB(255, 90, 46)
                          : m->pp * 2 <= m->maxPp ? UI_RGB(255, 194, 51)
                          : UI_RGB(53, 208, 255);
            uint16_t shine = m->pp == 0 ? UI_RGB(210, 80, 70)
                           : m->pp * 4 <= m->maxPp ? UI_RGB(255, 190, 140)
                           : m->pp * 2 <= m->maxPp ? UI_RGB(255, 245, 180)
                           : UI_RGB(155, 234, 255);

            UiRoundFill(bar.x, bar.y, bar.w, bar.h, 2, UI_RGB(11, 31, 58));
            Fill(bar.x + 1, bar.y + 1, bar.w - 2, bar.h - 2, UI_RGB(15, 27, 51));
            for (int seg = 0; seg < 10; ++seg) {
                if (seg < lit) {
                    Fill(bar.x + 1 + seg * 7, bar.y + 1, 6, bar.h - 2, neon);
                    Fill(bar.x + 1 + seg * 7, bar.y + 1, 6, 1, shine);
                }
            }
        }
    }
}

/* Sculpted CANCEL button with an inner metallic bevel ring. */
static void BCancel(const CtrPokeTouchRect *r, int pressed)
{
    int state = pressed ? UI_PRESSED : 0;
    CtrPokeTouchRect rim = UiInset(r, 1);

    UiLabelButton(r, &sUiS->cancel, state, "CANCEL", r->h >= 28 ? 2 : 1);
    if (r->h >= 36) {
        int dy = pressed ? 1 : 0;
        BWatermarkBallIn(r, r->x + r->w / 2, r->y + r->h / 2 + dy, 14,
                         pressed ? UI_RGB(110, 12, 14) : UI_RGB(155, 18, 20));
        /* Bold white text with black drop shadow over the watermark */
        int scale = 2;
        int top = UiButtonLabelTop(r, state, scale);
        int tw = UiTextWidth("CANCEL", scale);
        int tx = r->x + (r->w - tw) / 2;
        UiText("CANCEL", tx + 2, top + 2, UIS_OUTLINE, UIS_OUTLINE, scale, r->x + r->w);
        UiText("CANCEL", tx + 1, top + 1, UIS_OUTLINE, UIS_OUTLINE, scale, r->x + r->w);
        UiText("CANCEL", tx, top, UI_TEXT, UIS_OUTLINE, scale, r->x + r->w);
    }
    UiRing(&rim, UI_RADIUS_BUTTON - 1, 1, pressed ? sUiS->cancel.body : UIS_BORDER_LIT);
}

/* Live move description footer on the left of the compact CANCEL button in BUI_MOVE. */
static void BMoveFooter(const CtrBattleView *v)
{
    const CtrPokeTouchRect *r = &sBMoveFooter;
    int sel = v->cursor >= 0 && v->cursor < 4 && v->move[v->cursor].present ? v->cursor : 0;
    const CtrBattleMove *m = &v->move[sel];
    uint16_t accent = m->present ? UiTypeColor(m->type) : UIS_BORDER_LIT;

    UiPanel(r, UIS_PANEL_LO, UIS_BORDER);
    Fill(r->x + 3, r->y + 1, r->w - 6, 1, UIS_BORDER_LIT);
    Fill(r->x + 2, r->y + 5, 2, r->h - 10, accent);
    if (!m->present) {
        UiText("Select a move.", r->x + 9, r->y + 16, UIS_TEXT_DIM, UIS_TEXT_SH, 1, r->x + r->w - 6);
        return;
    }
    {
        char stats[64];
        const char *acc = m->accuracy[0] && m->accuracy[0] != '-' ? m->accuracy : "--";
        int n = snprintf(stats, sizeof(stats), "ACC %s%s", acc, acc[0] == '-' ? "" : "%");

        if (m->effectChance && n > 0 && (size_t)n < sizeof(stats))
            n += snprintf(stats + n, sizeof(stats) - (size_t)n, "  EFF %u%%", m->effectChance);
        if (m->priority && n > 0 && (size_t)n < sizeof(stats))
            snprintf(stats + n, sizeof(stats) - (size_t)n, "  PRI %+d", m->priority);
        UiText(stats, r->x + 8, r->y + 4, UI_RGB(240, 205, 100), UIS_TEXT_SH, 1, r->x + r->w - (m->stab ? 70 : 8));
        if (m->stab) {
            CtrPokeTouchRect stab = {(int16_t)(r->x + r->w - 66), (int16_t)(r->y + 3), 60, 11};
            UiRoundFill(stab.x, stab.y, stab.w, stab.h, 2, UI_RGB(236, 174, 36));
            UiRoundRect(&stab, 2, UI_RGB(255, 220, 80));
            UiTextCentered("STAB x1.5", &stab, -1, UI_RGB(60, 40, 0), 0, 1);
        }
    }
    /* Crimson divider line across panel */
    Fill(r->x + 6, r->y + 16, r->w - 12, 1, UI_RGB(160, 24, 28));
    Fill(r->x + 6, r->y + 17, r->w - 12, 1, UI_RGB(24, 4, 6));

    if (m->desc[0]) {
        BDescribeN(m->desc, r->x + 8, r->y + 20, r->w - 16, 2, UI_TEXT);
    } else {
        const char *cat = m->type < 10 ? "PHYSICAL MOVE" : "SPECIAL MOVE";
        UiText(m->name, r->x + 8, r->y + 20, UI_TEXT, UIS_TEXT_SH, 1, r->x + r->w - 6);
        UiText(cat, r->x + 8, r->y + 31, UIS_TEXT_DIM, UIS_TEXT_SH, 1, r->x + r->w - 6);
    }
}

static void BMove(const CtrBattleView *v)
{
    for (int i = 0; i < 4; ++i)
        BCard(i, v);
    BMoveFooter(v);
    BCancel(&sBMoveCancel, v->pressed == BUI_CANCEL);
}

/* Aiming a move in a double battle: a card for each battler FireRed lets it be aimed at, the opposing ones on top in
 * FIGHT's red, the player's below in POKeMON's blue: the name, the level, the matchup badge, the side and the HP bar. */
static void BTargetCard(int i, const CtrBattleView *v)
{
    static const UiButtonStyle sFoe = UI_STYLE_FIGHT, sAlly = UI_STYLE_POKEMON;
    const CtrPokeTouchRect *r = &sBCard[i];
    const CtrBattleTarget *t = &v->target[i];
    int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);
    int dy;

    if (!t->present) {
        BCardFrame(r, 0, -1, NULL, 0);
        return;
    }
    dy = BCardFrame(r, t->side ? sAlly.body : sFoe.body, -1, t->name, state);
    {
        char level[12], hp[20];
        CtrPokeTouchRect tag = {(int16_t)(r->x + r->w - 46), (int16_t)(r->y + 27 + dy), 38, 14};
        CtrPokeTouchRect bar = {(int16_t)(r->x + 8), (int16_t)(r->y + 46 + dy), (int16_t)(r->w - 16), 6};

        snprintf(level, sizeof(level), "Lv%u", t->level);
        UiText(level, r->x + 8, r->y + 30 + dy, UI_TEXT, UIS_TEXT_SH, 1, r->x + 60);
        BMatchupTag(t->matchup, r->x + r->w - 90, r->y + 27 + dy);
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

static void BSafariButton(int i, const CtrBattleView *v)
{
    const UiButtonStyle *sStyle[4] = {&sUiS->fight, &sUiS->bag, &sUiS->pokemon, &sUiS->run};
    static const char *const label[4] = {"BALL", "BAIT", "ROCK", "RUN"};
    static const int sSafariIcon[4] = {BI_POKEBALL, BI_BAIT, BI_ROCK, BI_SHOE};
    static const uint16_t sSafariAccent[4] = {UI_RGB(70, 172, 255), UI_RGB(236, 112, 140), UI_RGB(190, 182, 170), UI_RGB(72, 206, 104)};
    const CtrPokeTouchRect *r = &sBSafari[i];
    int state = (v->pressed == i ? UI_PRESSED : 0) | (v->showCursor && v->cursor == i ? UI_FOCUSED : 0);
    int dy = (state & UI_PRESSED) ? 1 : 0, cx = r->x + r->w / 2, cy = r->y + 30 + dy;

    BIconsPrepare();
    UiButton(r, sStyle[i], state);
    BAccent(r, dy, sSafariAccent[i]);
    BIconAt(sSafariIcon[i], cx - 12, cy - 12 + 4);
    UiText(label[i], cx - UiTextWidth(label[i], 2) / 2, r->y + 52 + dy, UI_TEXT, sStyle[i]->shade, 2, r->x + r->w);
}

/* The Safari Zone's menu: BALL, BAIT, ROCK, RUN (the words of FireRed's gText_SafariZoneMenu), as MAIN's buttons. */
static void BSafari(const CtrBattleView *v)
{
    for (int i = 0; i < 4; ++i)
        BSafariButton(i, v);
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

/* Whether two Party views differ only by `cursor` (with no modal open), so only the affected slot panels need redraw. */
static int BPartySlotsOnly(const CtrBattleParty *a, const CtrBattleParty *b)
{
    CtrBattleParty copy = *a;

    if (a->panel != BUI_PTY_NONE || b->panel != BUI_PTY_NONE)
        return 0;
    copy.cursor = b->cursor;
    return memcmp(&copy, b, sizeof(copy)) == 0;
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
        || (v->state == BUI_BAG && (memcmp(&old->bag, &v->bag, sizeof(v->bag)) || old->pressed != v->pressed))
        || (v->state == BUI_PARTY && !BPartySlotsOnly(&old->pty, &v->pty))
        || (v->state == BUI_TARGET && memcmp(old->target, v->target, sizeof(v->target)))) {
        BBody(v);
        BMark(dirty, &sBBody);
    } else if (v->state == BUI_MOVE && (old->pressed != v->pressed || old->cursor != v->cursor || old->showCursor != v->showCursor)) {
        for (int i = 0; i < 4; ++i)
            if (BLooksChanged(old, v, i)) {
                Fill(sBCard[i].x, sBCard[i].y, sBCard[i].w, sBCard[i].h, UIS_BG);
                BCard(i, v);
                BMark(dirty, &sBCard[i]);
            }
        if (old->cursor != v->cursor) {
            Fill(sBMoveFooter.x, sBMoveFooter.y, sBMoveFooter.w, sBMoveFooter.h, UIS_BG);
            BMoveFooter(v);
            BMark(dirty, &sBMoveFooter);
        }
        if ((old->pressed == BUI_CANCEL) != (v->pressed == BUI_CANCEL)) {
            Fill(sBMoveCancel.x, sBMoveCancel.y, sBMoveCancel.w, sBMoveCancel.h, UIS_BG);
            BCancel(&sBMoveCancel, v->pressed == BUI_CANCEL);
            BMark(dirty, &sBMoveCancel);
        }
    } else if (v->state == BUI_TARGET && (old->pressed != v->pressed || old->cursor != v->cursor || old->showCursor != v->showCursor)) {
        for (int i = 0; i < 4; ++i)
            if (BLooksChanged(old, v, i)) {
                Fill(sBCard[i].x, sBCard[i].y, sBCard[i].w, sBCard[i].h, UIS_BG);
                BTargetCard(i, v);
                BMark(dirty, &sBCard[i]);
            }
        if ((old->pressed == BUI_CANCEL) != (v->pressed == BUI_CANCEL)) {
            Fill(sBCancel.x, sBCancel.y, sBCancel.w, sBCancel.h, UIS_BG);
            BCancel(&sBCancel, v->pressed == BUI_CANCEL);
            BMark(dirty, &sBCancel);
        }
    } else if (v->state == BUI_SAFARI && (old->pressed != v->pressed || old->cursor != v->cursor || old->showCursor != v->showCursor)) {
        for (int i = 0; i < 4; ++i)
            if (BLooksChanged(old, v, i)) {
                Fill(sBSafari[i].x, sBSafari[i].y, sBSafari[i].w, sBSafari[i].h, UIS_BG);
                BSafariButton(i, v);
                BMark(dirty, &sBSafari[i]);
            }
    } else if (v->state == BUI_PARTY && (old->pressed != v->pressed || old->pty.cursor != v->pty.cursor)) {
        for (int i = 0; i < 6; ++i)
            if ((old->pty.cursor == i) != (v->pty.cursor == i)
                || (old->pressed == BUI_PTY_SLOT + i) != (v->pressed == BUI_PTY_SLOT + i)) {
                CtrPokeTouchRect r = BPanel(i);
                Fill(r.x, r.y, r.w, r.h, UIS_BG);
                BPartyPanel(i, v);
                BMark(dirty, &r);
            }
        if (v->pty.mode != BUI_PTY_FORCED && (old->pressed == BUI_PTY_CANCEL) != (v->pressed == BUI_PTY_CANCEL)) {
            Fill(sBPCancel.x, sBPCancel.y, sBPCancel.w, sBPCancel.h, UIS_BG);
            BCancel(&sBPCancel, v->pressed == BUI_PTY_CANCEL);
            BMark(dirty, &sBPCancel);
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
