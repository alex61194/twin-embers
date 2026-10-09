/* CtrFastForward: the START-controlled overworld turbo, N game frames per shown frame. */
#include <assert.h>
#include <stdio.h>

#include "3ds_fast_forward.h"

static unsigned Shown(CtrFastForward *ff, unsigned frames)
{
    unsigned shown = 0;
    for (unsigned i = 0; i < frames; ++i)
        shown += CtrFastForward_Show(ff);
    return shown;
}

static void Frame(CtrFastForward *ff, bool press, bool canCycle, bool applies)
{
    CtrFastForward_Turbo(ff, press, canCycle, applies);
}

int main(void)
{
    CtrFastForward ff = {0};

    /* Starts OFF: every frame is shown. */
    Frame(&ff, false, true, true);
    assert(CtrFastForward_Speed(&ff) == 1 && Shown(&ff, 60) == 60);

    /* START cycles OFF -> 2x -> 3x -> 4x -> OFF, one step per press. */
    Frame(&ff, true, true, true); assert(ff.turbo == 2 && CtrFastForward_Speed(&ff) == 2);
    assert(CtrFastForward_Show(&ff) && !CtrFastForward_Show(&ff) && CtrFastForward_Show(&ff));      /* the first frame at once */
    ff.phase = 0; assert(Shown(&ff, 60) == 30);
    Frame(&ff, true, true, true); assert(ff.turbo == 3 && CtrFastForward_Speed(&ff) == 3 && Shown(&ff, 60) == 20);
    Frame(&ff, true, true, true); assert(ff.turbo == 4 && CtrFastForward_Speed(&ff) == 4 && Shown(&ff, 60) == 15);
    Frame(&ff, true, true, true); assert(ff.turbo == 0 && CtrFastForward_Speed(&ff) == 1 && Shown(&ff, 10) == 10);
    /* No press, no step (a held START is not a new press: the caller passes edges only). */
    for (int i = 0; i < 10; ++i) Frame(&ff, false, true, true);
    assert(ff.turbo == 0);

    /* Not where it may cycle (a menu, a battle, a message): a press does nothing. */
    Frame(&ff, true, false, false); assert(ff.turbo == 0);
    Frame(&ff, true, false, true); assert(ff.turbo == 0);

    /* The choice is remembered but applied only in the stable field: 3x, a menu opens, the game runs at 1x,
     * the field comes back and 3x resumes, with its first frame shown at once. */
    Frame(&ff, true, true, true); Frame(&ff, true, true, true); assert(ff.turbo == 3 && CtrFastForward_Speed(&ff) == 3);
    Frame(&ff, false, false, false); assert(ff.turbo == 3 && CtrFastForward_Speed(&ff) == 1 && Shown(&ff, 30) == 30);
    Frame(&ff, true, false, false); assert(ff.turbo == 3);                                            /* START in the menu: nothing */
    Frame(&ff, false, true, true); assert(CtrFastForward_Speed(&ff) == 3);
    assert(CtrFastForward_Show(&ff) && Shown(&ff, 59) == 19);
    Frame(&ff, false, true, false); assert(CtrFastForward_Speed(&ff) == 1 && CtrFastForward_Show(&ff));   /* a battle: 1x at once */

    /* A press and the field leaving on the same frame: the step is taken, and not applied. */
    Frame(&ff, true, true, false); assert(ff.turbo == 4 && CtrFastForward_Speed(&ff) == 1);
    puts("PASS turbo: starts off, START cycles 2x/3x/4x/off one step a press, remembered through menus, applied only in the stable field, first frame shown at once");
    return 0;
}
