/* The trade's choice of a Pokemon on the floor of HOME's layout (3ds_poketouch.h CtrPokeTouchTrade), included at the end of 3ds_poketouch.c
 * after the PC box, whose icons and text it uses. Tabs for the two parties (yours, the other player's), the six cards of the party shown
 * (icon, nickname, level, HP bar; blue, red for one with no HP left, grey for an egg), the chosen one ringed, and CANCEL. Every word of a
 * Pokemon is FireRed's. */

static CtrPokeTouchRect FTradeCard(int k)
{
    CtrPokeTouchRect r = {(int16_t)(PT_VIEW_X + 2 + (k % 2) * 120), (int16_t)(PT_VIEW_Y + 24 + (k / 2) * 39), 116, 36};

    return r;
}

static CtrPokeTouchRect FTradeTab(int side)
{
    CtrPokeTouchRect r = {(int16_t)(PT_VIEW_X + 2 + side * 120), (int16_t)PT_VIEW_Y, 116, 20};

    return r;
}

static CtrPokeTouchRect FTradeCancel(void)
{
    CtrPokeTouchRect r = {(int16_t)(PT_VIEW_X + 70), (int16_t)(PT_VIEW_Y + 141), 100, 18};

    return r;
}

/* The hit code under (x, y) (floor coordinates of the bottom screen), -1 for none: a card only where a Pokemon is. */
static int FTradeHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchTrade *t = &v->trade;
    CtrPokeTouchRect r;

    x -= MENU_SHIFT;
    y -= MENU_SHIFT;
    for (int k = 0; k < 6; ++k) {
        r = FTradeCard(k);
        if (t->mon[t->side & 1][k].species && UiIn(&r, x, y))
            return PT_CHOICE_HIT_ROW + k;
    }
    for (int side = 0; side < 2; ++side) {
        r = FTradeTab(side);
        if (UiIn(&r, x, y))
            return PT_CHOICE_HIT_ROW + PT_TRADE_TAB_YOURS + side;
    }
    r = FTradeCancel();
    return UiIn(&r, x, y) ? PT_CHOICE_HIT_ROW + PT_TRADE_CANCEL : -1;
}

static void FTradeFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchTrade *t = &v->trade;
    int side = t->side & 1;
    char partner[16], label[40];

    EmSurface();
    TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    FNText(t->partner, (int)sizeof(t->partner), partner, (int)sizeof(partner));
    for (int s = 0; s < 2; ++s) {
        CtrPokeTouchRect r = FTradeTab(s);
        const EmPal *p = s == side ? &kEmFlame : &kEmCream;
        int fy = EmButton(r.x, r.y, r.w, r.h, p, 2, v->choice.pressed == PT_CHOICE_HIT_ROW + PT_TRADE_TAB_YOURS + s);

        if (s == 0)
            snprintf(label, sizeof(label), "%s", CTR_TR("Your team", "Tu equipo", "Dein Team"));
        else
            snprintf(label, sizeof(label), CTR_TR("%s's team", "Equipo de %s", "Team von %s"), partner);
        EmLabel(label, r.x, r.w, fy, r.h - 2, p);
    }
    for (int k = 0; k < 6; ++k) {
        CtrPokeTouchRect r = FTradeCard(k);
        const CtrPokeTouchBoxMon *m = &t->mon[side][k];
        const EmPal *p;
        int fy, chosen = t->cursor == side * 6 + k;
        char level[8];

        if (!m->species) {
            UiRoundFill(r.x, r.y, r.w, r.h, 5, UI_RGB(34, 6, 6));
            continue;
        }
        p = m->egg ? &kEmGrey : t->hp[side][k] == 0 ? &kEmPartyRed : &kEmPartyBlue;
        if (chosen)
            LlSelectRing(r.x, r.y, r.w, r.h);
        fy = EmButton(r.x, r.y, r.w, r.h, p, 3, v->choice.pressed == PT_CHOICE_HIT_ROW + k);
        FXIcon(m, r.x + 18, fy + 20);
        FXText(t->name[side][k], (int)sizeof(t->name[side][k]), r.x + 38, fy + 2, EM_WHITE, p->inkSh, r.x + r.w - 4, 0);
        if (!m->egg) {
            snprintf(level, sizeof(level), "%s%u", CTR_TR("Lv", "Nv", "Lv"), (unsigned)t->level[side][k]);
            UiText(level, r.x + 38, fy + 12, EM_WHITE, p->inkSh, 1, r.x + r.w - 4);
            LlHpBar(r.x + 38, fy + 22, r.w - 44, t->hp[side][k], 100);
        }
    }
    {
        CtrPokeTouchRect r = FTradeCancel();
        int fy = EmButton(r.x, r.y, r.w, r.h, LlPick(t->cursor == 12, &kEmSlate), 2, v->choice.pressed == PT_CHOICE_HIT_ROW + PT_TRADE_CANCEL);

        if (t->cursor == 12)
            LlSelectRing(r.x, r.y, r.w, r.h);
        EmLabel(CTR_TR("CANCEL", "CANCELAR", "ABBRECHEN"), r.x, r.w, fy, r.h - 2, LlPick(t->cursor == 12, &kEmSlate));
    }
    sUiS = &sUiNavy;
}
