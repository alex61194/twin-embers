/* The Union Room chat's keyboard on the floor of HOME's layout (3ds_poketouch.h CtrPokeTouchChat), included at the end of 3ds_poketouch.c
 * after the trade's cards. From the top: the page tabs, the message typed, the keys of the page (FireRed's 5 x 10 grid laid on its side,
 * 10 across), and Delete, Send and Quit. The registered page lists its phrases, two a row. Every glyph is FireRed's. */

enum { FCH_TAB_H = 16, FCH_MSG_Y = 19, FCH_MSG_H = 17, FCH_KEY_Y = 39, FCH_KEY_H = 19, FCH_BTN_Y = 137, FCH_BTN_H = 21 };

/* The tabs: the four pages of keys, then CHAT (index PT_CHAT_PAGES), which shows what everybody said in place of the keys. */
static CtrPokeTouchRect FChatTab(int page)
{
    int w = (PT_VIEW_W - 4) / (PT_CHAT_PAGES + 1);
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

/* The card of the messages, where the keys were. */
static CtrPokeTouchRect FChatLogCard(void)
{
    CtrPokeTouchRect r = {(int16_t)(PT_VIEW_X + 2), (int16_t)(PT_VIEW_Y + FCH_KEY_Y), (int16_t)(PT_VIEW_W - 4), (int16_t)(FCH_BTN_Y - FCH_KEY_Y - 3)};

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
    for (int row = 0; row < 10 && !t->logOpen; ++row)       /* with the messages open the keys are not there */
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
    r = FChatTab(PT_CHAT_PAGES);
    if (UiIn(&r, x, y))
        return PT_CHOICE_HIT_ROW + PT_CHAT_LOG_TAB;
    for (int k = 0; k < 3; ++k) {
        r = FChatButton(k);
        if (UiIn(&r, x, y))
            return PT_CHOICE_HIT_ROW + PT_CHAT_DELETE + k;
    }
    return -1;
}

/* One event of the room as a line of text: "Name: what was said" with the name apart (name, said), or one text for a join or a leave (name empty). */
static void FChatLine(const CtrPokeTouchChat *t, int i, char *name, int nameSize, char *said, int saidSize)
{
    char who[40];

    FNText(t->logName[i], (int)sizeof(t->logName[i]), who, (int)sizeof(who));
    name[0] = 0;
    said[0] = 0;
    if (t->logKind[i] == PT_CHAT_JOINED)
        snprintf(said, (size_t)saidSize, CTR_TR("%s joined the chat.", "%s se ha unido al chat.", "%s ist dem Chat beigetreten."), who);
    else if (t->logKind[i] == PT_CHAT_LEFT)
        snprintf(said, (size_t)saidSize, CTR_TR("%s left the chat.", "%s ha salido del chat.", "%s hat den Chat verlassen."), who);
    else {
        snprintf(name, (size_t)nameSize, "%s:", who);
        FNText(t->logText[i], (int)sizeof(t->logText[i]), said, saidSize);
    }
}

/* A line of the room's messages at (x, top): the name red, what was said dark; a join or a leave dim. */
static void FChatLineDraw(const CtrPokeTouchChat *t, int i, int x, int top, int limit, uint16_t face)
{
    char name[48], said[128];
    int nw;

    FChatLine(t, i, name, (int)sizeof(name), said, (int)sizeof(said));
    if (!name[0]) {
        SmallText(said, x, top, BM_DIM, face, limit);
        return;
    }
    nw = SmallWidth(name);
    SmallText(name, x, top, BL_REDTXT, face, limit);
    SmallText(said, x + nw + 5, top, BM_INK, face, limit);
}

/* The messages instead of the keys: a card of up to PT_CHAT_LOG lines, the oldest at the top, the newest at the bottom. */
static void FChatLogView(const CtrPokeTouchChat *t)
{
    CtrPokeTouchRect r = FChatLogCard();
    const int step = 14;

    BmCard(r.x, r.y, r.w, r.h);
    if (!t->logCount) {
        const char *none = CTR_TR("Nobody has said anything yet.", "Nadie ha dicho nada todav\xC3\xAD" "a.", "Noch hat niemand etwas gesagt.");

        UiText(none, r.x + (r.w - UiTextWidth(none, 1)) / 2, r.y + (r.h - UI_TEXT_H) / 2, BM_DIM, BM_CARD_FACE, 1, r.x + r.w - 6);
        return;
    }
    for (int i = 0; i < t->logCount && i < PT_CHAT_LOG; ++i) {
        if (i)
            Fill(r.x + 8, r.y + 5 + i * step - 1, r.w - 16, 1, BM_RULE);
        FChatLineDraw(t, i, r.x + 8, r.y + 5 + i * step, r.x + r.w - 6, BM_CARD_FACE);
    }
}

/* The note under the keyboard: the last three things said in the room (the newest at the bottom), or the hint while nothing has been. */
static void FChatNote(const CtrPokeTouchView *v)
{
    const CtrPokeTouchChat *t = &v->chat;
    const int first = t->logCount > 3 ? t->logCount - 3 : 0;

    if (t->logOpen || !t->logCount) {
        MenuWrap(t->logOpen ? CTR_TR("The room's messages. Tap a page to type.", "Mensajes de la sala. Toca una p\xC3\xA1gina para escribir.",
                                     "Nachrichten im Raum. Seite antippen zum Schreiben.")
                            : v->choice.prompt,
                 14, BO_BAND_Y + 14, 292, 3, 14, BL_BAND, BL_WHITE);
        return;
    }
    for (int i = first; i < t->logCount; ++i)
        FChatLineDraw(t, i, 14, BO_BAND_Y + 14 + (i - first) * 14, 306, BL_WHITE);
}

static void FChatFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchChat *t = &v->chat;
    int pressed = v->choice.pressed;
    char text[128];                 /* 15 glyphs of up to 3 UTF-8 bytes (the hearts, the symbols) must not be cut */

    EmSurface();
    if (!sBallFull)
        TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    for (int p = 0; p <= PT_CHAT_PAGES; ++p) {
        CtrPokeTouchRect r = FChatTab(p);
        const int isLog = p == PT_CHAT_PAGES;
        const EmPal *pal = (isLog ? t->logOpen : (p == t->page && !t->logOpen)) ? &kEmFlame : &kEmCream;
        int fy = BxFace(r.x, r.y, r.w, r.h, pal, 2, pressed == PT_CHOICE_HIT_ROW + (isLog ? PT_CHAT_LOG_TAB : PT_CHAT_TAB + p));
        static const char *const names[PT_CHAT_PAGES] = {"ABC", "abc", "!?&", "\xE2\x99\xA5"};

        BxLabel(isLog ? CTR_TR("Chat", "Chat", "Chat") : p == 3 ? CTR_TR("Saved", "Frases", "Eigene") : names[p], r.x, r.w, fy, r.h - 2, pal);
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
    if (t->logOpen)
        FChatLogView(t);
    for (int row = 0; row < 10 && !t->logOpen; ++row)
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
