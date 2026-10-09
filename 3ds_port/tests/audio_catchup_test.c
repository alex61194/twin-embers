/* CtrAudio_CatchUpPlan: missed sound frames are repaid once, capped, and only
 * while the NDSP queue is under its target depth. */
#include <assert.h>
#include <stdint.h>

#include "3ds_audio_catchup.h"

enum { TARGET = 4, CAP = 3, BUFFERS = 8 };
#define P 2000000ull /* one sound frame in ticks; any value works */

static unsigned Step(CtrAudioCatchUp *s, uint64_t *now, uint64_t delta, unsigned queued)
{
    *now += delta;
    return CtrAudio_CatchUpPlan(s, *now, P, queued, TARGET, CAP);
}

int main(void)
{
    CtrAudioCatchUp s = {0};
    uint64_t now = 1000;

    assert(CtrAudio_CatchUpPlan(&s, now, P, 3, TARGET, CAP) == 0); /* anchors */

    /* Normal 60 FPS: game frame slightly shorter or longer than a sound frame. */
    for (int i = 0; i < 600; ++i)
        assert(Step(&s, &now, P - P / 50 + (i % 3) * (P / 100), 3) == 0);

    /* One missed frame (two periods) with the queue below target: one tick. */
    assert(Step(&s, &now, 2 * P, 2) == 1);
    assert(Step(&s, &now, P, 3) == 0);

    /* Long stall is capped at 3 and the rest is dropped, not banked. */
    assert(Step(&s, &now, 20 * P, 0) == 3);
    assert(Step(&s, &now, P, 3) == 0);
    assert(s.acc == 0);

    /* Queue at/above target: never catch up, debt is dropped. */
    assert(Step(&s, &now, 3 * P, TARGET) == 0);
    assert(Step(&s, &now, P, TARGET) == 0);
    assert(Step(&s, &now, 3 * P, TARGET + 2) == 0);
    assert(Step(&s, &now, P, 3) == 0);

    /* Room limits the burst: queue 3 of target 4 allows only one. */
    assert(Step(&s, &now, 6 * P, 3) == 1);

    /* Unrepaid fraction below a frame is carried, so a steady 30 FPS game
     * (2 periods minus a hair per frame) gets its ticks without drift. */
    uint64_t produced = 0, elapsed = 0;
    CtrAudioCatchUp t = {0};
    uint64_t tnow = 5;
    (void)CtrAudio_CatchUpPlan(&t, tnow, P, 3, TARGET, CAP);
    for (int i = 0; i < 3000; ++i)
    {
        uint64_t d = 2 * P - P / 50; /* 30 FPS-ish, 2 % under two periods */
        tnow += d;
        elapsed += d;
        produced += 1 + CtrAudio_CatchUpPlan(&t, tnow, P, 2, TARGET, CAP);
    }
    /* Audio time produced tracks real time to within one sound frame. */
    assert(produced * P <= elapsed + P);
    assert(produced * P + P >= elapsed);

    /* Over a long mixed run with a queue that drains one buffer per sound
     * frame of real time, the queue never exceeds BUFFERS. */
    CtrAudioCatchUp u = {0};
    uint64_t unow = 0, clock = 0;
    int queued = 3;
    (void)CtrAudio_CatchUpPlan(&u, unow, P, (unsigned)queued, TARGET, CAP);
    for (int i = 0; i < 5000; ++i)
    {
        uint64_t d = (i % 17 == 0) ? 7 * P : (i % 5 == 0 ? 2 * P : P);
        unow += d;
        clock += d;
        queued -= (int)(clock / P);
        clock %= P;
        if (queued < 0)
            queued = 0;
        if (queued < BUFFERS)
            ++queued; /* normal tick */
        unsigned extra = CtrAudio_CatchUpPlan(&u, unow, P, (unsigned)queued, TARGET, CAP);
        assert(extra <= CAP);
        assert((unsigned)queued + extra <= BUFFERS);
        assert(extra == 0 || (unsigned)queued + extra <= TARGET);
        queued += (int)extra;
    }

    /* Rate unknown: no plan, state stays unanchored. */
    CtrAudioCatchUp z = {0};
    assert(CtrAudio_CatchUpPlan(&z, 99, 0, 0, TARGET, CAP) == 0 && !z.valid);
    return 0;
}
