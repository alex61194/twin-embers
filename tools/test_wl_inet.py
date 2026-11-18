#!/usr/bin/env python3
"""The internet radio of the local-wireless layer against the reference relay (docs/WL-RELAY-PROTOCOL.md).

Starts tools/wl_relay.py on a free port, builds and runs 3ds_port/tests/wl_inet_test.c against it, and checks the relay by
hand on the wire: a malformed hello, a silent connection, a wrong version.
"""
import os
import pathlib
import socket
import struct
import subprocess
import sys
import tempfile
import time
import unittest
import hashlib

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import test_wl_crypto as tc   # the independent Python AEAD

ROOT = pathlib.Path(__file__).resolve().parents[1]


def start_relay():
    proc = subprocess.Popen([sys.executable, '-u', str(ROOT / 'tools/wl_relay.py'), '--host', '127.0.0.1', '--port', '0'],
                            stdout=subprocess.PIPE, text=True)
    line = proc.stdout.readline()
    assert line.startswith('LISTEN '), line
    return proc, int(line.split()[1])


def message(kind, body=b''):
    payload = bytes([kind]) + body
    return struct.pack('<H', len(payload)) + payload


def read_message(sock):
    header = b''
    while len(header) < 2:
        chunk = sock.recv(2 - len(header))
        if not chunk:
            return None
        header += chunk
    (length,) = struct.unpack('<H', header)
    body = b''
    while len(body) < length:
        chunk = sock.recv(length - len(body))
        if not chunk:
            return None
        body += chunk
    return body


