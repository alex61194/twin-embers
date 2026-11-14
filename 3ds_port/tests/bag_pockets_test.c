/* The Bag's pocket switching (3ds_bag_pockets.h, recipe 0261) against FireRed's own rule, and the battle Bag's: the shoulder buttons
 * change no pocket there (issue #16), the D-pad still does (the port's router walks to the Berry Pouch with it). */
#include <stdio.h>

#include "3ds_bag_pockets.h"

static int failures, checks;
#define CHECK(c, ...) do { ++checks; if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

/* FireRed's ProcessPocketSwitchInput as it is in the decomp (item_menu.c), written out: the model the header must equal in the field. */
static int GameSwitch(int pocket, int dpadLeft, int dpadRight, int l, int r)
{
    if (dpadLeft || l)
    {
        if (pocket == 0)
            return 0;
        return 1;
    }
    if (dpadRight || r)
    {
        if (pocket >= 2)
            return 0;
        return 2;
    }
    return 0;
}

int main(void)
{
    /* the field: the same as the game, for every pocket and every key combination */
    for (int pocket = 0; pocket < CTR_BAGP_COUNT; ++pocket)
        for (int keys = 0; keys < 16; ++keys)
        {
            int dl = keys & 1, dr = (keys >> 1) & 1, l = (keys >> 2) & 1, r = (keys >> 3) & 1;

            CHECK(CtrBagPockets_Switch(pocket, 0, dl, dr, l, r) == GameSwitch(pocket, dl, dr, l, r), "field: pocket %d keys %d", pocket, keys);
        }

    /* the order and the ends: Items, Key Items, Poke Balls; no wrap-around */
    CHECK(CTR_BAGP_ITEMS == 0 && CTR_BAGP_KEY_ITEMS == 1 && CTR_BAGP_POKE_BALLS == 2 && CTR_BAGP_COUNT == 3, "pocket order");
    CHECK(CtrBagPockets_Switch(0, 0, 0, 0, 0, 1) == CTR_BAGP_NEXT && CtrBagPockets_After(0, CTR_BAGP_NEXT) == 1, "R: Items -> Key Items");
    CHECK(CtrBagPockets_Switch(1, 0, 0, 0, 0, 1) == CTR_BAGP_NEXT && CtrBagPockets_After(1, CTR_BAGP_NEXT) == 2, "R: Key Items -> Poke Balls");
    CHECK(CtrBagPockets_Switch(2, 0, 0, 0, 0, 1) == CTR_BAGP_NONE && CtrBagPockets_After(2, CTR_BAGP_NONE) == 2, "R on the last: nothing");
    CHECK(CtrBagPockets_Switch(2, 0, 0, 0, 1, 0) == CTR_BAGP_PREV && CtrBagPockets_After(2, CTR_BAGP_PREV) == 1, "L: Poke Balls -> Key Items");
    CHECK(CtrBagPockets_Switch(1, 0, 0, 0, 1, 0) == CTR_BAGP_PREV && CtrBagPockets_After(1, CTR_BAGP_PREV) == 0, "L: Key Items -> Items");
    CHECK(CtrBagPockets_Switch(0, 0, 0, 0, 1, 0) == CTR_BAGP_NONE, "L on the first: nothing");
    CHECK(CtrBagPockets_Switch(1, 0, 1, 0, 0, 0) == CTR_BAGP_PREV && CtrBagPockets_Switch(1, 0, 0, 1, 0, 0) == CTR_BAGP_NEXT, "the D-pad walks the same way");
    CHECK(CtrBagPockets_Switch(1, 0, 1, 1, 0, 0) == CTR_BAGP_PREV, "both at once: LEFT first, as the game tests it");
    CHECK(CtrBagPockets_Switch(1, 0, 0, 0, 0, 0) == CTR_BAGP_NONE, "no key: nothing");

    /* walking the whole Bag with R and back with L never leaves 0..2 and never wraps */
    {
        int pocket = 0;

        for (int i = 0; i < 10; ++i)
            pocket = CtrBagPockets_After(pocket, CtrBagPockets_Switch(pocket, 0, 0, 0, 0, 1));
        CHECK(pocket == 2, "ten R presses end on the last pocket (%d)", pocket);
        for (int i = 0; i < 10; ++i)
            pocket = CtrBagPockets_After(pocket, CtrBagPockets_Switch(pocket, 0, 0, 0, 1, 0));
        CHECK(pocket == 0, "ten L presses end on the first pocket (%d)", pocket);
    }

    /* the battle Bag: L and R do nothing in any pocket, the D-pad (the router's) works as in the field */
    for (int pocket = 0; pocket < CTR_BAGP_COUNT; ++pocket)
    {
        CHECK(CtrBagPockets_Switch(pocket, 1, 0, 0, 1, 0) == CTR_BAGP_NONE, "battle: L changes no pocket (from %d)", pocket);
        CHECK(CtrBagPockets_Switch(pocket, 1, 0, 0, 0, 1) == CTR_BAGP_NONE, "battle: R changes no pocket (from %d)", pocket);
        CHECK(CtrBagPockets_Switch(pocket, 1, 0, 0, 1, 1) == CTR_BAGP_NONE, "battle: L and R together change none (from %d)", pocket);
        CHECK(CtrBagPockets_Switch(pocket, 1, 1, 0, 0, 0) == GameSwitch(pocket, 1, 0, 0, 0), "battle: LEFT is the router's (from %d)", pocket);
        CHECK(CtrBagPockets_Switch(pocket, 1, 0, 1, 0, 0) == GameSwitch(pocket, 0, 1, 0, 0), "battle: RIGHT is the router's (from %d)", pocket);
        CHECK(CtrBagPockets_Switch(pocket, 1, 1, 0, 0, 1) == GameSwitch(pocket, 1, 0, 0, 0), "battle: LEFT with R held: only LEFT counts (from %d)", pocket);
    }
    /* the router's BERRIES walk: Items -> Key Items with RIGHT, and back to Items with LEFT, with the shoulder buttons held or not */
    {
        int pocket = 0;

        pocket = CtrBagPockets_After(pocket, CtrBagPockets_Switch(pocket, 1, 0, 1, 0, 0));
        CHECK(pocket == 1, "battle: RIGHT reaches the Key Items pocket (the Berry Pouch)");
        pocket = CtrBagPockets_After(pocket, CtrBagPockets_Switch(pocket, 1, 1, 0, 1, 0));
        CHECK(pocket == 0, "battle: LEFT comes back to Items");
    }

    printf("bag_pockets_test: %d checks, %d failures\n", checks, failures);
    return failures != 0;
}
