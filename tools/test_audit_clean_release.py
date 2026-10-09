#!/usr/bin/env python3
"""Self-test of the clean-release audit: map parsing, classes, RomFS listing."""
import json
import struct
import sys
import tempfile
import unittest
import zlib
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
import audit_clean_release as a

MAP = """
Linker script and memory map

.rodata         0x00324000   0x1000
 .rodata.gItems
                0x00324000      0x400 build/game/item.o
 .rodata        0x00324400      0x100 build/game_data/maps.o
 .rodata.sPtFont
                0x00324500       0x80 build/3ds_poketouch.o
 .rodata.str1.4
                0x00324580       0x10 /opt/devkitpro/libc.a(libc_a-x.o)
.bss            0x00700000   0x2000
 .bss.ctr_asset.abcd
                0x00700000     0x1000 build/game/item.o
 .bss.ctr_script_blob
                0x00701000      0x800 build/3ds_script_blob.o
"""


def romfs(files):
    """Smallest valid RomFS level-3 image with files in the root and one directory."""
    def name(s):
        raw = s.encode('utf-16-le')
        return raw + b'\0' * (-len(raw) % 4), len(raw)
    dirs = bytearray()
    root_name, rn = name('')
    sub_name, sn = name('sub')
    # root dir (0) with child dir (0x18+..) and first file; sub dir has no children
    sub_off = 24
    dirs += struct.pack('<6I', 0, 0xffffffff, sub_off, 0, 0xffffffff, 0)
    dirs += struct.pack('<6I', 0, 0xffffffff, 0xffffffff, 0xffffffff, 0xffffffff, sn) + sub_name
    ftab = bytearray()
    offsets = []
    for n in files:
        offsets.append(len(ftab))
        raw, ln = name(n)
        ftab += b'\0' * 0
        ftab += struct.pack('<IIQQII', 0, 0xffffffff, 0, 0, 0xffffffff, ln) + raw
    for i, off in enumerate(offsets[:-1]):
        struct.pack_into('<I', ftab, off + 4, offsets[i + 1])
    struct.pack_into('<I', dirs, 12, offsets[0])
    head = 0x28
    dir_off = head
    file_off = dir_off + len(dirs)
    data_off = file_off + len(ftab)
    hdr = struct.pack('<10I', 0x28, 0, 0, dir_off, len(dirs), 0, 0, file_off, len(ftab), data_off)
    return hdr + bytes(dirs) + bytes(ftab)


class AuditTests(unittest.TestCase):
    def test_map_attribution(self):
        rows = a.parse_map(MAP)
        by = {(r['input'], r['obj']): r for r in rows}
        self.assertEqual(by[('.rodata.gItems', 'build/game/item.o')]['size'], 0x400)
        self.assertEqual(by[('.rodata', 'build/game_data/maps.o')]['output'], '.rodata')

    def test_classes(self):
        rows = {r['obj']: r for r in a.parse_map(MAP) if r['output'] == '.rodata'}
        self.assertEqual(a.classify_contribution(rows['build/game/item.o'])[0], 'EMBEDDED')
        self.assertEqual(a.classify_contribution(rows['build/game_data/maps.o'])[0], 'EMBEDDED')
        self.assertEqual(a.classify_contribution(rows['build/3ds_poketouch.o'])[0], 'ENGINE')
        self.assertEqual(a.classify_contribution(rows['/opt/devkitpro/libc.a(libc_a-x.o)'])[0], 'ENGINE')
        self.assertEqual(a.category_of('build/game_data/maps.o'), 'maps: layouts, blockdata, headers')

    def test_romfs_listing(self):
        image = romfs(['engine.bin', 'scripts.bin'])
        self.assertEqual(a.romfs_files(image), ['engine.bin', 'scripts.bin'])

    def test_payload_filter(self):
        names = ['engine/abi.bin', 'engine/assets.bin', 'scripts/scripts.rel', 'data.embedded',
                 'scripts/scripts.bin', 'graphics/x.4bpp']
        self.assertEqual(a.payload_files(names), ['scripts/scripts.bin', 'graphics/x.4bpp'])



class FakeElf:
    def __init__(self, names):
        self.symbols = [dict(name=n, shndx=1) for n in names]


def assets_bin(rows):
    strings, records = bytearray(), bytearray()
    for path, size, crc in rows:
        records += struct.pack('<IIIII', 0x100000, size, len(strings), crc, 1)
        strings += path.encode() + b'\0'
    body = bytes(records) + bytes(strings)
    return struct.pack('<4sIIIII', b'F3AS', 1, 0x100000, len(rows), len(strings), zlib.crc32(body)) + body


