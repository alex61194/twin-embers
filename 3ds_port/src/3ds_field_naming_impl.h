/* FireRed's naming screen in the viewport (3ds_poketouch.h CtrPokeTouchName), included at the end of 3ds_poketouch.c
 * after the Pokedex, whose picture helpers it shares. A remote for FireRed's own keyboard: the name so far on an amber
 * band, one slot per letter it takes, tabs for its three pages with DELETE and OK, and the keys of its page (FireRed's
 * own characters) on light keys, FireRed's cursor amber. The question and, for a Pokemon, its picture are in the note.
 * Every character is FireRed's (CtrBottomNaming_*), but for the tabs' and buttons' labels (3ds_ui_strings.h). */

#define FN_SLOT_TOP UI_RGB(62, 14, 14)
#define FN_SLOT_BOT UI_RGB(42, 8, 9)
#define FN_SLOT_EDGE UI_RGB(20, 3, 4)

static const CtrPokeTouchRect sFNBand = {4, 4, 240, 34};
static const CtrPokeTouchRect sFNDelete = {142, 42, 50, 22};
static const CtrPokeTouchRect sFNOk = {194, 42, 50, 22};

static CtrPokeTouchRect FNTab(int page)
{
    /* Left to right: capitals, small letters, then FireRed's symbols page. */
    static const int8_t kSlot[3] = {2, 0, 1};
    CtrPokeTouchRect r = {(int16_t)(4 + 46 * kSlot[page]), 42, 44, 22};
    return r;
}

static CtrPokeTouchRect FNKey(const CtrPokeTouchName *n, int row, int col)
{
    int pitch = n->cols > 6 ? 30 : 40;
    int left = 4 + (240 - (pitch * n->cols - 2)) / 2;
    CtrPokeTouchRect r = {(int16_t)(left + pitch * col), (int16_t)(68 + 24 * row), (int16_t)(pitch - 2), 22};
    return r;
}

int CtrPokeTouch_NameHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchName *n = &v->name;

    if (!n->on)
        return -1;
    for (int p = 0; p < 3; ++p) {
        CtrPokeTouchRect r = FNTab(p);
        if (UiIn(&r, x, y))
            return PT_NAME_HIT_TAB + p;
    }
    if (UiIn(&sFNDelete, x, y))
        return PT_NAME_HIT_DELETE;
    if (UiIn(&sFNOk, x, y))
        return PT_NAME_HIT_OK;
    for (int row = 0; row < PT_NAME_ROWS; ++row)
        for (int col = 0; col < n->cols && col < PT_NAME_COLS; ++col) {
            CtrPokeTouchRect r = FNKey(n, row, col);
            if (n->key[row][col] != 0xff && UiIn(&r, x, y))
                return PT_NAME_HIT_KEY + row * PT_NAME_COLS + col;
        }
    return -1;
}

/* One FireRed code as text for UiText: its glyph as it is (GlyphCode's 0x01 escape), the space as a space. */
static const char *FNChar(uint8_t code, char out[3])
{
    out[0] = code ? 0x01 : ' ';
    out[1] = (char)code;
    out[2] = 0;
    if (!code)
        out[1] = 0;
    return out;
}

/* How many argument bytes FireRed's text effect `code` (after 0xfc) takes, as GetStringWidth in text.c counts them. */
static int FNEffectArgs(uint8_t code)
{
    switch (code) {
    case 0x04:                          /* colour, highlight and shadow */
        return 3;
    case 0x0b: case 0x10:               /* play a song or a sound */
        return 2;
    case 0x01: case 0x02: case 0x03: case 0x05: case 0x06: case 0x08: case 0x0c: case 0x0d: case 0x0e:
    case 0x11: case 0x12: case 0x13: case 0x14:
        return 1;
    default:
        return 0;
    }
}

/* FireRed's extra symbol `code` (after 0xf9) as text for UiText, or NULL when it has no glyph here. */
static const char *FNSymbol(uint8_t code)
{
    switch (code) {
    case 0x04: return "\x01\x2e";          /* + */
    case 0x05: return "\x01\x34";          /* Lv, FireRed's own glyph */
    case 0x06: return "PP";
    case 0x07: return "ID";
    case 0x08: return "No";
    case 0x13: return "(";
    case 0x14: return ")";
    default: return NULL;
    }
}

