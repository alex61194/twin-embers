/* The player's Trainer Card in the viewport (3ds_poketouch.h CtrPokeTouchCard), included at the end of 3ds_poketouch.c
 * after the Pokedex, whose picture helpers it shares. A remote for FireRed's own card: a light card with an flame band,
 * its front (FireRed's labels and numbers, the trainer's front picture, the stars and the eight badge slots) or its back
 * (FireRed's records). Every word, number and picture is the card's (CtrBottomCard_*, CtrPokeTouch_Card*), but for the
 * port's title and hint (3ds_ui_strings.h). */

static const CtrPokeTouchRect sFCCard = {4, 4, 240, 160};

int CtrPokeTouch_CardHit(const CtrPokeTouchView *v, int x, int y)
{
    return v->card.on && UiIn(&sFCCard, x, y);
}

/* A flat five-pointed star, 11 pixels across, for the card's stars. */
static void FCStar(int x, int y, uint16_t c, uint16_t sh)
{
    static const char *const kStar[11] = {
        "     #     ", "    ###    ", "    ###    ", "###########", " ######### ", "  #######  ",
        "  #######  ", " ######### ", " ###   ### ", "##       ##", "           ",
    };

    for (int pass = 0; pass < 2; ++pass)
        for (int r = 0; r < 11; ++r)
            for (int q = 0; q < 11; ++q)
                if (kStar[r][q] == '#')
                    Px(x + q + (pass ? 0 : 1), y + r + (pass ? 0 : 1), pass ? c : sh);
}

static void FCTrainer(int x, int y)
{
    static unsigned char tiles[0x800];
    static unsigned short pal[16];
    static uint8_t px[64 * 64];
    static uint16_t pal565[16];

    if (!CtrPokeTouch_CardTrainerPic(tiles, pal))
        return;
    FDTiles(tiles, 64, 64, px);
    FDPal(pal, pal565);
    UiImage(px, pal565, 64, 64, 64, x, y, 1, 1);
}

static void FCBadges(const CtrPokeTouchCard *c, int x, int y)
{
    static unsigned char tiles[0x400];
    static unsigned short pal[16];
    static uint8_t px[128 * 16];
    static uint16_t pal565[16];
    int ok = CtrPokeTouch_CardBadges(tiles, pal);

    if (ok) {
        FDTiles(tiles, 128, 16, px);
        FDPal(pal, pal565);
    }
    for (int i = 0; i < 8; ++i) {
        int sx = x + i * 28;

        UiRoundFill(sx, y, 26, 26, 5, UI_RGB(206, 196, 184));
        UiRoundFill(sx + 1, y + 1, 24, 24, 4, UI_RGB(236, 230, 220));
        if (ok && (c->badges & (1u << i)))
            UiImage(px + i * 16, pal565, 128, 16, 16, sx + 5, y + 5, 1, 1);
    }
}

/* The flame band across the top of the card's window, with `title` on it (the SAVE window's: 20 tall, no outline, white ink). */
#define FC_BAND_H 20
static int FCBand(const char *title)
{
    const CtrPokeTouchRect *r = &sFCCard;
    uint16_t band[FC_BAND_H];
    int split = FC_BAND_H * 42 / 100;

    for (int row = 0; row < FC_BAND_H; ++row)
        band[row] = row < split ? Em2Lerp(UI_RGB(255, 196, 90), UI_RGB(255, 154, 56), row, split - 1)
                                : Em2Lerp(UI_RGB(248, 100, 42), UI_RGB(216, 48, 30), row - split, FC_BAND_H - split - 1);
    EmFillRows(r->x + 1, r->y + 1, r->w - 2, FC_BAND_H, 4, band, 0, 10);
    for (int row = 10; row < FC_BAND_H; ++row)
        Fill(r->x + 1, r->y + 1 + row, r->w - 2, 1, band[row]);
    Em2Text(title, r->x + 11, r->y + 1 + (FC_BAND_H - UI_TEXT_H) / 2, EM_WHITE, kEmFlame.inkSh, r->x + r->w - 8);
    return r->x + 11 + UiTextWidth(title, 1);
}

