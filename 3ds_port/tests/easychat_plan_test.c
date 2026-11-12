/* The walks of the lower screen's Easy Chat (3ds_bottom_easychat_impl.h) against a model of the game's own screen (src/easy_chat_2.c, in the
 * recipe's terms): every key press is applied the way the game's joypad handlers do, the screen is "not ready" for a couple of frames after a
 * key that starts a command, and the walk reads the state again every frame. Synthetic data only. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "3ds_input.h"
#include "3ds_text_case.h"
#include "3ds_poketouch.h"

#define MODE_GBAVIEW 18
#define K_START (1u << 3)
#define K_A (1u << 0)
#define K_B (1u << 1)
#define K_SELECT (1u << 2)
#define K_UP (1u << 6)
#define K_DOWN (1u << 7)
#define K_LEFT (1u << 5)
#define K_RIGHT (1u << 4)

typedef struct {
    unsigned char mode;
    CtrPokeTouchView touch;
} Snapshot;
static Snapshot sShown;
static int sHaveShown;
static uint16_t sRouteKey, sInjected;
static CtrInput sIn;
const CtrInput *CtrInput_Get(void) { return &sIn; }
static int CtrBottom_IsFading(void) { return 0; }

/* ---- the model of the game's screen ---- */
static struct {
    int open, wait, cols, rows, state, mcol, mrow, words, alpha, gx, gy, gabove, grows, wabove, wrows, wx, wy, ngroups, total, backup, askcur, frame2;
    int closed;                     /* the screen was left by YES */
    int tick, alphaToggles;
} G;

static void game_open(int ngroups, int total, int frame2)
{
    memset(&G, 0, sizeof(G));
    G.open = 1;
    G.cols = 2;
    G.rows = frame2 ? 5 : 3;
    G.words = G.cols * G.rows > 9 ? 9 : G.cols * G.rows;
    G.frame2 = frame2;
    G.ngroups = ngroups;
    G.grows = (ngroups - 1) / 2 + 1;
    G.total = total;
}
int CtrBottomEasyChat_IsOpen(void) { return G.open; }
int CtrBottomEasyChat_Ready(void) { return G.open && G.wait == 0; }
int CtrBottomEasyChat_Get(int w)
{
    if (!G.open)
        return -1;
    switch (w) {
    case 0: return 5;
    case 1: return G.cols;
    case 2: return G.rows;
    case 3: return G.state;
    case 4: return G.mcol;
    case 5: return G.mrow;
    case 6: return G.words;
    case 7: return G.alpha;
    case 8: return G.gx;
    case 9: return G.gy;
    case 10: return G.gabove;
    case 11: return G.grows;
    case 12: return G.wabove;
    case 13: return G.wrows;
    case 14: return G.wx;
    case 15: return G.wy;
    case 16: return G.ngroups;
    case 17: return G.state == 3 ? G.total : 0;
    case 18: return G.backup;
    case 19: return G.askcur;
    }
    return -1;
}
void CtrBottomEasyChat_Phrase(int slot, char *out, int size) { snprintf(out, (size_t)size, slot % 3 ? "WORD%d" : "", slot); }
void CtrBottomEasyChat_GroupName(int i, char *out, int size) { snprintf(out, (size_t)size, "GROUP %d", i); }
/* The list is in order: word i starts with the letter 26 * i / total. */
void CtrBottomEasyChat_ListWord(int i, char *out, int size)
{
    out[0] = 0;
    if (G.state == 3 && i >= 0 && i < G.total)
        snprintf(out, (size_t)size, "%cWORD%d", 'A' + i * 26 / G.total, i);
}
void CtrBottomEasyChat_Title(char *out, int size) { snprintf(out, (size_t)size, "TITLE"); }
void CtrBottomEasyChat_Question(int which, int line, char *out, int size) { snprintf(out, (size_t)size, line ? "" : "Q%d?", which); }

