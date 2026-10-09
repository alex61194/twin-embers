#!/usr/bin/env python3
"""Synthetic ARM ELF tests for array/symbol/interior-pointer preservation."""
import json
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
import asset_bundle as assets
from rex import tool

ROOT = Path(__file__).resolve().parents[1]
INCBIN = 'INCBIN_' + 'U8'


class AssetsTests(unittest.TestCase):
    def setUp(self):
        (ROOT / 'build/tests').mkdir(parents=True, exist_ok=True)
        self.tmp = tempfile.TemporaryDirectory(dir=ROOT / 'build/tests')
        self.root = Path(self.tmp.name)
        self.previous = assets.ROOT
        assets.ROOT = self.root
        (self.root / 'graphics/fonts').mkdir(parents=True)
        (self.root / 'graphics/fonts/a.bin').write_bytes(bytes(range(16)))
        (self.root / 'graphics/fonts/b.bin').write_bytes(bytes(range(16, 32)))
        self.obj = self.root / '3ds_port/build/game/probe.o'
        self.obj.parent.mkdir(parents=True)
        self.pre = self.obj.with_suffix('.incbin.i')
        self.pre.write_text(f'const u8 gA[] = {INCBIN}("graphics/fonts/a.bin");\n'
            f'const u8 gRows[][16] = {{{INCBIN}("graphics/fonts/a.bin"), {INCBIN}("graphics/fonts/b.bin")}};\n'
            f'static const u8 sB[] = {INCBIN}("graphics/fonts/b.bin");\n')
        text = ('typedef unsigned char u8;\n'
            'const u8 gA[] = {' + ','.join(map(str, range(16))) + '};\n'
            'const u8 gRows[][16] = {{' + ','.join(map(str, range(16))) + '},{' + ','.join(map(str, range(16,32))) + '}};\n'
            'static const u8 sB[] = {' + ','.join(map(str, range(16,32))) + '};\n'
            'const u8 * const keep[] = {gA + 3, gRows[1] + 2, sB + 5};\n'
            'unsigned AgbMain(void) { return sizeof(gA) + sizeof(gRows) + sizeof(sB); }\n')
        self.source = self.root / 'probe.c'
        self.source.write_text(text)
        subprocess.run([tool('arm-none-eabi-gcc'), '-O2', '-ffunction-sections', '-fdata-sections',
            '-c', str(self.source), '-o', str(self.obj)], check=True)

    def tearDown(self):
        assets.ROOT = self.previous
        self.tmp.cleanup()

    def test_symbols_alignment_sizes_relocations_and_linked_index(self):
        before = assets.Elf(self.obj)
        assets.transform(self.obj, self.pre)
        after = assets.Elf(self.obj)
        for name in ('gA', 'gRows', 'sB'):
            self.assertEqual(before.symbols[name][:3], after.symbols[name][:3])
            section = after.sections[after.symbols[name][3]]
            self.assertEqual(section[1], 8)
            self.assertEqual(section[5], before.symbols[name][1])
        # Pointer-table bytes/addends and relocation records remain identical.
        for name in before.names:
            if name.startswith(('.rel.rodata.keep', '.rodata.keep', '.text')):
                self.assertEqual(before.data(before.sections[before.names.index(name)]),
                                 after.data(after.sections[after.names.index(name)]))
        linked = self.obj.with_suffix('.elf')
        subprocess.run([tool('arm-none-eabi-ld'), '--gc-sections', '--undefined=keep',
            '--entry=AgbMain', str(self.obj), '-o', str(linked)], check=True)
        assets.index(linked)
        rows = json.loads((self.root / '3ds_port/build/graphics-assets.json').read_text())
        self.assertEqual(len(rows), 4)
        multi = [r for r in rows if r['symbol'] == 'gRows']
        self.assertEqual(multi[1]['address'] - multi[0]['address'], 16)
        self.assertEqual(sum(r['size'] for r in rows), 64)

    def test_byte_mismatch_is_rejected_without_modifying_object(self):
        original = self.obj.read_bytes()
        (self.root / 'graphics/fonts/a.bin').write_bytes(b'\xff' * 16)
        with self.assertRaises(ValueError):
            assets.transform(self.obj, self.pre)
        self.assertEqual(self.obj.read_bytes(), original)

    def test_explicit_zero_row_preserves_size_and_interior_offsets(self):
        self.source.write_text(self.source.read_text().replace('},{', '},{},{', 1)
            .replace('gRows[1] + 2', 'gRows[2] + 2'))
        self.pre.write_text(self.pre.read_text().replace(
            f'{INCBIN}("graphics/fonts/a.bin"), {INCBIN}("graphics/fonts/b.bin")',
            f'{INCBIN}("graphics/fonts/a.bin"), {{}}, {INCBIN}("graphics/fonts/b.bin")'))
        subprocess.run([tool('arm-none-eabi-gcc'), '-O2', '-ffunction-sections', '-fdata-sections',
            '-c', str(self.source), '-o', str(self.obj)], check=True)
        before = assets.Elf(self.obj)
        assets.transform(self.obj, self.pre)
        after = assets.Elf(self.obj)
        self.assertEqual(before.symbols['gRows'][:3], after.symbols['gRows'][:3])
        self.assertEqual(after.symbols['gRows'][1], 48)
        rows = [r for r in json.loads(self.obj.with_suffix('.assets.json').read_text())
                if r['symbol'] == 'gRows']
        self.assertEqual([r['offset'] for r in rows], [0, 32])
        original = before.data(before.sections[before.symbols['gRows'][3]])
        self.assertEqual(original[16:32], bytes(16))
        for name in before.names:
            if name.startswith(('.rel.rodata.keep', '.rodata.keep', '.text')):
                self.assertEqual(before.data(before.sections[before.names.index(name)]),
                    after.data(after.sections[after.names.index(name)]))

    def test_empty_row_with_wrong_stride_is_rejected(self):
        text = f'const u8 row[][32] = {{{INCBIN}("graphics/fonts/a.bin"), {{}}}};'
        with self.assertRaises(ValueError):
            assets.payload_layout(text, 'row', [bytes(16)])

    def test_inline_spinda_bitmap_preserves_struct_and_pointer_semantics(self):
        (self.root / 'graphics/spinda_spots').mkdir()
        payload = bytes(range(32))
        (self.root / 'graphics/spinda_spots/spot_0.bin').write_bytes(payload)
        words = [int.from_bytes(payload[i:i+2], 'little') for i in range(0, 32, 2)]
        self.source.write_text('typedef unsigned char u8; typedef unsigned short u16;\n'
            'struct SpindaSpot {u8 x,y; u16 image[16];};\n'
            'static const struct SpindaSpot sSpots[] = {{16,7,{' + ','.join(map(str, words)) + '}}};\n'
            'const u16 * const keep[] = {sSpots[0].image + 3};\n'
            'unsigned AgbMain(void) {return sizeof(sSpots);}\n')
        call = 'INCBIN_' + 'U16'
        self.pre.write_text('static const struct SpindaSpot sSpots[] = '
            f'{{{{.x=16,.y=7,.image={call}("graphics/spinda_spots/spot_0.bin")}}}};')
        subprocess.run([tool('arm-none-eabi-gcc'), '-O2', '-ffunction-sections', '-fdata-sections',
            '-c', str(self.source), '-o', str(self.obj)], check=True)
        before = assets.Elf(self.obj)
        assets.transform(self.obj, self.pre)
        after = assets.Elf(self.obj)
        self.assertEqual(before.symbols['sSpots'][:3], after.symbols['sSpots'][:3])
        self.assertEqual(after.symbols['sSpots'][1], 34)
        row = json.loads(self.obj.with_suffix('.assets.json').read_text())[0]
        self.assertEqual((row['offset'], row['size'], row['prefix']), (2, 32, [16, 7]))
        for name in before.names:
            if name.startswith(('.rel.rodata.keep', '.rodata.keep', '.text')):
                self.assertEqual(before.data(before.sections[before.names.index(name)]),
                    after.data(after.sections[after.names.index(name)]))
        linked = self.obj.with_suffix('.elf')
        subprocess.run([tool('arm-none-eabi-ld'), '--gc-sections', '--undefined=keep',
            '--entry=AgbMain', str(self.obj), '-o', str(linked)], check=True)
        assets.index(linked)
        raw = (self.root / '3ds_port/romfs/engine/assets.bin').read_bytes()
        record = assets.RECORD.unpack_from(raw, assets.HEADER.size)
        self.assertEqual(record[4], 5 | (16 << 8) | (7 << 16) | (1 << 24))

    def test_arm_array_read_follows_the_data_boundary(self):
        self.source.write_text(self.source.read_text() + '\n#include "3ds_assets.h"\n'
            'unsigned ReadAssetByte(unsigned i) {CTR_ASSET_READ(gA, sizeof(gA)); return gA[i];}\n')
        subprocess.run([tool('arm-none-eabi-gcc'), '-O2', '-ffunction-sections', '-fdata-sections',
            '-DCTR_EXTERNAL_ASSETS=1', '-I' + str(ROOT/'3ds_port/include'),
            '-c', str(self.source), '-o', str(self.obj)], check=True)
        assembly = subprocess.check_output([tool('arm-none-eabi-objdump'), '-dr',
            '--disassemble=ReadAssetByte', str(self.obj)], text=True)
        self.assertLess(assembly.index('CtrAssets_RequireRange'), assembly.index('\tldrb'))
        before = assets.Elf(self.obj)
        assets.transform(self.obj, self.pre)
        after = assets.Elf(self.obj)
        name = '.text.ReadAssetByte'
        self.assertEqual(before.data(before.sections[before.names.index(name)]),
            after.data(after.sections[after.names.index(name)]))

    def test_relative_make_paths(self):
        previous = Path.cwd()
        try:
            os.chdir(self.root / '3ds_port')
            assets.transform(Path('build/game/probe.o'), Path('build/game/probe.incbin.i'))
            self.assertEqual(len(json.loads(Path('build/game/probe.assets.json').read_text())), 4)
        finally:
            os.chdir(previous)

    def test_non_file_and_mixed_tables_are_excluded(self):
        self.assertEqual(list(assets.declarations('const u8 table[] = {1,2};')), [])
        self.assertEqual(list(assets.declarations(f'const u8 table[] = {{1, {INCBIN}("graphics/fonts/a.bin")}};')), [])
        paths = list(assets.declarations(f'const u8 joined[] = {INCBIN}("graphics/fonts/a.bin", "graphics/fonts/b.bin");'))
        self.assertEqual(paths[0][1], ['graphics/fonts/a.bin', 'graphics/fonts/b.bin'])


if __name__ == '__main__':
    unittest.main()
