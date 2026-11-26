"""Recipe 0279: every naming screen holds the upper screen from the moment it is allocated until it is freed, so that the upper screen
keeps the frame it had (a battle, the field, a dialogue) and never shows FireRed's own keyboard, loading, up or fading out. Checked by the text
of the lines the recipe adds and by the lower screen's use of it."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ROOT / 'patches/pokefirered/0279-naming-screen-holds-the-top.json'


class NamingHoldTest(unittest.TestCase):
    def test_the_hold_lasts_as_long_as_the_screen(self):
        doc = json.loads(RECIPE.read_text(encoding='utf-8'))
        self.assertEqual([f['path'] for f in doc['files']], ['src/naming_screen.c'])
        added = ''.join(line['port'] for line in doc['files'][0]['lines'] if 'port' in line)
        self.assertIn('int CtrNaming_Hold(void)', added)
        self.assertIn('return sNamingScreen != NULL;', added)      # allocated in DoNamingScreen, freed when the screen exits

    def test_the_lower_screen_holds_the_top_for_it(self):
        text = (ROOT / '3ds_port/src/3ds_bottom.c').read_text(encoding='utf-8')
        self.assertIn('extern int CtrNaming_Hold(void);', text)
        self.assertIn('|| CtrOak_NamingHold() || CtrNaming_Hold();', text)
        self.assertLess(len(text.encode('utf-8')), 256 * 1024)       # the 256 KiB limit of a source file


if __name__ == '__main__':
    unittest.main()
