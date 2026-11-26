/* The Poke Ball SAVE, Trainer Card and Pokedex on the whole lower screen (included at the end of 3ds_poketouch.c, after 3ds_ball_options_impl.h and its
 * grammar): the red ground down to y 176, the dark band there with the exit piece on it (BallExitR, 240,162 74x38) and the note on the white under it;
 * on the ground white cards with a dark-red edge (SAVE and the Card carry a dark-red strip with their title across the top), and pills (white for what
 * is chosen, dark red for the rest). Each screen is drawn again as a whole when anything changes, and its hit zones follow the geometry below while it
 * is up (sBallFull). */

#define BM_CARD_EDGE UI_RGB(150, 22, 26)
#define BM_CARD_FACE UI_RGB(250, 250, 248)
#define BM_INK UI_RGB(60, 52, 56)
#define BM_DIM UI_RGB(120, 112, 116)
#define BM_RULE UI_RGB(230, 224, 224)
#define BM_PILL UI_RGB(176, 30, 34)
#define BM_PILL_EDGE UI_RGB(120, 16, 20)
#define BM_PRESSED UI_RGB(222, 218, 218)
#define BM_STRIP UI_RGB(150, 22, 26)
#define BM_STRIP_HI UI_RGB(178, 34, 40)

/* A white card with the dark-red edge. */
static void BmCard(int x, int y, int w, int h)
{
    UiRoundFill(x, y, w, h, 6, BM_CARD_EDGE);
    UiRoundFill(x + 2, y + 2, w - 4, h - 4, 5, BM_CARD_FACE);
}

/* The dark-red strip across the top of a card (inside its edge), with `title` on it in white. */
static void BmStrip(int x, int y, int w, const char *title)
{
    UiRoundFill(x, y, w, 20, 5, BM_STRIP);
    Fill(x, y + 10, w, 10, BM_STRIP);
    Fill(x + 4, y + 1, w - 8, 8, BM_STRIP_HI);
    UiText(title, x + 10, y + 5, EM_WHITE, BM_PILL_EDGE, 1, x + w - 8);
}

/* A pill with its word in the middle: white with a red word when chosen, dark red with a white one when not; one under a touch is a shade off. A
 * chosen one with `cursor` has the right-pointing triangle before its word. */
static void BmPill(const CtrPokeTouchRect *r, const char *label, int chosen, int pressed, int cursor)
{
    uint16_t face = chosen ? (pressed ? BM_PRESSED : BL_WHITE) : (pressed ? BM_CARD_EDGE : BM_PILL);
    int tw = UiTextWidth(label, 1), room = r->w - 8;

    UiRoundFill(r->x, r->y, r->w, r->h, 8, chosen ? BM_CARD_EDGE : BM_PILL_EDGE);
    UiRoundFill(r->x + 2, r->y + 2, r->w - 4, r->h - 4, 6, face);
    if (tw > room)
        tw = room;
    if (chosen && cursor)
        LlArrow(r->x + 9, r->y + (r->h - 10) / 2, 6, 10, 3, BL_REDTXT);
    UiText(label, r->x + (r->w - tw) / 2 + (chosen && cursor ? 5 : 0), r->y + (r->h - UI_TEXT_H) / 2, chosen ? BL_REDTXT : EM_WHITE, face, 1, r->x + r->w - 4);
}

/* The exit piece on the band (the same as the OPTIONS' one), the header word at the top left (none when `title` is NULL: the screen has its own top)
 * and the note on the white under the band. */
static void BmFrame(const char *title, const char *noteTitle, const char *note, const char *exitLabel, int backPressed, int backOn)
{
    int tw = exitLabel ? UiTextWidth(exitLabel, 1) : 0;

    sBallExitR = exitLabel ? (CtrPokeTouchRect){240, 162, 74, 38} : (CtrPokeTouchRect){0, 0, 0, 0};   /* no label: no exit piece (SAVE has no BACK) */
    EmSurface();
    TsBallGround(BO_BAND_Y);
    if (title) {
        UiText(title, 14, 10, EM_WHITE, BL_DEEP, 1, 220);
        Fill(8, 26, 304, 1, BL_HAIR);
    }
    if (exitLabel) {
        UiRoundFill(sBallExitR.x, sBallExitR.y, sBallExitR.w, sBallExitR.h, 5, BL_BAND);
        UiRoundFill(sBallExitR.x + 3, sBallExitR.y + 3, sBallExitR.w - 6, sBallExitR.h - 6, 4,
                    !backOn ? UI_RGB(196, 190, 190) : backPressed ? UI_RGB(222, 218, 218) : BL_WHITE);
        UiText(exitLabel, sBallExitR.x + (sBallExitR.w - tw) / 2, sBallExitR.y + (sBallExitR.h - UI_TEXT_H) / 2, BL_BAND, BL_WHITE, 1, sBallExitR.x + sBallExitR.w - 4);
    }
    UiText(noteTitle, 14, BO_BAND_Y + 14, BL_BAND, BL_WHITE, 1, 232);
    if (note[0])
        MenuWrap(note, 14, BO_BAND_Y + 30, exitLabel ? 216 : 292, 3, 12, UI_RGB(132, 128, 130), BL_WHITE);
}

