#ifndef CTR_BAG_POCKETS_H
#define CTR_BAG_POCKETS_H

/* How the Bag's pockets are switched, in ONE place (recipe 0261, ProcessPocketSwitchInput in item_menu.c; 3ds_port/tests/bag_pockets_test.c).
 *
 * FireRed's Bag has three pockets, walked in this order by LEFT / RIGHT and by the shoulder buttons L / R (when the button mode is
 * L/R): ITEMS (0), KEY ITEMS (1), POKE BALLS (2). The ends do not wrap: LEFT on ITEMS and RIGHT on POKE BALLS do nothing. The field
 * Bag's tabs (3ds_field_bag_impl.h) are drawn in that same order and a tap on one walks to it with the D-pad.
 *
 * The battle Bag is the ITEMS pocket only: the balls have their own button on the action screen, the Berry Pouch its BERRIES, and the
 * port's router walks to the KEY ITEMS pocket by itself with the D-pad when BERRIES is used (3ds_bottom.c). The lower screen shows no
 * tabs there, so a shoulder button must not change the pocket: it used to, under the title "Items" (KEY ITEMS, then POKE BALLS, in the
 * game's order), and the router then walked back to ITEMS (issue #16). The D-pad stays the router's.
 *
 * Header-only, no tables and no switches: the game's objects include it (clean-release audit). */

enum { CTR_BAGP_ITEMS = 0, CTR_BAGP_KEY_ITEMS = 1, CTR_BAGP_POKE_BALLS = 2, CTR_BAGP_COUNT = 3 };
/* The game's own return values of ProcessPocketSwitchInput. */
enum { CTR_BAGP_NONE = 0, CTR_BAGP_PREV = 1, CTR_BAGP_NEXT = 2 };

/* The pocket after a switch result. */
static inline int CtrBagPockets_After(int pocket, int result)
{
    return result == CTR_BAGP_PREV ? pocket - 1 : result == CTR_BAGP_NEXT ? pocket + 1 : pocket;
}

/* What a frame's keys do to the pockets: CTR_BAGP_PREV, _NEXT or _NONE. pocket is the pocket now shown (0 to 2). battle: the Bag is the
 * battle's, where the shoulder buttons are ignored. LEFT wins over RIGHT when both are new, as in the game's own order of tests. */
static inline int CtrBagPockets_Switch(int pocket, int battle, int dpadLeft, int dpadRight, int lPressed, int rPressed)
{
    if (battle)
    {
        lPressed = 0;
        rPressed = 0;
    }
    if (dpadLeft || lPressed)
        return pocket <= CTR_BAGP_ITEMS ? CTR_BAGP_NONE : CTR_BAGP_PREV;
    if (dpadRight || rPressed)
        return pocket >= CTR_BAGP_POKE_BALLS ? CTR_BAGP_NONE : CTR_BAGP_NEXT;
    return CTR_BAGP_NONE;
}

#endif
