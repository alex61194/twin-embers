"""Map-edge fallback (3ds_port/include/3ds_map_edge.h).

Without arguments: the rule on synthetic maps. With --tree DIR (a workspace made by
tools/bootstrap.py, patches applied): the production fieldmap.c and field_camera.c are
compiled on the host against the pinned upstream map data (3ds_port/tests/
map_edge_draw_test.c); nothing derived from the game is stored in the repository.
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
sys.path.insert(0, str(Path(__file__).resolve().parent))
TREE = None


def compiler(cc=None):
    return cc or os.environ.get('CC') or shutil.which('gcc') or 'gcc'


class MapEdgeRule(unittest.TestCase):
    def test_rule_on_synthetic_maps(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / ('map-edge-test.exe' if os.name == 'nt' else 'map-edge-test')
            subprocess.run([compiler(), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-I' + str(ROOT / '3ds_port/include'), str(ROOT / '3ds_port/tests/map_edge_test.c'),
                            '-o', str(exe)], check=True)
            out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
            self.assertIn('PASS map edge', out)


class MapEdgeProduction(unittest.TestCase):
    def test_production_camera_on_every_outdoor_map(self):
        if not os.environ.get('MAP_EDGE_TREE'):
            self.skipTest('needs --tree (a bootstrapped, patched workspace)')
        import map_edge_world
        tree = Path(os.environ['MAP_EDGE_TREE']).resolve()
        self.assertIn('GetEdgeExtensionMetatile', (tree / 'src/field_camera.c').read_text(),
                      'the workspace does not carry patch 0111')
        with tempfile.TemporaryDirectory() as tmp:
            tmp = Path(tmp)
            world = tmp / 'world.txt'
            world.write_text(map_edge_world.build(tree))
            exe = tmp / ('map-edge-draw.exe' if os.name == 'nt' else 'map-edge-draw')
            subprocess.run([compiler(), '-std=gnu11', '-O2', '-w', '-DPLATFORM_3DS', '-DMODERN=1', '-DFIRERED',
                            '-DENGLISH', '-DREVISION=0', '-iquote', str(ROOT / '3ds_port/include'),
                            '-iquote', str(tree / 'include'), '-iquote', str(tree / 'src'),
                            str(ROOT / '3ds_port/tests/map_edge_draw_test.c'), '-o', str(exe)], check=True)
            run = subprocess.run([str(exe), str(tree), str(world)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout[-4000:] + run.stderr[-2000:])
            self.assertIn('PASS map edge draw', run.stdout)
            print(run.stdout)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tree', type=Path)
    args, rest = parser.parse_known_args()
    if args.tree:
        os.environ['MAP_EDGE_TREE'] = str(args.tree)
    unittest.main(argv=[sys.argv[0]] + rest)
