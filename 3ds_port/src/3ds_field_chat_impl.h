/* The Union Room chat's keyboard on the floor of HOME's layout (3ds_poketouch.h CtrPokeTouchChat), included at the end of 3ds_poketouch.c
 * after the trade's cards. From the top: the page tabs, the message typed, the keys of the page (FireRed's 5 x 10 grid laid on its side,
 * 10 across), and Delete, Send and Quit. The registered page lists its phrases, two a row. Every glyph is FireRed's. */

enum { FCH_TAB_H = 16, FCH_MSG_Y = 19, FCH_MSG_H = 17, FCH_KEY_Y = 39, FCH_KEY_H = 19, FCH_BTN_Y = 137, FCH_BTN_H = 21 };

static CtrPokeTouchRect FChatTab(int page)
{
    int w = (PT_VIEW_W - 4) / PT_CHAT_PAGES;
    CtrPokeTouchRect r = {(int16_t)(PT_VIEW_X + 2 + page * w), (int16_t)PT_VIEW_Y, (int16_t)(w - 2), FCH_TAB_H};

    return r;
}

/* The key of FireRed's (row, column) of a keyboard page. On the registered page a phrase is a row: two to a line, five lines. */
static CtrPokeTouchRect FChatKey(int page, int row, int col)
{
    CtrPokeTouchRect r;

    if (page == 3) {
        r.x = (int16_t)(PT_VIEW_X + 2 + (row / 5) * 119);
        r.y = (int16_t)(PT_VIEW_Y + FCH_KEY_Y + (row % 5) * (FCH_KEY_H + 1));
        r.w = 117;
    } else {
        r.x = (int16_t)(PT_VIEW_X + 2 + row * (PT_VIEW_W - 4) / 10);
        r.y = (int16_t)(PT_VIEW_Y + FCH_KEY_Y + col * (FCH_KEY_H + 1));
        r.w = 22;
    }
    r.h = FCH_KEY_H;
    return r;
}

static CtrPokeTouchRect FChatButton(int k)
{
    int w = (PT_VIEW_W - 4 - 8) / 3;
    CtrPokeTouchRect r = {(int16_t)(PT_VIEW_X + 2 + k * (w + 4)), (int16_t)(PT_VIEW_Y + FCH_BTN_Y), (int16_t)w, FCH_BTN_H};

    return r;
}

static int FChatHasKey(const CtrPokeTouchChat *t, int row, int col)
{
    return t->page == 3 ? t->reg[row][0] != 0xff : t->key[row][col][0] != 0xff;
}

static int FChatHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchChat *t = &v->chat;
    CtrPokeTouchRect r;

    x -= BxShiftX();
    y -= BxShiftY();
    for (int row = 0; row < 10; ++row)
        for (int col = 0; col < (t->page == 3 ? 1 : 5); ++col) {
            r = FChatKey(t->page, row, col);
            if (FChatHasKey(t, row, col) && UiIn(&r, x, y))
                return PT_CHOICE_HIT_ROW + row * 5 + col;
        }
    for (int p = 0; p < PT_CHAT_PAGES; ++p) {
        r = FChatTab(p);
        if (UiIn(&r, x, y))
            return PT_CHOICE_HIT_ROW + PT_CHAT_TAB + p;
    }
    for (int k = 0; k < 3; ++k) {
        r = FChatButton(k);
        if (UiIn(&r, x, y))
            return PT_CHOICE_HIT_ROW + PT_CHAT_DELETE + k;
    }
    return -1;
}

static void FChatFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchChat *t = &v->chat;
    int pressed = v->choice.pressed;
    char text[64];

    EmSurface();
    if (!sBallFull)
        TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    for (int p = 0; p < PT_CHAT_PAGES; ++p) {
        CtrPokeTouchRect r = FChatTab(p);
        const EmPal *pal = p == t->page ? &kEmFlame : &kEmCream;
        int fy = BxFace(r.x, r.y, r.w, r.h, pal, 2, pressed == PT_CHOICE_HIT_ROW + PT_CHAT_TAB + p);
        static const char *const names[PT_CHAT_PAGES] = {"ABC", "abc", "!?&", "\xE2\x99\xA5"};

        BxLabel(p == 3 ? CTR_TR("Saved", "Frases", "Eigene") : names[p], r.x, r.w, fy, r.h - 2, pal);
    }
    {
        CtrPokeTouchRect r = {(int16_t)(PT_VIEW_X + 2), (int16_t)(PT_VIEW_Y + FCH_MSG_Y), (int16_t)(PT_VIEW_W - 4), FCH_MSG_H};

        if (sStyleBall) {                               /* the message so far on a white card */
            BmCard(r.x, r.y, r.w, r.h);
            FNText(t->msg, (int)sizeof(t->msg), text, (int)sizeof(text));
            if (t->length)
                UiText(text, r.x + 6, r.y + 4, BM_INK, BM_CARD_FACE, 1, r.x + r.w - 5);
            else
                UiText(CTR_TR("Type a message", "Escribe un mensaje", "Nachricht eingeben"), r.x + 6, r.y + 4, BM_DIM, BM_CARD_FACE, 1, r.x + r.w - 5);
        } else {
            UiRoundFill(r.x, r.y, r.w, r.h, 5, UI_RGB(34, 6, 6));
            FNText(t->msg, (int)sizeof(t->msg), text, (int)sizeof(text));
            if (t->length)
                UiText(text, r.x + 5, r.y + 4, EM_WHITE, UI_RGB(60, 12, 12), 1, r.x + r.w - 5);
            else
                UiText(CTR_TR("Type a message", "Escribe un mensaje", "Nachricht eingeben"), r.x + 5, r.y + 4, UI_RGB(150, 110, 100),
                       UI_RGB(60, 12, 12), 1, r.x + r.w - 5);
        }
    }
    for (int row = 0; row < 10; ++row)
        for (int col = 0; col < (t->page == 3 ? 1 : 5); ++col) {
            CtrPokeTouchRect r;
            int fy;

            if (!FChatHasKey(t, row, col))
                continue;
            r = FChatKey(t->page, row, col);
            fy = BxFace(r.x, r.y, r.w, r.h, &kEmCream, 2, pressed == PT_CHOICE_HIT_ROW + row * 5 + col);
            if (t->page == 3) {
                FNText(t->reg[row], (int)sizeof(t->reg[row]), text, (int)sizeof(text));
                BxLabel(text, r.x, r.w, fy, r.h - 2, &kEmCream);
            } else {
                FNText(t->key[row][col], 3, text, (int)sizeof(text));
                BxLabel(text, r.x, r.w, fy, r.h - 2, &kEmCream);
            }
        }
    for (int k = 0; k < 3; ++k) {
        CtrPokeTouchRect r = FChatButton(k);
        const EmPal *pal = k == 1 ? (t->length ? &kEmFlame : &kEmGrey) : k == 0 ? (t->length ? &kEmSlate : &kEmGrey) : (t->length ? &kEmGrey : &kEmSlate);
        int fy = BxFace(r.x, r.y, r.w, r.h, pal, 2, pressed == PT_CHOICE_HIT_ROW + PT_CHAT_DELETE + k);
        const char *label = k == 0 ? CTR_TR("DELETE", "BORRAR", "L\xC3\x96SCHEN") : k == 1 ? CTR_TR("SEND", "ENVIAR", "SENDEN")
                                                                                           : CTR_TR("QUIT", "SALIR", "VERLASSEN");

        BxLabel(label, r.x, r.w, fy, r.h - 2, pal);
    }
    sUiS = &sUiNavy;
}
