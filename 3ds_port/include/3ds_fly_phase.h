#ifndef CTR_FLY_PHASE_H
#define CTR_FLY_PHASE_H
#include <stdbool.h>

/* The half-circle's midpoint is angle 64. Keep the pickup bird there until
 * the player's jump hands it the actor. Arrival can detach only after the
 * preceding sprite update actually presented that same midpoint. */
static inline unsigned CtrFly_NextSwoopAngle(unsigned angle, bool waitingForBoarding)
{
    if (waitingForBoarding && angle >= 64 && angle < 128)
        return 64;
    return (angle + 4) & 255;
}

static inline bool CtrFly_CanDismount(unsigned nextAngle)
{
    return nextAngle >= 66;
}
#endif
