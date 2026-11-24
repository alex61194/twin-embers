"""The release can not be made silently without its relay: the gate script, and the Makefile's early refusal."""
import os
import shutil
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import check_release_relay as gate  # noqa: E402

RELAY = 'relay.example.test:47311'


class Gate(unittest.TestCase):
    def test_relay_present(self):
        ok, _ = gate.check(b'\0\0' + RELAY.encode() + b'\0', RELAY, False)
        self.assertTrue(ok)

    def test_relay_forgotten(self):
        ok, message = gate.check(b'code only', '', False)
        self.assertFalse(ok)
        self.assertIn('TWIN_RELAY', message)

    def test_stale_object_without_the_relay(self):
        ok, message = gate.check(b'compiled without it', RELAY, False)
        self.assertFalse(ok)
        self.assertIn('main_game_full.o', message)

    def test_no_relay_only_by_decision(self):
        self.assertTrue(gate.check(b'x', '', True)[0])
        self.assertFalse(gate.check(b'x', '', False)[0])

    def test_a_relay_that_is_not_host_port(self):
        for bad in ('nohost', ':47311', 'a b:1', 'host:0', 'host:70000', 'host:12:34'):
            self.assertFalse(gate.check(RELAY.encode(), bad, False)[0], bad)

    def test_command_line(self):
        with tempfile.TemporaryDirectory() as tmp:
            image = Path(tmp) / 'x.3dsx'
            image.write_bytes(b'..' + RELAY.encode())
            run = lambda *a: subprocess.run([sys.executable, str(ROOT / 'tools/check_release_relay.py'), str(image), *a], capture_output=True, text=True)
            self.assertEqual(run('--relay', RELAY).returncode, 0)
            self.assertEqual(run().returncode, 1)
            self.assertEqual(run('--allow-none').returncode, 0)
            self.assertEqual(run('--relay', 'other.example.test:1').returncode, 1)


@unittest.skipUnless(shutil.which('make'), 'make is needed')
class MakefileGuard(unittest.TestCase):
    def make(self, *args):
        env = dict(os.environ, DEVKITPRO='/nonexistent')
        return subprocess.run(['make', '-n', '-C', str(ROOT / '3ds_port'), *args], capture_output=True, text=True, env=env)

    def test_require_relay_stops_a_build_without_it(self):
        run = self.make('REQUIRE_RELAY=1', 'verify-game-3dsx')
        self.assertNotEqual(run.returncode, 0)
        self.assertIn('REQUIRE_RELAY is set but TWIN_RELAY is not', run.stdout + run.stderr)

    def test_clean_release_stops_early_without_a_relay_decision(self):
        run = self.make('verify-clean-release')
        self.assertNotEqual(run.returncode, 0)
        self.assertIn('TWIN_RELAY', run.stdout + run.stderr)

    def test_clean_release_goes_on_with_a_relay_or_an_explicit_decision(self):
        for extra in (['TWIN_RELAY=' + RELAY], ['ALLOW_NO_RELAY=1']):
            run = self.make('MAKE=true', 'verify-clean-release', *extra)     # MAKE=true: the dry run must not run the nested build
            self.assertNotIn('is set but', run.stdout + run.stderr, extra)
            self.assertNotIn('no relay for this release', run.stdout + run.stderr, extra)


if __name__ == '__main__':
    unittest.main()
