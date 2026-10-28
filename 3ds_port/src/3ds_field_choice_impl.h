/* A choice of the field in the viewport (3ds_poketouch.h CtrPokeTouchChoice), included at the end of 3ds_poketouch.c
 * after the Bag, whose cards and buttons it shares. A remote for FireRed's menu (a script's multichoice, YES/NO or
 * grid, the PC's menus): its rows as ember buttons, one column (two past five rows, or the grid's own columns) centred
 * on the embers, the cursor's amber; the question of the message box in the note. Every word is FireRed's
 * (CtrFieldChoice_*). */

static int FChColumns(const CtrPokeTouchChoice *c)
{
    return c->cols > 0 ? c->cols : c->count > 5 ? 2 : 1;
}

static CtrPokeTouchRect FChRow(const CtrPokeTouchChoice *c, int i)
{
    int cols = FChColumns(c), rows = (c->count + cols - 1) / cols, gap = 4;
    int h = rows ? (PT_VIEW_H - 16 - (rows - 1) * gap) / rows : 0;
    int w = cols == 1 ? 184 : (PT_VIEW_W - 16 - (cols - 1) * gap) / cols;
    int x0, y0;

    if (h > 30)
        h = 30;
    x0 = PT_VIEW_X + (PT_VIEW_W - (cols * w + (cols - 1) * gap)) / 2;
    y0 = PT_VIEW_Y + (PT_VIEW_H - (rows * h + (rows - 1) * gap)) / 2;
    CtrPokeTouchRect r = {(int16_t)(x0 + (i % cols) * (w + gap)), (int16_t)(y0 + (i / cols) * (h + gap)),
                          (int16_t)w, (int16_t)h};
    return r;
}

int CtrPokeTouch_ChoiceHit(const CtrPokeTouchView *v, int x, int y)
{
    const CtrPokeTouchChoice *c = &v->choice;

    if (!c->on)
        return -1;
    for (int i = 0; i < c->count && i < PT_CHOICE_MAX; ++i) {
        CtrPokeTouchRect r = FChRow(c, i);
        if (UiIn(&r, x, y))
            return PT_CHOICE_HIT_ROW + i;
    }
    return -1;
}

static void FChoiceFrame(const CtrPokeTouchView *v)
{
    const CtrPokeTouchChoice *c = &v->choice;

    EmSurface();
    TsBgRect(PT_VIEW_X - 4, PT_VIEW_Y - 4, PT_VIEW_W + 8, PT_VIEW_H + 8);
    for (int i = 0; i < c->count && i < PT_CHOICE_MAX; ++i) {
        CtrPokeTouchRect r = FChRow(c, i);

        FBButton(&r, c->cursor == i ? &kEmAmber : &kEmCream, c->label[i], c->pressed == PT_CHOICE_HIT_ROW + i);
    }
    sUiS = &sUiNavy;
}

static void FChoiceNote(const CtrPokeTouchView *v, int w)
{
    EmSurface();
    EmNote(NOTE_X, NOTE_Y, w, NOTE_H);
    if (v->choice.prompt[0])
        BDescribeN(v->choice.prompt, NOTE_X + 12, NOTE_Y + 8, w - 18, 3, EM_DARK);
    sUiS = &sUiNavy;
}
