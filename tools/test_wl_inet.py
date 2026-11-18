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
                        str(ROOT / '3ds_port/src/3ds_wl_net.c'), '-o', exe] + libs, check=True)
        out = subprocess.run([exe, str(self.port)], capture_output=True, text=True, timeout=120)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
        self.assertIn('wl_inet_test ok', out.stdout)

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
