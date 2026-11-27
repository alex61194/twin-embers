/* The trade's choice of a Pokemon on the lower screen (included by 3ds_bottom.c). While FireRed's trade menu waits for the Pokemon to offer,
 * the lower screen shows both parties as cards (3ds_field_trade_impl.h): a tap on a card sends the cursor there and presses A (FireRed opens
 * its own menu next, SUMMARY or TRADE, which the field choice shows), the tabs only turn from one party to the other, and CANCEL asks
 * FireRed's cancel prompt. The game says what it has (CtrTrade_*, recipe 0275). */

extern int CtrTrade_Active(void);
extern int CtrTrade_Hold(void);              /* the upper screen keeps the room for the whole trade (recipe 0287) */
extern int CtrTrade_Cursor(void);
extern int CtrTrade_Count(int side);
extern void CtrTrade_Ask(int position);
extern int CtrTrade_Mon(int side, int index, uint16_t *species, uint8_t *level, uint8_t *hpPercent, uint8_t *egg, uint32_t *personality,
                        uint8_t *nickname);
extern void CtrTrade_PartnerName(uint8_t *out);

static int sTradeSide;              /* the party shown: 0 yours, 1 the partner's */
static int sTradeCursorSide = -1;   /* the side FireRed's cursor was last on: the tab follows it when it moves to the other */

static void TradeFill(CtrPokeTouchView *v)
{
    CtrPokeTouchTrade *t = &v->trade;
    int cursor = CtrTrade_Cursor(), side = cursor < 6 ? 0 : cursor < 12 ? 1 : -1;

    if (side >= 0 && side != sTradeCursorSide)
        sTradeSide = side;
    if (side >= 0)
        sTradeCursorSide = side;
    memset(t, 0, sizeof(*t));
    t->on = 1;
    t->side = (uint8_t)sTradeSide;
    t->cursor = (int8_t)cursor;
    for (int s = 0; s < 2; ++s)
        for (int i = 0; i < 6; ++i) {
            CtrPokeTouchBoxMon *m = &t->mon[s][i];
            uint8_t egg = 0;

            memset(t->name[s][i], 0xff, sizeof(t->name[s][i]));
            if (CtrTrade_Mon(s, i, &m->species, &t->level[s][i], &t->hp[s][i], &egg, &m->personality, t->name[s][i]))
                m->egg = egg;
            else
                m->species = 0;
        }
    memset(t->partner, 0xff, sizeof(t->partner));
    CtrTrade_PartnerName(t->partner);
    snprintf(v->choice.prompt, sizeof(v->choice.prompt), "%s",
             CTR_TR("Choose a Pokemon to trade.", "Elige un Pok\xC3\xA9mon para el intercambio.", "W\xC3\xA4hle ein Pok\xC3\xA9mon zum Tauschen."));
    v->choice.count = 0;
}

/* A tap's key: A for a card (after asking FireRed's cursor to go there) and for CANCEL, nothing for a tab (it only turns the page). */
static uint16_t TradeTap(int code)
{
    int k = code - PT_CHOICE_HIT_ROW;

    if (k >= 0 && k < 6) {
        if (k >= CtrTrade_Count(sTradeSide))
            return 0;
        CtrTrade_Ask(sTradeSide * 6 + k);
        return K_A;
    }
    if (k == PT_TRADE_TAB_YOURS || k == PT_TRADE_TAB_THEIRS) {
        sTradeSide = k - PT_TRADE_TAB_YOURS;
        return 0;
    }
    if (k == PT_TRADE_CANCEL) {
        CtrTrade_Ask(12);
        return K_A;
    }
    return 0;
}
