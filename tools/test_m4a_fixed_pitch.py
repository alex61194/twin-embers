"""Fixed-pitch DirectSound voices (drum kits) through the real ARM11 m4a mixer.

With --elf PATH (3ds_port/build/firered_game.elf from a bootstrapped tree): SoundMain is run
in unicorn on the linked game image, with a synthetic SoundInfo, one channel and a synthetic
ramp as its sample; nothing from the game is read. The real SampleFreqSet sets the mode first
(only the two samples-per-frame entries it reads are written into the pack-loaded table). FireRed's own mode is 13379 Hz (224
samples a frame); the 3DS build mixes at 31536 Hz (528). A fixed-pitch voice must take as many
source samples per frame as on the GBA (224), each held, not blended, as the GBA's DAC holds
it; resampled voices keep their own rate and interpolation; at 224 samples a frame the
original path is bit-exact. Needs unicorn and pyelftools; skipped without --elf.
"""
import argparse
import os
from pathlib import Path
import struct
import sys
import unittest

ELF = None

ID_NUMBER = 0x68736D53
SF_START = 0x80
TYPE_FIX = 0x08
# SoundInfo / SoundChannel / WaveData offsets (constants/m4a_constants.inc).
SI_IDENT, SI_DMA_COUNTER, SI_REVERB, SI_MAXCHANS, SI_MASTERVOL = 0, 4, 5, 6, 7
SI_DMA_PERIOD, SI_MAXLINES, SI_SPV, SI_PCMFREQ, SI_DIVFREQ = 11, 12, 16, 20, 24
SI_MPLAYHEAD, SI_CGBSOUND, SI_CHANS, SI_PCMBUF = 32, 40, 80, 848
PCM_DMA_BUF_SIZE = 1584
CH_STATUS, CH_TYPE, CH_RVOL, CH_LVOL, CH_ATTACK = 0, 1, 2, 3, 4
CH_COUNT, CH_FW, CH_FREQ, CH_WAV, CH_CUR = 24, 28, 32, 36, 40

MODE_13379, MODE_31536 = 0x40000, 0x90000
SPV = {MODE_13379: 224, MODE_31536: 528}     # samples a frame (gPcmSamplesPerVBlankTable)

BASE = 0x08000000           # synthetic area, clear of the 3DSX image
INFO, WAVE, STUB, STACK_TOP = BASE, BASE + 0x2000, BASE + 0x8000, BASE + 0x20000
WAVE_LEN = 4096


def ramp(i):
    return (i % 200) - 100   # every value distinct within 200 samples


class Machine:
    def __init__(self, elf_path):
        from elftools.elf.elffile import ELFFile
        from unicorn import Uc, UC_ARCH_ARM, UC_MODE_ARM
        self.uc = Uc(UC_ARCH_ARM, UC_MODE_ARM)
        with open(elf_path, 'rb') as f:
            elf = ELFFile(f)
            for seg in elf.iter_segments():
                if seg['p_type'] != 'PT_LOAD':
                    continue
                start = seg['p_vaddr'] & ~0xFFF
                end = (seg['p_vaddr'] + seg['p_memsz'] + 0xFFF) & ~0xFFF
                self.uc.mem_map(start, end - start)
                self.uc.mem_write(seg['p_vaddr'], seg.data())
            symtab = elf.get_section_by_name('.symtab')
            self.sym = {s.name: s['st_value'] for s in symtab.iter_symbols() if s.name}
        self.uc.mem_map(BASE, 0x20000)

    def call(self, address, *args):
        from unicorn.arm_const import UC_ARM_REG_SP, UC_ARM_REG_LR, UC_ARM_REG_R0
        for n, value in enumerate(args):
            self.uc.reg_write(UC_ARM_REG_R0 + n, value)
        self.uc.reg_write(UC_ARM_REG_SP, STACK_TOP - 0x100)
        self.uc.reg_write(UC_ARM_REG_LR, STUB + 4)
        self.uc.emu_start(address, STUB + 4, count=2_000_000)

    def run_frame(self, mode, channel, size=WAVE_LEN, loop=None, frames=1, step=None):
        uc = self.uc
        spv = SPV[mode]
        uc.mem_write(BASE, bytes(0x20000))
        uc.mem_write(STUB, struct.pack('<II', 0xE12FFF1E, 0xE12FFF1E))   # bx lr (ARM)
        table = self.sym['gPcmSamplesPerVBlankTable']
        for m, n in SPV.items():
            uc.mem_write(table + 2 * ((m >> 16) - 1), struct.pack('<H', n))
        info = bytearray(SI_PCMBUF + PCM_DMA_BUF_SIZE * 2)
        struct.pack_into('<I', info, SI_IDENT, ID_NUMBER)
        info[SI_MAXCHANS] = 1
        info[SI_MASTERVOL] = 15
        struct.pack_into('<I', info, SI_MPLAYHEAD, 0)
        struct.pack_into('<I', info, SI_CGBSOUND, STUB)
        ch = SI_CHANS
        info[ch + CH_STATUS] = SF_START
        info[ch + CH_TYPE] = channel['type']
        info[ch + CH_RVOL] = info[ch + CH_LVOL] = 128
        info[ch + CH_ATTACK] = 255
        info[ch + CH_ATTACK + 1] = info[ch + CH_ATTACK + 2] = 255   # decay, sustain: a steady note
        struct.pack_into('<IIII', info, ch + CH_COUNT, 0, 0, channel.get('freq', 0), WAVE)
        uc.mem_write(INFO, bytes(info))
        wave = struct.pack('<HBBIII', 0, 0, 0xC0 if loop is not None else 0, 0, loop or 0, size)
        wave += bytes((ramp(i) & 0xFF) for i in range(size + 1))
        uc.mem_write(WAVE, wave)
        uc.mem_write(self.sym['gGbaSoundInfoPtr'], struct.pack('<I', INFO))
        self.call(self.sym['SampleFreqSet'], mode)
        got = struct.unpack('<I', uc.mem_read(INFO + SI_SPV, 4))[0]
        assert got == spv, 'SampleFreqSet chose %d samples a frame' % got
        if step is not None:
            uc.mem_write(self.sym['gCtrFixedPitchStep'], struct.pack('<I', step))
        outs = []
        for _ in range(frames):
            # SoundMain counts ident up a frame; m4aSoundVSync would put it back.
            uc.mem_write(INFO + SI_IDENT, struct.pack('<I', ID_NUMBER))
            uc.mem_write(INFO + SI_PCMBUF, bytes(spv))
            self.call(self.sym['SoundMain'] | 1)
            outs += [b - 256 if b > 127 else b for b in uc.mem_read(INFO + SI_PCMBUF, spv)]
        count = struct.unpack('<I', uc.mem_read(INFO + ch + CH_COUNT, 4))[0]
        fw = struct.unpack('<I', uc.mem_read(INFO + ch + CH_FW, 4))[0]
        self.status = uc.mem_read(INFO + ch + CH_STATUS, 1)[0]
        return size - count, outs, fw

    def step(self):
        address = self.sym.get('gCtrFixedPitchStep')
        return struct.unpack('<I', self.uc.mem_read(address, 4))[0] if address else None


