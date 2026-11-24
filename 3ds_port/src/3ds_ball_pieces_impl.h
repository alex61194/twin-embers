/* The pieces the Poke Ball style shares between the screens that keep their own layout (Easy Chat, the PC box, the trade's cards, the chat's keyboard,
 * the letter), included at the end of 3ds_poketouch.c after 3ds_ball_naming_impl.h and before those screens: pills, a rail and the Bx* stand-ins that
 * draw an Ascuas piece (EmButton, EmLabel, the select ring, a plain window) as its Poke Ball one while that style is on, and as the Ascuas one otherwise.
 * BallShiftedScreen puts a screen drawn for a 240 x 160 viewport at (4, 4) in the middle of the whole lower screen: the red ground down to the band, the
 * screen's own pieces, and the note on the white under the band; its touches are moved back by the same shift (BxShiftX, BxShiftY). */

#define BX_SHIFT_X 36                                   /* the viewport's left edge (4) moves to 40: the 240 px are centred */
#define BX_NOTE_UP 11                                   /* the PC box's note (drawn at y 203 for HOME's layout) moves up to 192, under the band */

/* A pill: `face` and `ink` as given, a dark-red edge (a bright one when `bright`: the chosen piece); a held touch is a shade off. */
static void FEBallPill(const CtrPokeTouchRect *r, const char *label, uint16_t face, uint16_t ink, int bright, int pressed)
{
    char fit[48];
    int tw;

    if (pressed)
        face = bright ? BM_PRESSED : BM_CARD_EDGE;
    UiRoundFill(r->x, r->y, r->w, r->h, r->h < 20 ? 6 : 8, bright ? BM_CARD_EDGE : BM_PILL_EDGE);
    UiRoundFill(r->x + 2, r->y + 2, r->w - 4, r->h - 4, r->h < 20 ? 4 : 6, face);
    Em2Fit(label, r->w - 12, fit, sizeof(fit));
    tw = UiTextWidth(fit, 1);
    UiText(fit, r->x + (r->w - tw) / 2, r->y + (r->h - UI_TEXT_H) / 2, pressed && !bright ? EM_WHITE : ink, face, 1, r->x + r->w - 4);
}

/* What an Ascuas palette stands for in the Poke Ball style: the chosen (flame) piece white with a red word, an action (gold) pink, CANCEL and BACK (slate)
 * near black, the rest dark red; the trade's healthy cards are white, a fainted one pink, an egg grey. */
static uint16_t BxFaceColour(const EmPal *p)
{
    return p == &kEmFlame ? BL_WHITE : p == &kEmSlate ? BL_BAND : p == &kEmGold ? BL_PINK : p == &kEmBlue ? BM_STRIP : p == &kEmGrey ? UI_RGB(204, 198, 198)
         : p == &kEmPartyBlue ? BM_CARD_FACE : p == &kEmPartyRed ? BL_PINK : BM_PILL;
}

static int BxBright(const EmPal *p)
{
    return p == &kEmFlame || p == &kEmGold || p == &kEmPartyBlue || p == &kEmPartyRed;
}

static uint16_t BxInkColour(const EmPal *p)
{
    return p == &kEmFlame || p == &kEmGold || p == &kEmPartyRed ? BL_REDTXT : p == &kEmGrey ? BM_DIM : p == &kEmPartyBlue ? BM_INK : EM_WHITE;
}

static void FEBallButton(const CtrPokeTouchRect *r, const EmPal *p, const char *label, int pressed, int ring)
{
    int chosen = p == &kEmFlame || ring;

    FEBallPill(r, label, chosen ? BL_WHITE : BxFaceColour(p), chosen ? BL_REDTXT : BxInkColour(p), chosen || BxBright(p), pressed);
}

static void FEBallRail(int x, int y, int w, int h, int top, int thumb)
{
    UiRoundFill(x + w / 2 - 4, y, 8, h, 4, BM_PILL_EDGE);
    UiRoundFill(x + w / 2 - 2, y + 2, 4, h - 4, 2, BM_PILL);
    if (thumb > 0 && thumb < h) {
        UiRoundFill(x + 1, y + top, w - 2, thumb, 6, BM_CARD_EDGE);
        UiRoundFill(x + 3, y + top + 2, w - 6, thumb - 4, 4, BL_WHITE);
    }
}

/* EmButton: a pill whose face top is where its label's box starts (the Ascuas button's depth is the bevel under it, so the face is `depth` shorter). */
static int BxFace(int x, int y, int w, int h, const EmPal *p, int depth, int pressed)
{
    uint16_t face;
    int bright, radius = h < 20 ? 6 : 8;

    if (!sStyleBall)
        return EmButton(x, y, w, h, p, depth, pressed);
    face = BxFaceColour(p);
    bright = BxBright(p);
    if (pressed)
        face = bright ? BM_PRESSED : BM_CARD_EDGE;
    UiRoundFill(x, y, w, h, radius, bright ? BM_CARD_EDGE : BM_PILL_EDGE);
    UiRoundFill(x + 2, y + 2, w - 4, h - 4, radius - 2, face);
    return y + depth / 2;
}