/* A labelled row of a card: the label on the left, the value (red) right-aligned at `right`, a hairline under it. */
static void BmRow(const char *label, const char *value, int x, int right, int y)
{
    char fit[40];

    UiText(label, x, y + 3, BM_DIM, BM_CARD_FACE, 1, right - UiTextWidth(value, 1) - 6);
    Em2Fit(value, 96, fit, sizeof(fit));
    UiTextRight(fit, right, y + 3, BL_REDTXT, BM_CARD_FACE, 1);
    Fill(x - 2, y + 18, right - x + 4, 1, BM_RULE);
}

/* ---- SAVE: the facts on a card under a strip, the question in a card of its own (a round "?" before it), the portrait and YES / NO at the right ---- */

static void BallSaveScreen(const CtrPokeTouchView *v)
{
    const int scale = 3, px0 = 253, py0 = 17;
    int confirm = v->saveState == SAVE_CONFIRM, writing = v->saveState == SAVE_WRITING;
    char text[24];
    const char *hint = v->bar[1][0] ? v->bar[1]
                     : confirm ? CTR_TR("It is saved to the SD card. Don't turn off the console.", "Se guarda en la tarjeta SD. No apagues la consola.",
                                        "Wird auf der SD-Karte gespeichert. Nicht ausschalten.")
                     : v->saveState == SAVE_SUCCESS ? CTR_TR("Your progress is safe.", "Tu progreso est\xC3\xA1 a salvo.", "Dein Fortschritt ist gesichert.") : "";

    BmFrame(NULL, CTR_TR("Save", "Guardar", "Sichern"), hint, NULL, 0, 0);   /* SAVE has no BACK: NO, B and the sidebar leave */
    BmCard(8, 8, 226, 102);
    BmStrip(10, 10, 222, CTR_TR("SAVE GAME", "GUARDAR PARTIDA", "SPIEL SICHERN"));
    for (int f = 0; f < SF_COUNT; ++f) {
        SaveValueText(f, v, text, sizeof(text));
        BmRow(sSaveLabel[f], text, 20, 222, 34 + f * 18);
    }
    BmCard(8, 116, 226, 40);
    if (writing) {
        Em2Wrap(v->bar[0], 18, 127, 160, 2, 14, BM_INK, BM_CARD_FACE);
        for (int i = 0; i < 3; ++i)
            TsDisc(192 + 14 * i, 137 - (i == 1 ? 2 : 0), 3, i == 1 ? BL_REDTXT : UI_RGB(233, 165, 160));
    } else {
        TsDisc(26, 136, 11, BL_REDTXT);
        if (v->saveState == SAVE_SUCCESS) {             /* a tick: the game is saved */
            for (int i = 0; i < 4; ++i)
                Fill(20 + i, 135 + i, 2, 2, EM_WHITE);
            for (int i = 0; i < 8; ++i)
                Fill(23 + i, 138 - i, 2, 2, EM_WHITE);
        } else
            UiText(v->saveState == SAVE_ERROR ? "!" : "?", 24, 130, EM_WHITE, UI_RGB(120, 16, 20), 1, 34);
        Em2Wrap(v->bar[0], 44, 127, 184, 2, 14, BM_INK, BM_CARD_FACE);
    }
    BmCard(242, 8, 70, 62);
    if (sPlayer.key != v->playerFemale)
        IconBuild(&sPlayer, v->playerFemale, CtrPokeTouch_MapPlayerIcon);
    if (sPlayer.ok)
        for (int py = 0; py < MARKER; ++py)
            for (int px = 0; px < MARKER; ++px) {
                uint8_t index = sPlayer.pixels[0][py * MARKER + px];

                if (index)
                    Fill(px0 + px * scale - 3, py0 + py * scale - 2, scale, scale, sPlayer.pal[index]);
            }
    if (confirm) {
        BmPill(&sBallSaveYes, CTR_TR("Yes", "S\xC3\xAD", "Ja"), v->saveSel == 0, v->savePressed == PT_SAVE_FIRST, 1);
        BmPill(&sBallSaveNo, CTR_TR("No", "No", "Nein"), v->saveSel == 1, v->savePressed == PT_SAVE_SECOND, 1);
    } else if (!writing)
        BmPill(&sBallSaveYes, "OK", 1, v->savePressed == PT_SAVE_FIRST, 0);
    sUiS = &sUiNavy;
}

/* ---- TRAINER CARD: one wide white card under a strip with the title, the stars and the ID; the front has four facts, the trainer's picture and the
 * eight badges, the back the records in two columns ---- */

