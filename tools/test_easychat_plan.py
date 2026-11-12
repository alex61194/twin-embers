"""Easy Chat's walks (3ds_bottom_easychat_impl.h) against a model of the game's own screen; synthetic data only."""
import os, subprocess, tempfile, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class EasyChatPlan(unittest.TestCase):
    def test_walks(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / ('plan.exe' if os.name == 'nt' else 'plan')
            subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-I' + str(ROOT / '3ds_port/include'), '-I' + str(ROOT / '3ds_port/src'),
                            str(ROOT / '3ds_port/tests/easychat_plan_test.c'), '-o', str(exe)], check=True)
            self.assertIn('PASS', subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout)


if __name__ == '__main__':
    unittest.main()
