/* Easy Chat as the lower screen's remote (included by 3ds_bottom.c after the relearner's; drawn by 3ds_field_easychat_impl.h).
 * MODE_GBAVIEW keeps the session and the held upper screen it always had; once the game's screen waits for keys (recipe 0255) the lower screen
 * draws it itself, and until then (its setup, its fade out) the viewport as before. The game decides everything: a touch is walked to with the
 * game's own keys, one a frame (a frame without between them), the state read again every frame, so it never goes out of step: a physical
 * key cancels the walk, and it gives up after PLAN_LIMIT frames or when the cursor stops moving. */

extern int CtrBottomEasyChat_IsOpen(void);
extern int CtrBottomEasyChat_Ready(void);
extern int CtrBottomEasyChat_Get(int what);
extern void CtrBottomEasyChat_Phrase(int slot, char *out, int size);
extern void CtrBottomEasyChat_GroupName(int index, char *out, int size);
extern void CtrBottomEasyChat_ListWord(int index, char *out, int size);
extern void CtrBottomEasyChat_Title(char *out, int size);
extern void CtrBottomEasyChat_Question(int which, int line, char *out, int size);

/* The numbers of CtrBottomEasyChat_Get (the recipe lists them). */
enum { EC_TYPE, EC_COLS, EC_ROWS, EC_STATE, EC_MAIN_COL, EC_MAIN_ROW, EC_WORDS, EC_ALPHA, EC_GROUP_X, EC_GROUP_Y, EC_GROUP_ABOVE,
       EC_GROUP_ROWS, EC_WORD_ABOVE, EC_WORD_ROWS, EC_WORD_X, EC_WORD_Y, EC_GROUPS, EC_LISTED, EC_BACKUP, EC_ASK_CURSOR };
enum { EC_ST_FIELD = 0, EC_ST_FOOTER, EC_ST_GROUP, EC_ST_WORD, EC_ST_QUIT, EC_ST_DELETE, EC_ST_CONFIRM };
enum { EP_NONE = 0, EP_CHIP, EP_DELETE, EP_GROUP, EP_LETTER, EP_WORD, EP_FIRST, EP_ASK, EP_TOGGLE };
#define EC_PLAN_LIMIT 240
#define EC_STALL_LIMIT 8

static struct {
    int down, inside, code, scrub, target;
} sEcTap = {0, 0, -1, 0, -1};
static struct {
    int kind, target, then, steps, stage, stall, last;
} sEcPlan;
static int sEcRelease;              /* the frame without a key between two */
static int sEcPref = -1;            /* the editor's mode buttons: the order the next word is picked in (-1: the game's) */
static int sEcPrev = -1;            /* the game's state a frame ago */
static int sEcEntry;                /* the group screen was just entered from the phrase */

static void EasyReset(void)
{
    memset(&sEcTap, 0, sizeof(sEcTap));
    sEcTap.code = -1;
    memset(&sEcPlan, 0, sizeof(sEcPlan));
    sEcRelease = sEcEntry = 0;
    sEcPref = sEcPrev = -1;
}

/* The letter 0..25 a word starts with (ASCII, either case), or 255: the game's own accented letters sort with no letter of the strip. */
static int EasyLetterOf(const char *word)
{
    char c = word[0];

    if (c >= 'a' && c <= 'z')
        c = (char)(c - 'a' + 'A');
    return c >= 'A' && c <= 'Z' ? c - 'A' : 255;
}

/* A letter of the A-Z grid as the game's cursor lies on it (rows of 6, 6, 7, 7), and back. */
static int EasyLetterAt(int x, int y)
{
    static const uint8_t kStart[4] = {0, 6, 12, 19};

    return y >= 0 && y < 4 && x >= 0 ? kStart[y] + x : -1;
}

static void EasyLetterPos(int letter, int *x, int *y)
{
    static const uint8_t kStart[4] = {0, 6, 12, 19};
    int row = letter < 6 ? 0 : letter < 12 ? 1 : letter < 19 ? 2 : 3;

    *y = row;
    *x = letter - kStart[row];
}

