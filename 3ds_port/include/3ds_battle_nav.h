#ifndef CTR_BATTLE_NAV_H
#define CTR_BATTLE_NAV_H
/*
 * The battle action menu's D-pad graph, in the layout the lower screen draws:
 *
 *          FIGHT
 *   BAG   POKeMON   RUN
 *
 * Cursors are FireRed's own (gActionSelectionCursor): 0 FIGHT, 1 BAG, 2 POKeMON, 3 RUN. One table
 * serves the physical D-pad (battle_controller_player.c) and the touch plans (3ds_bottom.c), so both
 * move the same cursor the same way. No wrapping: a direction with nowhere to go keeps the cursor.
 */
enum { CTR_NAV_UP, CTR_NAV_DOWN, CTR_NAV_LEFT, CTR_NAV_RIGHT, CTR_NAV_DIRS };

/* Game controllers share the port implementation instead of embedding a
 * second navigation table in an external game object. */
int CtrBattleAction_Next(int cursor, int dir);

/* The cursor after one step in `dir` (CTR_NAV_*) from `cursor`; `cursor` itself when there is no way. */
static inline int CtrBattleActionStep(int cursor, int dir)
{
    static const signed char kNext[4][CTR_NAV_DIRS] = {
        /*            UP  DOWN LEFT RIGHT */
        /* FIGHT */ { -1,   2,  -1,  -1 },
        /* BAG   */ {  0,  -1,  -1,   2 },
        /* POKeMON*/{  0,  -1,   1,   3 },
        /* RUN   */ {  0,  -1,   2,  -1 },
    };
    int next;

    if (cursor < 0 || cursor > 3 || dir < 0 || dir >= CTR_NAV_DIRS)
        return cursor;
    next = kNext[cursor][dir];
    return next < 0 ? cursor : next;
}

/*
 * The quick ball (issue #16): in a wild battle the lower screen draws a ball button between BAG and POKeMON,
 *
 *          FIGHT
 *   BAG  BALL  POKeMON  RUN
 *
 * and the controller's cursor can be on it too (CTR_ACTION_BALL, past FireRed's own four), while `ball` (a ball can be
 * thrown). CtrBattle_QuickBall (battle_controller_player.c) says what the button shows, CTR_QUICK_BALL_*: nothing (no
 * button: a trainer, the Safari Zone...), a ball ready, no balls, or the boxes full (shown, not usable).
 */
#define CTR_ACTION_BALL 4
enum { CTR_QUICK_BALL_NONE = 0, CTR_QUICK_BALL_READY, CTR_QUICK_BALL_EMPTY, CTR_QUICK_BALL_FULL };

static inline int CtrBattleActionStepBall(int cursor, int dir, int ball)
{
    if (!ball)
        return CtrBattleActionStep(cursor, dir);
    if (cursor == CTR_ACTION_BALL)
        return dir == CTR_NAV_UP ? 0 : dir == CTR_NAV_LEFT ? 1 : dir == CTR_NAV_RIGHT ? 2 : cursor;
    if ((cursor == 1 && dir == CTR_NAV_RIGHT) || (cursor == 2 && dir == CTR_NAV_LEFT))
        return CTR_ACTION_BALL;
    return CtrBattleActionStep(cursor, dir);
}

/* Steps of a shortest walk from `from` to `to`; -1 when there is none. */
static inline int CtrBattleActionDistance(int from, int to)
{
    int dist[4] = {-1, -1, -1, -1}, queue[4], head = 0, tail = 0, dir;

    if (from < 0 || from > 3 || to < 0 || to > 3)
        return -1;
    dist[from] = 0;
    queue[tail++] = from;
    while (head < tail) {
        int at = queue[head++];

        for (dir = 0; dir < CTR_NAV_DIRS; ++dir) {
            int next = CtrBattleActionStep(at, dir);

            if (dist[next] < 0) {
                dist[next] = dist[at] + 1;
                queue[tail++] = next;
            }
        }
    }
    return dist[to];
}

/* The first direction of a shortest walk from `cursor` to `target`; -1 when already there or unreachable. */
static inline int CtrBattleActionDir(int cursor, int target)
{
    int dir, best = -1, bestLength = 99;

    if (cursor == target)
        return -1;
    for (dir = 0; dir < CTR_NAV_DIRS; ++dir) {
        int next = CtrBattleActionStep(cursor, dir), rest;

        if (next == cursor)
            continue;
        rest = CtrBattleActionDistance(next, target);
        if (rest >= 0 && rest + 1 < bestLength) {
            bestLength = rest + 1;
            best = dir;
        }
    }
    return best;
}
#endif
