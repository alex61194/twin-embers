"""The battle screen wears the "Llama" look (the owner's mockup, llama2_mockup.html).

Draws every state of the controller through the production renderer with synthetic bridges
(3ds_port/tests/battle_ui_render_test.c): incremental updates equal full redraws, no colour of
the shared navy theme remains, the touch areas of the new layout answer where the cards are drawn,
and the fallback icons (no pack) move nothing. Up to the Ascuas restyle this test also compared the
buttons' hit areas and every state's silhouette with the renderer before it (BASE); the Llama look
moves the move cards, the targets, CANCEL and the Party's cards on purpose (the mockup draws them
elsewhere and at other sizes), so that comparison is gone and the new rectangles are asserted in the
C test instead.
"""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def cc():
    return os.environ.get('CC') or shutil.which('gcc') or 'gcc'


def build(out, extra, includes):
    cmd = [cc(), '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror'] + extra
    for inc in includes:
        cmd.append('-I' + str(inc))
    cmd += [str(ROOT / '3ds_port/tests/battle_ui_render_test.c'), '-o', str(out), '-lm']
    subprocess.run(cmd, check=True)


class BattleUi(unittest.TestCase):
    def test_llama_states_draw_and_update(self):
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            exe = tmp / ('battle-ui.exe' if os.name == 'nt' else 'battle-ui')
            build(exe, [], [ROOT / '3ds_port/include', ROOT / '3ds_port/src'])
            now = subprocess.run([str(exe), 'print'], check=True, capture_output=True, text=True).stdout
            self.assertIn('PASS battle ui', now)
            # No pack: the 12x12 menu samples stand in for the full-size icons; the same geometry either way.
            bare = subprocess.run([str(exe), 'print'], check=True, capture_output=True, text=True,
                                  env=dict(os.environ, BATTLE_UI_NO_PACK='1')).stdout
            self.assertIn('PASS battle ui', bare)
            self.assertEqual(now, bare, 'the fallback icons moved something')

    def test_move_cards_draw_the_matchup_tags(self):
        """x2 and x4 green, x1/2 orange, x0 red, a neutral hit nothing; incremental redraws equal full ones."""
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            exe = tmp / ('battle-ui.exe' if os.name == 'nt' else 'battle-ui')
            build(exe, [], [ROOT / '3ds_port/include', ROOT / '3ds_port/src'])
            run = subprocess.run([str(exe)], capture_output=True, text=True, env=dict(os.environ, BATTLE_UI_MATCHUPS='1'))
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('PASS battle ui', run.stdout)


if __name__ == '__main__':
    unittest.main()
