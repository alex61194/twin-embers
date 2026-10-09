#!/usr/bin/env python3
"""Stage the RomFS and, for private images, the game data.

The executable's RomFS always holds the engine metadata: the ABI word, the
graphics index (assets.bin) and the REX map (rex.bin). The game's own bytes are
reconstructed from a FireRed ROM by the Builder recipe and either

  --embed 1   copied into the RomFS (PRIVATE development image only), or
  --pack      written to build/twinembers.pak, exactly what the Builder gives players.

A public image (--embed 0, no --pack) needs no ROM at all and contains no game data.
"""
import argparse
import os
from pathlib import Path
import shutil
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'builder'))
from firered3ds_builder.pak import write_pak, PakReader
from firered3ds_builder.recipe import load_recipe, reconstruct
from firered3ds_builder.rom import SUPPORTED_SHA1, load_rom

RECIPE = ROOT / 'builder/firered3ds_builder/recipes/firered-41cb23d8.json'
ENGINE_FILES = ('engine/assets.bin', 'engine/rex.bin')
# romfs:/engine/profile.bin tells the runtime which data backends it may use.
PROFILES = {'development': b'FR3DDEV\0', 'clean': b'FR3DCLN\0'}


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--embed', type=int, choices=(0, 1), default=0)
    ap.add_argument('--pack', action='store_true')
    ap.add_argument('--rom', type=Path, default=os.environ.get('FIRERED_ROM') or None)
    ap.add_argument('--recipe', type=Path, default=RECIPE)
    ap.add_argument('--dest', type=Path, default=ROOT / '3ds_port/build/runtime-romfs')
    ap.add_argument('--profile', choices=('development', 'clean'), default='development')
    args = ap.parse_args()
    port = ROOT / '3ds_port'
    recipe = load_recipe(args.recipe)
    abi = recipe['engine_abi']
    dest = args.dest if args.dest.is_absolute() else port / args.dest
    if args.embed:
        sys.exit('staging: embedded game data is excluded from this distribution')
    if args.profile == 'clean' and (args.embed or args.pack or args.rom):
        sys.exit('staging: a clean release image never carries game data and needs no ROM')
    shutil.rmtree(dest, ignore_errors=True)
    for name in ENGINE_FILES:
        target = dest / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(port / 'romfs' / name, target)
    (dest / 'engine/abi.bin').write_bytes(struct.pack('<I', abi))
    (dest / 'engine/profile.bin').write_bytes(PROFILES[args.profile])
    files = []
    if args.embed or args.pack:
        if not args.rom:
            sys.exit('staging: this private image needs a FireRed ROM: pass --rom or set FIRERED_ROM '
                     '(SHA-1 %s). A public image (CLEAN_RELEASE=1) does not.' % SUPPORTED_SHA1)
        rom = load_rom(args.rom)
        files = reconstruct(recipe, rom.data)
    if args.embed:
        for name, data in files:
            target = dest / name
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        (dest / 'data.embedded').write_text('Private development image: the game data is inside this file\n')
    if args.pack:
        pak = port / 'build/twinembers.pak'
        write_pak(pak, files, abi, bytes.fromhex(SUPPORTED_SHA1))
        with PakReader(pak, abi, bytes.fromhex(SUPPORTED_SHA1)) as reader:
            reader.verify()
    print(f'engine ABI {abi:08x}; embed={args.embed}; pack={int(args.pack)}; payload files={len(files)}')


if __name__ == '__main__':
    main()
