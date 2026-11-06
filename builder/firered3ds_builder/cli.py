import argparse
import hashlib
from pathlib import Path
from .build import build
from . import gen4_hd, gen4_sprites
from .rom import load_rom, SUPPORTED_ROMS
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
    make.add_argument('--npc-sprites', action='store_true',
                      help="also download the field's people (Facets' sets; needs internet)")
    make.add_argument('--battle-backgrounds', action='store_true',
                      help='also download the battle backgrounds (LibertyTwins\' set through Facets; needs internet)')
    make.add_argument('--player-sprites', action='store_true',
                      help="also download the player as Ash and Green (Emerald Lens' HGSS-style looks; needs internet)")
    make.add_argument('--follower-sprites', action='store_true',
                      help="also download the Pokemon that walk behind you (HeartGold's; needs internet)")
    make.add_argument('--hd-backgrounds', action='store_true',
                      help="also download the HD battle scenes (Bentley734's HD Dynamic Battlefields; needs internet)")
    make.add_argument('--extra-dir', type=Path, help=argparse.SUPPRESS)   # local test files, added as they are
    make.add_argument('--gen4-sprites', action='store_true',
                      help='also download HeartGold battle sprites (optional, needs internet)')
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
            extra = []
            groups = tuple(group for flag, group in ((args.gen4_sprites, gen4_sprites.GROUP_POKEMON),
                                                     (args.npc_sprites, gen4_sprites.GROUP_NPC),
                                                     (args.battle_backgrounds, gen4_sprites.GROUP_BG),
                                                     (args.player_sprites, gen4_sprites.GROUP_PLAYER),
                                                     (args.follower_sprites, gen4_sprites.GROUP_FOLLOWER),
                                                     (args.hd_backgrounds, gen4_sprites.GROUP_HD)) if flag)
            if groups:
                extra, info = gen4_sprites.pack_files(gen4_sprites.Downloader(gen4_sprites.cache_dir()), groups=groups)
                if args.gen4_sprites:
                    print(f"HeartGold battle sprites: {info['species']} species "
                          f"({info['downloaded']} files downloaded), {len(info['missing'])} missing, "
                          f"{info['trainers']} trainer pictures, {info['backs']} player backs")
                if args.npc_sprites:
                    print(f"People of the field: {info['npcs']} of {len(gen4_sprites.NPC_FILES)}")
                if args.battle_backgrounds:
                    print(f"Battle backgrounds: {info['backgrounds']} of 20 terrains")
                if args.player_sprites:
                    print(f"The player as Ash and Green: {info['player']} pictures")
                if args.follower_sprites:
                    print(f"Pokemon that walk behind you: {info['followers']} of {len(gen4_sprites.FOLLOWER_FILES)} files")
                if args.hd_backgrounds:
                    print(f"HD battle scenes: {info['hd']} of {len(gen4_hd.FILES)} files")
            if args.extra_dir:
                # Files made by a local tool (tools/dev_hd_battlefields.py), each under its path in the folder.
                extra += [(file.relative_to(args.extra_dir).as_posix(), file.read_bytes())
                          for file in sorted(args.extra_dir.rglob('*')) if file.is_file()]
                print(f'Extra files: {len(extra)} in all')
            result = build(args.rom, args.recipe, args.output, extra)
            digest = hashlib.sha256(args.output.read_bytes()).hexdigest()
            print(f"Wrote {args.output}: {result['entries']} entries, {args.output.stat().st_size} bytes, "
                  f"engine ABI {result['abi']:08x}, SHA-256 {digest}")
            print('Copy it to /3ds/twinembers/twinembers.pak on the SD card.')
        else:
            with PakReader(args.pak, args.abi, None) as reader:
                if reader.rom_sha1.hex() not in SUPPORTED_ROMS:
                    raise PakError('pack built from an unsupported ROM')
                print(f'Verified {reader.verify()} payloads ({SUPPORTED_ROMS[reader.rom_sha1.hex()]} ROM)')
    except (ValueError, OSError, KeyError, TypeError, IndexError, PakError) as exc:
        parser.exit(1, f'builder: {exc}\n')