static void BmBadges(const CtrPokeTouchCard *c, int x, int y)
{
    static unsigned char tiles[0x400];
    static unsigned short pal[16];
    static uint8_t px[128 * 16];
    static uint16_t pal565[16];
    int ok = CtrPokeTouch_CardBadges(tiles, pal), have = 0;
    char text[16];

    if (ok) {
        FDTiles(tiles, 128, 16, px);
        FDPal(pal, pal565);
    }
    UiRoundFill(x - 4, y - 3, 282, 28, 13, UI_RGB(244, 238, 238));
    for (int i = 0; i < 8; ++i) {
        int sx = x + i * 25;

        UiRoundFill(sx, y, 22, 22, 11, BM_RULE);
        UiRoundFill(sx + 1, y + 1, 20, 20, 10, UI_RGB(250, 246, 244));
        if (c->badges & (1u << i)) {
            ++have;
            if (ok)
                UiImage(px + i * 16, pal565, 128, 16, 16, sx + 3, y + 3, 1, 1);
        }
    }
    snprintf(text, sizeof(text), "%d/8", have);
    UiTextRight(text, x + 270, y + 5, BL_REDTXT, UI_RGB(244, 238, 238), 1);
}

static void BallCardScreen(const CtrPokeTouchView *v)
{
    const CtrPokeTouchCard *c = &v->card;
    char text[24], label[24];
    int stars = c->stars < 5 ? c->stars : 5;

    BmFrame(NULL, CtrUiText(UIS_CARD_TITLE), CtrUiText(UIS_CARD_TURN), CTR_TR("BACK", "ATR\xC3\x81S", "ZUR\xC3\x9C" "CK"), v->backPressed, v->backEnabled);
    BmCard(sBallCardR.x, sBallCardR.y, sBallCardR.w, sBallCardR.h);
    if (c->pressed) {                                   /* held: the edge turns white */
        UiRoundFill(sBallCardR.x, sBallCardR.y, sBallCardR.w, sBallCardR.h, 6, BL_WHITE);
        UiRoundFill(sBallCardR.x + 2, sBallCardR.y + 2, sBallCardR.w - 4, sBallCardR.h - 4, 5, BM_CARD_FACE);
    }
    if (c->back) {
        BmStrip(10, 10, 300, c->name);
        for (int i = 0; i < c->rows && i < PT_CARD_ROWS; ++i)
            BmRow(c->row[i].label, c->row[i].value, 22 + (i % 2) * 150, 152 + (i % 2) * 148, 36 + (i / 2) * 28);
        sUiS = &sUiNavy;
        return;
    }
    BmStrip(10, 10, 300, CtrUiText(UIS_CARD_TITLE));
    snprintf(text, sizeof(text), "%s%05u", c->label[1], (unsigned)c->id);
    UiTextRight(text, 302, 15, EM_WHITE, BM_PILL_EDGE, 1);
    for (int i = 0; i < stars; ++i)
        FCStar(302 - UiTextWidth(text, 1) - 8 - (stars - i) * 13, 14, UI_RGB(255, 236, 140), BM_PILL_EDGE);
    snprintf(label, sizeof(label), "%s", c->label[0]);
    for (int n = (int)strlen(label); n > 0 && (label[n - 1] == ' ' || label[n - 1] == ':'); --n)
        label[n - 1] = 0;
    BmRow(label, c->name, 22, 188, 36);
    snprintf(text, sizeof(text), "%s%u", c->label[3], (unsigned)c->money);
    BmRow(c->label[2], text, 22, 188, 60);
    if (c->hasDex) {
        snprintf(text, sizeof(text), "%u", (unsigned)c->caught);
        BmRow(c->label[4], text, 22, 188, 84);
    }
    snprintf(text, sizeof(text), "%u:%02u", (unsigned)c->hours, (unsigned)c->minutes);
    BmRow(c->label[5], text, 22, 188, c->hasDex ? 108 : 84);
    UiRoundFill(204, 36, 96, 84, 10, UI_RGB(234, 217, 195));
    UiRoundFill(206, 38, 92, 80, 9, UI_RGB(243, 235, 224));
    TsDisc(252, 78, 31, UI_RGB(226, 218, 206));
    FCTrainer(220, 46);
    BmBadges(c, 22, 126);
    sUiS = &sUiNavy;
}

/* ---- POKEDEX: the same pieces as the classic one (3ds_field_dex_impl.h: tabs, cards, rows, the page), their rectangles in screen pixels (sBallFull)
 * on the Poke Ball ground; the exit piece closes it, the note under the band says what the selection is (a Pokemon's page: its entry) ---- */

static void BallDexScreen(const CtrPokeTouchView *v)
{
    const CtrPokeTouchDex *d = &v->dex;
    char title[40], body[96];
    const char *entry;

    FDexNoteWords(d, title, sizeof(title), body, sizeof(body), &entry);
    BmFrame(NULL, entry ? d->sel.name : title, entry ? entry : body, CTR_TR("BACK", "ATR\xC3\x81S", "ZUR\xC3\x9C" "CK"), v->backPressed, v->backEnabled);
    if (d->state == PT_DEX_STATE_MENU)
        FDMenu(d, d->pressed);
    else if (d->state == PT_DEX_STATE_LIST)
        FDList(d, d->pressed);
    else if (d->state == PT_DEX_STATE_CATEGORY)
        FDCategory(d, d->pressed);
    else if (d->state == PT_DEX_STATE_PAGE)
        FDPage(d);
    sUiS = &sUiNavy;
}
