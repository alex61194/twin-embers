/* The move relearner's list as the lower screen's remote (included by 3ds_bottom.c after LearnSnapshot; drawn by 3ds_field_relearn_impl.h).
 * MODE_GBAVIEW keeps the session and the held upper screen it always had; while the list takes its keys the picture is the list drawn here, and
 * for everything else (its questions, the messages, the Summary's choice of a move to forget) the viewport as before.
 * A row is walked to with the list's own UP / DOWN (one a frame, the game plays its own sounds) and taken with its A. */

extern int CtrBottomRelearn_IsOpen(void);
extern int CtrBottomRelearn_Ready(void);
extern int CtrBottomRelearn_Count(void);
extern int CtrBottomRelearn_Cursor(void);
extern unsigned CtrBottomRelearn_Species(void);
extern void CtrBottomRelearn_Name(char *nickname, int nsize, char *level, int lsize);
extern void CtrBottomRelearn_Question(char *out, int size);
extern unsigned CtrBottomRelearn_Row(int i, int *type, char *name, int nsize, char *pp, int psize, char *power, int wsize,
                                     char *accuracy, int asize);
extern void CtrBottomRelearn_Description(unsigned move, char *out, int size);
extern void CtrBottomRelearn_TypeName(int type, char *out, int size);

static struct {
    int down, inside, code;
} sRelearnTap = {0, 0, -1};
static struct {
    int active, target, steps, release;
} sRelearnPlan;
static int sRelearnFirst;           /* the list's first row shown: it follows the cursor, as FireRed's own scrolling does */

/* The list as it is now; 0 where it is not the list (the viewport then shows FireRed's own screen). */
static int RelearnFill(CtrPokeTouchRelearn *l, int pressed)
{
    int count = CtrBottomRelearn_Count(), cur = CtrBottomRelearn_Cursor();   /* both 0 while it is not open */

    memset(l, 0, sizeof(*l));
    if (!CtrBottomRelearn_IsOpen() || count < 1) {
        sRelearnFirst = 0;
        return 0;
    }
    if (cur < sRelearnFirst)
        sRelearnFirst = cur;
    if (cur >= sRelearnFirst + PT_RL_ROWS)
        sRelearnFirst = cur - PT_RL_ROWS + 1;
    if (sRelearnFirst > count + 1 - PT_RL_ROWS)
        sRelearnFirst = count + 1 - PT_RL_ROWS;
    if (sRelearnFirst < 0)
        sRelearnFirst = 0;
    l->on = 1;
    l->count = (uint8_t)count;
    l->cursor = (int8_t)cur;
    l->first = (uint8_t)sRelearnFirst;
    l->pressed = (int16_t)pressed;
    l->species = (uint16_t)CtrBottomRelearn_Species();
    CtrBottomRelearn_Name(l->nickname, sizeof(l->nickname), l->level, sizeof(l->level));
    CtrBottomRelearn_Question(l->question, sizeof(l->question));
    for (int i = 0; i < PT_RL_ROWS; ++i) {
        int type = -1;

        if (l->first + i > count)
            break;
        l->row[i].move = (uint16_t)CtrBottomRelearn_Row(l->first + i, &type, l->row[i].name, sizeof(l->row[i].name), l->row[i].pp,
                                                        sizeof(l->row[i].pp), l->row[i].power, sizeof(l->row[i].power),
                                                        l->row[i].accuracy, sizeof(l->row[i].accuracy));
        l->row[i].type = (int8_t)type;
        CtrBottomRelearn_TypeName(type, l->row[i].typeName, sizeof(l->row[i].typeName));
        if (l->first + i == cur && l->row[i].move)
            CtrBottomRelearn_Description(l->row[i].move, l->desc, sizeof(l->desc));
    }
    return 1;
}

static int RelearnSnapshot(Snapshot *s, int mode)
{
    if (!RelearnFill(&s->touch.relearn, sRelearnTap.down && sRelearnTap.inside ? sRelearnTap.code : -1))
        return 0;
    s->touch.viewport = 0;
    s->touch.backVisible = 1;
    s->touch.backEnabled = (uint8_t)(BackEnabled(mode) && CtrBottomRelearn_Ready());
    s->touch.backPressed = (uint8_t)(sBack.inside != 0);
    return 1;
}

/* A row: the list's cursor walks to it (RunRelearnPlan), a tap on the selected row is its A; the two arrows are its UP and DOWN. */
static void ProcessRelearnTouch(const CtrInput *in)
{
    int hit = -1, ready = sHaveShown && sShown.mode == MODE_GBAVIEW && sShown.touch.relearn.on && CtrBottomRelearn_Ready()
                          && !CtrBottom_IsFading();

    if (in->touchActive && ready)
        hit = CtrPokeTouch_RelearnHit(&sShown.touch, (int)in->touchX, (int)in->touchY);
    if ((in->held & 0x3ffu) || !ready) {
        sRelearnTap.down = sRelearnTap.inside = 0;
        return;
    }
    if (in->touchDown) {
        sRelearnTap.down = sRelearnTap.inside = hit >= 0;
        sRelearnTap.code = hit;
        return;
    }
    if (!sRelearnTap.down)
        return;
    if (in->touchActive) {
        sRelearnTap.inside = hit == sRelearnTap.code;
        return;
    }
    if (sRelearnTap.inside && !sRelearnPlan.active) {
        int row = sShown.touch.relearn.first + sRelearnTap.code - PT_RL_HIT_ROW;

        if (sRelearnTap.code == PT_RL_HIT_UP)
            sRouteKey = K_UP;
        else if (sRelearnTap.code == PT_RL_HIT_DOWN)
            sRouteKey = K_DOWN;
        else if (row == CtrBottomRelearn_Cursor())
            sRouteKey = K_A;
        else {
            memset(&sRelearnPlan, 0, sizeof(sRelearnPlan));
            sRelearnPlan.active = 1;
            sRelearnPlan.target = row;
        }
    }
    sRelearnTap.down = sRelearnTap.inside = 0;
}

/* The walk: one UP or DOWN, then a frame without, until the cursor is on the row. A held physical button cancels it. */
static void RunRelearnPlan(int on)
{
    int cur;

    if (!on || (CtrInput_Get()->held & 0x3ffu) || !CtrBottomRelearn_IsOpen() || ++sRelearnPlan.steps > 60) {
        memset(&sRelearnPlan, 0, sizeof(sRelearnPlan));
        return;
    }
    if (!sRelearnPlan.active)
        sRelearnPlan.steps = 0;
    if (!sRelearnPlan.active)
        return;
    if (sRelearnPlan.release) {
        sRelearnPlan.release = 0;
        return;
    }
    if (!CtrBottomRelearn_Ready())
        return;
    cur = CtrBottomRelearn_Cursor();
    if (cur == sRelearnPlan.target) {
        memset(&sRelearnPlan, 0, sizeof(sRelearnPlan));
        return;
    }
    sInjected = sRelearnPlan.target > cur ? K_DOWN : K_UP;
    sRelearnPlan.release = 1;
}
