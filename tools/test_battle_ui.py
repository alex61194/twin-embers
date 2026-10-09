"""The battle screen wears PokeTouch's red without moving anything.

Draws every state of the controller through the production renderer with synthetic bridges
(3ds_port/tests/battle_ui_render_test.c): incremental updates equal full redraws, no colour of
the shared navy theme remains, and, against the renderer before the restyle (BASE), the buttons'
hit areas and the silhouette of every state (which pixels are not the backdrop) are identical,
so sizes, positions and touch behaviour are unchanged. The baseline comparison needs BASE in
the local Git history and is skipped, after the other checks, in a checkout without it.
"""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = '587c748'   # battle, summary and theme renderer as it was before the restyle
FILES = ('3ds_port/src/3ds_poketouch.c', '3ds_port/src/3ds_battle_ui_impl.h', '3ds_port/src/3ds_summary_ui_impl.h',
         '3ds_port/src/3ds_ui_theme_impl.h',
         '3ds_port/include/3ds_ui_theme.h')


def cc():
    return os.environ.get('CC') or shutil.which('gcc') or 'gcc'


def build(out, extra, includes, source=None):
    cmd = [cc(), '-std=c11', '-O1', '-Wall', '-Wextra', '-Werror'] + extra
    for inc in includes:
        cmd.append('-I' + str(inc))
    if source:
        cmd.append('-DPT_SOURCE="' + str(source).replace('\\', '/') + '"')
    cmd += [str(ROOT / '3ds_port/tests/battle_ui_render_test.c'), '-o', str(out), '-lm']
    subprocess.run(cmd, check=True)


class BattleUi(unittest.TestCase):
    def test_red_restyle_keeps_geometry(self):
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
            known = subprocess.run(['git', '-C', str(ROOT), 'cat-file', '-t', BASE],
                                   capture_output=True, text=True).stdout.strip() == 'commit'
            if not known:
                self.skipTest(f'baseline {BASE} is not in the history of this checkout')
            base = tmp / 'base'
            base.mkdir()
            for path in FILES:
                raw = subprocess.check_output(['git', '-C', str(ROOT), 'show', BASE + ':' + path])
                (base / Path(path).name).write_bytes(raw)
            old = tmp / ('battle-ui-base.exe' if os.name == 'nt' else 'battle-ui-base')
            build(old, ['-DBATTLE_UI_BEFORE'], [base, ROOT / '3ds_port/include', ROOT / '3ds_port/src'],
                  base / '3ds_poketouch.c')
            before = subprocess.run([str(old), 'print'], check=True, capture_output=True, text=True).stdout
            pick = lambda text, key: [l for l in text.splitlines() if l.startswith(key)]
            self.assertTrue(pick(now, 'hits') and pick(now, 'mask'))
            self.assertEqual(pick(now, 'hits'), pick(before, 'hits'), 'a button answers to different points')
            self.assertEqual(pick(now, 'mask'), pick(before, 'mask'), 'a state\'s silhouette moved')


if __name__ == '__main__':
    unittest.main()