class Relay(unittest.TestCase):
    def setUp(self):
        self.proc, self.port = start_relay()

    def tearDown(self):
        self.proc.kill()
        self.proc.wait()
        self.proc.stdout.close()

    def connect(self):
        s = socket.create_connection(('127.0.0.1', self.port), timeout=3)
        return s

    def hello(self, room=b'WIRE', build=bytes(range(16)), version=1):
        return message(1, bytes([version]) + build + bytes([len(room)]) + room)

    def test_client_against_relay(self):
        exe_dir = tempfile.mkdtemp()
        exe = os.path.join(exe_dir, 'wl_inet_test.exe')
        cc = os.environ.get('CC', 'gcc')
        libs = ['-lws2_32'] if os.name == 'nt' else []
        subprocess.run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / '3ds_port/include'),
                        str(ROOT / '3ds_port/tests/wl_inet_test.c'), str(ROOT / '3ds_port/src/3ds_wl_inet.c'),
                        str(ROOT / '3ds_port/src/3ds_wl_net.c'), str(ROOT / '3ds_port/src/3ds_wl_crypto.c'), '-o', exe] + libs, check=True)
        out = subprocess.run([exe, str(self.port)], capture_output=True, text=True, timeout=120)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
        self.assertIn('wl_inet_test ok', out.stdout)

    def test_interop_with_python_and_tampering(self):
        """A console written again in Python, from docs/WL-RELAY-PROTOCOL.md and the key rules in 3ds_wl_inet.h, talks to the C one:
        what each seals the other opens, and a changed, moved or foreign frame is dropped."""
        exe_dir = tempfile.mkdtemp()
        exe = os.path.join(exe_dir, 'wl_inet_test.exe')
        cc = os.environ.get('CC', 'gcc')
        libs = ['-lws2_32'] if os.name == 'nt' else []
        subprocess.run([cc, '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / '3ds_port/include'),
                        str(ROOT / '3ds_port/tests/wl_inet_test.c'), str(ROOT / '3ds_port/src/3ds_wl_inet.c'),
                        str(ROOT / '3ds_port/src/3ds_wl_net.c'), str(ROOT / '3ds_port/src/3ds_wl_crypto.c'), '-o', exe] + libs, check=True)
        code = b'a-secret-code-for-the-test'
        build = bytes([1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16])      # kBuildA of the C test
        key = hashlib.pbkdf2_hmac('sha256', code, b'TwinEmbers relay v1' + build, 4096, 32)
        room = hashlib.sha256(b'TwinEmbers room' + key).hexdigest()[:16].encode()
        listener = subprocess.Popen([exe, 'listen', str(self.port), code.decode()], stdout=subprocess.PIPE, text=True)
        try:
            self.assertEqual(listener.stdout.readline().split(), ['READY', '1'])
            s = self.connect()
            s.sendall(self.hello(room=room, build=build))
            welcome = read_message(s)
            self.assertEqual((welcome[0], welcome[1], struct.unpack('<I', welcome[2:6])[0]), (2, 2, 1 << 1))
            session = os.urandom(8)
            counter = [0]

            def seal(dest, plain, src=2, aad_dest=None):
                nonce = session + struct.pack('<I', counter[0])
                counter[0] += 1
                aad = bytes([src, dest if aad_dest is None else aad_dest])
                ct, tag = tc.aead(key, nonce, aad, plain)
                return message(6, bytes([dest]) + nonce + ct + tag)

            # frames that must be dropped, sent before the good one
            good = seal(1, b'hello from python')
            bad = bytearray(seal(1, b'a changed frame'))
            bad[-3] ^= 1                                                  # a bit of the tag
            s.sendall(bytes(bad))
            s.sendall(seal(1, b'for another node', aad_dest=5))           # sealed for a node that is not the receiver
            s.sendall(seal(1, b'from another node', src=7))               # the relay will name node 2 as the sender
            wrong = hashlib.pbkdf2_hmac('sha256', b'another code', b'TwinEmbers relay v1' + build, 4096, 32)
            nonce = os.urandom(12)
            ct, tag = tc.aead(wrong, nonce, bytes([2, 1]), b'another key')
            s.sendall(message(6, bytes([1]) + nonce + ct + tag))
            s.sendall(message(6, bytes([1]) + b'too short to be sealed'[:20]))
            s.sendall(good)
            got = listener.stdout.readline().split()
            self.assertEqual(got[0:2], ['GOT', '2'])
            self.assertEqual(bytes.fromhex(got[2]), b'hello from python')
            reply = read_message(s)
            self.assertEqual(reply[0], 6)
            self.assertEqual(reply[1], 1)
            nonce, rest = reply[2:14], reply[14:]
            ct, tag = rest[:-16], rest[-16:]
            expected_ct, expected_tag = tc.aead(key, nonce, bytes([1, 2]), b'reply from c')
            self.assertEqual((ct, tag), (expected_ct, expected_tag))     # what the C console sealed is what Python would
            self.assertEqual(len(rest), 12 + 16)
            lines = listener.stdout.read().splitlines()
            self.assertIn('DROPPED 4', lines)       # the too short one is ignored, not counted
            self.assertFalse([l for l in lines if l.startswith('MORE')])
            s.close()
        finally:
            listener.kill()
            listener.wait()
            listener.stdout.close()

    def test_wire(self):
        a = self.connect()
        a.sendall(self.hello())
        welcome = read_message(a)
        self.assertEqual(welcome[0], 2)
        self.assertEqual(welcome[1], 1)
        self.assertEqual(struct.unpack('<I', welcome[2:6])[0], 0)
        b = self.connect()
        b.sendall(self.hello())
        welcome = read_message(b)
        self.assertEqual((welcome[1], struct.unpack('<I', welcome[2:6])[0]), (2, 1 << 1))
        self.assertEqual(read_message(a), bytes([4, 2]))              # JOINED 2
        b.sendall(message(6, bytes([1]) + b'hello'))                  # DATA to node 1
        self.assertEqual(read_message(a), bytes([6, 2]) + b'hello')   # DATA from node 2
        a.sendall(message(7))
        self.assertEqual(read_message(a), bytes([8]))                 # PONG
        a.sendall(message(6, bytes([2]) + b'x' * 1501))               # too long: dropped, nothing arrives
        a.sendall(message(6, bytes([1]) + b'to myself'))              # to itself: dropped
        a.sendall(message(6, bytes([2]) + b'ok'))
        self.assertEqual(read_message(b), bytes([6, 1]) + b'ok')
        b.close()
        self.assertEqual(read_message(a), bytes([5, 2]))              # LEFT 2
        a.close()

    def test_refusals(self):
        s = self.connect()
        s.sendall(self.hello(version=2))
        self.assertEqual(read_message(s), bytes([3, 2]))
        s.close()
        s = self.connect()
        s.sendall(message(1, b'\x01short'))
        self.assertEqual(read_message(s), bytes([3, 1]))
        s.close()
        s = self.connect()
        s.sendall(self.hello(room=b'bad room!'))
        self.assertEqual(read_message(s), bytes([3, 1]))
        s.close()
        s = self.connect()
        s.sendall(struct.pack('<H', 5000))                            # a length that is not the protocol
        self.assertEqual(read_message(s), bytes([3, 1]))
        s.close()
        first = self.connect()
        first.sendall(self.hello(room=b'LOCKED'))
        self.assertEqual(read_message(first)[0], 2)
        second = self.connect()
        second.sendall(self.hello(room=b'LOCKED', build=bytes(16)))
        self.assertEqual(read_message(second), bytes([3, 4]))
        second.close()
        first.close()
        time.sleep(0.1)


if __name__ == '__main__':
    unittest.main()
