/* The Poke Ball naming keyboard (3ds_field_naming_impl.h) on the whole lower screen (included at the end of 3ds_poketouch.c, after 3ds_ball_learn_impl.h,
 * whose pieces and the Poke Ball card and pill colours it shares): the name so far in boxes across the top of the red ground (a white box for each letter
 * written, a dark one for the rest, a white ring on the one the next letter takes), the three tabs with DELETE and OK, the keys as pills (the cursor's
 * white with a red letter, the others dark red), and under the dark band, on the white, the question and its hint (and, for a Pokemon, its picture). Every
 * character is FireRed's; the rectangles are in screen pixels and are the hit zones while the keyboard is up. */

#define BN_BAND_Y 164
#define BN_TAB_Y 38
#define BN_KEY_Y 64
#define BN_KEY_STEP 22
#define BN_KEY_H 20

static const CtrPokeTouchRect sBnDelete = {193, BN_TAB_Y, 60, BN_KEY_H};
static const CtrPokeTouchRect sBnOk = {256, BN_TAB_Y, 60, BN_KEY_H};

static CtrPokeTouchRect BnTab(int page)
{
    /* Left to right: capitals, small letters, then FireRed's symbols page. */
    static const int8_t kSlot[3] = {2, 0, 1};
    CtrPokeTouchRect r = {(int16_t)(4 + 63 * kSlot[page]), BN_TAB_Y, 60, BN_KEY_H};

    return r;
}

static CtrPokeTouchRect BnKey(const CtrPokeTouchName *n, int row, int col)
{
    int pitch = n->cols >= 9 ? 34 : n->cols > 6 ? 39 : 51;
    int left = 4 + (312 - (pitch * n->cols - 2)) / 2;
    CtrPokeTouchRect r = {(int16_t)(left + pitch * col), (int16_t)(BN_KEY_Y + BN_KEY_STEP * row), (int16_t)(pitch - 2), BN_KEY_H};

    return r;
}

static int BallNameHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchName *n = &v->name;

    for (int p = 0; p < 3; ++p) {
        CtrPokeTouchRect r = BnTab(p);

        if (UiIn(&r, x, y))
            return PT_NAME_HIT_TAB + p;
    }
    if (UiIn(&sBnDelete, x, y))
        return PT_NAME_HIT_DELETE;
    if (UiIn(&sBnOk, x, y))
        return PT_NAME_HIT_OK;
    for (int row = 0; row < PT_NAME_ROWS; ++row)
        for (int col = 0; col < n->cols && col < PT_NAME_COLS; ++col) {
            CtrPokeTouchRect r = BnKey(n, row, col);

            if (n->key[row][col] != 0xff && UiIn(&r, x, y))
                return PT_NAME_HIT_KEY + row * PT_NAME_COLS + col;
        }
    return -1;
}

/* A pill: `face` and `ink` as given (the cursor's is white with a red word, the page's tab pink, the others dark red); its edge is the dark red, a bright
 * one when the cursor is on it. The word is in the middle. */
static void BnPill(const CtrPokeTouchRect *r, const char *label, uint16_t face, uint16_t ink, int cursor, int pressed)
{
    int tw = UiTextWidth(label, 1);

    if (pressed)
        face = cursor ? BM_PRESSED : BM_CARD_EDGE;
    UiRoundFill(r->x, r->y, r->w, r->h, 7, cursor ? BM_CARD_EDGE : BM_PILL_EDGE);
    UiRoundFill(r->x + 2, r->y + 2, r->w - 4, r->h - 4, 5, face);
    if (tw > r->w - 6)
        tw = r->w - 6;
    UiText(label, r->x + (r->w - tw) / 2, r->y + (r->h - UI_TEXT_H) / 2, pressed && !cursor ? EM_WHITE : ink, face, 1, r->x + r->w - 3);
}