static void BxLabel(const char *label, int x, int w, int fy, int h, const EmPal *p)
{
    char fit[48];

    if (!sStyleBall) {
        EmLabel(label, x, w, fy, h, p);
        return;
    }
    Em2Fit(label, w - 8, fit, sizeof(fit));
    UiText(fit, x + (w - UiTextWidth(fit, 1)) / 2, fy + (h - UI_TEXT_H) / 2, BxInkColour(p), BxFaceColour(p), 1, x + w - 3);
}

static uint16_t BxInk(const EmPal *p)
{
    return sStyleBall ? BxInkColour(p) : p->ink;
}

static uint16_t BxInkSh(const EmPal *p)
{
    return sStyleBall ? BxFaceColour(p) : p->inkSh;
}

static uint16_t BxInkW(const EmPal *p)                  /* the word on a card that is white in the Ascuas style */
{
    return sStyleBall ? BxInkColour(p) : EM_WHITE;
}

/* The select ring: two lines round the piece, white outside and dark red inside, so that it shows on the red ground and on a white card. */
static void BxRing(int x, int y, int w, int h)
{
    if (!sStyleBall) {
        LlSelectRing(x, y, w, h);
        return;
    }
    Fill(x - 1, y - 3, w + 2, 1, BL_WHITE);
    Fill(x - 1, y + h + 2, w + 2, 1, BL_WHITE);
    Fill(x - 3, y - 1, 1, h + 2, BL_WHITE);
    Fill(x + w + 2, y - 1, 1, h + 2, BL_WHITE);
    Fill(x, y - 2, w, 1, BM_CARD_EDGE);
    Fill(x, y + h + 1, w, 1, BM_CARD_EDGE);
    Fill(x - 2, y, 1, h, BM_CARD_EDGE);
    Fill(x + w + 1, y, 1, h, BM_CARD_EDGE);
}

static void BxWindow(int x, int y, int w, int h)
{
    if (sStyleBall)
        BmCard(x, y, w, h);
    else
        LlWindowPlain(x, y, w, h);
}

static void BxLlButton(const CtrPokeTouchRect *r, const EmPal *p, const char *label, int pressed, int ring)
{
    if (sStyleBall)
        FEBallButton(r, p, label, pressed, ring);
    else
        FBLlButton(r, p, label, pressed, ring);
}

static int BxShiftX(void) { return sStyleBall ? BX_SHIFT_X : MENU_SHIFT; }
static int BxShiftY(void) { return sStyleBall ? 0 : MENU_SHIFT; }

/* ---- the screens that keep their layout, on the whole lower screen ---- */

static void BallShiftedScreen(const CtrPokeTouchView *v)
{
    uint16_t *base = sCanvas;

    sBallExitR = (CtrPokeTouchRect){0, 0, 0, 0};        /* these have their own CLOSE BOX, CANCEL or QUIT: no exit piece */
    EmSurface();
    TsBallGround(BO_BAND_Y);
    sCanvas = base + (size_t)BX_SHIFT_X * H;
    if (v->box.on)
        FBoxFrame(v);
    else if (v->trade.on)
        FTradeFrame(v);
    else
        FChatFrame(v);
    sCanvas = base;
    if (v->box.on) {
        sCanvas = base + BX_NOTE_UP;
        FBoxNote(v, 0);
        sCanvas = base;
    } else
        MenuWrap(v->choice.prompt, 14, BO_BAND_Y + 14, 292, 3, 14, BL_BAND, BL_WHITE);
    sUiS = &sUiNavy;
}

/* ---- the letter ---- */

static void BallMailScreen(const CtrPokeTouchView *v)
{
    const CtrPokeTouchMail *m = &v->mail;
    const char *label = CTR_TR("From:", "De:", "Von:");
    int nw = UiTextWidth(m->name, 1), lw = UiTextWidth(label, 1) + 5, fw = UiTextWidth(m->from, 1), tw, cx = 8 + 232 / 2;

    BmFrame(NULL, CTR_TR("Tap to close", "Toca para cerrar", "Tippen zum Schliessen"), "", CTR_TR("BACK", "ATR\xC3\x81S", "ZUR\xC3\x9C" "CK"), v->backPressed,
            v->backEnabled);
    BmCard(8, 8, 232, 148);                             /* inside the viewport's rectangle (4, 4, 240, 160): a touch of it closes the letter */
    BmStrip(10, 10, 228, CTR_TR("MAIL", "CORREO", "BRIEF"));
    TsDisc(cx, 66, 26, UI_RGB(244, 238, 238));
    ItemArt(ICON_MAIL, m->item, cx - 10, 54, 1);
    if (nw > 216)
        nw = 216;
    UiText(m->name, cx - nw / 2, 98, BM_INK, BM_CARD_FACE, 1, 232);
    Fill(cx - 80, 116, 160, 1, BM_RULE);
    if (m->from[0]) {
        tw = lw + fw;
        if (tw > 216)
            tw = 216;
        UiText(label, cx - tw / 2, 126, BM_DIM, BM_CARD_FACE, 1, 232);
        UiText(m->from, cx - tw / 2 + lw, 126, BL_REDTXT, BM_CARD_FACE, 1, 232);
    }
    sUiS = &sUiNavy;
}
