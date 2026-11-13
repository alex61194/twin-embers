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


if __name__ == '__main__':
    unittest.main()
