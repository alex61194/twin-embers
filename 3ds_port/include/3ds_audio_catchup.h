#ifndef FIRERED_3DS_AUDIO_CATCHUP_H
#define FIRERED_3DS_AUDIO_CATCHUP_H

#include <stdint.h>

/* Pure planner behind CtrAudio_CatchUpFrames(), shared with the host test.
 *
 * Called once per game frame after the normal m4a tick. `acc` is the real
 * time (ticks) not yet covered by a sound frame. Each call adds the time
 * since the previous call, pays one sound frame for the normal tick (never
 * going below zero: a game frame a little shorter than a sound frame must
 * not bank negative time) and turns every whole sound frame left over into
 * an extra tick. Extra ticks are limited by `cap` and by the room left under
 * the NDSP target depth. Time that cannot be paid is dropped, not carried,
 * so a stall never turns into a later burst or a permanent tempo offset. */
typedef struct {
    uint64_t lastTick;
    uint64_t acc;
    int valid;
} CtrAudioCatchUp;

static inline unsigned CtrAudio_CatchUpPlan(CtrAudioCatchUp *s, uint64_t now,
                                            uint64_t frameTicks, unsigned queued,
                                            unsigned target, unsigned cap)
{
    uint64_t owed, room, n;

    if (!s->valid || frameTicks == 0)
    {
        s->lastTick = now;
        s->acc = 0;
        s->valid = frameTicks != 0;
        return 0;
    }
    s->acc += now - s->lastTick;
    s->lastTick = now;
    s->acc = s->acc >= frameTicks ? s->acc - frameTicks : 0;
    owed = s->acc / frameTicks;
    if (owed == 0)
        return 0;
    room = queued < target ? target - queued : 0;
    n = owed < cap ? owed : cap;
    if (n > room)
        n = room;
    if (n < owed)
        s->acc = 0;
    else
        s->acc -= n * frameTicks;
    return (unsigned)n;
}

#endif
