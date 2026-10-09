"""Production first-battle bridge and scripted portrait geometry, no game assets."""
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def additions(recipe, path):
    doc = json.loads((ROOT/'patches/pokefirered'/recipe).read_text())
    row = next(row for row in doc['files'] if row['path'] == path)
    return ''.join(op['port'] for op in row['lines'] if 'port' in op)


class FirstBattle(unittest.TestCase):
    def compile_run(self, source):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            c = folder/'test.c'
            c.write_text(source)
            exe = folder/('test.exe' if os.name == 'nt' else 'test')
            subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-O2',
                            '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'3ds_port/include'),
                            str(c), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_actual_bridge_and_touch_graph(self):
        source = (ROOT/'3ds_port/src/3ds_bottom.c').read_text()
        start = source.index('int CtrBattleAction_Next(')
        end = source.index('\n}', start) + 2
        harness = (ROOT/'3ds_port/tests/first_battle_bridge_test.c').read_text()
        self.compile_run(harness.replace('/* PORT NAVIGATION FUNCTION */', source[start:end]))

    def test_first_controller_hooks(self):
        name = '0099-first-battle-bottom-menu.json'
        oak = additions(name, 'src/battle_controller_oak_old_man.c')
        player = additions(name, 'src/battle_controller_player.c')
        self.assertIn('#include "3ds_first_battle_bridge.h"', oak)
        self.assertEqual(oak.count('CtrBattle_HideUpperMenus();'), 2)
        self.assertIn('BATTLE_TYPE_FIRST_BATTLE) ? B_WIN_MSG : B_WIN_ACTION_PROMPT', oak)
        self.assertIn('BATTLE_TYPE_FIRST_BATTLE) ? 0 : 160', oak)
        self.assertIn('BATTLE_TYPE_FIRST_BATTLE) ? 0 : 320', oak)
        self.assertIn('CtrBattleAction_Next(cursor, dir)', oak)
        for call in ('CtrBattleFirst_Chooser(state)', 'CtrBattleFirst_BagPending()',
                     'CtrBattleFirst_PartyPending()'):
            self.assertIn(call, player)

    def test_production_portrait_statement_aligns_with_field_ui(self):
        source = additions('0100-field-picture-ui-origin.json', 'src/script_menu.c')
        start = source.index('    spriteId = CreateMonSprite_PicBox(')
        end = source.index(');', start) + 2
        statement = source[start:end]
        # This executes the actual port statement extracted from the recipe.
        # Synthetic sprite capture verifies both its 3DS and GBA coordinates.
        harness = '''
#include <assert.h>
#include "3ds_video.h"
enum { FALSE = 0, DISPLAY_WIDTH = 400, DISPLAY_HEIGHT = 240 };
static int gotX, gotY;
static int CreateMonSprite_PicBox(int species, int x, int y, int flag)
{ assert(species == 123 && !flag); gotX=x; gotY=y; return 7; }
static void picture(int x, int y)
{
    int species=123, spriteId;
STATEMENT
    assert(spriteId == 7);
    /* The window's 8x8 interior starts at (x+1,y+1) tiles plus BG0's origin. */
    int left=(x+1)*8+CTR_STAGE_X, top=(y+1)*8+CTR_GAME_HEIGHT-160;
    assert(gotX-32 == left && gotY-32 == top);
    assert(gotX+32 == left+64 && gotY+32 == top+64);
}
int main(void)
{ picture(10,3); picture(0,0); picture(20,9); return 0; }
'''
        self.compile_run(harness.replace('STATEMENT', statement))


if __name__ == '__main__':
    unittest.main()
