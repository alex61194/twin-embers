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


if __name__ == '__main__':
    unittest.main()
