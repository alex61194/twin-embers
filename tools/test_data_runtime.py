#!/usr/bin/env python3
"""Exercise production CtrData startup/access using a host GNU stdio compiler."""
import argparse
from pathlib import Path
import os
import shlex
import struct
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'builder'))
from firered3ds_builder.pak import write_pak, PakReader, PakError, path_id
from firered3ds_builder.rom import SUPPORTED_SHA1
from host_compiler import compile_host


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--cc', default='gcc')
    args = parser.parse_args()
    dest = ROOT / 'build/tests'
    dest.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=dest) as tmp:
        tmp = Path(tmp)
        (tmp / '3ds.h').write_text('typedef int LightLock;\n'
            'static inline void LightLock_Init(LightLock *p) { *p = 0; }\n'
            'static inline void LightLock_Lock(LightLock *p) { (void)p; }\n'
            'static inline void LightLock_Unlock(LightLock *p) { (void)p; }\n')
        exe = tmp / 'runtime-test.exe'
        command = [args.cc, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
            '-DCTR_DATA_BACKEND=3', '-DCTR_DATA_DIR="data/"',
            '-DCTR_DATA_ROMFS_DIR="romfs/"', '-I' + str(tmp),
            '-I' + str(ROOT / '3ds_port/include'),
            str(ROOT / '3ds_port/tests/data_runtime_test.c'),
            str(ROOT / '3ds_port/src/3ds_data.c'),
            str(ROOT / '3ds_port/src/3ds_pak.c'), '-o', str(exe)]
        compile_host(command)
        (tmp / 'romfs/engine').mkdir(parents=True)
        (tmp / 'romfs/engine/abi.bin').write_bytes(struct.pack('<I', 123))
        (tmp / 'data').mkdir()
        pak = tmp / 'data/twinembers.pak'
        write_pak(pak, [('graphics/good.bin', b'\x11' * 4096),
                        ('graphics/bad.bin', b'\x22' * 8192)], 123, bytes.fromhex(SUPPORTED_SHA1))
        with PakReader(pak) as reader:
            offset = reader.entries[path_id('graphics/bad.bin')].offset
        original = pak.read_bytes()
        corrupt = bytearray(original)
        corrupt[offset + 73] ^= 1
        pak.write_bytes(corrupt)
        subprocess.run([str(exe), 'payload-bad'], cwd=tmp, check=True)
        try:
            with PakReader(pak) as reader:
                reader.verify()
        except PakError:
            pass
        else:
            raise AssertionError('PC verification accepted payload corruption')
        corrupt = bytearray(original)
        corrupt[60] ^= 1
        pak.write_bytes(corrupt)
        subprocess.run([str(exe), 'metadata-bad'], cwd=tmp, check=True)
        run_time_backend_checks(args, tmp)
    print('PASS: metadata startup; accessed CRCs; streams; exact sizes; PC full CRC scan; clean profile')


def run_time_backend_checks(args, tmp):
    """A clean image reads only the pack, whatever markers or files surround it."""
    exe = tmp / 'profile-test.exe'
    command = [args.cc, '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror',
        '-DCTR_DATA_DIR="data/"', '-DCTR_DATA_ROMFS_DIR="romfs/"', '-I' + str(tmp),
        '-I' + str(ROOT / '3ds_port/include'), str(ROOT / '3ds_port/tests/data_runtime_test.c'),
        str(ROOT / '3ds_port/src/3ds_data.c'), str(ROOT / '3ds_port/src/3ds_pak.c'), '-o', str(exe)]
    compile_host(command)
    for profile, marker, pack, mode in (
            (b'FR3DCLN\0', True, False, 'clean-missing-pack'),     # embedded marker must be ignored
            (b'FR3DCLN\0', False, False, 'clean-missing-pack'),
            (b'FR3DCLN\0', True, True, 'clean-reads-pack'),
            (b'FR3DDEV\0', True, False, 'development-uses-embedded')):
        work = tmp / f'profile-{mode}-{int(marker)}{int(pack)}'
        (work / 'romfs/engine').mkdir(parents=True)
        (work / 'data/devdata').mkdir(parents=True)
        (work / 'romfs/engine/abi.bin').write_bytes(struct.pack('<I', 123))
        (work / 'romfs/engine/profile.bin').write_bytes(profile)
        (work / 'romfs/only-in-romfs.bin').write_bytes(b'embedded game data')
        if marker:
            (work / 'romfs/data.embedded').write_text('x')
            if profile == b'FR3DCLN\0':   # a clean image must ignore this development switch too
                (work / 'data/devdata/.firered3ds-dev').write_text('x')
        if pack:
            write_pak(work / 'data/twinembers.pak', [('graphics/good.bin', b'\x11' * 4096)], 123, bytes.fromhex(SUPPORTED_SHA1))
        subprocess.run([str(exe), mode], cwd=work, check=True)


if __name__ == '__main__':
    main()