/* The screen as the game has it now; 0 where it is not open for keys (the viewport then shows FireRed's own picture). */
static int EasyFill(CtrPokeTouchEasy *e, int pressed)
{
    int state, base, cols, n, gx, gy, above;

    memset(e, 0, sizeof(*e));
    if (!CtrBottomEasyChat_Ready()) {
        if (!CtrBottomEasyChat_IsOpen())
            sEcPref = sEcPrev = -1;
        return 0;
    }
    state = CtrBottomEasyChat_Get(EC_STATE);
    base = state >= EC_ST_QUIT ? CtrBottomEasyChat_Get(EC_BACKUP) : state;
    cols = CtrBottomEasyChat_Get(EC_COLS);
    n = CtrBottomEasyChat_Get(EC_WORDS);
    n = n < 1 ? 1 : n > PT_EC_WORDS ? PT_EC_WORDS : n;
    e->on = 1;
    e->pressed = (int16_t)pressed;
    e->alpha = CtrBottomEasyChat_Get(EC_ALPHA) != 0;
    e->wantAlpha = (uint8_t)(sEcPref >= 0 ? sEcPref : e->alpha);
    e->numWords = (uint8_t)n;
    e->cursor = e->footer = e->cell = -1;
    e->letter = 255;
    e->screen = state == EC_ST_GROUP ? (e->alpha ? PT_EC_LETTER : PT_EC_GROUP) : state == EC_ST_WORD ? PT_EC_LIST : PT_EC_EDIT;
    if (state >= EC_ST_QUIT) {
        e->ask = (uint8_t)(state == EC_ST_QUIT ? PT_EC_ASK_QUIT : state == EC_ST_DELETE ? PT_EC_ASK_DELETE : PT_EC_ASK_CONFIRM);
        e->askCursor = CtrBottomEasyChat_Get(EC_ASK_CURSOR) == 1;
        CtrBottomEasyChat_Question(state - EC_ST_QUIT, 0, e->ask1, sizeof(e->ask1));
        CtrBottomEasyChat_Question(state - EC_ST_QUIT, 1, e->ask2, sizeof(e->ask2));
    }
    CtrBottomEasyChat_Title(e->title, sizeof(e->title));
    for (int i = 0; i < n; ++i)
        CtrBottomEasyChat_Phrase(i, e->chip[i], sizeof(e->chip[i]));
    if (e->screen == PT_EC_EDIT) {
        int row = CtrBottomEasyChat_Get(EC_MAIN_ROW), col = CtrBottomEasyChat_Get(EC_MAIN_COL);

        if (base == EC_ST_FIELD && cols > 0 && row * cols + col < n)
            e->cursor = (int8_t)(row * cols + col);
        else if (base == EC_ST_FOOTER)
            e->footer = (int8_t)col;
    }
    gx = CtrBottomEasyChat_Get(EC_GROUP_X);
    gy = CtrBottomEasyChat_Get(EC_GROUP_Y);
    above = CtrBottomEasyChat_Get(EC_GROUP_ABOVE);
    if (e->screen == PT_EC_GROUP || e->screen == PT_EC_LETTER) {
        int letters = e->screen == PT_EC_LETTER;

        e->groupCount = (uint8_t)CtrBottomEasyChat_Get(EC_GROUPS);
        for (int i = 0; i < PT_EC_GROUPS; ++i)
            CtrBottomEasyChat_GroupName(i, e->group[i], sizeof(e->group[i]));
        if (gx >= 0)
            e->cell = (int8_t)(letters ? EasyLetterAt(gx, gy) : 2 * (gy + above) + gx);
        else                                /* the game's blue box: A-Z, DELETE (the phrase's word: not drawn), BACK */
            e->cell = (int8_t)(gy == 0 ? PT_EC_CELL_TOGGLE : gy == 2 ? PT_EC_CELL_BACK : -1);
    } else if (e->screen == PT_EC_LIST) {
        int count = CtrBottomEasyChat_Get(EC_LISTED), rows = (count + 1) / 2, last = rows > PT_EC_ROWS ? rows - PT_EC_ROWS : 0;
        int wa = CtrBottomEasyChat_Get(EC_WORD_ABOVE);

        e->listCount = (uint16_t)count;
        e->first = (uint16_t)(wa > last ? last : wa);
        e->word = (int16_t)(2 * (CtrBottomEasyChat_Get(EC_WORD_Y) + wa) + CtrBottomEasyChat_Get(EC_WORD_X));
        for (int k = 0; k < 2 * PT_EC_ROWS; ++k)
            CtrBottomEasyChat_ListWord(2 * e->first + k, e->list[k], sizeof(e->list[k]));
        e->letter = (uint8_t)EasyLetterOf(e->list[0]);
        if (e->alpha) {
            e->listTitle[0] = (char)('A' + EasyLetterAt(gx < 0 ? 0 : gx, gy) % 26);
            e->listTitle[1] = 0;
        } else
            CtrBottomEasyChat_GroupName(2 * (gy + above) + gx, e->listTitle, sizeof(e->listTitle));
    }
    return 1;
}

