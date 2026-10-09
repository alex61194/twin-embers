#!/usr/bin/env python3
"""Host production asset-loader checks: rebase, boundaries, manifest CRCs."""
import argparse
import os
import re
from pathlib import Path
import shutil
import struct
import subprocess
import tempfile
import zlib
from host_compiler import compile_host

ROOT = Path(__file__).resolve().parents[1]


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--tree', type=Path, help='also exercise the bootstrapped game bitmap consumer')
    ap.add_argument('--cc')
    ap.add_argument('--profile', action='store_true', help='exercise production profiling with a synthetic clock')
    args = ap.parse_args()
    gcc = str(Path('C:/devkitPro/msys2/mingw64/bin/gcc.exe')) if os.name == 'nt' else shutil.which('gcc')
    if args.cc: gcc = args.cc
    folder = ROOT / 'build/tests'
    folder.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=folder) as tmp:
        tmp = Path(tmp)
        exe = tmp / 'assets-test.exe'
        env = dict(os.environ)
        env['PATH'] = str(Path(gcc).parent) + os.pathsep + env.get('PATH', '')
        extra = []
        if args.profile:
            extra += [str(ROOT / '3ds_port/src/3ds_perf.c')]
        if args.tree:
            # Minimal SDK-free types; the compiled bitmap algorithm is the
            # actual patched game source, not a duplicated implementation.
            (tmp / 'global.h').write_text('#define PLATFORM_3DS 1\n#include <stdint.h>\n#include <stddef.h>\n'
                'typedef uint8_t u8; typedef uint16_t u16; typedef uint32_t u32; typedef int32_t s32;\n'
                '#include "3ds_assets.h"\n')
            shutil.copyfile(args.tree / 'include/blit.h', tmp / 'blit.h')
            extra += ['-DCTR_TEST_BLIT=1', '-I' + str(tmp),
                     '-Wno-sign-compare', str(args.tree / 'src/blit.c')]
            text = (args.tree / 'src/pokemon.c').read_text(encoding='utf-8')
            start = text.index('#define DRAW_SPINDA_SPOTS')
            lines = text[start:].splitlines(keepends=True)
            end = next(i for i, line in enumerate(lines) if not line.rstrip().endswith('\\'))
            macro = ''.join(lines[:end + 1])
            guard = next(line for line in text.splitlines() if line.startswith('#define SPINDA_SPOT_GRAPHICS_READ() CTR_ASSET_READ'))
            constants = '\n'.join(line for line in text.splitlines()
                if re.match(r'#define (FIRST_SPOT_COLOR|LAST_SPOT_COLOR|SPOT_COLOR_ADJUSTMENT)\b', line))
            (tmp / 'spinda.c').write_text('#include "global.h"\n'
                'extern unsigned char testStorage[];\n'
                'struct SpindaSpot {u8 x,y; u16 image[16];};\n'
                '#define ARRAY_COUNT(x) (sizeof(x)/sizeof((x)[0]))\n'
                '#define SPINDA_SPOT_HEIGHT 16\n#define SPINDA_SPOT_WIDTH 16\n#define TILE_SIZE_4BPP 32\n'
                + constants + '\n' + guard + '\n' + macro
                + '\nvoid TestDrawSpinda(u32 personality, u8 *dest) {\n'
                '#define sSpindaSpotGraphics (*(struct SpindaSpot (*)[1])(testStorage + 14))\n'
                'DRAW_SPINDA_SPOTS(personality, dest);\n#undef sSpindaSpotGraphics\n}\n'
                'void TestDrawSpindaReference(u32 personality, u8 *dest) {\n'
                'struct SpindaSpot sSpindaSpotGraphics[1] = {{16, 7, {0}}};\n'
                'for(unsigned i=0;i<16;i++) sSpindaSpotGraphics[0].image[i] = 2*i | ((2*i+1)<<8);\n'
                'DRAW_SPINDA_SPOTS(personality, dest);\n}\n')
            extra += ['-DCTR_TEST_SPINDA_DRAW=1', str(tmp / 'spinda.c')]
        compile_host([gcc, '-std=c11', '-D_GNU_SOURCE', '-O2', '-Wall', '-Wextra', '-Werror', '-pthread', *extra,
            '-DCTR_EXTERNAL_ASSETS=1', '-DCTR_PERF_ENABLED=' + str(int(args.profile)),
            '-DCTR_ASSETS_INDEX_PATH="assets.bin"', '-DCTR_ASSETS_BSS_START=testStorage',
            '-DCTR_ASSETS_BSS_END=(testStorage+1024)', '-I' + str(ROOT / '3ds_port/include'),
            str(ROOT / '3ds_port/tests/assets_runtime_test.c'),
            str(ROOT / '3ds_port/src/3ds_assets.c'), str(ROOT / '3ds_port/src/3ds_pak.c'),
            str(ROOT / '3ds_port/src/3ds_asset_memory.c'), str(ROOT / '3ds_port/src/3ds_bios.c'),
            str(ROOT / '3ds_port/src/3ds_dma.c'), str(ROOT / '3ds_port/src/gba_shadow.c'),
            '-o', str(exe)], check=True, env=env)
        (tmp / 'graphics/fonts').mkdir(parents=True)
        payload = bytes(range(16))
        (tmp / 'graphics/fonts/good.bin').write_bytes(payload)
        def fixture(address=0x08000010, size=16, path=0, crc=zlib.crc32(payload), group=1):
            strings = b'graphics/fonts/good.bin\0'
            body = struct.pack('<IIIII', address, size, path, crc, group) + strings
            return struct.pack('<4sIIIII', b'F3AS', 1, 0x08000000, 1, len(strings), zlib.crc32(body)) + body
        def run(raw, mode):
            (tmp / 'assets.bin').write_bytes(raw)
            subprocess.run([str(exe), mode], cwd=tmp, check=True, env=env)
        run(fixture(), 'good')
        run(fixture(crc=1), 'payload-bad')
        run(fixture(size=15), 'payload-bad')
        run(fixture(address=0x08000400), 'metadata-bad')
        run(fixture(address=0x07ffffff), 'metadata-bad')
        run(fixture(path=100), 'metadata-bad')
        run(fixture(group=99), 'metadata-bad')
        raw = bytearray(fixture())
        raw[20] ^= 1
        run(raw, 'metadata-bad')
        run(fixture()[:-1], 'metadata-bad')
        run(fixture() + b'\0', 'metadata-bad')
        run(fixture(group=2), 'lazy-good')
        run(fixture(group=2, crc=1), 'lazy-bad')
        strings = b'graphics/fonts/good.bin\0'
        body = struct.pack('<IIIII', 0x08000010, 16, 0, zlib.crc32(payload), 1)
        body += struct.pack('<IIIII', 0x08000020, 16, 0, zlib.crc32(payload), 2) + strings
        run(struct.pack('<4sIIIII', b'F3AS', 1, 0x08000000, 2, len(strings), zlib.crc32(body)) + body, 'adjacent')
        def warm(entries, mode):
            """entries: (path, group) in address order; every file is the same payload."""
            data = bytes(range(16))
            body, strings = b'', b''
            for k, (path, group) in enumerate(entries):
                (tmp / path).parent.mkdir(parents=True, exist_ok=True)
                (tmp / path).write_bytes(data)
                body += struct.pack('<IIIII', 0x08000010 + 16 * k, 16, len(strings), zlib.crc32(data), group)
                strings += path.encode() + b'\0'
            body += strings
            run(struct.pack('<4sIIIII', b'F3AS', 1, 0x08000000, len(entries), len(strings),
                            zlib.crc32(body)) + body, mode)
        f1 = ('graphics/fonts/f1.bin', 1)
        warm([f1, ('graphics/z5.bin', 5), ('graphics/b2.bin', 2), ('graphics/c3b.bin', 3),
              ('graphics/c3a.bin', 3), ('graphics/d4.bin', 4)], 'warm-order')
        warm([f1, ('graphics/slow.bin', 3)], 'warm-claim')
        warm([f1, ('graphics/once.bin', 3)], 'warm-fail')
        warm([f1, ('graphics/slow.bin', 3)], 'warm-shutdown')
        compressed = b'\x10\x04\0\0\0\x0a\x0b\x0c\x0d' + bytes(7)
        (tmp / 'graphics/fonts/good.bin').write_bytes(compressed)
        run(fixture(group=2, crc=zlib.crc32(compressed)), 'lazy-lz')
        if args.tree:
            payload = bytes(range(32))
            (tmp / 'graphics/fonts/good.bin').write_bytes(payload)
            strings = b'graphics/fonts/good.bin\0graphics/fonts/missing.bin\0'
            body = struct.pack('<IIIII', 0x08000010, 32, 0, zlib.crc32(payload), 5)
            body += struct.pack('<IIIII', 0x08000030, 32, 24, 1, 5) + strings
            run(struct.pack('<4sIIIII', b'F3AS', 1, 0x08000000, 2, len(strings), zlib.crc32(body))
                + body, 'lazy-blit')
        payload = bytes(range(32))
        (tmp / 'graphics/fonts/good.bin').write_bytes(payload)
        coords = (1 << 24) | (16 << 8) | (7 << 16)
        run(fixture(size=32, crc=zlib.crc32(payload), group=5 | coords), 'spinda-good')
        run(fixture(size=32, crc=zlib.crc32(payload), group=2 | coords), 'metadata-bad')
        run(fixture(size=16, group=5 | coords), 'metadata-bad')
        run(fixture(address=0x08000001, size=32, group=5 | coords), 'metadata-bad')
        strings = b'graphics/fonts/good.bin\0'
        body = struct.pack('<IIIII', 0x08000010, 16, 0, 0, 2)
        body += struct.pack('<IIIII', 0x08000020, 32, 0, 0, 5 | coords) + strings
        run(struct.pack('<4sIIIII', b'F3AS', 1, 0x08000000, 2, len(strings), zlib.crc32(body))
            + body, 'metadata-bad')
    print('PASS: asset rebase/bounds/CRC; lazy interior/cross-range reads; CPU/DMA/LZ consumers; resident cache')


if __name__ == '__main__':
    main()
