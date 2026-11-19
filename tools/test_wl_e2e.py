#!/usr/bin/env python3
"""Two consoles over the internet radio, with FireRed's own wireless library (recipe 0272) in each.

Builds 3ds_port/tests/rfu_game_test.c against the patched sources of a bootstrapped workspace (a build/<dir> made by tools/bootstrap.py
with the current recipes), starts tools/wl_relay.py, and runs a parent and a child as two processes: the child must find the parent, connect, and
both must stay linked through a second of data exchange. Skipped when there is no such workspace or no C compiler with the console's headers.
"""
import hashlib
import os
import pathlib
import re
import shutil
import struct
import socket
import subprocess
import sys
import tempfile
import time
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
SRC = ROOT / '3ds_port'


def workspace():
    """A bootstrapped workspace whose librfu already has the emulated adapter."""
    found = []
    for d in sorted((ROOT / 'build').glob('*/src/librfu_stwi.c'), key=lambda p: p.stat().st_mtime, reverse=True):
        if 'CtrRfu_Command' in d.read_text(errors='replace'):
            found.append(d.parents[1])
    return found[0] if found else None


def libctru_include():
    for base in (os.environ.get('DEVKITPRO'), '/opt/devkitpro', 'C:/devkitPro'):
        if base and (pathlib.Path(base) / 'libctru/include/3ds/services/uds.h').exists():
            return pathlib.Path(base) / 'libctru/include'
    return None