static void EasyCase(CtrPokeTouchEasy *e)
{
    CtrTextCase(e->title);
    CtrTextCase(e->listTitle);
    CtrTextCase(e->ask1);
    CtrTextCase(e->ask2);
    for (int i = 0; i < PT_EC_WORDS; ++i)
        CtrTextCase(e->chip[i]);
    for (int i = 0; i < PT_EC_GROUPS; ++i)
        CtrTextCase(e->group[i]);
    for (int k = 0; k < 2 * PT_EC_ROWS; ++k)
        CtrTextCase(e->list[k]);
}

static int EasySnapshot(Snapshot *s)
{
    if (!EasyFill(&s->touch.ec, sEcTap.down && sEcTap.inside ? sEcTap.code : -1))
        return 0;
    EasyCase(&s->touch.ec);
    s->touch.viewport = 0;
    s->touch.backVisible = 0;
    return 1;
}

/* ---- the walks ---- */

static void EasyPlanStart(int kind, int target, int then)
{
    memset(&sEcPlan, 0, sizeof(sEcPlan));
    sEcPlan.kind = kind;
    sEcPlan.target = target;
    sEcPlan.then = then;
    sEcPlan.last = -1;
}

/* One frame of the walk: the key to send, or 0 (waiting, or it has arrived: the plan ends). */
static uint16_t EasyPlanKey(void)
{
    int state = CtrBottomEasyChat_Get(EC_STATE), t = sEcPlan.target, then = sEcPlan.then ? K_A : 0;
    int x, y, r, cols, here;

    switch (sEcPlan.kind) {
    case EP_CHIP:
        if (state != EC_ST_FIELD && state != EC_ST_FOOTER)
            break;
        cols = CtrBottomEasyChat_Get(EC_COLS);
        if (cols < 1)
            break;
        y = CtrBottomEasyChat_Get(EC_MAIN_ROW);
        x = CtrBottomEasyChat_Get(EC_MAIN_COL);
        r = t / cols;
        if (state == EC_ST_FIELD && y == r && x == t % cols) {
            sEcPlan.kind = EP_NONE;
            return (uint16_t)then;
        }
        return state == EC_ST_FOOTER || y > r ? K_UP : y < r ? K_DOWN : x < t % cols ? K_RIGHT : K_LEFT;
    case EP_DELETE:
        if (state != EC_ST_FIELD && state != EC_ST_FOOTER)
            break;
        y = CtrBottomEasyChat_Get(EC_MAIN_ROW);
        x = CtrBottomEasyChat_Get(EC_MAIN_COL);
        if (state == EC_ST_FIELD)           /* off the phrase, by the shorter way (UP from the first row wraps to the buttons) */
            return CtrBottomEasyChat_Get(EC_ROWS) - y <= y + 1 ? K_DOWN : K_UP;
        if (x == 0) {
            sEcPlan.kind = EP_NONE;
            return K_A;
        }
        return K_LEFT;
    case EP_GROUP:
    case EP_LETTER:
        if (state != EC_ST_GROUP)
            break;
        x = CtrBottomEasyChat_Get(EC_GROUP_X);
        y = CtrBottomEasyChat_Get(EC_GROUP_Y);
        if (x < 0)                          /* in the blue box: RIGHT leaves it */
            return K_RIGHT;
        if (sEcPlan.kind == EP_GROUP) {
            r = t / 2;
            y += CtrBottomEasyChat_Get(EC_GROUP_ABOVE);
            here = t % 2;
        } else {
            EasyLetterPos(t, &here, &r);
        }
        if (y == r && x == here) {
            sEcPlan.kind = EP_NONE;
            return (uint16_t)then;
        }
        return y < r ? K_DOWN : y > r ? K_UP : x < here ? K_RIGHT : K_LEFT;
    case EP_WORD:
        if (state != EC_ST_WORD)
            break;
        x = CtrBottomEasyChat_Get(EC_WORD_X);
        y = CtrBottomEasyChat_Get(EC_WORD_Y) + CtrBottomEasyChat_Get(EC_WORD_ABOVE);
        if (y == t / 2 && x == t % 2) {
            sEcPlan.kind = EP_NONE;
            return (uint16_t)then;
        }
        return y < t / 2 ? K_DOWN : y > t / 2 ? K_UP : x < t % 2 ? K_RIGHT : K_LEFT;
    case EP_FIRST: {                        /* scroll to the row: four at a time with START / SELECT, then one a key */
        int above, rows, last, first;

        if (state != EC_ST_WORD)
            break;
        above = CtrBottomEasyChat_Get(EC_WORD_ABOVE);
        rows = CtrBottomEasyChat_Get(EC_WORD_ROWS) + 1;
        last = rows > PT_EC_ROWS ? rows - PT_EC_ROWS : 0;
        first = above > last ? last : above;
        t = t > last ? last : t < 0 ? 0 : t;
        here = above * 64 + CtrBottomEasyChat_Get(EC_WORD_Y) * 8 + CtrBottomEasyChat_Get(EC_WORD_X);
        if (first == t) {
            sEcPlan.kind = EP_NONE;
            return 0;
        }
        sEcPlan.stall = here == sEcPlan.last ? sEcPlan.stall + 1 : 0;       /* a key that moved nothing: the end of the list */
        sEcPlan.last = here;
        if (sEcPlan.stall >= EC_STALL_LIMIT) {
            sEcPlan.kind = EP_NONE;
            return 0;
        }
        return first < t ? (t - above >= 4 ? K_SELECT : K_DOWN) : (above - t >= 4 ? K_START : K_UP);
    }
    case EP_ASK:
        if (state < EC_ST_QUIT)
            break;
        here = CtrBottomEasyChat_Get(EC_ASK_CURSOR);
        if (here == t) {
            sEcPlan.kind = EP_NONE;
            return K_A;
        }
        return t ? K_DOWN : K_UP;
    case EP_TOGGLE:                         /* from a list: BACK to its menu, then the order's key */
        if (sEcPlan.stage == 0 && state == EC_ST_WORD) {
            sEcPlan.stage = 1;
            return K_B;
        }
        if (sEcPlan.stage == 1 && state == EC_ST_GROUP) {
            sEcPlan.kind = EP_NONE;
            return K_SELECT;
        }
        if (sEcPlan.stage == 1 && state != EC_ST_WORD)
            break;
        return 0;
    }
    sEcPlan.kind = EP_NONE;
    return 0;
}

