"""The visible wilds' game glue (recipe 0249) is checked by its text: the host test runs the pure core, and this keeps the glue that
calls it thin and safe. Only the lines the recipe adds are read (the recipe stores the unchanged ones as spans)."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ROOT / 'patches/pokefirered/0249-visible-wilds.json'


def added(path):
    doc = json.loads(RECIPE.read_text(encoding='utf-8'))
    row = next(f for f in doc['files'] if f['path'] == path)
    return ''.join(line['port'] for line in row['lines'] if 'port' in line)


class WildsRecipeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.move = added('src/event_object_movement.c')

    def test_idle_path_is_the_cores(self):
        # The early out is the core's: it stops the system working itself (no Frame runs from there).
        self.assertIn('CtrWildsCore_Idle(&gCtrWilds, density) || CtrWildsCore_Unsupported(&gCtrWilds)', self.move)

    def test_wild_object_carries_the_mark(self):
        self.assertIn('gObjectEvents[id].trainerRange_berryTreeId = CTRW_MARK;', self.move)
        self.assertIn('_Static_assert(CTR_WILD_LOCALID == CTRW_LOCALID', self.move)

    def test_sweep_only_takes_marked_orphans(self):
        body = self.move[self.move.index('static void CtrWild_Sweep(bool8 drawn)'):]
        body = body[:body.index('static void CtrWild_NewMap')]
        self.assertIn('CtrWildsCore_IsOrphan(&gCtrWilds, i, objectEvent->localId', body)
        self.assertIn('objectEvent->graphicsId == OBJ_EVENT_GFX_BOY', body)
        self.assertIn('objectEvent->movementType == MOVEMENT_TYPE_NONE', body)
        self.assertIn('objectEvent->trainerRange_berryTreeId == CTRW_MARK', body)
        # No sprite yet when the field is rebuilt from a save: the object is only deactivated there.
        self.assertRegex(body, r'if \(drawn\)\s+RemoveObjectEvent\(objectEvent\);\s+else\s+objectEvent->active = FALSE;')

    def test_sweep_runs_on_a_new_map_and_a_loaded_field(self):
        self.assertIn('CtrWild_Sweep(TRUE);', self.move)
        self.assertEqual(self.move.count('CtrWild_Sweep(FALSE);'), 1)
        self.assertRegex(self.move, r'#ifdef PLATFORM_3DS\nstatic void CtrWild_Sweep\(bool8 drawn\);\n#endif')
        self.assertRegex(self.move, r'#ifdef PLATFORM_3DS\n\s*CtrWild_Sweep\(FALSE\);[^\n]*\n#endif')
        self.assertEqual(len(re.findall(r'CtrWild_Sweep\(', self.move)), 4)    # declaration, definition, two calls

    def test_no_unmarked_sweep_is_left(self):
        # The first version removed anything with a wild's local id: that could take a legitimate object event.
        self.assertNotIn('CTR_IS_WILD_LOCALID(gObjectEvents[i].localId)', self.move)

    def test_each_failed_attempt_says_why(self):
        body = self.move[self.move.index('static void CtrWild_Spawn('):]
        body = body[:body.index('static u8 CtrWild_DirTo')]
        for reason in ('CTRW_FAIL_NOCELL', 'CTRW_FAIL_ROLL', 'CTRW_FAIL_NOOBJ'):
            self.assertRegex(body, r'CtrWildsCore_NoteFail\(&gCtrWilds, ' + reason + r'\);\n\s+CtrWildsCore_SpawnDone\(&gCtrWilds, FALSE\);')
        self.assertEqual(body.count('CtrWildsCore_SpawnDone('), 4)      # three failures and the success

    def test_summary_is_logged_once_per_map_and_every_30_seconds(self):
        body = self.move[self.move.index('static void CtrWild_NewMap('):]
        body = body[:body.index('static void CtrWilds_Update(')]
        # the map that is left is summed up before the counters are reset, and only if it had attempts
        self.assertLess(body.index('CtrWild_LogStats()'), body.index('CtrWildsCore_MapChanged'))
        self.assertIn('gCtrWilds.stats.attempts != 0', body)
        self.assertIn('CtrWildsCore_StatsDue(&gCtrWilds)', self.move)
        # nothing is written per frame: the periodic check sits in the 30-frame tick
        tick = self.move[self.move.index('shed = CtrWildsCore_Tick('):]
        self.assertLess(tick.index('if (shed >= 0)'), tick.index('CtrWildsCore_StatsDue'))
        self.assertLess(tick.index('CtrWildsCore_StatsDue'), tick.index('CtrWildsCore_SpawnDue'))
        self.assertIn('extern void CtrWilds_Log(const char *line);', self.move)

    def test_log_line_buffer_is_local_not_static(self):
        body = self.move[self.move.index('static void CtrWild_LogStats(void)'):]
        body = body[:body.index('// A new map')]
        self.assertIn('char line[112];', body)
        self.assertEqual(body.count('static'), 1)

    def test_counters_follow_port_log(self):
        # The counters work only while port.log is on: the summary is not formatted (nor written) otherwise, and the log's state is
        # asked at a new map and in the 30-frame pass, never in the per-frame update.
        body = self.move[self.move.index('static void CtrWild_LogStats(void)'):]
        body = body[:body.index('// A new map')]
        self.assertLess(body.index('if (!gCtrWilds.statsOn)'), body.index('CtrWildsCore_StatsLine('))
        self.assertLess(body.index('CtrWildsCore_StatsLine('), body.index('CtrWilds_Log(line)'))
        new_map = self.move[self.move.index('static void CtrWild_NewMap('):]
        new_map = new_map[:new_map.index('static void CtrWilds_Update(')]
        self.assertIn('if (gCtrWilds.statsOn && gCtrWilds.stats.attempts != 0)', new_map)
        self.assertLess(new_map.index('CtrWildsCore_MapChanged'), new_map.index('CtrWildsCore_SetStats(&gCtrWilds, CtrWilds_LogOn())'))
        refresh = self.move[self.move.index('static void CtrWild_Refresh('):]
        refresh = refresh[:refresh.index('static void CtrWild_Spawn(')]
        self.assertEqual(refresh.count('CtrWildsCore_SetStats(&gCtrWilds, CtrWilds_LogOn())'), 1)
        self.assertEqual(len(re.findall(r'CtrWilds_LogOn\(\)', self.move)), 2)      # the new map and the pass: no other place asks
        update = self.move[self.move.index('static void CtrWilds_Update('):]
        update = update[:update.index('static bool8 CtrWild_Fight(')]
        self.assertNotIn('CtrWilds_LogOn', update)
        self.assertIn('extern int CtrWilds_LogOn(void);', self.move)

    def test_the_charge_is_decided_by_the_core_and_kept_until_it_can_be_used(self):
        # Before: sCtrWildCharge was cleared first and the player's step checked afterwards, so an encounter that arrived during a step was
        # thrown away (and its wild left in ENCOUNTER until its timer ended). Now the core keeps it and decides, from what the game sees.
        self.assertNotIn('sCtrWildCharge', self.move)
        self.assertIn('CtrWildsCore_Arm(&gCtrWilds, slot);', self.move)
        body = self.move[self.move.index('bool8 CtrWilds_TryCharge(void)'):]
        body = body[:body.index('// For the compositor')]
        self.assertIn('in.moving = gPlayerAvatar.tileTransitionState != T_NOT_MOVING;', body)
        self.assertIn('in.locked = ArePlayerFieldControlsLocked();', body)
        self.assertIn('AreElevationsCompatible(', body)
        self.assertIn('CtrWildsCore_Charge(&gCtrWilds, &in) != CTRW_CH_FIGHT', body)
        self.assertEqual(body.count('tileTransitionState'), 1)                  # only as an input to the decision, never as a way out
        self.assertLess(body.index('CtrWildsCore_Charge('), body.index('CtrWild_Fight(slot)'))
        self.assertIn('CtrWildsCore_Calm(&gCtrWilds, slot, 900);', body)           # a Repel or disabled encounters still calm it down

    def test_a_script_or_a_battle_ends_the_charge(self):
        update = self.move[self.move.index('static void CtrWilds_Update('):]
        update = update[:update.index('static bool8 CtrWild_Fight(')]
        self.assertRegex(update, r'if \(ArePlayerFieldControlsLocked\(\)\)\s+CtrWildsCore_Interrupt\(&gCtrWilds\);')
        fight = self.move[self.move.index('static bool8 CtrWild_Fight('):]
        fight = fight[:fight.index('// The player walked into the tile')]
        self.assertLess(fight.index('CtrWilds_Start('), fight.index('CtrWildsCore_BattleStarted(&gCtrWilds);'))
        self.assertLess(fight.index('CtrWildsCore_BattleStarted(&gCtrWilds);'), fight.index('CtrWild_Remove(slot);'))

    def test_the_wander_step_tells_the_core_which_way_it_went(self):
        run = self.move[self.move.index('static void CtrWild_Run('):]
        run = run[:run.index('static void CtrWild_Sweep')]
        self.assertIn('CtrWildsCore_StepTaken(&gCtrWilds, slot, act.dir[i]);', run)
        self.assertLess(run.index('GetWalkNormalMovementAction(act.dir[i])'), run.index('CtrWildsCore_StepTaken('))


if __name__ == '__main__':
    unittest.main()
