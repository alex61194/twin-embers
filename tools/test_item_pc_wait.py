"""The Item PC setup loop yields to the native VBlank while a graphics copy is queued (patch 0119), no emulator."""
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
LOOP = '''static void ItemPc_RunSetup(void)
{
    while (1)
    {
        if (ItemPc_DoGfxSetup() == TRUE)
            break;
        if (MenuHelpers_IsLinkActive() == TRUE)
            break;
%s    }
}
'''


class ItemPcWait(unittest.TestCase):
    def production_yield(self):
        recipe = json.loads((ROOT/'patches/pokefirered/0119-item-pc-native-dma-wait.json').read_text(encoding='utf-8'))
        lines = recipe['files'][0]['lines']
        runs, run = [], ''
        for op in lines:
            if 'port' in op:
                run += op['port']
            elif run:
                runs.append(run)
                run = ''
        if run:
            runs.append(run)
        yields = [r for r in runs if 'WaitDma3Request(-1) == -1' in r]
        self.assertEqual(len(yields), 1, 'the recipe holds one yield')
        return yields[0]

    def test_actual_yield_returns_and_resumes(self):
        yielding = self.production_yield()
        self.assertIn('#ifdef PLATFORM_3DS', yielding)
        harness = (ROOT/'3ds_port/tests/item_pc_wait_test.c').read_text(encoding='utf-8')
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            c = folder/'test.c'
            c.write_text(harness.replace('/* PRODUCTION LOOP */', LOOP % yielding), encoding='utf-8')
            for platform in ('native', 'gba'):
                exe = folder/(platform + ('.exe' if os.name == 'nt' else ''))
                command = [os.environ.get('CC', 'gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror']
                if platform == 'native':
                    command += ['-DPLATFORM_3DS']
                subprocess.run(command + [str(c), '-o', str(exe)], check=True)
                subprocess.run([str(exe)], check=True)


if __name__ == '__main__':
    unittest.main()