/* Once a frame (3ds_bottom.c): the pending walk, or the editor's mode buttons' choice on entering the group screen. */
static void RunEasyPlan(int on)
{
    int state;
    uint16_t key;

    if (!on || !CtrBottomEasyChat_IsOpen() || (CtrInput_Get()->held & 0x3ffu) || ++sEcPlan.steps > EC_PLAN_LIMIT) {
        memset(&sEcPlan, 0, sizeof(sEcPlan));
        sEcRelease = sEcEntry = 0;
        if (!on || !CtrBottomEasyChat_IsOpen())
            sEcPref = sEcPrev = -1;
        return;
    }
    if (sEcPlan.kind == EP_NONE)
        sEcPlan.steps = 0;
    state = CtrBottomEasyChat_Get(EC_STATE);
    if (state == EC_ST_GROUP && sEcPrev != EC_ST_GROUP && sEcPrev != EC_ST_WORD)
        sEcEntry = 1;
    sEcPrev = state;
    if (sEcRelease) {
        sEcRelease = 0;
        return;
    }
    if (!CtrBottomEasyChat_Ready())
        return;
    if (sEcEntry && state == EC_ST_GROUP) {
        sEcEntry = 0;
        if (sEcPref >= 0 && sEcPref != (CtrBottomEasyChat_Get(EC_ALPHA) != 0)) {
            sEcPref = -1;
            sInjected = K_SELECT;
            sEcRelease = 1;
            return;
        }
        sEcPref = -1;
    }
    if (sEcPlan.kind == EP_NONE)
        return;
    key = EasyPlanKey();
    if (key) {
        sInjected = key;
        sEcRelease = 1;
    }
}

/* The touch: a tap on a chip, group, letter, word or button is walked to and taken (a tap on what the game's cursor is on is its A); the rail
 * and the strip are sliders, a tap jumping to where it is pressed and a drag following the finger (the scroll walked to every frame). */
