"""Issue #87: a reversed compressed sample (Growl, Hyper Voice) that plays to its first sample asks SoundMainRAM_Unk2 (src/m4a_1.s)
for sample index -1. The block index was made with `lsr r3, #6`: block 0x03FFFFFF, and wav + 0x21 * 0x03FFFFFF + 0x10 is
wav + 0x84000000 - 0x11. The GBA bus ignores address bits 28-31, so there that reads the ROM just before the sample; the ARM11 has no
such mirror and aborts (data abort, far = 0x8495A493 in the report). Recipe 0259 uses `asr` under PLATFORM_3DS: block -1, the very
address the GBA reads through its mirror (wav - 0x11, inside the sample pack), and the same block for every index >= 0.

Host test: the recipe's text, the two instruction encodings, and a model of the block and address arithmetic (no emulator needed).
With arm-none-eabi-as on the PATH (or DEVKITARM set) the two variants are also assembled and the words compared."""
import json
import os
import shutil
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ROOT / 'patches/pokefirered/0259-m4a-reverse-block-asr.json'
M32 = 0xFFFFFFFF


def encode_mov_shift(rd, rm, kind, imm):
    """ARM `mov rd, rm, <kind> #imm` (data processing, MOV, register shifted by an immediate); kind 0 lsl, 1 lsr, 2 asr."""
    return 0xE1A00000 | (rd << 12) | (imm << 7) | (kind << 5) | rm


def lsr(v, n):
    return (v & M32) >> n


def asr(v, n):
    v &= M32
    return ((v >> n) | (M32 << (32 - n)) & M32) if v & 0x80000000 else v >> n


def unk2_source(block, wav):
    """The first byte Unk2 reads: wav + 0x21 * block + 0x10 (mul, add, add), as 32-bit registers."""
    return (0x21 * block + wav + 0x10) & M32


def recipe_text():
    doc = json.loads(RECIPE.read_text(encoding='utf-8'))
    assert [f['path'] for f in doc['files']] == ['src/m4a_1.s']
    return ''.join(line['port'] for line in doc['files'][0]['lines'] if 'port' in line)


class Recipe(unittest.TestCase):
    def test_the_recipe_is_the_arithmetic_shift_for_the_3ds_only(self):
        doc = json.loads(RECIPE.read_text(encoding='utf-8'))
        entries = doc['files'][0]['lines']
        ports = [e.get('port') for e in entries]
        i = ports.index('\t.ifdef PLATFORM_3DS\n')
        self.assertEqual(ports[i + 1], '\tmov r0, r3, asr 6\n')
        self.assertEqual(ports[i + 2], '\t.else\n')
        self.assertIsNone(ports[i + 3])                       # the game's own `mov r0, r3, lsr 6` line, kept as it was
        self.assertEqual(ports[i + 4], '\t.endif\n')
        self.assertNotIn('lsr 6', recipe_text())
        self.assertEqual(recipe_text().count('asr 6'), 1)

    def test_the_recipe_is_in_the_unk2_function_and_nothing_else_changes(self):
        doc = json.loads(RECIPE.read_text(encoding='utf-8'))
        added = [line['port'] for line in doc['files'][0]['lines'] if 'port' in line]
        # one function, one place: the added lines are the comment, the .ifdef block and the lines the old mov line leaves out of it
        self.assertLessEqual(len(added), 12)
        self.assertNotIn('SoundMainRAM_Unk1', ''.join(added))


class Encodings(unittest.TestCase):
    def test_words(self):
        self.assertEqual(encode_mov_shift(0, 3, 1, 6), 0xE1A00323)    # lsr r0, r3, #6 (the issue's file offset 0x8C8)
        self.assertEqual(encode_mov_shift(0, 3, 2, 6), 0xE1A00343)    # asr r0, r3, #6

    def test_only_the_shift_type_bit_differs(self):
        self.assertEqual(0xE1A00323 ^ 0xE1A00343, 0x60)