/* The stubs of the renderer's side: what is under the finger is the test's to say. */
static int gHitCode = -1, gRail = -1, gStrip = -1;
int CtrPokeTouch_EasyHit(const CtrPokeTouchView *v, int x, int y) { (void)v; (void)x; (void)y; return gHitCode; }
int CtrPokeTouch_EasyRailTarget(const CtrPokeTouchEasy *e, int y) { (void)e; (void)y; return gRail; }
int CtrPokeTouch_EasyStripLetter(int x, int y) { (void)x; (void)y; return gStrip; }

#include "../src/3ds_bottom_easychat_impl.h"

static void group_blue(void) { G.gx = -1; if (G.gy) G.gy--; }
static void fix_field(void)
{
    if (G.mrow < 0) G.mrow = G.rows;
    if (G.mrow > G.rows) G.mrow = 0;
    if (G.mrow == G.rows) { if (G.mcol > 2) G.mcol = 2; G.state = 1; return; }
    if (G.mcol < 0) G.mcol = G.cols - 1;
    if (G.mcol >= G.cols) G.mcol = 0;
    if (G.frame2 && G.mcol == 1 && G.mrow == 4) G.mcol = 0;
    G.state = 0;
}
static int group_far(void)
{
    if (!G.alpha) return 2 * (G.gy + G.gabove) + G.gx >= G.ngroups;
    return G.gx > (G.gy == 1 ? 5 : 6);
}
static void group_clamp(void) { while (group_far() && G.gx) G.gx--; }
static int word_far(void) { return 2 * (G.wy + G.wabove) + G.wx >= G.total; }
static void word_clamp(void) { while (word_far() && G.wx) G.wx--; }
static void game_group_key(int d)   /* 0 right, 1 left, 2 up, 3 down (the game's own numbers) */
{
    if (G.gx != -1 && !G.alpha) {
        switch (d) {
        case 2: if (G.gy != -G.gabove) { if (G.gy) G.gy--; else G.gabove--; } break;
        case 3:
            if (G.gy + G.gabove < G.grows - 1) { if (G.gy < 3) G.gy++; else G.gabove++; group_clamp(); }
            break;
        case 1: if (G.gx) G.gx--; else group_blue(); break;
        case 0: if (G.gx < 1) { G.gx++; if (group_far()) group_blue(); } else group_blue(); break;
        }
    } else if (G.gx != -1) {
        switch (d) {
        case 2: G.gy = G.gy > 0 ? G.gy - 1 : 3; group_clamp(); break;
        case 3: G.gy = G.gy < 3 ? G.gy + 1 : 0; group_clamp(); break;
        case 0: G.gx++; if (group_far()) group_blue(); break;
        case 1: G.gx--; if (G.gx < 0) group_blue(); break;
        }
    } else {
        switch (d) {
        case 2: G.gy = G.gy ? G.gy - 1 : 2; break;
        case 3: G.gy = G.gy < 2 ? G.gy + 1 : 0; break;
        case 1: G.gy++; if (!G.alpha) { G.gx = 1; group_clamp(); } else G.gx = G.gy == 1 ? 5 : 6; break;
        case 0: G.gx = 0; G.gy++; break;
        }
    }
}
static void game_word_key(int d)
{
    switch (d) {
    case 2: if (G.wy + G.wabove > 0) { if (G.wy > 0) G.wy--; else G.wabove--; word_clamp(); } break;
    case 3: if (G.wy + G.wabove < G.wrows) { if (G.wy < 3) G.wy++; else G.wabove++; word_clamp(); } break;
    case 1: G.wx = G.wx > 0 ? G.wx - 1 : 1; word_clamp(); break;
    case 0: if (G.wx < 1) { G.wx++; if (word_far()) G.wx = 0; } else G.wx = 0; break;
    case 4: if (G.wabove) G.wabove = G.wabove > 3 ? G.wabove - 4 : 0; break;
    case 5:
        if (G.wabove <= G.wrows - 4) {
            G.wabove += 4;
            if (G.wabove > G.wrows - 3) G.wabove = G.wrows - 3;
            word_clamp();
        }
        break;
    }
}
/* One key, as the game's joypad handler of its state takes it; a key that does something puts the screen out of the waiting state for two frames. */
static void game_key(uint16_t k)
{
    int d = k == K_RIGHT ? 0 : k == K_LEFT ? 1 : k == K_UP ? 2 : k == K_DOWN ? 3 : -1, acted = 1;

    assert(G.open && G.wait == 0);
    switch (G.state) {
    case 0:
    case 1:
        if (k == K_B) { G.backup = G.state; G.state = 4; G.askcur = 1; break; }
        if (k == K_START) { G.backup = G.state; G.state = 6; G.askcur = 0; break; }
        if (G.state == 0 && k == K_A) { G.state = 2; G.gx = G.gy = G.gabove = 0; break; }
        if (G.state == 1 && k == K_A) {
            G.backup = 1;
            G.state = G.mcol == 0 ? 5 : G.mcol == 1 ? 4 : 6;
            G.askcur = G.state == 6 ? 0 : 1;
            break;
        }
        if (d < 0) { acted = 0; break; }
        if (G.state == 0) {
            if (d == 2) G.mrow--; else if (d == 1) G.mcol--; else if (d == 3) G.mrow++; else G.mcol++;
            fix_field();
        } else {
            if (d == 2) G.mrow--; else if (d == 1) G.mcol--; else if (d == 3) G.mrow = 0; else G.mcol++;
            if (G.mrow == G.rows) { if (G.mcol < 0) G.mcol = 2; if (G.mcol >= 3) G.mcol = 0; }
            else { if (G.mcol >= G.cols) G.mcol = G.cols - 1; if (G.frame2 && G.mcol == 1 && G.mrow == 4) G.mcol = 0; G.state = 0; }
        }
        break;
    case 2:
        if (k == K_B) { G.state = 0; break; }
        if (k == K_SELECT) { G.gx = G.gy = G.gabove = 0; G.alpha ^= 1; G.alphaToggles++; break; }
        if (k == K_A) {
            if (G.gx != -1) { G.state = 3; G.wrows = (G.total - 1) / 2; G.wabove = G.wx = G.wy = 0; break; }
            if (G.gy == 0) { G.gx = G.gy = G.gabove = 0; G.alpha ^= 1; G.alphaToggles++; }
            else if (G.gy == 2) G.state = 0;
            break;
        }
        if (d < 0) { acted = 0; break; }
        game_group_key(d);
        break;
    case 3:
        if (k == K_B) { G.state = 2; break; }
        if (k == K_A) { G.state = 0; break; }
        if (k == K_START) { game_word_key(4); break; }
        if (k == K_SELECT) { game_word_key(5); break; }
        if (d < 0) { acted = 0; break; }
        game_word_key(d);
        break;
    default:                        /* a question */
        if (d == 2 && G.askcur) G.askcur = 0;
        else if (d == 3 && !G.askcur) G.askcur = 1;
        else if (k == K_A && G.askcur == 0) G.closed = 1;
        else if (k == K_A || k == K_B) G.state = G.backup;
        else acted = 0;
        break;
    }
    if (acted)
        G.wait = 2;
}