static void ProcessEasyTouch(const CtrInput *in)
{
    const CtrPokeTouchEasy *e = &sShown.touch.ec;
    int ready = sHaveShown && sShown.mode == MODE_GBAVIEW && e->on && CtrBottomEasyChat_Ready() && !CtrBottom_IsFading();
    int hit = -1, code;

    if (in->touchActive && ready)
        hit = CtrPokeTouch_EasyHit(&sShown.touch, (int)in->touchX, (int)in->touchY);
    if ((in->held & 0x3ffu) || !ready) {
        sEcTap.down = sEcTap.inside = sEcTap.scrub = 0;
        return;
    }
    if (in->touchDown) {
        sEcTap.down = sEcTap.inside = hit >= 0;
        sEcTap.code = hit;
        sEcTap.scrub = hit == PT_EC_HIT_RAIL || hit == PT_EC_HIT_STRIP;
        sEcTap.target = -1;
    } else if (!sEcTap.down)
        return;
    if (in->touchActive) {
        if (!sEcTap.scrub) {
            if (!in->touchDown)
                sEcTap.inside = hit == sEcTap.code;
            return;
        }
        /* A slider: the scroll it stands for, wherever the finger is along its length. */
        int target = sEcTap.code == PT_EC_HIT_RAIL ? CtrPokeTouch_EasyRailTarget(e, (int)in->touchY) : -1;

        if (sEcTap.code == PT_EC_HIT_STRIP) {
            int letter = CtrPokeTouch_EasyStripLetter(in->touchX < 284 ? 284 : (int)in->touchX, in->touchY < 28 ? 28 : (int)in->touchY);

            if (letter >= 0) {              /* the first word that starts with it, or past it (the list is in order): its row */
                int i = 0;
                char w[PT_EC_WORD_LEN];

                while (i < (int)e->listCount) {
                    CtrBottomEasyChat_ListWord(i, w, sizeof(w));
                    if (EasyLetterOf(w) == 255 || EasyLetterOf(w) >= letter)
                        break;
                    i += 1;
                }
                target = i / 2;
            }
        }
        if (target >= 0 && target != sEcTap.target) {
            sEcTap.target = target;
            EasyPlanStart(EP_FIRST, target, 0);
        }
        return;
    }
    if (!sEcTap.inside || sEcTap.scrub) {
        sEcTap.down = sEcTap.inside = sEcTap.scrub = 0;
        return;
    }
    code = sEcTap.code;
    sEcTap.down = sEcTap.inside = 0;
    if (e->ask) {
        if (code == PT_EC_HIT_YES || code == PT_EC_HIT_NO)
            EasyPlanStart(EP_ASK, code == PT_EC_HIT_NO, 0);
    } else if (e->screen == PT_EC_EDIT) {
        if (code >= PT_EC_HIT_CHIP && code < PT_EC_HIT_CHIP + PT_EC_WORDS)
            EasyPlanStart(EP_CHIP, code - PT_EC_HIT_CHIP, 1);
        else if (code == PT_EC_HIT_MODE_GROUPS || code == PT_EC_HIT_MODE_ALPHA)
            sEcPref = code == PT_EC_HIT_MODE_ALPHA;
        else if (code == PT_EC_HIT_DELETE)
            EasyPlanStart(EP_DELETE, 0, 1);
        else if (code == PT_EC_HIT_CANCEL)
            sRouteKey = K_B;
        else if (code == PT_EC_HIT_CONFIRM)
            sRouteKey = K_START;
    } else if (e->screen == PT_EC_LIST) {
        if (code >= PT_EC_HIT_ROW && code < PT_EC_HIT_ROW + 2 * PT_EC_ROWS) {
            int word = 2 * (e->first + (code - PT_EC_HIT_ROW) / 2) + (code - PT_EC_HIT_ROW) % 2;

            if (word == e->word)
                sRouteKey = K_A;
            else
                EasyPlanStart(EP_WORD, word, 0);
        } else if (code == PT_EC_HIT_UP)
            sRouteKey = K_UP;
        else if (code == PT_EC_HIT_DOWN)
            sRouteKey = K_DOWN;
        else if (code == PT_EC_HIT_BACK)
            sRouteKey = K_B;
        else if (code == PT_EC_HIT_PLACE)
            sRouteKey = K_A;
        else if (code == PT_EC_HIT_TOGGLE)
            EasyPlanStart(EP_TOGGLE, 0, 0);
    } else if (code >= PT_EC_HIT_CELL && code < PT_EC_HIT_CELL + 26) {
        int cell = code - PT_EC_HIT_CELL;

        if (cell == PT_EC_CELL_TOGGLE)
            sRouteKey = K_SELECT;
        else if (cell == PT_EC_CELL_BACK)
            sRouteKey = K_B;
        else if (cell == e->cell)
            sRouteKey = K_A;
        else
            EasyPlanStart(e->screen == PT_EC_LETTER ? EP_LETTER : EP_GROUP, cell, 1);
    }
}
