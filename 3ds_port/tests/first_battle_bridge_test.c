/* Synthetic callbacks exercise the exact bridge included by the controller. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include "3ds_battle_nav.h"
/* PORT NAVIGATION FUNCTION */
#define BATTLE_TYPE_FIRST_BATTLE 1u
#define MAX_BATTLERS_COUNT 4
enum { CTR_BATTLE_BUSY, CTR_BATTLE_ACTION, CTR_BATTLE_MOVE };
typedef void (*Callback)(void);
static struct { Callback callback2; } gMain;
static unsigned gBattleTypeFlags;
static int gBattlersCount;
static Callback gBattlerControllerFuncs[4];
static void BattleMainCB2(void) {}
static void OtherScreen(void) {}
static void HandleInputChooseAction(void) {}
static void OakOldManHandleInputChooseMove(void) {}
static void OpenBagAndChooseItem(void) {}
static void OpenPartyMenuToChooseMon(void) {}
static void SimulateInputChooseAction(void) {}
#include "3ds_first_battle_bridge.h"

int main(void)
{
    int state = CTR_BATTLE_BUSY;
    gMain.callback2 = BattleMainCB2;
    gBattleTypeFlags = BATTLE_TYPE_FIRST_BATTLE;
    gBattlersCount = 2;
    gBattlerControllerFuncs[0] = SimulateInputChooseAction;
    gBattlerControllerFuncs[1] = HandleInputChooseAction;
    assert(CtrBattleFirst_Chooser(&state) == 1 && state == CTR_BATTLE_ACTION);
    gBattlerControllerFuncs[1] = OakOldManHandleInputChooseMove;
    assert(CtrBattleFirst_Chooser(&state) == 1 && state == CTR_BATTLE_MOVE);
    gBattlerControllerFuncs[1] = OpenBagAndChooseItem;
    assert(CtrBattleFirst_Chooser(&state) == -1 && CtrBattleFirst_BagPending());
    assert(!CtrBattleFirst_PartyPending());
    gBattlerControllerFuncs[1] = OpenPartyMenuToChooseMon;
    assert(CtrBattleFirst_PartyPending() && !CtrBattleFirst_BagPending());
    /* A rebuilt Bag/Party screen, ordinary battle and autonomous tutorial
     * must never be reported as interactive first-battle input. */
    gMain.callback2 = OtherScreen;
    assert(CtrBattleFirst_Chooser(&state) == -1 && !CtrBattleFirst_PartyPending());
    gMain.callback2 = BattleMainCB2;
    gBattleTypeFlags = 0;
    gBattlerControllerFuncs[1] = HandleInputChooseAction;
    assert(CtrBattleFirst_Chooser(&state) == -1);
    gBattleTypeFlags = BATTLE_TYPE_FIRST_BATTLE;
    gBattlerControllerFuncs[1] = SimulateInputChooseAction;
    assert(CtrBattleFirst_Chooser(&state) == -1);
    gBattlersCount = 0;
    assert(CtrBattleFirst_Chooser(&state) == -1);
    /* Every touch destination follows the same graph as the physical keys. */
    for (int from = 0; from < 4; ++from)
        for (int to = 0; to < 4; ++to)
        {
            int cursor = from, steps = 0;
            while (cursor != to && steps++ < 4)
                cursor = CtrBattleAction_Next(cursor, CtrBattleActionDir(cursor, to));
            assert(cursor == to);
        }
    puts("First rival controller: action/move readiness, Bag/Party holds, demo exclusion and all touch routes PASS");
    return 0;
}
