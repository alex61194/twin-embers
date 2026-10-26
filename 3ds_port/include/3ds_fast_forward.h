#ifndef FIRERED_3DS_FAST_FORWARD_H
#define FIRERED_3DS_FAST_FORWARD_H

#include <stdbool.h>

/* The overworld turbo, without fast music. Shared by the game bridge and its host test.
 *
 * At speed N, the game runs N frames for each one shown: only the shown one
 * is presented (which waits for the screen) and advances the sound engine, so
 * the game runs up to N times faster while music and sound effects keep their
 * pace. Every frame still runs the game, its VBlank handler, input, the bottom
 * screen and the save. How much faster it really gets depends on how many
 * frames the console can run in one screen refresh.
 *
 * START (a new press, in the stable field or a battle's stable core) cycles OFF -> 2x -> 3x -> 4x -> OFF. The
 * choice is remembered; it is applied only while one of them is the screen's owner, and the game runs at
 * 1x everywhere else (menus, the Bag, the Party and the Summary opened from a battle, loads, fades, battle
 * transitions). The ZL/ZR fast-forward is gone. */
enum {
    CTR_TURBO_OFF = 0,
    CTR_TURBO_MIN = 2,
    CTR_TURBO_MAX = 4,
};

typedef struct {
    unsigned turbo;  /* the chosen multiplier: 0 (off) or 2..4 */
    bool applies;    /* the stable field is up now */
    unsigned phase;  /* frames since the last shown one */
    bool unseen;     /* nothing on either screen changes now: a menu changing over behind a held picture */
} CtrFastForward;

/* While a menu changes over (the Party closing for the Bag, a battle's Bag or Party setting up) both screens keep
 * their last picture, so the game runs this many frames for each one shown: the fades and setup frames nobody sees
 * pass at this speed, and the sound keeps its pace as in the turbo. */
#define CTR_UNSEEN_SPEED 4

static inline unsigned CtrFastForward_Speed(const CtrFastForward *ff)
{
    unsigned speed = ff->applies && ff->turbo >= CTR_TURBO_MIN && ff->turbo <= CTR_TURBO_MAX ? ff->turbo : 1;

    return ff->unseen && speed < CTR_UNSEEN_SPEED ? CTR_UNSEEN_SPEED : speed;
}

/* Once per game frame: a START press (only when `canCycle`, the stable field with nothing said or
 * scripted) takes the next step; `applies` says whether the multiplier is in force this frame. Any change
 * of the speed in force shows its first frame at once. */
static inline void CtrFastForward_Turbo(CtrFastForward *ff, bool press, bool canCycle, bool applies, bool unseen)
{
    unsigned before = CtrFastForward_Speed(ff);

    if (press && canCycle)
        ff->turbo = ff->turbo < CTR_TURBO_MIN ? CTR_TURBO_MIN : ff->turbo >= CTR_TURBO_MAX ? CTR_TURBO_OFF : ff->turbo + 1;
    ff->applies = applies;
    ff->unseen = unseen;
    if (CtrFastForward_Speed(ff) != before)
        ff->phase = 0;
}

/* Once per game frame: is this one shown (presented, with sound)? The first
 * frame after any change is shown, so turning it off never skips one. */
static inline bool CtrFastForward_Show(CtrFastForward *ff)
{
    unsigned speed = CtrFastForward_Speed(ff);
    bool show = ff->phase == 0;

    ff->phase = speed > 1 ? (ff->phase + 1) % speed : 0;
    return show;
}

#endif
