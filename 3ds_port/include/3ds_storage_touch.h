#ifndef CTR_STORAGE_TOUCH_H
#define CTR_STORAGE_TOUCH_H
/* Standalone geometry/route rules for the GBA 240x160 PC image sampled 1:1.
 * UI touch coordinates are source GBA pixels; no Pokemon state is written. */
enum {
    ST_HIT_NONE = -1, ST_HIT_TITLE = 40, ST_HIT_BUTTON_PARTY = 41,
    ST_HIT_BUTTON_CLOSE = 42, ST_HIT_PREV = 43, ST_HIT_NEXT = 44,
    ST_HIT_PARTY_BASE = 50, ST_HIT_MENU_BASE = 100,
    ST_HIT_PC_BASE = 200
};
enum { ST_AREA_BOX, ST_AREA_PARTY, ST_AREA_TITLE, ST_AREA_BUTTONS };
enum { ST_DIR_NONE, ST_DIR_UP, ST_DIR_DOWN, ST_DIR_LEFT, ST_DIR_RIGHT, ST_DIR_A, ST_DIR_B, ST_DIR_START };
/* Normal storage grid: 6 columns x 5 rows, one 24px cell per monster. */
static inline int CtrStorage_BoxHit(int x, int y, int partyShown)
{
    if (x < 0 || y < 0 || x >= 240 || y >= 160) return ST_HIT_NONE;
    if (partyShown) {
        /* Icon centres are (104,64) and (152,16+24*(slot-1)). */
        if (x >= 88 && x < 120 && y >= 48 && y < 80) return ST_HIT_PARTY_BASE;
        if (x >= 136 && x < 168 && y >= 4 && y < 124)
            return ST_HIT_PARTY_BASE + 1 + (y - 4) / 24;
        if (x >= 132 && x < 210 && y >= 124) return ST_HIT_PARTY_BASE + 6;
        return ST_HIT_NONE;
    }
    /* FireRed's original cursor centres, from GetCursorCoordsByPos:
     * LEFT/RIGHT arrows 92/228 at y28; PARTY/CLOSE buttons 120/208 at
     * y14; box title 162 at y12. These controls are ABOVE the grid,
     * not in the bottom row. Reserve their real top band first. */
    if (y < 36) {
        if (y >= 18 && x >= 80 && x < 108) return ST_HIT_PREV;
        if (y >= 18 && x >= 218) return ST_HIT_NEXT;
        if (y < 24 && x >= 108 && x < 138) return ST_HIT_BUTTON_PARTY;
        if (y < 24 && x >= 195 && x < 218) return ST_HIT_BUTTON_CLOSE;
        if (x >= 138 && x < 195) return ST_HIT_TITLE;
    }
    /* The 30 icons are centred at x=100+24*col, y=44+24*row.
     * Clip row zero above 36px so taps on the arrows do not move a mon. */
    if (x >= 88 && x < 232 && y >= 36 && y < 152)
        return ((y - 32) / 24) * 6 + (x - 88) / 24;
    return ST_HIT_NONE;
}
static inline int CtrStorage_MenuHit(int x, int y, int left, int top, int width, int count)
{
    if (count < 1 || count > 7 || x < left || x >= left + width
        || y < top || y >= top + 16 * count)
        return ST_HIT_NONE;
    return ST_HIT_MENU_BASE + (y - top) / 16;
}
/* Choose Box popup's original FireRed sprite centres:
 * left arrow (124,88), current box (160,96), right arrow (196,88). */
static inline int CtrStorage_ChooseHit(int x, int y)
{
    if (x < 108 || x >= 212 || y < 72 || y >= 114) return ST_HIT_NONE;
    if (x < 140) return ST_HIT_PREV;
    if (x >= 180) return ST_HIT_NEXT;
    return ST_HIT_TITLE;
}
static inline int CtrStorage_PcHit(int x, int y, int count)
{
    if (count < 1 || count > 5 || x < 8 || x >= 146 || y < 8 || y >= 8 + 16 * count)
        return ST_HIT_NONE;
    return ST_HIT_PC_BASE + (y - 8) / 16;
}
/* Each move follows FireRed's own cursor graph; no direct data mutation. */
static inline int CtrStorage_Next(int area, int pos, int targetArea, int targetPos)
{
    if (targetArea == ST_AREA_TITLE) {
        if (area == ST_AREA_TITLE) return ST_DIR_A;
        if (area == ST_AREA_PARTY) return ST_DIR_B;
        return ST_DIR_START;
    }
    if (targetArea == ST_AREA_PARTY) {
        if (area == ST_AREA_PARTY)
            return pos == targetPos ? ST_DIR_A : (pos < targetPos ? ST_DIR_DOWN : ST_DIR_UP);
        if (area == ST_AREA_BOX) return ST_DIR_DOWN;
        if (area == ST_AREA_TITLE) return ST_DIR_UP;
        if (area == ST_AREA_BUTTONS) return pos == 0 ? ST_DIR_A : ST_DIR_LEFT;
        return ST_DIR_NONE;
    }
    if (targetArea == ST_AREA_BUTTONS) {
        if (area == ST_AREA_BUTTONS) return pos == targetPos ? ST_DIR_A : ST_DIR_RIGHT;
        if (area == ST_AREA_TITLE) return ST_DIR_UP;
        if (area == ST_AREA_BOX) return ST_DIR_DOWN;
        if (area == ST_AREA_PARTY) return ST_DIR_B;
        return ST_DIR_NONE;
    }
    if (targetArea != ST_AREA_BOX || targetPos < 0 || targetPos >= 30) return ST_DIR_NONE;
    if (area == ST_AREA_TITLE) return ST_DIR_DOWN;
    if (area == ST_AREA_BUTTONS) return ST_DIR_UP;
    if (area == ST_AREA_PARTY) return ST_DIR_B;
    if (area != ST_AREA_BOX || pos < 0 || pos >= 30) return ST_DIR_NONE;
    if (pos / 6 > targetPos / 6) return ST_DIR_UP;
    if (pos / 6 < targetPos / 6) return ST_DIR_DOWN;
    if (pos % 6 > targetPos % 6) return ST_DIR_LEFT;
    if (pos % 6 < targetPos % 6) return ST_DIR_RIGHT;
    return ST_DIR_A;
}
#endif