/* FireRed codes (0xff ended) as text for UiText: its control codes are left out, a line break is a space. */
static void FNText(const uint8_t *raw, int n, char *out, int size)
{
    int o = 0, jp = 0;

    for (int i = 0; i < n && raw[i] != 0xff && o + 3 <= size; ++i) {
        uint8_t c = raw[i];

        if (c == 0xfc && i + 1 < n && (raw[i + 1] == 0x15 || raw[i + 1] == 0x16)) {
            jp = raw[i + 1] == 0x15;    /* a Japanese Pokemon's name, between JPN and ENG: its own glyphs */
            ++i;
            continue;
        }
        if (c == 0xfc) {                /* a text effect: its code and arguments are not glyphs */
            i += 1 + (i + 1 < n ? FNEffectArgs(raw[i + 1]) : 0);
            continue;
        }
        if (c == 0xf9) {                /* an extra symbol: Lv in the box's note, PP, ID... */
            const char *sym = i + 1 < n ? FNSymbol(raw[i + 1]) : NULL;

            for (; sym && *sym && o + 1 < size; ++sym)
                out[o++] = *sym;
            ++i;
            continue;
        }
        if (c == 0xf7 || c == 0xf8 || c == 0xfd) {     /* a buffer, a button icon or a placeholder: one more byte */
            ++i;
            continue;
        }
        if (c == 0xfe || c == 0)
            out[o++] = ' ';
        else if (c < 0xf7) {
            out[o++] = jp ? 0x02 : 0x01;
            out[o++] = (char)c;
        }
    }
    out[o] = 0;
}

/* The space key's mark: a flat open bracket under the middle of the key. */
static void FNSpaceMark(int cx, int y, uint16_t c)
{
    Fill(cx - 6, y + 6, 13, 1, c);
    Fill(cx - 6, y + 3, 1, 3, c);
    Fill(cx + 6, y + 3, 1, 3, c);
}

static void FNEntry(const CtrPokeTouchName *n)
{
    const CtrPokeTouchRect *b = &sFNBand;
    int slots = n->maxChars ? n->maxChars : 1, pitch = (b->w - 16) / slots, x0, sw, sy = b->y + 6, sh = 22;
    char g[3];

    if (pitch > 31)
        pitch = 31;
    x0 = b->x + (b->w - pitch * slots + 4) / 2;
    sw = pitch - 4;
    /* The mockup's dark slot, a light letter box for each letter written, a ring on the one the next letter takes. */
    UiRoundFill(b->x, b->y, b->w, b->h, 5, FN_SLOT_EDGE);
    EmGradFill(b->x + 1, b->y + 1, b->w - 2, b->h - 2, 4, FN_SLOT_TOP, FN_SLOT_BOT);
    for (int i = 0; i < slots; ++i) {
        int sx = x0 + i * pitch;
        int written = i < n->len && n->text[i];

        UiRoundFill(sx, sy, sw, sh, 3, written || i != n->len ? LL_SLATE : LL_RING_IN);
        if (written)
            EmGradFill(sx + 1, sy + 1, sw - 2, sh - 2, 2, EM_WHITE, UI_RGB(240, 230, 204));
        else
            EmGradFill(sx + 1, sy + 1, sw - 2, sh - 2, 2, UI_RGB(92, 44, 44), UI_RGB(84, 38, 40));
        if (written) {
            FNChar(n->text[i], g);
            UiText(g, sx + (sw - UiTextWidth(g, 1)) / 2, sy + (sh - UI_TEXT_H) / 2, EM_DARK, EM_DARK_SH, 1, sx + sw);
        }
    }
    if (n->gender) {
        char sign[3];

        FNChar(n->gender == 2 ? 0xb6 : 0xb5, sign);
        UiText(sign, x0 + slots * pitch + 2, b->y + 10, n->gender == 2 ? UI_RGB(255, 214, 214) : UI_RGB(210, 232, 255),
               FN_SLOT_EDGE, 1, b->x + b->w - 2);
    }
}

