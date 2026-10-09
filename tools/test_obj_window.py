"""Compile and run the sprite-window host test (3ds_port/tests/obj_window_test.c)."""
import os, subprocess, tempfile, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class SpriteWindow(unittest.TestCase):
    def test_native_field_sprite_window(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / ('obj-window-test.exe' if os.name == 'nt' else 'obj-window-test')
            subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                            '-I' + str(ROOT / '3ds_port/include'), str(ROOT / '3ds_port/tests/obj_window_test.c'),
                            '-o', str(exe)], check=True)
            out = subprocess.run([str(exe)], check=True, capture_output=True, text=True).stdout
            self.assertIn('PASS sprite window', out)


if __name__ == '__main__':
    unittest.main()
