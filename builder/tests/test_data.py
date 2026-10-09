"""Cross-language corruption/identity checks using synthetic data only."""
import ctypes as c
import hashlib
import os
import json
from pathlib import Path
import shutil
import struct
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'builder'))
from firered3ds_builder.pak import write_pak, PakReader, PakError, engine_abi, path_id, HEADER, ENTRY
from firered3ds_builder.rom import load_rom, SUPPORTED_SHA1
from firered3ds_builder.recipe import reconstruct, lz77
from firered3ds_builder.build import build


class Header(c.Structure):
    _fields_ = [('schema', c.c_uint32), ('abi', c.c_uint32), ('count', c.c_uint32), ('crc', c.c_uint32), ('sha', c.c_uint8 * 20), ('index', c.c_uint64), ('data', c.c_uint64)]


class Entry(c.Structure):
    _fields_ = [('id', c.c_uint64), ('type', c.c_uint32), ('flags', c.c_uint32), ('offset', c.c_uint64), ('stored', c.c_uint32), ('raw', c.c_uint32), ('crc', c.c_uint32), ('reserved', c.c_uint32)]


class DataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        folder = ROOT / 'build/tests'
        folder.mkdir(parents=True, exist_ok=True)
        native = Path(r'C:\devkitPro\msys2\mingw64\bin\gcc.exe')
        gcc = str(native) if sys.platform == 'win32' and native.exists() else shutil.which('gcc')
        if not gcc:
            raise RuntimeError('Native host GCC is required for the C/Python pack checks')
        lib = folder / ('pak.dll' if sys.platform == 'win32' else 'pak.so')
        env = dict(os.environ)
        env['PATH'] = str(Path(gcc).parent) + os.pathsep + env.get('PATH', '')
        subprocess.run([gcc, '-shared', '-static-libgcc', '-O2', '-std=c11', '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / '3ds_port/include'), str(ROOT / '3ds_port/src/3ds_pak.c'), '-o', str(lib)], check=True, env=env)
        cls.lib = c.CDLL(str(lib))
        cls.lib.CtrPak_ParseHeader.argtypes = [c.c_void_p, c.c_uint32, c.c_void_p, c.POINTER(Header)]
        cls.lib.CtrPak_ParseIndex.argtypes = [c.c_void_p, c.POINTER(Header), c.POINTER(Entry)]
        cls.lib.CtrPak_ValidateRanges.argtypes = [c.POINTER(Header), c.POINTER(Entry), c.c_uint64]
        cls.files = [('scripts/test.bin', b'synthetic script fixture'), ('sound/test.bin', bytes(range(64)))]
        cls.abi = engine_abi((p, len(b), zlib.crc32(b)) for p, b in cls.files)
        cls.sha = bytes.fromhex(SUPPORTED_SHA1)

    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory(dir=ROOT / 'build/tests')
        self.path = Path(self.tmp.name) / 'test.pak'
        write_pak(self.path, self.files, self.abi, self.sha)

    def tearDown(self):
        self.tmp.cleanup()

    def c_status(self, raw):
        h = Header()
        result = self.lib.CtrPak_ParseHeader(raw[:64], self.abi, self.sha, c.byref(h))
        if result:
            return result
        entries = (Entry * h.count)()
        result = self.lib.CtrPak_ParseIndex(raw[64:64 + h.count * 40], c.byref(h), entries)
        return result or self.lib.CtrPak_ValidateRanges(c.byref(h), entries, len(raw))

    def mutate(self, offset, value, fmt='<I', index=False):
        raw = bytearray(self.path.read_bytes())
        struct.pack_into(fmt, raw, offset, value)
        if index:
            struct.pack_into('<I', raw, 56, zlib.crc32(raw[64:144]))
        struct.pack_into('<I', raw, 60, zlib.crc32(raw[:60]))
        self.path.write_bytes(raw)
        return bytes(raw)

    def reject(self, raw):
        self.assertNotEqual(self.c_status(raw), 0)
        with self.assertRaises(PakError):
            with PakReader(self.path, self.abi, self.sha) as reader:
                reader.verify()

    def test_roundtrip_deterministic_cross_language(self):
        self.assertEqual(self.c_status(self.path.read_bytes()), 0)
        before = self.path.read_bytes()
        write_pak(self.path, reversed(self.files), self.abi, self.sha)
        self.assertEqual(before, self.path.read_bytes())
        with PakReader(self.path, self.abi, self.sha) as reader:
            self.assertEqual(reader.verify(), 2)
            for path, data in self.files:
                self.assertEqual(reader.read(path), data)

    def test_abi(self): self.reject(self.mutate(12, self.abi ^ 1))
    def test_schema(self):
        self.reject(self.mutate(8, 1))
        self.reject(self.mutate(8, 3))
    def test_lexical_payload_layout(self):
        raw = self.path.read_bytes()
        offsets = {}
        with PakReader(self.path, self.abi, self.sha) as reader:
            for name, _ in self.files:
                offsets[name] = reader.entries[path_id(name)].offset
        ordered = sorted(offsets, key=offsets.get)
        self.assertEqual(ordered, sorted(offsets))
        # Index order (by id) differs from payload order in this fixture.
        ids = [struct.unpack_from('<QIIQ', raw, 64 + 40 * i) for i in range(2)]
        self.assertGreater(ids[0][3], ids[1][3])
    def test_gap(self):
        raw = bytearray(self.path.read_bytes())
        last = max((80, 120), key=lambda o: struct.unpack_from('<Q', raw, o)[0])
        start = struct.unpack_from('<Q', raw, last)[0]
        struct.pack_into('<Q', raw, last, start + 32)
        raw[start:start] = bytes(32)
        struct.pack_into('<I', raw, 56, zlib.crc32(raw[64:144]))
        struct.pack_into('<I', raw, 60, zlib.crc32(raw[:60]))
        self.path.write_bytes(raw)
        self.reject(bytes(raw))
    def test_rom(self): self.reject(self.mutate(16, 0))
    def test_count_limit(self): self.reject(self.mutate(36, 65537))
    def test_index_offset(self): self.reject(self.mutate(40, 32, '<Q'))
    def test_payload_overlap(self):
        # Fixture payloads are 24 and 64 bytes: the only 32-byte aligned
        # overlap moves the later payload onto the earlier one.
        raw = self.path.read_bytes()
        offsets = {o: struct.unpack_from('<Q', raw, o)[0] for o in (80, 120)}
        later = max(offsets, key=offsets.get)
        self.reject(self.mutate(later, min(offsets.values()), '<Q', True))
    def test_overflow_offset(self): self.reject(self.mutate(80, 0xffffffffffffffe0, '<Q', True))
    def test_flags(self): self.reject(self.mutate(76, 2, index=True))
    def test_reserved(self): self.reject(self.mutate(100, 1, index=True))
    def test_oversize_payload(self): self.reject(self.mutate(88, 0xffffffff, index=True))
    def test_truncated(self):
        raw = self.path.read_bytes()[:-32]
        self.path.write_bytes(raw)
        self.reject(raw)
    def test_payload_crc(self):
        raw = bytearray(self.path.read_bytes())
        raw[160] ^= 1
        self.path.write_bytes(raw)
        with self.assertRaises(PakError):
            with PakReader(self.path, self.abi, self.sha) as reader:
                reader.verify()
    def test_header_crc(self):
        raw = bytearray(self.path.read_bytes())
        raw[12] ^= 1
        self.path.write_bytes(raw)
        self.reject(bytes(raw))
    def test_paths(self):
        for name in ('../rom.gba', '/a', 'a//b', 'a/./b', 'sdmc:/a', 'a\x00b', 'é/a'):
            with self.assertRaises(PakError):
                write_pak(self.path, [(name, b'fixture')], self.abi, self.sha)
    def test_recipe_copy_fill(self):
        data = b'hello' + b'\x00' * 3
        abi = engine_abi([('scripts/test.bin', 8, zlib.crc32(data))])
        recipe = {'schema': 1, 'rom_sha1': SUPPORTED_SHA1, 'engine_abi': abi, 'entries': [{'path': 'scripts/test.bin', 'size': 8, 'crc32': zlib.crc32(data), 'ops': [['C', 0, 5], ['F', 0, 3]]}]}
        self.assertEqual(reconstruct(recipe, b'hello'), [('scripts/test.bin', data)])
        recipe['entries'][0]['ops'][0][1] = 5
        with self.assertRaises(ValueError): reconstruct(recipe, b'hello')
    def test_lz_bounds(self):
        self.assertEqual(lz77(b'\x10\x03\x00\x00\x00abc', 0, 3), b'abc')
        for data in (b'', b'\x10\x03\x00\x00\x80\x00\x00', b'\x10\x03\x00\x00\x00a'):
            with self.assertRaises(ValueError): lz77(data, 0, 3)
    def test_wrong_rom_unchanged(self):
        path = Path(self.tmp.name) / 'own.gba'
        data = bytes(16 * 1024 * 1024)
        path.write_bytes(data)
        with self.assertRaises(ValueError): load_rom(path)
        self.assertEqual(hashlib.sha1(path.read_bytes()).digest(), hashlib.sha1(data).digest())

    @unittest.skipUnless(os.environ.get('FIRERED_TEST_ROM'), 'Set FIRERED_TEST_ROM to exercise an own verified ROM locally')
    def test_own_rom_recipe_pack_pipeline(self):
        rom_path = Path(os.environ['FIRERED_TEST_ROM'])
        rom = load_rom(rom_path)
        # Small local reconstruction probe, NOT the game's complete recipe.
        data = rom.data[0xa0:0xb0]
        abi = engine_abi([('gamedata/identity.bin', 16, zlib.crc32(data))])
        recipe = {'schema': 1, 'rom_sha1': SUPPORTED_SHA1, 'engine_abi': abi, 'entries': [{'path': 'gamedata/identity.bin', 'size': 16, 'crc32': zlib.crc32(data), 'ops': [['C', 0xa0, 16]]}]}
        recipe_path = Path(self.tmp.name) / 'probe.recipe'
        recipe_path.write_text(json.dumps(recipe))
        output = Path(self.tmp.name) / 'probe.pak'
        build(rom_path, recipe_path, output)
        with PakReader(output, abi, self.sha) as reader:
            self.assertEqual(reader.read('gamedata/identity.bin'), data)
        self.assertEqual(hashlib.sha1(rom_path.read_bytes()).hexdigest(), SUPPORTED_SHA1)


if __name__ == '__main__': unittest.main()