static void FNButton(const CtrPokeTouchRect *r, const char *label, const EmPal *p, int pressed)
{
    int fy = EmButton(r->x, r->y, r->w, r->h, p, FB_DEPTH, pressed);

    EmLabel(label, r->x, r->w, fy, r->h - FB_DEPTH, p);
}

static void FNTabs(const CtrPokeTouchName *n)
{
    static const int kLabel[3] = {UIS_NAME_SYMBOLS, UIS_NAME_UPPER, UIS_NAME_LOWER};
    int onButtons = n->curCol >= n->cols;

    for (int p = 0; p < 3; ++p) {
        CtrPokeTouchRect r = FNTab(p);
        /* The cursor on the page button (FireRed's first button): the tab its A turns to is the one it marks. */
        const EmPal *pal = LlPick(n->page == p, &kEmCream);

        if (onButtons && n->curRow == 0 && p == (n->page + 1) % 3)
            LlSelectRing(r.x, r.y, r.w, r.h);
        FNButton(&r, CtrUiText(kLabel[p]), pal, n->pressed == PT_NAME_HIT_TAB + p);
    }
    if (onButtons && n->curRow == 1)
        LlSelectRing(sFNDelete.x, sFNDelete.y, sFNDelete.w, sFNDelete.h);
    if (onButtons && n->curRow == 2)
        LlSelectRing(sFNOk.x, sFNOk.y, sFNOk.w, sFNOk.h);
    FNButton(&sFNDelete, CtrUiText(UIS_NAME_DELETE), LlPick(onButtons && n->curRow == 1, &kEmGold), n->pressed == PT_NAME_HIT_DELETE);
    FNButton(&sFNOk, CtrUiText(UIS_NAME_OK), &kEmFlame, n->pressed == PT_NAME_HIT_OK);
}

static void FNKeys(const CtrPokeTouchName *n)
{
    char g[3];

    for (int row = 0; row < PT_NAME_ROWS; ++row)
        for (int col = 0; col < n->cols && col < PT_NAME_COLS; ++col) {
            CtrPokeTouchRect r = FNKey(n, row, col);
            uint8_t code = n->key[row][col];
            const EmPal *p = LlPick(n->curCol == col && n->curRow == row, &kEmCream);
            int fy;

            if (code == 0xff)
                continue;
            fy = EmButton(r.x, r.y, r.w, r.h, p, FB_DEPTH, n->pressed == PT_NAME_HIT_KEY + row * PT_NAME_COLS + col);
            if (code == 0)
                FNSpaceMark(r.x + r.w / 2, fy + 5, p->ink);
            else
                EmLabel(FNChar(code, g), r.x, r.w, fy, r.h - FB_DEPTH, p);
        }
    /* The chosen key's ring last: its neighbours are 2 px away and would cover it. */
    if (n->curCol < n->cols && n->curCol < PT_NAME_COLS && n->curRow < PT_NAME_ROWS && n->key[n->curRow][n->curCol] != 0xff) {
        CtrPokeTouchRect r = FNKey(n, n->curRow, n->curCol);

        LlSelectRing(r.x, r.y, r.w, r.h);
    }
}

static void FNameFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchName *n = &v->name;

    EmSurface();
    TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    FNEntry(n);
    FNTabs(n);
    FNKeys(n);
    sUiS = &sUiNavy;
}

static void FNameNote(const CtrPokeTouchView *v, int w)
{
    const CtrPokeTouchName *n = &v->name;
    char title[66];
    int textW = n->species ? w - 76 : w - 18;

    EmSurface();
    LlWindow(NOTE_X, NOTE_Y, w, NOTE_H);
    FNText(n->title, (int)sizeof(n->title), title, (int)sizeof(title));
    UiText(title, NOTE_X + 12, NOTE_Y + 6, EM_DARK, EM_DARK_SH, 1, NOTE_X + 12 + textW);
    BPWrap(CtrUiText(UIS_NAME_HINT), NOTE_X + 12, NOTE_Y + 21, textW, 3, EM_DIM, EM_DARK_SH);
    if (n->species)
        FDPicDraw(n->species, NOTE_X + w - 68, NOTE_Y, 0);
    sUiS = &sUiNavy;
}