/* One frame: the walk's key goes to the game, a frame with none after it, as 3ds_bottom.c does. */
static void frame(void)
{
    sInjected = 0;
    RunEasyPlan(1);
    if (G.wait) G.wait--;
    else if (sInjected) game_key(sInjected);
    if (sRouteKey && !(sIn.held & 0x3ff)) { if (G.wait == 0) game_key(sRouteKey); }
    sRouteKey = 0;
    ++G.tick;
}
static int walk(int kind, int target, int then)
{
    int n = 0;

    EasyPlanStart(kind, target, then);
    while (sEcPlan.kind != EP_NONE && n < 400) { frame(); ++n; }
    for (int i = 0; i < 4; ++i) frame();
    return n;
}
static void settle(void) { for (int i = 0; i < 10; ++i) frame(); }

static void test_chips(void)
{
    for (int frame2 = 0; frame2 < 2; ++frame2)
        for (int from = 0; from < 3; ++from)                    /* the field's first row, its last, the footer */
            for (int t = 0; t < (frame2 ? 9 : 6); ++t) {
                game_open(14, 100, frame2);
                if (from == 1) { G.mrow = G.rows - 1; G.mcol = 1; if (frame2) G.mcol = 0; }
                if (from == 2) { G.state = 1; G.mrow = G.rows; G.mcol = 2; }
                int n = walk(EP_CHIP, t, 1);
                assert(n < 40);
                assert(G.state == 2);                           /* arrived and took it: the group menu is open */
            }
}
static void test_delete(void)
{
    for (int from = 0; from < 3; ++from) {
        game_open(14, 100, 1);
        if (from == 1) { G.mrow = 4; G.mcol = 0; }
        if (from == 2) { G.state = 1; G.mrow = G.rows; G.mcol = 2; }
        walk(EP_DELETE, 0, 1);
        assert(G.state == 5 && G.askcur == 1);                  /* the question is open, its cursor on NO */
        walk(EP_ASK, 1, 0);
        assert(G.state == 1 && G.mcol == 0);                    /* NO: back on the buttons */
    }
    game_open(14, 100, 0);
    G.state = 1; G.mrow = G.rows; G.mcol = 1;
    walk(EP_DELETE, 0, 1); walk(EP_ASK, 0, 0);
    assert(G.closed);                                           /* YES: a real answer */
    game_open(14, 100, 0); G.state = 6; G.askcur = 0; G.backup = 0;
    walk(EP_ASK, 1, 0); assert(G.state == 0);
    game_open(14, 100, 0); G.state = 4; G.askcur = 1; G.backup = 0;
    walk(EP_ASK, 0, 0); assert(G.closed);
}
static void test_groups(void)
{
    for (int count = 14; count <= 21; count += 7)
        for (int from = 0; from < count; from += 3)
            for (int t = 0; t < count; ++t) {
                game_open(count, 100, 0);
                G.state = 2; G.gx = from % 2; G.gy = (from / 2) % 4; G.gabove = from / 2 - G.gy;
                if (G.gabove < 0) { G.gabove = 0; G.gy = from / 2; }
                walk(EP_GROUP, t, 1);
                assert(G.state == 3);                           /* the group's words are open */
            }
    game_open(21, 100, 0); G.state = 2; G.gx = -1; G.gy = 1; walk(EP_GROUP, 17, 0);   /* out of the blue box */
    assert(2 * (G.gy + G.gabove) + G.gx == 17);
    for (int t = 0; t < 26; ++t) {                              /* the letters, from the blue box and from elsewhere */
        game_open(21, 100, 0); G.state = 2; G.alpha = 1; G.gx = t % 7 > 5 ? 5 : t % 7; G.gy = t % 4;
        if (t % 5 == 0) { G.gx = -1; G.gy = 0; }
        walk(EP_LETTER, t, 0);
        int lx, ly; EasyLetterPos(t, &lx, &ly);
        assert(G.gx == lx && G.gy == ly && G.state == 2);
    }
}
static void test_words(void)
{
    for (int total = 5; total <= 201; total += 98)
        for (int t = 0; t < total && t < 70; t += 7) {
            game_open(21, total, 0);
            G.state = 3; G.wrows = (total - 1) / 2;
            walk(EP_WORD, t, 0);
            if (2 * (G.wy + G.wabove) + G.wx != t) fprintf(stderr, "total %d t %d y %d a %d x %d%c", total, t, G.wy, G.wabove, G.wx, 10);
            assert(2 * (G.wy + G.wabove) + G.wx == t);
        }
    /* the scroll: every first row, from the top, the end and the middle; a target past the end stops at the end */
    for (int total = 30; total <= 400; total += 123)
        for (int from = 0; from < 3; ++from)
            for (int t = 0; t <= (total + 1) / 2 - PT_EC_ROWS; t += 5) {
                int rows = (total + 1) / 2, last = rows - PT_EC_ROWS, n;
                game_open(21, total, 0);
                G.state = 3; G.wrows = (total - 1) / 2;
                if (from == 1) { G.wabove = (rows - 3) < 0 ? 0 : rows - 3; G.wy = 3; }
                if (from == 2) { G.wabove = last / 2; G.wy = 1; }
                n = walk(EP_FIRST, t, 0);
                assert((G.wabove > last ? last : G.wabove) == t || (t > last));
                assert(n < 240);
            }
    game_open(21, 40, 0); G.state = 3; G.wrows = 19; walk(EP_FIRST, 500, 0);         /* more than there is: the end, no endless keys */
    assert(G.wabove == 14);
}
static void test_toggle_and_pref(void)
{
    game_open(21, 100, 0); G.state = 3; walk(EP_TOGGLE, 0, 0);
    assert(G.state == 2 && G.alpha == 1);                       /* BACK, then the order's key */
    walk(EP_GROUP, 0, 0);                                       /* nothing happens in the letters' state but a walk of the other kind: it ends */
    game_open(21, 100, 0);
    sEcPref = 1; sEcPrev = -1; sEcEntry = 0;
    for (int i = 0; i < 4; ++i) frame();
    game_key(K_A); settle();                                    /* the phrase's A: the groups open, and the editor's choice is applied */
    assert(G.state == 2 && G.alpha == 1 && G.alphaToggles == 1 && sEcPref == -1);
    game_key(K_A); settle(); game_key(K_B); settle();           /* into a list and out of it: not applied again */
    assert(G.alphaToggles == 1);
    game_open(21, 100, 0); sEcPref = 0; sEcPrev = -1; sEcEntry = 0; game_key(K_A); settle();
    assert(G.alphaToggles == 0 && sEcPref == -1);               /* it is the game's already */
}
static void test_cancel_and_limits(void)
{
    game_open(21, 100, 0); G.state = 2;
    EasyPlanStart(EP_GROUP, 15, 1);
    for (int i = 0; i < 6; ++i) frame();
    sIn.held = K_UP;                                            /* a physical key: the walk ends and sends nothing */
    sInjected = 0; RunEasyPlan(1); assert(sEcPlan.kind == EP_NONE && sInjected == 0);
    sIn.held = 0;
    game_open(21, 100, 0); G.state = 2; G.open = 1; EasyPlanStart(EP_GROUP, 15, 0);
    G.state = 0;                                                /* the game left the group menu under the walk: it ends, sends nothing */
    sInjected = 0; RunEasyPlan(1); assert(sEcPlan.kind == EP_NONE && sInjected == 0);
    game_open(21, 100, 0); G.state = 3; G.wrows = 49; EasyPlanStart(EP_FIRST, 40, 0);
    int keys = 0;
    for (int i = 0; i < 3000 && sEcPlan.kind; ++i) { sInjected = 0; RunEasyPlan(1); if (G.wait) G.wait--; else if (sInjected) { ++keys; /* the key is lost: the game does not move */ } }
    assert(sEcPlan.kind == EP_NONE && keys <= EC_STALL_LIMIT + 2);   /* a game that does not answer: given up */
    G.open = 0; sInjected = 0; RunEasyPlan(1); assert(sEcPlan.kind == EP_NONE && sEcPref == -1);
}
static void touch(int down, int active) { sIn.touchDown = down; sIn.touchActive = active; sIn.touchX = 100; sIn.touchY = 100; }
static void shown(void)
{
    memset(&sShown, 0, sizeof(sShown));
    sShown.mode = MODE_GBAVIEW; sHaveShown = 1;
    assert(EasyFill(&sShown.touch.ec, -1));
}
static void tap(int code)
{
    shown(); gHitCode = code;
    touch(1, 1); ProcessEasyTouch(&sIn);
    touch(0, 1); ProcessEasyTouch(&sIn);
    touch(0, 0); ProcessEasyTouch(&sIn);
    gHitCode = -1;
}
static void test_touch(void)
{
    game_open(14, 100, 1);
    tap(PT_EC_HIT_CHIP + 5); assert(sEcPlan.kind == EP_CHIP && sEcPlan.target == 5 && sEcPlan.then);
    for (int i = 0; i < 60 && sEcPlan.kind; ++i) frame();
    settle(); assert(G.state == 2);
    memset(&sEcPlan, 0, sizeof(sEcPlan));
    shown(); assert(sShown.touch.ec.screen == PT_EC_GROUP && sShown.touch.ec.groupCount == 14 && sShown.touch.ec.cell == 0);
    tap(PT_EC_HIT_CELL + 0); assert(sRouteKey == K_A); sRouteKey = 0;                   /* the cell the game's cursor is on: its A */
    tap(PT_EC_HIT_CELL + 9); assert(sEcPlan.kind == EP_GROUP && sEcPlan.target == 9);
    memset(&sEcPlan, 0, sizeof(sEcPlan));
    tap(PT_EC_HIT_CELL + PT_EC_CELL_TOGGLE); assert(sRouteKey == K_SELECT); sRouteKey = 0;
    tap(PT_EC_HIT_CELL + PT_EC_CELL_BACK); assert(sRouteKey == K_B); sRouteKey = 0;
    /* a held touch that slides off its control does nothing; a physical key cancels it */
    shown(); gHitCode = PT_EC_HIT_CELL + 3; touch(1, 1); ProcessEasyTouch(&sIn); gHitCode = PT_EC_HIT_CELL + 4; touch(0, 1); ProcessEasyTouch(&sIn);
    touch(0, 0); ProcessEasyTouch(&sIn); assert(!sEcPlan.kind && !sRouteKey);
    shown(); gHitCode = PT_EC_HIT_CELL + 3; touch(1, 1); sIn.held = K_UP; ProcessEasyTouch(&sIn); sIn.held = 0; touch(0, 0); ProcessEasyTouch(&sIn);
    assert(!sEcPlan.kind && !sRouteKey);
    /* the footer */
    game_open(14, 100, 0); tap(PT_EC_HIT_CANCEL); assert(sRouteKey == K_B); sRouteKey = 0;
    tap(PT_EC_HIT_CONFIRM); assert(sRouteKey == K_START); sRouteKey = 0;
    tap(PT_EC_HIT_DELETE); assert(sEcPlan.kind == EP_DELETE); memset(&sEcPlan, 0, sizeof(sEcPlan));
    tap(PT_EC_HIT_MODE_ALPHA); assert(sEcPref == 1); tap(PT_EC_HIT_MODE_GROUPS); assert(sEcPref == 0); sEcPref = -1;
    /* a list: a word, the buttons, the sliders */
    game_open(14, 100, 0); G.state = 3; G.wrows = 49; G.wabove = 6; G.wy = 1; G.wx = 1;
    shown(); assert(sShown.touch.ec.screen == PT_EC_LIST && sShown.touch.ec.first == 6 && sShown.touch.ec.word == 2 * 7 + 1);
    tap(PT_EC_HIT_ROW + 2 * 1 + 1); assert(sRouteKey == K_A); sRouteKey = 0;            /* the selected word itself: taken */
    tap(PT_EC_HIT_ROW + 2 * 4); assert(sEcPlan.kind == EP_WORD && sEcPlan.target == 2 * 10); memset(&sEcPlan, 0, sizeof(sEcPlan));
    tap(PT_EC_HIT_TOGGLE); assert(sEcPlan.kind == EP_TOGGLE); memset(&sEcPlan, 0, sizeof(sEcPlan));
    tap(PT_EC_HIT_BACK); assert(sRouteKey == K_B); sRouteKey = 0;
    tap(PT_EC_HIT_PLACE); assert(sRouteKey == K_A); sRouteKey = 0;
    tap(PT_EC_HIT_UP); assert(sRouteKey == K_UP); sRouteKey = 0;
    shown(); gHitCode = PT_EC_HIT_RAIL; gRail = 30; touch(1, 1); ProcessEasyTouch(&sIn);          /* a tap on the rail jumps ... */
    assert(sEcPlan.kind == EP_FIRST && sEcPlan.target == 30);
    for (int i = 0; i < 200 && sEcPlan.kind; ++i) frame();
    assert(G.wabove == 30);
    gRail = 12; touch(0, 1); ProcessEasyTouch(&sIn);                                              /* ... a drag follows the finger */
    assert(sEcPlan.kind == EP_FIRST && sEcPlan.target == 12);
    gHitCode = -1; touch(0, 0); ProcessEasyTouch(&sIn);
    for (int i = 0; i < 200 && sEcPlan.kind; ++i) frame();
    assert(G.wabove == 12 && !sEcTap.down);
    /* the strip: the first word of the letter (the list has 100, word i starts with 'A' + i * 26 / 100) */
    shown(); gHitCode = PT_EC_HIT_STRIP; gStrip = 'M' - 'A'; touch(1, 1); ProcessEasyTouch(&sIn);
    for (int i = 0; i < 200 && sEcPlan.kind; ++i) frame();
    touch(0, 0); ProcessEasyTouch(&sIn);
    {
        char w[PT_EC_WORD_LEN];
        int first = G.wabove;

        CtrBottomEasyChat_ListWord(2 * first, w, sizeof(w));
        {
            char w2[PT_EC_WORD_LEN];

            CtrBottomEasyChat_ListWord(2 * first + 1, w2, sizeof(w2));
            assert(w[0] == 'M' || w2[0] == 'M');                                      /* its first row holds the first word that starts with M */
        }
        CtrBottomEasyChat_ListWord(2 * first - 1, w, sizeof(w));
        assert(w[0] < 'M');
    }
    gHitCode = -1; gStrip = -1;
}
static void test_fill(void)
{
    game_open(21, 100, 1);
    G.mrow = 4; G.mcol = 0; shown();
    assert(sShown.touch.ec.screen == PT_EC_EDIT && sShown.touch.ec.cursor == 8 && sShown.touch.ec.numWords == 9 && sShown.touch.ec.footer == -1);
    G.state = 1; G.mrow = 5; G.mcol = 1; shown(); assert(sShown.touch.ec.cursor == -1 && sShown.touch.ec.footer == 1);
    G.state = 4; G.backup = 1; G.askcur = 1; shown(); assert(sShown.touch.ec.ask == PT_EC_ASK_QUIT && sShown.touch.ec.askCursor == 1 && sShown.touch.ec.footer == 1);
    G.state = 6; G.askcur = 0; shown(); assert(sShown.touch.ec.ask == PT_EC_ASK_CONFIRM && sShown.touch.ec.askCursor == 0);
    G.state = 2; G.gx = -1; G.gy = 2; shown(); assert(sShown.touch.ec.cell == PT_EC_CELL_BACK);
    G.gy = 0; shown(); assert(sShown.touch.ec.cell == PT_EC_CELL_TOGGLE);
    G.gy = 1; shown(); assert(sShown.touch.ec.cell == -1);
    G.alpha = 1; G.gx = 3; G.gy = 2; shown(); assert(sShown.touch.ec.screen == PT_EC_LETTER && sShown.touch.ec.cell == 15);
    G.alpha = 0; G.gx = 1; G.gy = 2; G.gabove = 3; shown(); assert(sShown.touch.ec.cell == 11);
    G.state = 3; G.total = 7; G.wrows = 3; shown(); assert(sShown.touch.ec.listCount == 7 && sShown.touch.ec.first == 0 && sShown.touch.ec.letter == 0);
    G.total = 100; G.wrows = 49; G.wabove = 46; G.wy = 3; G.wx = 1; shown();
    assert(sShown.touch.ec.first == 50 - PT_EC_ROWS && sShown.touch.ec.word == 2 * 49 + 1);       /* the cursor stays in the six rows shown */
    assert(sShown.touch.ec.word >= 2 * sShown.touch.ec.first && sShown.touch.ec.word < 2 * (sShown.touch.ec.first + PT_EC_ROWS));
    G.wait = 3; assert(!EasyFill(&sShown.touch.ec, -1));            /* not waiting for a key: the viewport as before */
}

int main(void)
{
    game_open(14, 100, 0);
    EasyReset();
    {
        Snapshot snap;

        memset(&snap, 0, sizeof(snap));
        assert(EasySnapshot(&snap) && snap.touch.ec.on && !snap.touch.viewport && !snap.touch.backVisible);
        assert(snap.touch.ec.chip[1][0] == 'W' && snap.touch.ec.chip[1][1] == 'o');      /* the game's ALL CAPS words, cased for the lower screen */
    }
    test_fill();
    test_chips();
    test_delete();
    test_groups();
    test_words();
    test_toggle_and_pref();
    test_cancel_and_limits();
    test_touch();
    puts("PASS Easy Chat walks against the model of the game's screen");
    return 0;
}