def rex_bin(rows):
    strings, regions = bytearray(), bytearray()
    for path, size, crc in rows:
        regions += struct.pack('<4I', len(strings), size, crc, 1)
        strings += path.encode() + b'\0'
    body = bytes(regions) + bytes(strings)
    return struct.pack('<8I', 0x58523346, 1, 0x100000, len(rows), 0, 0, len(strings), zlib.crc32(body)) + body


class ReleaseGate(unittest.TestCase):
    RECIPE = dict(engine_abi=0x1234abcd, entries=[
        dict(path='gamedata/c/item.bin', size=100, crc32=7, ops=[]),
        dict(path='graphics/a.4bpp', size=32, crc32=9, ops=[]),
        dict(path='graphics/unused.4bpp', size=32, crc32=11, ops=[])])
    SYMBOLS = a.REQUIRED_SYMBOLS

    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.recipe = Path(self.dir.name) / 'recipe.json'
        self.recipe.write_text(json.dumps(self.RECIPE))
        self.entries = {'engine/abi.bin': struct.pack('<I', 0x1234abcd), 'engine/profile.bin': a.CLEAN_PROFILE,
                        'engine/assets.bin': assets_bin([('graphics/a.4bpp', 32, 9)]),
                        'engine/rex.bin': rex_bin([('gamedata/c/item.bin', 100, 7)])}
        self.result = dict(embedded_bytes=0, unknown_bytes=0)

    def tearDown(self):
        self.dir.cleanup()

    def findings(self, **change):
        entries = dict(self.entries)
        for key, value in change.items():
            entries[key.replace('__', '/')] = value
        return a.release_findings(self.result, entries, self.recipe, FakeElf(self.SYMBOLS))

    def test_clean_image_passes_and_may_ship_unused_entries(self):
        self.assertEqual(self.findings(), [])

    def test_embedded_bytes_block(self):
        self.result['embedded_bytes'] = 12
        self.assertTrue(any('12 bytes' in p for p in self.findings()))

    def test_any_payload_file_blocks(self):
        self.assertTrue(any('beyond the engine metadata' in p for p in self.findings(**{'graphics__a.4bpp': b'x'})))
        self.assertTrue(any('beyond' in p for p in self.findings(**{'twinembers.pak': b'x'})))

    def test_embedded_marker_and_profile(self):
        self.assertTrue(any('data.embedded' in p for p in self.findings(**{'data.embedded': b''})))
        self.assertTrue(any('profile' in p for p in self.findings(**{'engine__profile.bin': b'FR3DDEV\0'})))

    def test_abi_mismatch(self):
        self.assertTrue(any('ABI' in p for p in self.findings(**{'engine__abi.bin': struct.pack('<I', 1)})))

    def test_missing_pack_reader(self):
        self.SYMBOLS = tuple(s for s in a.REQUIRED_SYMBOLS if s != 'CtrPak_ParseHeader')
        self.assertTrue(any('CtrPak_ParseHeader' in p for p in self.findings()))

    def test_executable_expects_something_the_recipe_lacks(self):
        bad = {'engine__assets.bin': assets_bin([('graphics/missing.4bpp', 32, 9)])}
        self.assertTrue(any('missing.4bpp' in p for p in self.findings(**bad)))

    def test_size_or_crc_disagreement(self):
        bad = {'engine__rex.bin': rex_bin([('gamedata/c/item.bin', 100, 8)])}
        self.assertTrue(any('size/CRC' in p for p in self.findings(**bad)))

    def test_no_romfs_at_all(self):
        self.assertTrue(a.release_findings(self.result, None, self.recipe, FakeElf(self.SYMBOLS)))

    def test_rom_leak_scan_finds_copied_payload(self):
        import random
        sys.path.insert(0, str(a.ROOT / 'builder'))
        from firered3ds_builder.pak import engine_abi
        rom = random.Random(1).randbytes(16 * 1024 * 1024)
        data = rom[4096:4096 + 256]
        entry = dict(path='graphics/x.bin', size=256, crc32=zlib.crc32(data), ops=[['C', 4096, 256]])
        recipe = dict(schema=1, rom_sha1='41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc', entries=[entry],
                      engine_abi=engine_abi([('graphics/x.bin', 256, zlib.crc32(data))]))
        self.assertTrue(a.scan_rom_leaks(b'\0' * 100 + data + b'\0' * 100, recipe, rom))
        self.assertEqual(a.scan_rom_leaks(b'\0' * 400, recipe, rom), [])


if __name__ == '__main__':
    unittest.main()
