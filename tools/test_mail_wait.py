"""Run the production mail init callback with blocked DMA/link steps, no emulator."""
import json
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


class MailWait(unittest.TestCase):
    def test_actual_callback_yields_and_resumes(self):
        recipe = json.loads((ROOT/'patches/pokefirered/0102-mail-init-vblank-wait.json').read_text(encoding='utf-8'))
        production = ''.join(op['port'] for op in recipe['files'][0]['lines'] if 'port' in op)
        harness = (ROOT/'3ds_port/tests/mail_wait_test.c').read_text(encoding='utf-8')
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            c = folder/'test.c'
            c.write_text(harness.replace('/* PRODUCTION CALLBACK */', production), encoding='utf-8')
            for platform in ('native','gba'):
                exe = folder/(platform + ('.exe' if os.name == 'nt' else ''))
                command = [os.environ.get('CC','gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror']
                if platform == 'native':
                    command += ['-DPLATFORM_3DS']
                subprocess.run(command+[str(c),'-o',str(exe)],check=True)
                subprocess.run([str(exe)],check=True)


if __name__ == '__main__':
    unittest.main()
