/* The companion across a map connection (3ds_follower_trail.h, recipe 0260). A model of what CtrFollower_Update remembers (the
 * coordinates seen last and the tiles the player left) and of what the game does to object events when the camera crosses into
 * the next map: it subtracts the camera's shift from the player's, the companion's and everyone's coordinates.
 *
 * Without the shift the next step after the crossing looks like a jump and the companion hides; with it, it is a step and the companion
 * keeps walking. A warp (no shift) is still a jump.
 *
 * usage: follower_trail_test */
#include <assert.h>
#include <stdio.h>
#include "3ds_follower_trail.h"

typedef struct { short x, y; } Pt;
typedef struct {
    Pt seen;                /* sCtrPlayerX/Y */
    Pt trail[4];            /* sCtrTrailX/Y */
    unsigned count;         /* sCtrTrailCount */
    int hidden;             /* sCtrFollowerWaiting */
} Mem;

typedef struct { Pt cur, prev, follower; } World;    /* the player's coordinates, where it came from, and the companion */

/* One frame of CtrFollower_Update's step branch. */
static void Update(Mem *m, const World *w)
{
    if (w->cur.x == m->seen.x && w->cur.y == m->seen.y)
        return;
    if (CtrFollowerTrail_Continues(w->prev.x, w->prev.y, m->seen.x, m->seen.y)) {
        if (m->count == 4) {                        /* the oldest tile goes, as in the game's own code */
            unsigned n;

            for (n = 1; n < 4; n++)
                m->trail[n - 1] = m->trail[n];
            m->count--;
        }
        m->trail[m->count].x = w->prev.x;
        m->trail[m->count].y = w->prev.y;
        m->count++;
    } else {
        m->hidden = 1;
        m->count = 0;
    }
    m->seen = w->cur;
}

/* What the game does at the crossing, and what the port adds to it. */
static void Cross(World *w, Mem *m, int dx, int dy, int withShift)
{
    w->cur.x = (short)(w->cur.x - dx); w->cur.y = (short)(w->cur.y - dy);
    w->prev.x = (short)(w->prev.x - dx); w->prev.y = (short)(w->prev.y - dy);
    w->follower.x = (short)(w->follower.x - dx); w->follower.y = (short)(w->follower.y - dy);
    if (withShift) {
        short tx[4], ty[4]; unsigned i;
        for (i = 0; i < 4; i++) { tx[i] = m->trail[i].x; ty[i] = m->trail[i].y; }
        CtrFollowerTrail_Shift(tx, ty, m->count, &m->seen.x, &m->seen.y, dx, dy);
        for (i = 0; i < 4; i++) { m->trail[i].x = tx[i]; m->trail[i].y = ty[i]; }
    }
}

static void Walk(World *w, Mem *m, int dx, int dy)      /* the player begins a step by (dx, dy) */
{
    w->prev = w->cur;
    w->cur.x = (short)(w->cur.x + dx); w->cur.y = (short)(w->cur.y + dy);
    Update(m, w);
}

static int Scenario(int dx, int dy, int withShift, int stepsBefore, int stepsAfter)
{
    World w = {{40, 30}, {39, 30}, {38, 30}};
    Mem m = {{40, 30}, {{39, 30}}, 1, 0};
    int i;

    for (i = 0; i < stepsBefore; i++)
        Walk(&w, &m, 1, 0);
    Cross(&w, &m, dx, dy, withShift);                  /* the camera crosses into the next map in the middle of a step */
    for (i = 0; i < stepsAfter; i++)
        Walk(&w, &m, 1, 0);
    return m.hidden;
}

int main(void)
{
    short x[4] = {10, 11, 12, 13}, y[4] = {5, 6, 7, 8}, sx = 14, sy = 9;
    int dx, dy;

    /* The shift moves exactly the used tiles and the seen coordinates, by what the game subtracts. */
    CtrFollowerTrail_Shift(x, y, 2, &sx, &sy, 3, -4);
    assert(x[0] == 7 && y[0] == 9 && x[1] == 8 && y[1] == 10);
    assert(x[2] == 12 && y[2] == 7 && x[3] == 13 && y[3] == 8);          /* tiles not in use stay */
    assert(sx == 11 && sy == 13);
    CtrFollowerTrail_Shift(x, y, 0, &sx, &sy, 0, 0);
    assert(sx == 11 && sy == 13);                                        /* nothing to shift by */

    /* The rule: a step is followed only from the tile seen last. */
    assert(CtrFollowerTrail_Continues(5, 6, 5, 6));
    assert(!CtrFollowerTrail_Continues(5, 6, 6, 6) && !CtrFollowerTrail_Continues(5, 6, 5, 7));

    /* Crossings in every direction and by the connections' offsets, after 0, 1 and 3 steps, then a step in the new map. */
    for (dx = -40; dx <= 40; dx += 8)
        for (dy = -40; dy <= 40; dy += 8) {
            int before;

            if (dx == 0 && dy == 0)
                continue;
            for (before = 0; before <= 3; before++) {
                assert(Scenario(dx, dy, 1, before, 2) == 0);             /* with the shift: it keeps following */
                assert(Scenario(dx, dy, 0, before, 1) == 1);             /* without it (the old behaviour): it hid */
            }
        }

    /* A warp: the player is put somewhere else, nothing is shifted: a jump, the companion hides and starts again. */
    {
        World w = {{40, 30}, {39, 30}, {38, 30}};
        Mem m = {{40, 30}, {{39, 30}}, 1, 0};

        w.cur.x = 12; w.cur.y = 9; w.prev.x = 12; w.prev.y = 9;
        Update(&m, &w);
        assert(m.hidden == 1 && m.count == 0 && m.seen.x == 12 && m.seen.y == 9);
    }

    printf("follower trail test passed\n");
    return 0;
}
