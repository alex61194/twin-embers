import argparse
import hashlib
from pathlib import Path
from .build import build
from .rom import load_rom, SUPPORTED_SHA1
from .pak import PakReader, PakError


def main() -> None:
    parser = argparse.ArgumentParser(prog='firered3ds-builder',
                                     description='Builds twinembers.pak from your own Pokemon FireRed ROM')
    sub = parser.add_subparsers(dest='command', required=True)
    verify = sub.add_parser('verify-rom', help='check that a ROM is the supported dump')
    verify.add_argument('rom', type=Path)
    make = sub.add_parser('build', help='create the data pack the 3DS game reads')
    make.add_argument('--rom', type=Path, required=True)
    make.add_argument('--output', type=Path, default=Path('twinembers.pak'))
    make.add_argument('--recipe', type=Path, help=argparse.SUPPRESS)
    pack = sub.add_parser('verify-pak', help='check every payload of an existing pack')
    pack.add_argument('pak', type=Path)
    pack.add_argument('--abi', type=lambda s: int(s, 16), required=True)
    args = parser.parse_args()
    try:
        if args.command == 'verify-rom':
            print(f'ROM verified: {load_rom(args.rom).sha1}')
        elif args.command == 'build':
            rom = load_rom(args.rom)
            print(f'ROM verified: {rom.sha1}')
            result = build(args.rom, args.recipe, args.output)
            digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
            print(f"Wrote {args.output}: {result['entries']} entries, {args.output.stat().st_size} bytes, "
                  f"engine ABI {result['abi']:08x}, SHA-256 {digest}")
            print('Copy it to /3ds/twinembers/twinembers.pak on the SD card.')
        else:
            with PakReader(args.pak, args.abi, bytes.fromhex(SUPPORTED_SHA1)) as reader:
                print(f'Verified {reader.verify()} payloads')
    except (ValueError, OSError, KeyError, TypeError, IndexError, PakError) as exc:
        parser.exit(1, f'builder: {exc}\n')