/* The name so far: a box for each letter it takes. */
static void BnEntry(const CtrPokeTouchName *n)
{
    int slots = n->maxChars ? n->maxChars : 1, pitch = 296 / slots, x0, sw, sy = 6, sh = 26;
    char g[3];

    if (pitch > 31)
        pitch = 31;
    x0 = (320 - pitch * slots + 4) / 2;
    sw = pitch - 4;
    for (int i = 0; i < slots; ++i) {
        int sx = x0 + i * pitch, written = i < n->len;          /* a typed space is the code 0, and is written all the same */
        int next = !written && i == n->len;

        UiRoundFill(sx, sy, sw, sh, 6, next ? BL_WHITE : written ? BM_CARD_EDGE : BM_PILL_EDGE);
        UiRoundFill(sx + 2, sy + 2, sw - 4, sh - 4, 4, written ? BM_CARD_FACE : BM_PILL);
        if (written) {
            FNChar(n->text[i], g);
            UiText(g, sx + (sw - UiTextWidth(g, 1)) / 2, sy + (sh - UI_TEXT_H) / 2, BM_INK, BM_CARD_FACE, 1, sx + sw);
        }
    }
    if (n->gender) {
        char sign[3];

        FNChar(n->gender == 2 ? 0xb6 : 0xb5, sign);
        UiText(sign, x0 + slots * pitch + 2, sy + 7, EM_WHITE, BL_DEEP, 1, 318);
    }
}

static void BallNameScreen(const CtrPokeTouchView *v)
{
    const CtrPokeTouchName *n = &v->name;
    static const int kLabel[3] = {UIS_NAME_SYMBOLS, UIS_NAME_UPPER, UIS_NAME_LOWER};
    int onButtons = n->curCol >= n->cols;
    char title[66], g[3];
    int textW = n->species ? 232 : 296;

    EmSurface();
    TsBallGround(BN_BAND_Y);
    BnEntry(n);
    for (int p = 0; p < 3; ++p) {
        CtrPokeTouchRect r = BnTab(p);
        int cursor = onButtons && n->curRow == 0 && p == (n->page + 1) % 3;   /* the page button: the tab its A turns to is the one it marks */

        BnPill(&r, CtrUiText(kLabel[p]), cursor ? BL_WHITE : n->page == p ? BL_PINK : BM_PILL, cursor || n->page == p ? BL_REDTXT : EM_WHITE, cursor,
               n->pressed == PT_NAME_HIT_TAB + p);
    }
    BnPill(&sBnDelete, CtrUiText(UIS_NAME_DELETE), onButtons && n->curRow == 1 ? BL_WHITE : BM_PILL, onButtons && n->curRow == 1 ? BL_REDTXT : EM_WHITE,
           onButtons && n->curRow == 1, n->pressed == PT_NAME_HIT_DELETE);
    BnPill(&sBnOk, CtrUiText(UIS_NAME_OK), onButtons && n->curRow == 2 ? BL_WHITE : BL_BAND, onButtons && n->curRow == 2 ? BL_REDTXT : EM_WHITE,
           onButtons && n->curRow == 2, n->pressed == PT_NAME_HIT_OK);
    for (int row = 0; row < PT_NAME_ROWS; ++row)
        for (int col = 0; col < n->cols && col < PT_NAME_COLS; ++col) {
            CtrPokeTouchRect r = BnKey(n, row, col);
            uint8_t code = n->key[row][col];
            int cursor = n->curCol == col && n->curRow == row;
            uint16_t face = cursor ? BL_WHITE : BM_PILL, ink = cursor ? BL_REDTXT : EM_WHITE;

            if (code == 0xff) {                               /* an empty cell: a bare white pill only when the cursor stops on it */
                if (cursor)
                    BnPill(&r, "", face, ink, 1, 0);
                continue;
            }
            if (code == 0) {                                  /* the space key: the flat open bracket under its middle */
                BnPill(&r, "", face, ink, cursor, n->pressed == PT_NAME_HIT_KEY + row * PT_NAME_COLS + col);
                FNSpaceMark(r.x + r.w / 2, r.y + 5, cursor ? BL_REDTXT : EM_WHITE);
            } else
                BnPill(&r, FNChar(code, g), face, ink, cursor, n->pressed == PT_NAME_HIT_KEY + row * PT_NAME_COLS + col);
        }
    FNText(n->title, (int)sizeof(n->title), title, (int)sizeof(title));
    UiText(title, 14, BN_BAND_Y + 14, BL_BAND, BL_WHITE, 1, 14 + textW);
    BPWrap(CtrUiText(UIS_NAME_HINT), 14, BN_BAND_Y + 31, textW, 3, UI_RGB(132, 128, 130), BL_WHITE);
    if (n->species)
        FDPicDraw(n->species, 320 - 76, BN_BAND_Y + 10, 0);
    sUiS = &sUiNavy;
}
