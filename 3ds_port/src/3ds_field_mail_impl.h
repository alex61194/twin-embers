/* The letter being read, in the viewport's place (3ds_poketouch.h CtrPokeTouchMail), included at the end of 3ds_poketouch.c. The upper screen keeps
 * drawing the letter on its own stationery, exactly as the game does; here, in FireRed's text window (the viewport's rectangle, so a touch of it
 * closes the letter as BACK does), a flame ribbon with "Mail", the paper's icon on a plate, the paper's name, and who wrote it in blue. The hint
 * is the note's text. The names are FireRed's (cased like the Bag's); the writer's stays as typed. */

#define MAIL_CX (PT_VIEW_X + PT_VIEW_W / 2)

static void FMailFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchMail *m = &v->mail;
    const char *label = CTR_TR("From:", "De:", "Von:");
    int x0 = PT_VIEW_X + 8, x1 = PT_VIEW_X + PT_VIEW_W - 8, nw = UiTextWidth(m->name, 1), lw = UiTextWidth(label, 1) + 5;
    int fw = UiTextWidth(m->from, 1), tw;

    EmSurface();
    LlWindow(PT_VIEW_X, PT_VIEW_Y, PT_VIEW_W, PT_VIEW_H);
    LlRibbon(PT_VIEW_X + 8, PT_VIEW_Y + 8, CTR_TR("Mail", "Correo", "Brief"), LL_RIB_TOP, LL_RIB_BOT);
    LlPlate(MAIL_CX - 22, PT_VIEW_Y + 36, 44, LL_SLATE, 0, 0);
    ItemArt(ICON_MAIL, m->item, MAIL_CX - 10, PT_VIEW_Y + 48, 1);
    if (nw > x1 - x0)
        nw = x1 - x0;
    Em2Text(m->name, MAIL_CX - nw / 2, PT_VIEW_Y + 94, EM_DARK, EM_DARK_SH, x1);
    LlRule(MAIL_CX - 80, PT_VIEW_Y + 112, 160);
    if (m->from[0]) {
        tw = lw + fw;
        if (tw > x1 - x0)
            tw = x1 - x0;
        Em2Text(label, MAIL_CX - tw / 2, PT_VIEW_Y + 122, EM_DIM, EM_DARK_SH, x1);
        Em2Text(m->from, MAIL_CX - tw / 2 + lw, PT_VIEW_Y + 122, EM2_COUNT_INK, UI_RGB(192, 208, 240), x1);
    }
    sUiS = &sUiNavy;
}

static void FMailNote(const CtrPokeTouchView *v, int w)
{
    (void)v;
    EmSurface();
    LlWindow(NOTE_X, NOTE_Y, w, NOTE_H);
    Em2Text(CTR_TR("Tap to close", "Toca para cerrar", "Tippen zum Schliessen"), NOTE_X + 13, NOTE_Y + (NOTE_H - UI_TEXT_H) / 2, EM_DARK,
            EM_DARK_SH, NOTE_X + w - 13);
    sUiS = &sUiNavy;
}
