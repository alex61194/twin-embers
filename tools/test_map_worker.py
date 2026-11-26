"""The HOME map's worker, the console's own code on a computer (3ds_port/tests/map_worker_test.c).

The test builds 3ds_home_map_job_impl.h with POSIX threads standing in for libctru's (3ds_port/tests/shim/3ds/thread.h) and checks that a built
image equals a plain single-threaded build, that a stop asked of a worker costs the game's thread no wait, that the region changed again and again
never shows another map's image, that a map with no art is decided without a worker, and that the end of the process joins the worker wherever it is.

It also runs the test under a thread sanitizer when the compiler has one (gcc and clang on Linux and macOS do; MinGW does not, and then that part
is skipped and said so). What no computer shows: the console's own scheduler (the worker runs at priority 0x3F on the second core, the game's thread
at the normal one) and the cost of a step on an Old 3DS. Both are for a console, or at least an emulator, see docs/MAP-WORKER.md.
"""
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = ROOT / '3ds_port/tests/map_worker_test.c'


def cc():
    return os.environ.get('CC') or shutil.which('gcc') or 'gcc'


def build(out, extra):
    cmd = [cc(), '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', '-pthread'] + extra
    cmd += ['-I' + str(ROOT / '3ds_port/tests/shim'), '-I' + str(ROOT / '3ds_port/include'), '-I' + str(ROOT / '3ds_port/src'),
            str(SRC), '-o', str(out), '-lm']
    return subprocess.run(cmd, capture_output=True, text=True)


class MapWorker(unittest.TestCase):
    def test_worker_lifecycle(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / ('map-worker.exe' if os.name == 'nt' else 'map-worker')
            built = build(exe, [])
            self.assertEqual(built.returncode, 0, built.stderr)
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=300)
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertIn('PASS map worker', run.stdout)

    def test_worker_under_a_thread_sanitizer(self):
        with tempfile.TemporaryDirectory() as tmp:
            exe = Path(tmp) / ('map-worker-tsan.exe' if os.name == 'nt' else 'map-worker-tsan')
            built = build(exe, ['-fsanitize=thread'])
            if built.returncode != 0:
                self.skipTest('this compiler has no thread sanitizer (MinGW has none): run it on Linux or macOS')
            run = subprocess.run([str(exe)], capture_output=True, text=True, timeout=900,
                                 env=dict(os.environ, TSAN_OPTIONS='halt_on_error=1'))
            self.assertEqual(run.returncode, 0, run.stdout + run.stderr)
            self.assertNotIn('WARNING: ThreadSanitizer', run.stderr)
            self.assertIn('PASS map worker', run.stdout)


if __name__ == '__main__':
    unittest.main()