class EndToEnd(unittest.TestCase):
    def test_parent_and_child_link_up(self):
        ws, ctru, cc = workspace(), libctru_include(), shutil.which(os.environ.get('CC', 'gcc'))
        if not (ws and ctru and cc):
            self.skipTest('needs a bootstrapped workspace (tools/bootstrap.py), libctru headers and gcc')
        windows = sys.platform == 'win32'
        with tempfile.TemporaryDirectory() as tmp:
            out = pathlib.Path(tmp)
            objs = []
            for name in ('gba_shadow', '3ds_bios', '3ds_bios_math', '3ds_dma'):
                obj = out / (name + '.o')
                subprocess.run([cc, '-std=c11', '-O1', '-w', '-I' + str(SRC / 'include'), '-c', str(SRC / 'src' / (name + '.c')), '-o', str(obj)], check=True)
                objs.append(str(obj))
            # The library carves its tables out of one buffer at offsets fixed for 32-bit pointers; on a computer the structures are larger:
            # the test's copy takes the offsets from the structures.
            text = (ws / 'src/librfu_rfu.c').read_text()
            for old, new in (('0xb4;', 'sizeof(*gRfuLinkStatus);'),
                             ('0xdc;', 'sizeof(*gRfuLinkStatus) + sizeof(*gRfuStatic);'),
                             ('0x1bc;', 'sizeof(*gRfuLinkStatus) + sizeof(*gRfuStatic) + sizeof(*gRfuFixed);'),
                             ('0x37c;', 'sizeof(*gRfuLinkStatus) + sizeof(*gRfuStatic) + sizeof(*gRfuFixed) + sizeof(*gRfuSlotStatusNI[0]) * RFU_CHILD_MAX;')):
                text = re.sub(r'\(void \*\)APIBuffer \+ ' + re.escape(old), '(void *)APIBuffer + ' + new, text)
            (out / 'librfu_rfu64.c').write_text(text)
            flags = ['-std=gnu11', '-O1', '-w', '-fsigned-char', '-fno-strict-aliasing', '-DPLATFORM_3DS', '-DMODERN=1', '-DFIRERED', '-DENGLISH',
                     '-DREVISION=0', '-DNDEBUG', '-DCTR_EXTERNAL_ASSETS=1',
                     '-iquote', str(SRC / 'include'), '-iquote', str(ws / 'include'), '-iquote', str(ws / 'include/constants'), '-iquote', str(ws),
                     '-I' + str(SRC / 'include'), '-I' + str(ctru), '-I' + str(SRC / 'tests')]
            sources = [SRC / 'tests/rfu_game_test.c', out / 'librfu_rfu64.c', ws / 'src/librfu_stwi.c', ws / 'src/librfu_sio32id.c',
                       ws / 'src/AgbRfu_LinkManager.c'] + [SRC / 'src' / n for n in
                       ('3ds_wl_glue.c', '3ds_wl_net.c', '3ds_wl_rfu.c', '3ds_wl_inet.c', '3ds_wl_crypto.c', '3ds_wl_uds.c')] + [SRC / 'tests/uds_mock_unit.c']
            exe = out / 'rfu.exe'
            link = ['-lws2_32', '-Wl,--disable-dynamicbase', '-Wl,--image-base=0x10000000', '-Wl,--disable-high-entropy-va'] if windows else []
            subprocess.run([cc] + flags + [str(s) for s in sources] + objs + link + ['-o', str(exe)], check=True)

            # Fixed roles; both searching as parent and child in turn as in the Union Room (holding the link 10 s); and a parent that hands
            # the clock over and waits in a loop for its child's answer, as link_rfu_2.c's linked parent does (it froze the contacted console
            # in the Union Room until the glue gave the adapter a turn in that loop, CtrRfu_Wait).
            for parent, child, hold, extra in (('parent', 'child', '1', {}), ('pcparent', 'pcchild', '10', {}),
                                               ('parent', 'child', '5', {'E2E_PARENT_CHANGE': '1', 'E2E_IDLE': '1'})):
                self.run_pair(exe, out, parent, child, hold, extra)
            self.run_full_public_room(exe, out)

    def run_full_public_room(self, exe, out):
        """Sixteen consoles fill the public room: the next one goes to twinembers-public-2 (3ds_wl_glue.c, CTR_WL_PUBLIC_ROOMS)."""
        build = bytes([7, 7, 7] + [0] * 13)                 # rfu_game_test's build id

        def room_of(code):
            material = hashlib.pbkdf2_hmac('sha256', code.encode(), b'TwinEmbers relay v2' + build, 4096, 64)    # the public room's code: 4096 rounds
            return hashlib.sha256(b'TwinEmbers room v2' + material[32:]).hexdigest()[:16].encode()

        relay = subprocess.Popen([sys.executable, '-u', str(ROOT / 'tools/wl_relay.py'), '--host', '127.0.0.1', '--port', '0',
                                  '--max-per-address', '64', '--log'], stdout=subprocess.PIPE, stderr=subprocess.PIPE, text=True)
        held, console, log = [], None, None
        try:
            port = int(relay.stdout.readline().split()[1])
            first = room_of('twinembers-public')
            for _ in range(16):
                s = socket.create_connection(('127.0.0.1', port), 5)
                body = bytes([1]) + build + bytes([len(first)]) + first
                s.sendall(struct.pack('<H', len(body) + 1) + bytes([1]) + body)
                held.append(s)
            time.sleep(0.5)
            log = open(out / 'spill.txt', 'w')
            console = subprocess.Popen([str(exe), 'parent', str(port), 'twinembers-public'], stdout=log, stderr=subprocess.STDOUT)
            time.sleep(4)
        finally:
            if console:
                console.kill()
                console.wait()
            if log:
                log.close()
            for s in held:
                s.close()
            relay.kill()
            notes = relay.stderr.read()
            relay.wait()
            relay.stdout.close()
            relay.stderr.close()
        second = room_of('twinembers-public-2').decode()
        self.assertIn('room %s node 1 joined' % second, notes, 'the 17th console did not go to the second public room:\n' + notes[-1500:])

    def run_pair(self, exe, out, parent, child, hold, extra):
        files = []
        relay = subprocess.Popen([sys.executable, '-u', str(ROOT / 'tools/wl_relay.py'), '--host', '127.0.0.1', '--port', '0'],
                                 stdout=subprocess.PIPE, text=True)
        try:
            port = relay.stdout.readline().split()[1]
            logs, procs = {}, {}
            env = dict(os.environ, E2E_HOLD=hold, **extra)
            # Output goes to files: a pipe nobody reads fills up and stops the console that writes to it.
            for role in (parent, child):
                logs[role] = out / (role + '.txt')
                files.append(open(logs[role], 'w'))
                procs[role] = subprocess.Popen([str(exe), role, port, 'e2e-test-room'], stdout=files[-1], stderr=subprocess.STDOUT, env=env)
                time.sleep(0.5)
            for role, proc in procs.items():
                try:
                    proc.wait(timeout=120)
                except subprocess.TimeoutExpired:
                    proc.kill()
            text = {role: logs[role].read_text(errors='replace') for role in logs}
            for role in text:
                self.assertIn('DONE', text[role], role + ' did not finish: ' + text[role][-2000:])
                self.assertEqual(procs[role].returncode, 0, role)
                self.assertNotIn('STUCK', text[role], role + ' froze waiting for the adapter')
            self.assertIn('EVENT child detected', text[parent])
            self.assertIn('EVENT child accepted', text[parent])
            self.assertIn('EVENT parent found', text[child])
            self.assertIn('EVENT connected to parent', text[child])
            self.assertIn('EVENT name sent', text[child])
            for role in text:
                got = re.search(r'UNI got (\d+)', text[role])
                self.assertTrue(got and int(got.group(1)) > 20, role + ' received too little data: ' + (got.group(0) if got else 'none'))
            if extra.get('E2E_IDLE'):
                # Both keep the frame rate while linked: the child answers in its spare time, the parent waits a few milliseconds at most.
                for role in text:
                    fps = re.search(r'FPS ([0-9.]+)', text[role])
                    self.assertTrue(fps and float(fps.group(1)) > 45, role + ' ran slowly: ' + (fps.group(0) if fps else 'no rate'))
            if extra.get('E2E_PARENT_CHANGE'):
                waits = re.search(r'waits (\d+)', text[parent])
                self.assertTrue(waits and int(waits.group(1)) > 20, 'the parent did not get through its waits')
        finally:
            relay.kill()
            relay.wait()
            relay.stdout.close()
            for f in files:
                f.close()

if __name__ == '__main__':
    unittest.main()