class Arithmetic(unittest.TestCase):
    WAV = 0x0095A4A4

    def test_forward_playback_is_identical(self):
        for r3 in list(range(0, 1 << 17)) + [0x7FFFFFFF, 0x1234567]:
            self.assertEqual(lsr(r3, 6), asr(r3, 6), hex(r3))
            self.assertEqual(r3 & 0x3F, r3 & 0x3F)                      # the offset in the block is not touched

    def test_the_old_shift_reproduces_the_report(self):
        block = lsr(M32, 6)
        self.assertEqual(block, 0x03FFFFFF)
        self.assertEqual(unk2_source(block, self.WAV), 0x8495A493)       # far in the Luma3DS dump
        self.assertGreaterEqual(unk2_source(block, self.WAV), 0x80000000)

    def test_the_new_shift_reads_where_the_gba_read_through_its_mirror(self):
        block = asr(M32, 6)
        self.assertEqual(block, M32)                                     # block -1
        src = unk2_source(block, self.WAV)
        self.assertEqual(src, self.WAV - 0x11)
        self.assertLess(src, 0x80000000)
        # on the GBA the sample is in the ROM (wav = 0x08xxxxxx): with the logical shift the read is at wav + 0x84000000 - 0x11, and
        # with address bits 28-31 ignored that is the ROM mirror at 0x0C000000: the same ROM offset as wav - 0x11
        wav_gba = 0x08000000 | self.WAV
        gba = unk2_source(lsr(M32, 6), wav_gba)
        self.assertEqual(gba & 0x0FFFFFFF & 0x01FFFFFF, (wav_gba - 0x11) & 0x01FFFFFF)
        self.assertEqual(src & 0x01FFFFFF, (wav_gba - 0x11) & 0x01FFFFFF)

    def test_a_block_is_one_header_byte_and_32_data_bytes(self):
        self.assertEqual(0x21, 1 + 32)

    def test_reversed_playback_to_the_first_sample(self):
        """A reversed sample of n samples asks for the indexes n-1 .. 0 and then -1 (the interpolation); only -1 differs."""
        n = 200
        old = [lsr(i & M32, 6) for i in range(n - 1, -2, -1)]
        new = [asr(i & M32, 6) for i in range(n - 1, -2, -1)]
        self.assertEqual(old[:-1], new[:-1])
        self.assertNotEqual(old[-1], new[-1])
        self.assertEqual(new[-1], M32)
        for blk in new:
            src = unk2_source(blk, self.WAV)
            self.assertLess(src + 0x21, 0x80000000)                      # every read stays inside the normal address space

    def test_the_block_cache_is_consistent(self):
        """Unk2 compares the block with the one it last decoded and decodes again only when it changed: -1 is a block of its own."""
        last = asr(0, 6)
        self.assertEqual(last, 0)
        self.assertNotEqual(asr(M32, 6), last)


def find_as():
    for cand in (shutil.which('arm-none-eabi-as'),
                 os.path.join(os.environ.get('DEVKITARM', ''), 'bin', 'arm-none-eabi-as'),
                 '/opt/devkitpro/devkitARM/bin/arm-none-eabi-as', 'C:/devkitPro/devkitARM/bin/arm-none-eabi-as.exe'):
        if cand and (os.path.exists(cand) or shutil.which(cand)):
            return cand
    return None


@unittest.skipUnless(find_as(), 'arm-none-eabi-as not found')
class Assembled(unittest.TestCase):
    SNIPPET = ('\t.syntax unified\n\t.text\n\t.arm\nf:\n\t.ifdef PLATFORM_3DS\n\tmov r0, r3, asr 6\n\t.else\n\tmov r0, r3, lsr 6\n\t.endif\n')

    def words(self, defsym):
        with tempfile.TemporaryDirectory() as d:
            src, obj, binf = Path(d, 'a.s'), Path(d, 'a.o'), Path(d, 'a.bin')
            src.write_text(self.SNIPPET)
            asm = find_as()
            cmd = [asm, '-mcpu=mpcore'] + (['--defsym', 'PLATFORM_3DS=1'] if defsym else []) + ['-o', str(obj), str(src)]
            subprocess.run(cmd, check=True, capture_output=True)
            objcopy = asm[:-len('as.exe')] + 'objcopy.exe' if asm.endswith('.exe') else asm[:-2] + 'objcopy'
            subprocess.run([objcopy, '-O', 'binary', str(obj), str(binf)], check=True, capture_output=True)
            data = binf.read_bytes()
        return int.from_bytes(data[:4], 'little'), len(data)

    def test_3ds_build_assembles_asr(self):
        self.assertEqual(self.words(True), (0xE1A00343, 4))

    def test_other_platforms_keep_lsr(self):
        self.assertEqual(self.words(False), (0xE1A00323, 4))


if __name__ == '__main__':
    unittest.main()
