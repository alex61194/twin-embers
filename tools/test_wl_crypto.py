#!/usr/bin/env python3
"""3ds_wl_crypto.c against independent implementations: Python's hashlib / hmac for SHA-256, HMAC and PBKDF2, and ChaCha20,
Poly1305 and the AEAD written here again from RFC 8439 with plain integers (nothing shared with the C code)."""
import hashlib
import hmac
import os
import pathlib
import struct
import subprocess
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parents[1]
M32 = 0xFFFFFFFF


def pattern(n, seed):
    return bytes((i * 7 + 3 + seed * 31) & 255 for i in range(n))


def rol(x, n):
    return ((x << n) & M32) | (x >> (32 - n))


def chacha_block(key, counter, nonce):
    s = [0x61707865, 0x3320646E, 0x79622D32, 0x6B206574] + list(struct.unpack('<8L', key)) + [counter] + list(struct.unpack('<3L', nonce))
    x = s[:]

    def qr(a, b, c, d):
        x[a] = (x[a] + x[b]) & M32; x[d] = rol(x[d] ^ x[a], 16)
        x[c] = (x[c] + x[d]) & M32; x[b] = rol(x[b] ^ x[c], 12)
        x[a] = (x[a] + x[b]) & M32; x[d] = rol(x[d] ^ x[a], 8)
        x[c] = (x[c] + x[d]) & M32; x[b] = rol(x[b] ^ x[c], 7)

    for _ in range(10):
        qr(0, 4, 8, 12); qr(1, 5, 9, 13); qr(2, 6, 10, 14); qr(3, 7, 11, 15)
        qr(0, 5, 10, 15); qr(1, 6, 11, 12); qr(2, 7, 8, 13); qr(3, 4, 9, 14)
    return struct.pack('<16L', *[(x[i] + s[i]) & M32 for i in range(16)])


def chacha(key, counter, nonce, data):
    out = bytearray()
    for i in range(0, len(data), 64):
        block = chacha_block(key, counter + i // 64, nonce)
        out += bytes(a ^ b for a, b in zip(data[i:i + 64], block))
    return bytes(out)


def poly(key, msg):
    r = int.from_bytes(key[:16], 'little') & 0x0FFFFFFC0FFFFFFC0FFFFFFC0FFFFFFF
    s = int.from_bytes(key[16:], 'little')
    p = (1 << 130) - 5
    acc = 0
    for i in range(0, len(msg), 16):
        chunk = msg[i:i + 16]
        acc = ((acc + int.from_bytes(chunk + b'\x01', 'little')) * r) % p
    return ((acc + s) & ((1 << 128) - 1)).to_bytes(16, 'little')


def aead(key, nonce, aad, plain):
    ct = chacha(key, 1, nonce, plain)
    pad = lambda b: b'\x00' * ((16 - len(b) % 16) % 16)
    mac = aad + pad(aad) + ct + pad(ct) + struct.pack('<QQ', len(aad), len(ct))
    return ct, poly(chacha_block(key, 0, nonce)[:32], mac)


class Crypto(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.exe = os.path.join(tempfile.mkdtemp(), 'wl_crypto_test.exe')
        subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror', '-I', str(ROOT / '3ds_port/include'),
                        str(ROOT / '3ds_port/tests/wl_crypto_test.c'), str(ROOT / '3ds_port/src/3ds_wl_crypto.c'), '-o', cls.exe],
                       check=True)

    def test_vectors(self):
        out = subprocess.run([self.exe], capture_output=True, text=True, timeout=60)
        self.assertEqual(out.returncode, 0, out.stdout + out.stderr)
        self.assertIn('wl_crypto_test ok', out.stdout)

    def test_against_python(self):
        out = subprocess.run([self.exe, 'dump'], capture_output=True, text=True, timeout=120, check=True).stdout.split('\n')
        key, nonce, aad = pattern(32, 1), pattern(12, 2), pattern(40, 3)
        seen = set()
        for line in out:
            if not line:
                continue
            kind, n, hexv = line.split(' ')
            n = int(n)
            got = bytes.fromhex(hexv)
            data = pattern(n, 4)
            if kind == 'sha256':
                want = hashlib.sha256(data).digest()
            elif kind == 'hmac':
                want = hmac.new(data, data, hashlib.sha256).digest()
            elif kind == 'chacha':
                want = chacha(key, 7, nonce, data)
            elif kind == 'poly':
                want = poly(key, data)
            elif kind == 'aeadct':
                want = aead(key, nonce, aad[:n % 41], data)[0]
            elif kind == 'aeadtag':
                want = aead(key, nonce, aad[:n % 41], data)[1]
            elif kind == 'pbkdf2':
                if n == 4096:
                    want = hashlib.pbkdf2_hmac('sha256', b'a-long-room-code', b'TwinEmbers', 4096, 32)
                else:
                    want = hashlib.pbkdf2_hmac('sha256', b'password', b'salt', n, 40)
            else:
                self.fail(line)
            self.assertEqual(got, want, '%s %d' % (kind, n))
            seen.add(kind)
        self.assertEqual(seen, {'sha256', 'hmac', 'chacha', 'poly', 'aeadct', 'aeadtag', 'pbkdf2'})

    def test_python_side_matches_the_rfc(self):
        key = bytes(range(0x80, 0xA0))
        nonce = bytes([7, 0, 0, 0]) + bytes(range(0x40, 0x48))
        text = b"Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it."
        ct, tag = aead(key, nonce, bytes([0x50, 0x51, 0x52, 0x53]) + bytes(range(0xC0, 0xC8)), text)
        self.assertEqual(tag.hex(), '1ae10b594f09e26a7e902ecbd0600691')
        self.assertEqual(ct[:16].hex(), 'd31a8d34648e60db7b86afbc53ef7ec2')


if __name__ == '__main__':
    unittest.main()
