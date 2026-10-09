"""Save options halfword layout (3ds_port/tests/save_option_bits_test.c).

With --tree DIR (a workspace made by tools/bootstrap.py): SaveBlock2 keeps FireRed's size and
every option keeps its bits; the two spare bits (13 and 14) are reserved, read by nothing, and
survive option writes, so a save holding any value there loads with its other options unchanged.
Also checks that no code outside the declaration reads the reserved bits.
"""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


class SaveOptionBits(unittest.TestCase):
    def tree(self):
        if not os.environ.get('SAVE_BITS_TREE'):
            self.skipTest('needs --tree (a bootstrapped, patched workspace)')
        return Path(os.environ['SAVE_BITS_TREE']).resolve()

    def test_layout_and_old_values(self):
        tree = self.tree()
        cc = os.environ.get('CC') or shutil.which('gcc') or 'gcc'
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / ('save-bits.exe' if os.name == 'nt' else 'save-bits')
            subprocess.run([cc, '-std=gnu11', '-O2', '-Wall', '-Werror', '-DPLATFORM_3DS', '-DMODERN=1',
                            '-DFIRERED', '-DENGLISH', '-DREVISION=0', '-iquote', str(ROOT / '3ds_port/include'),
                            '-iquote', str(tree / 'include'), str(ROOT / '3ds_port/tests/save_option_bits_test.c'),
                            '-o', str(exe)], check=True)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout)
            self.assertIn('PASS save option bits', run.stdout)

    def test_reserved_bits_unused(self):
        tree = self.tree()
        for folder in ('src', 'include'):
            for path in (tree / folder).rglob('*.[ch]'):
                text = path.read_text(errors='replace')
                if path.name != 'global.h':
                    self.assertFalse('optionsReserved3ds' in text, f'{path}: reads the reserved bits')
        for path in (ROOT / '3ds_port').rglob('*.[ch]'):
            if path.name == 'save_option_bits_test.c':
                continue
            text = path.read_text(errors='replace')
            self.assertFalse('optionsReserved3ds' in text, f'{path}: reads the reserved bits')


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tree', type=Path)
    args, rest = parser.parse_known_args()
    if args.tree:
        os.environ['SAVE_BITS_TREE'] = str(args.tree)
    unittest.main(argv=[sys.argv[0]] + rest)