class FixedPitch(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        if not ELF:
            raise unittest.SkipTest('needs --elf (a linked firered_game.elf)')
        cls.m = Machine(ELF)

    def scale(self, sample):
        # envelope 255, master 15: volume 128 * 255 * 16 >> 4 >> 8 = 127; mixer >> 8 per side.
        return (sample * 127) >> 8

    def test_gba_rate_is_bit_exact(self):
        taken, out, fw = self.m.run_frame(MODE_13379, {'type': TYPE_FIX})
        self.assertEqual(self.m.step(), 0x800000)
        self.assertEqual(taken, 224)
        self.assertEqual(out, [self.scale(ramp(i)) for i in range(224)])

    def test_native_rate_keeps_the_gba_tempo_and_holds_samples(self):
        taken, out, fw = self.m.run_frame(MODE_31536, {'type': TYPE_FIX})
        self.assertIn(taken, (223, 224), 'a fixed-pitch voice took %d samples in a frame' % taken)
        step = (0x800000 * 224 + 264) // 528
        expect = [self.scale(ramp((i * step) >> 23)) for i in range(528)]
        self.assertEqual(out, expect, 'fixed-pitch output is not the GBA samples, each held')
        runs = sum(1 for i in range(1, 528) if out[i] != out[i - 1])
        self.assertLess(runs, 230, 'samples are blended, not held')

    def held(self, n, size, loop=None):
        """The GBA's samples, each held for 528/224 mixed samples: index, then the wave's loop."""
        step = (0x800000 * 224 + 264) // 528
        out, index, frac = [], 0, 0
        for _ in range(n):
            if index is None:
                out.append(0)
                continue
            out.append(self.scale(ramp(index)))
            frac += step
            index += frac >> 23
            frac &= 0x7FFFFF
            if index >= size:
                index = None if loop is None else loop + (index - size) % (size - loop)
        return out

    def test_sample_end_mid_frame(self):
        taken, out, fw = self.m.run_frame(MODE_31536, {'type': TYPE_FIX}, size=150)
        self.assertEqual(out, self.held(528, 150))
        self.assertEqual(self.m.status, 0, 'the channel did not stop at the end of its sample')

    def test_looping_sample_across_frames(self):
        taken, out, fw = self.m.run_frame(MODE_31536, {'type': TYPE_FIX}, size=300, loop=120, frames=4)
        self.assertEqual(out, self.held(4 * 528, 300, 120))
        self.assertNotEqual(self.m.status, 0)

    def test_unset_step_is_the_original_path(self):
        # 0 (zero-initialised, before SampleFreqSet) must not stall the voice: one sample a mix.
        taken, out, fw = self.m.run_frame(MODE_31536, {'type': TYPE_FIX}, step=0)
        self.assertEqual(taken, 528)
        self.assertEqual(out, [self.scale(ramp(i)) for i in range(528)])

    def test_resampled_voices_unchanged(self):
        # A resampled voice at the frequency that plays one sample per output sample at 13379 Hz.
        freq = 13379 * 1024
        taken224, _, _ = self.m.run_frame(MODE_13379, {'type': 0, 'freq': freq})
        taken528, _, _ = self.m.run_frame(MODE_31536, {'type': 0, 'freq': freq})
        self.assertAlmostEqual(taken224, taken528, delta=2)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--elf', type=Path)
    args, rest = parser.parse_known_args()
    ELF = str(args.elf) if args.elf else os.environ.get('M4A_ELF')
    unittest.main(argv=[sys.argv[0]] + rest)
