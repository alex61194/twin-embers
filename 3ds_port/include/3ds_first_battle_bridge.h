#ifndef CTR_FIRST_BATTLE_BRIDGE_H
#define CTR_FIRST_BATTLE_BRIDGE_H
/* Original port bridge, included by the external Oak controller so its
 * private input handlers remain private. No battle rules are duplicated. */
/* Read the interactive tutorial controller without changing its decisions.
 * Exclude the autonomous Old Man demonstration, setup, fades and submenus. */
int CtrBattleFirst_Chooser(int *state)
{
    int i;
    if (!(gBattleTypeFlags & BATTLE_TYPE_FIRST_BATTLE) || gMain.callback2 != BattleMainCB2)
        return -1;
    for (i = 0; i < gBattlersCount && i < MAX_BATTLERS_COUNT; ++i)
    {
        if (gBattlerControllerFuncs[i] == HandleInputChooseAction)
        {
            *state = CTR_BATTLE_ACTION;
            return i;
        }
        if (gBattlerControllerFuncs[i] == OakOldManHandleInputChooseMove)
        {
            *state = CTR_BATTLE_MOVE;
            return i;
        }
    }
    return -1;
}

int CtrBattleFirst_BagPending(void)
{
    int i;
    if (!(gBattleTypeFlags & BATTLE_TYPE_FIRST_BATTLE) || gMain.callback2 != BattleMainCB2)
        return 0;
    for (i = 0; i < gBattlersCount && i < MAX_BATTLERS_COUNT; ++i)
        if (gBattlerControllerFuncs[i] == OpenBagAndChooseItem)
            return 1;
    return 0;
}

int CtrBattleFirst_PartyPending(void)
{
    int i;
    if (!(gBattleTypeFlags & BATTLE_TYPE_FIRST_BATTLE) || gMain.callback2 != BattleMainCB2)
        return 0;
    for (i = 0; i < gBattlersCount && i < MAX_BATTLERS_COUNT; ++i)
        if (gBattlerControllerFuncs[i] == OpenPartyMenuToChooseMon)
            return 1;
    return 0;
}
#endif