/* A labelled row: the label on the left, the value right-aligned at `right`, a hairline under it. */
static void FCRow(const char *label, const char *value, int x, int right, int y)
{
    UiText(label, x, y, EM_DARK, EM_DARK_SH, 1, right - UiTextWidth(value, 1) - 6);
    UiTextRight(value, right, y, EM_VALUE, EM_DARK_SH, 1);
    LlRule(x - 2, y + 15, right - x + 4);
}

static void FCFront(const CtrPokeTouchCard *c)
{
    char text[24], label[24], title[24];
    int stars = c->stars < 5 ? c->stars : 5, idLeft;

    snprintf(text, sizeof(text), "%s%05u", c->label[1], (unsigned)c->id);
    idLeft = sFCCard.x + sFCCard.w - 12 - UiTextWidth(text, 1);
    /* The title, its first word only when the stars and the ID would not fit after it (Spanish "FICHA ENTRENADOR"). */
    snprintf(title, sizeof(title), "%s", CtrUiText(UIS_CARD_TITLE));
    if (sFCCard.x + 12 + UiTextWidth(title, 1) + 8 + stars * 13 > idLeft - 4 && strchr(title, ' '))
        *strchr(title, ' ') = 0;
    int x = FCBand(title) + 8;

    for (int i = 0; i < stars; ++i)
        FCStar(x + i * 13, sFCCard.y + 5, UI_RGB(255, 236, 140), kEmFlame.lip);
    UiTextRight(text, sFCCard.x + sFCCard.w - 12, sFCCard.y + 1 + (FC_BAND_H - UI_TEXT_H) / 2, EM_WHITE, kEmFlame.inkSh, 1);
    /* NAME: FireRed's label ends in ": " */
    snprintf(label, sizeof(label), "%s", c->label[0]);
    for (int n = (int)strlen(label); n > 0 && (label[n - 1] == ' ' || label[n - 1] == ':'); --n)
        label[n - 1] = 0;
    FCRow(label, c->name, 16, 156, 40);
    snprintf(text, sizeof(text), "%s%u", c->label[3], (unsigned)c->money);
    FCRow(c->label[2], text, 16, 156, 61);
    if (c->hasDex) {
        snprintf(text, sizeof(text), "%u", (unsigned)c->caught);
        FCRow(c->label[4], text, 16, 156, 82);
    }
    snprintf(text, sizeof(text), "%u:%02u", (unsigned)c->hours, (unsigned)c->minutes);
    FCRow(c->label[5], text, 16, 156, c->hasDex ? 103 : 82);
    LlWell(166, 36, 72, 84);
    FCTrainer(170, 48);
    FCBadges(c, 12, 130);
}

static void FCBack(const CtrPokeTouchCard *c)
{
    FCBand(c->name);
    for (int i = 0; i < c->rows && i < PT_CARD_ROWS; ++i)
        FCRow(c->row[i].label, c->row[i].value, 16, 232, 42 + 22 * i);
}

static void FCardFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchCard *c = &v->card;
    const CtrPokeTouchRect *r = &sFCCard;

    EmSurface();
    TsBgRect(r->x - 4, r->y - 4, r->w + 8, r->h + 8);
    LlWindow(r->x, r->y, r->w, r->h);
    if (c->pressed) {   /* held: an amber edge */
        Fill(r->x + 3, r->y, r->w - 6, 1, EM_STRIPE);
        Fill(r->x + 3, r->y + r->h - 1, r->w - 6, 1, EM_STRIPE);
        Fill(r->x, r->y + 3, 1, r->h - 6, EM_STRIPE);
        Fill(r->x + r->w - 1, r->y + 3, 1, r->h - 6, EM_STRIPE);
    }
    if (c->back)
        FCBack(c);
    else
        FCFront(c);
    sUiS = &sUiNavy;
}

static void FCardNote(const CtrPokeTouchView *v, int w)
{
    (void)v;
    EmSurface();
    LlWindow(NOTE_X, NOTE_Y, w, NOTE_H);
    Em2Text(CtrUiText(UIS_CARD_TITLE), NOTE_X + 13, NOTE_Y + 8, EM_DARK, EM_DARK_SH, NOTE_X + w - 13);
    LlRule(NOTE_X + 13, NOTE_Y + 21, w - 26);
    BPWrap(CtrUiText(UIS_CARD_TURN), NOTE_X + 13, NOTE_Y + 25, w - 26, 3, EM_DIM, EM_DARK_SH);
    sUiS = &sUiNavy;
}
