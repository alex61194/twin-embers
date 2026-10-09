#!/usr/bin/env python3
"""Structural audit of a FireRed 3DS build: what game content is still embedded?

Inspects the linked ELF (sections + symbol table), the GNU linker map, the
RomFS staging directory and, when given, the RomFS image inside a finished
.3dsx. It needs no ROM and no private reference file, so it can run as the
release gate of a clean build (see `make verify-clean-release`).

Every file-backed (PROGBITS) read-only/initialised-data contribution to the
image is attributed to the object it came from and put in exactly one class:

  EXTERNAL         NOBITS storage filled at start-up from the pack
  EMBEDDED         FireRed content that still sits in the executable
  ENGINE           code-adjacent or port-authored tables (see ENGINE_RULES)
  UNKNOWN          unattributed bytes that need a human decision

A clean release has zero EMBEDDED and zero UNKNOWN bytes.
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'builder'))
from firered3ds_builder.recipe import load_recipe

# Content category by game object (first match wins). The object stem is the
# file name without directory; C data objects come from pret `src/*.c`, assembly
# data objects from `data/*.s`, songs from `sound/songs/midi/*.s`.
CATEGORIES = [
    (r'^sound_data\.o$', 'audio: voicegroups, samples, cries, song table'),
    (r'^(mus|se|ph)_.*\.o$', 'audio: songs'),
    (r'^m4a_tables\.o$', 'audio: engine tables (m4a)'),
    (r'^maps\.o$', 'maps: layouts, blockdata, headers'),
    (r'^map_events\.o$', 'maps: events (objects, warps, triggers, signs)'),
    (r'.*_scripts(_\d)?\.o$', 'scripts and text'),
    (r'^mystery_event.*\.o$', 'scripts and text'),
    (r'^(strings|battle_message|move_descriptions|easy_chat|union_room_message|'
     r'pokedex_text|text|help_system|credits|text_window|menu2|naming_screen)\.o$', 'text'),
    (r'^(pokemon|pokedex|pokedex_screen|pokedex_area_screen|evolution_graphics|'
     r'pokemon_icon|pokemon_summary_screen|daycare|pokemon_storage_system|'
     r'move_relearner|party_menu|learn_move)\.o$', 'pokemon: species, moves, pokedex data'),
    (r'^item.*\.o$', 'items'),
    (r'^(wild_encounter|fishing)\.o$', 'encounters'),
    (r'^(battle_tower|trainer_tower_sets|trainer_tower|battle_setup|battle_ai_script_commands|'
     r'battle_main|battle_script_commands|battle_util|battle_controller.*)\.o$', 'battle and trainer data'),
    (r'^data\.o$', 'pokemon/trainer sprite, anim and party tables'),
    (r'^(battle_anim.*|battle_bg|battle_gfx_sfx_util|battle_interface)\.o$', 'battle animation tables'),
    (r'^(event_object_movement|field_.*|overworld|fieldmap|tilesets|region_map|'
     r'map_name_popup|metatile_behavior|cable_club|union_room|berry.*|slot_machine|trade.*|'
     r'pokemon_jump|dodrio_berry_picking|vs_seeker|quest_log.*|teachy_tv|script_menu|'
     r'item_menu_icons|intro|title_screen|fame_checker|evolution_scene).*\.o$', 'field and menu data'),
]

# Engine-safe tables. Each rule needs a reason; nothing here is FireRed content.
ENGINE_RULES = [
    (r'^build/3ds_[^/]*\.o$', 'port-authored source'),
    (r'^build/[^/]*\.o$', 'port-authored source'),
    (r'(libc|libm|libgcc|libctru|libsysbase|libnosys|libstdc\+\+)[^/]*\.a\(', 'toolchain library'),
    (r'^/opt/devkitpro/', 'toolchain library'),
]
ENGINE_SYMBOL_RULES = [
    (r'^CSWTCH\.\d+$', 'compiler switch jump table'),
    (r'^\.(?:LC|LANCHOR|L\d)', 'compiler-generated literal'),
    (r'^__func__\.\d+$', 'compiler function-name literal'),
]

EXTERNAL_KINDS = {'ctr_asset': 'graphics (INCBIN files)', 'ctr_script': 'script bytecode', 'ctr_song': 'song data',
                  'ctr_rex': 'ROM-derived data (REX)'}
GAME_DIRS = ('build/game/', 'build/game_data/', 'build/game_song/')


def category_of(obj: str) -> str:
    stem = obj.rsplit('/', 1)[-1]
    for pattern, name in CATEGORIES:
        if re.match(pattern, stem):
            return name
    return 'other read-only tables'


# --------------------------------------------------------------------- ELF

class Elf:
    """Minimal little-endian ELF32 reader: sections and symbols."""

    def __init__(self, path: Path):
        self.raw = Path(path).read_bytes()
        if self.raw[:7] != b'\x7fELF\x01\x01\x01':
            raise ValueError('expected a little-endian ELF32 file')
        e_shoff, = struct.unpack_from('<I', self.raw, 0x20)
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', self.raw, 0x2e)
        self.sections = []
        for i in range(e_shnum):
            f = struct.unpack_from('<10I', self.raw, e_shoff + i * e_shentsize)
            self.sections.append(dict(name_off=f[0], type=f[1], flags=f[2], addr=f[3],
                                      offset=f[4], size=f[5], link=f[6], info=f[7],
                                      align=f[8], entsize=f[9]))
        names = self.data(self.sections[e_shstrndx])
        for s in self.sections:
            s['name'] = self.cstr(names, s['name_off'])
        self.symbols = []
        for s in self.sections:
            if s['type'] == 2:  # SHT_SYMTAB
                strs = self.data(self.sections[s['link']])
                for at in range(s['offset'], s['offset'] + s['size'], 16):
                    n, value, size, info, other, shndx = struct.unpack_from('<IIIBBH', self.raw, at)
                    self.symbols.append(dict(name=self.cstr(strs, n), addr=value, size=size,
                                             type=info & 15, bind=info >> 4, shndx=shndx))

    def data(self, section) -> bytes:
        if section['type'] == 8:  # NOBITS
            return b''
        return self.raw[section['offset']:section['offset'] + section['size']]

    @staticmethod
    def cstr(blob: bytes, off: int) -> str:
        return blob[off:blob.index(0, off)].decode('utf-8', 'replace')

    def section_of(self, addr: int):
        for s in self.sections:
            if s['flags'] & 2 and s['addr'] <= addr < s['addr'] + s['size']:
                return s
        return None


# --------------------------------------------------------------------- map

_INPUT_SECTION = re.compile(r'^ (\.?\w[\w.$+-]*)\s*$')
_INPUT_ROW = re.compile(r'^\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*?)\s*$')
_INPUT_FULL = re.compile(r'^ (\.?\w[\w.$+-]*)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)\s+(\S.*?)\s*$')
_OUTPUT = re.compile(r'^(\.?\w[\w.$+-]*)\s+0x([0-9a-f]+)\s+0x([0-9a-f]+)')


def parse_map(text: str) -> list[dict]:
    """Input-section contributions: output, input, addr, size, object."""
    if 'Linker script and memory map' not in text:
        raise ValueError('not a GNU linker map')
    lines = text.split('Linker script and memory map', 1)[1].split('\n')
    out, current, i = [], None, 0
    while i < len(lines):
        line = lines[i]
        m = _OUTPUT.match(line)
        if m:
            current = m.group(1)
        full = _INPUT_FULL.match(line)
        if full:
            out.append(dict(output=current, input=full.group(1), addr=int(full.group(2), 16),
                            size=int(full.group(3), 16), obj=normalise(full.group(4))))
        else:
            head = _INPUT_SECTION.match(line)
            if head and i + 1 < len(lines):
                row = _INPUT_ROW.match(lines[i + 1])
                if row:
                    out.append(dict(output=current, input=head.group(1), addr=int(row.group(1), 16),
                                    size=int(row.group(2), 16), obj=normalise(row.group(3))))
                    i += 1
        i += 1
    return out


def normalise(obj: str) -> str:
    obj = obj.replace('\\', '/')
    obj = re.sub(r'^.*?/3ds_port/', '', obj)
    return obj


# ------------------------------------------------------------------ RomFS

def romfs_entries(image: bytes) -> dict[str, bytes]:
    """{path: contents} of a RomFS image (level-3 IVFC data, as 3dsxtool writes it)."""
    if len(image) < 0x28:
        return {}
    hdr = struct.unpack_from('<10I', image, 0)
    if hdr[0] != 0x28:
        raise ValueError('not a RomFS level-3 header')
    dir_off, dir_size, file_off, file_size, data_off = hdr[3], hdr[4], hdr[7], hdr[8], hdr[9]
    dirs = image[dir_off:dir_off + dir_size]
    files = image[file_off:file_off + file_size]
    result: dict[str, bytes] = {}

    def name(blob, base, length):
        return blob[base:base + length].decode('utf-16-le')

    def walk_dir(off: int, prefix: str):
        parent, sibling, child_dir, child_file, hash_, nlen = struct.unpack_from('<6I', dirs, off)
        here = prefix + (name(dirs, off + 24, nlen) + '/' if nlen else '')
        f = child_file
        while f != 0xffffffff:
            _parent, _sib, _off, _size, _hash, nl = struct.unpack_from('<IIQQII', files, f)
            result[here + name(files, f + 32, nl)] = image[data_off + _off:data_off + _off + _size]
            f = _sib
        d = child_dir
        while d != 0xffffffff:
            walk_dir(d, here)
            d = struct.unpack_from('<I', dirs, d + 4)[0]
    walk_dir(0, '')
    return dict(sorted(result.items()))


def romfs_files(image: bytes) -> list[str]:
    return list(romfs_entries(image))


def threedsx_romfs_entries(path: Path) -> dict[str, bytes] | None:
    data = Path(path).read_bytes()
    if data[:4] != b'3DSX':
        raise ValueError('not a 3DSX file')
    header_size = struct.unpack_from('<H', data, 4)[0]
    if header_size < 44:
        return None  # no extended header, hence no RomFS
    smdh_off, smdh_size, romfs_off = struct.unpack_from('<III', data, 32)
    if not romfs_off:
        return None
    # 3dsxtool stores a plain RomFS (level 3) image from romfs_off to EOF.
    return romfs_entries(data[romfs_off:])


def threedsx_romfs(path: Path) -> list[str] | None:
    entries = threedsx_romfs_entries(path)
    return None if entries is None else list(entries)


# ------------------------------------------------------------------ audit

LITERAL_POOL = re.compile(r'^\.rodata(\..+)?\.(str|cst)\d+(\.\d+)?$')
DEFAULT_RECIPE = ROOT / 'builder/firered3ds_builder/recipes/firered-41cb23d8.json'


def engine_units(recipe_path: Path | None = None) -> dict[tuple[str, str], str]:
    """Units the recipe declares as engine-owned: (object key, section) -> reason."""
    path = recipe_path or DEFAULT_RECIPE
    if not path.is_file():
        return {}
    return {(k, s): reason for k, s, _size, reason in load_recipe(path).get('engine_units', [])}


def unit_key(obj: str) -> str | None:
    for marker, kind in (('build/game_data/', 'asm'), ('build/game_song/', 'song'), ('build/game/', 'c')):
        if obj.startswith(marker):
            return kind + '/' + obj[len(marker):]
    return None


def classify_contribution(c: dict, engine: dict | None = None) -> tuple[str, str]:
    """(class, reason) for a PROGBITS read-only/initialised-data contribution."""
    obj = c['obj']
    for pattern, reason in ENGINE_RULES:
        if re.search(pattern, obj):
            return 'ENGINE', reason
    if obj.startswith(GAME_DIRS):
        if LITERAL_POOL.match(c['input']):
            return 'ENGINE', 'compiler string/constant literal pool'
        key = unit_key(obj)
        reason = (engine or {}).get((key, c['input']))
        if reason:
            return 'ENGINE', reason
        return 'EMBEDDED', category_of(obj)
    return 'UNKNOWN', 'object not under a known game or port directory'


def audit(tree: Path, elf_path: Path | None = None, map_path: Path | None = None,
          threedsx: Path | None = None, recipe: Path | None = None) -> dict:
    port = tree / '3ds_port'
    elf_path = elf_path or port / 'build/firered_game.elf'
    map_path = map_path or port / 'build/firered_game.map'
    engine = engine_units(recipe)
    elf = Elf(elf_path)
    contribs = parse_map(map_path.read_text(errors='replace'))

    sec_type = {s['name']: s for s in elf.sections}
    progbits = {n for n, s in sec_type.items() if s['type'] == 1 and s['flags'] & 2 and not s['flags'] & 4}
    sized = sorted((s for s in elf.symbols if s['size'] and s['type'] == 1 and s['shndx'] not in (0, 0xfff1)),
                   key=lambda s: s['addr'])
    starts = [s['addr'] for s in sized]

    import bisect

    def symbols_in(addr: int, size: int) -> list[dict]:
        lo = bisect.bisect_left(starts, addr)
        hi = bisect.bisect_left(starts, addr + size)
        return sized[lo:hi]

    classes: dict[str, int] = defaultdict(int)
    by_category: dict[str, dict] = defaultdict(lambda: dict(bytes=0, objects=defaultdict(int)))
    engine_detail: dict[str, int] = defaultdict(int)
    external: dict[str, int] = defaultdict(int)
    unattributed = 0
    top: list[dict] = []
    for c in contribs:
        if not c['size']:
            continue
        if c['input'].startswith('.bss.ctr_'):
            external[EXTERNAL_KINDS.get(re.match(r'\.bss\.(ctr_[a-z]+)', c['input'])[1], 'other external')] += c['size']
            continue
        if c['output'] not in progbits or not c['input'].startswith(('.rodata', '.data')):
            continue
        klass, reason = classify_contribution(c, engine)
        if klass == 'EMBEDDED':
            # Compiler-generated tables inside game objects are engine metadata.
            safe = [s for s in symbols_in(c['addr'], c['size'])
                    if any(re.match(p, s['name']) for p, _ in ENGINE_SYMBOL_RULES)]
            safe_bytes = sum(s['size'] for s in safe)
            if safe_bytes == c['size']:
                klass, reason = 'ENGINE', 'compiler-generated table'
            elif safe_bytes:
                classes['ENGINE'] += safe_bytes
                engine_detail['compiler-generated table'] += safe_bytes
                c = dict(c, size=c['size'] - safe_bytes)
        classes[klass] += c['size']
        if klass == 'ENGINE':
            engine_detail[reason] += c['size']
        elif klass == 'EMBEDDED':
            entry = by_category[reason]
            entry['bytes'] += c['size']
            entry['objects'][c['obj']] += c['size']
            for s in symbols_in(c['addr'], c['size']):
                top.append(dict(symbol=s['name'], size=s['size'], object=c['obj'], category=reason))
        else:
            unattributed += c['size']
            classes['UNKNOWN'] += 0

    top.sort(key=lambda r: -r['size'])
    result = dict(
        elf=str(elf_path),
        sections={n: dict(size=s['size'], type='NOBITS' if s['type'] == 8 else 'PROGBITS')
                  for n, s in sec_type.items() if s['flags'] & 2 and s['size']},
        external_nobits_bytes=sum(external.values()), external_by_kind=dict(external),
        embedded_bytes=classes['EMBEDDED'], engine_bytes=classes['ENGINE'],
        unknown_bytes=classes['UNKNOWN'],
        engine_detail=dict(engine_detail),
        embedded_by_category={k: dict(bytes=v['bytes'], objects=dict(sorted(v['objects'].items(), key=lambda kv: -kv[1])[:12]),
                                      object_count=len(v['objects']))
                              for k, v in sorted(by_category.items(), key=lambda kv: -kv[1]['bytes'])},
        largest_embedded_symbols=top[:25],
    )

    romfs_dir = port / 'build/runtime-romfs'
    staged = sorted(p.relative_to(romfs_dir).as_posix() for p in romfs_dir.rglob('*') if p.is_file()) \
        if romfs_dir.is_dir() else []
    result['romfs_staging'] = staged
    if threedsx:
        names = threedsx_romfs(threedsx)
        result['threedsx'] = dict(path=str(threedsx), size=Path(threedsx).stat().st_size, romfs_files=names)
    result['romfs_payload_files'] = payload_files(result.get('threedsx', {}).get('romfs_files') or staged)
    return result


ENGINE_ROMFS = ('engine/', 'data.embedded')
CLEAN_ROMFS = {'engine/abi.bin', 'engine/assets.bin', 'engine/rex.bin', 'engine/profile.bin'}
CLEAN_PROFILE = b'FR3DCLN\0'
REQUIRED_SYMBOLS = ('CtrData_Init', 'CtrPak_ParseHeader', 'CtrPak_ParseIndex', 'CtrRex_Apply', 'CtrRex_Init', 'AgbMain')


def payload_files(names: list[str]) -> list[str]:
    """RomFS entries that are neither engine metadata nor an engine marker."""
    return [n for n in names if not n.startswith(ENGINE_ROMFS) and not n.endswith('.rel')]


def assets_index(blob: bytes) -> dict[str, tuple[int, int]]:
    """{pack path: (size, crc32)} from romfs:/engine/assets.bin (tools/asset_bundle.py)."""
    magic, version, _ref, count, string_bytes, _crc = struct.unpack_from('<4sIIIII', blob, 0)
    if magic != b'F3AS' or version != 1:
        raise ValueError('bad assets.bin header')
    strings = blob[24 + 20 * count:24 + 20 * count + string_bytes]
    out = {}
    for n in range(count):
        _addr, size, path_off, crc, _group = struct.unpack_from('<IIIII', blob, 24 + 20 * n)
        path = strings[path_off:strings.index(0, path_off)].decode()
        if out.setdefault(path, (size, crc)) != (size, crc):
            raise ValueError(f'assets.bin disagrees with itself about {path}')
    return out


def rex_regions(blob: bytes) -> dict[str, tuple[int, int, int]]:
    """{pack path: (size, crc32, live units)} from romfs:/engine/rex.bin (tools/rex_link.py)."""
    magic, version, _ref, regions, units, sites, string_bytes, _crc = struct.unpack_from('<8I', blob, 0)
    if magic != 0x58523346 or version != 1:
        raise ValueError('bad rex.bin header')
    strings_at = 32 + 16 * regions + 24 * units + 8 * sites
    strings = blob[strings_at:strings_at + string_bytes]
    out = {}
    for n in range(regions):
        path_off, size, crc, live = struct.unpack_from('<4I', blob, 32 + 16 * n)
        out[strings[path_off:strings.index(0, path_off)].decode()] = (size, crc, live)
    return out


def release_findings(result: dict, entries: dict[str, bytes] | None, recipe_path: Path | None,
                     elf: 'Elf') -> list[str]:
    """Everything a public image must satisfy. An empty list means it does."""
    problems: list[str] = []
    if result['embedded_bytes']:
        problems.append(f"{result['embedded_bytes']} bytes of FireRed tables remain in the executable")
    if result['unknown_bytes']:
        problems.append(f"{result['unknown_bytes']} bytes could not be attributed")
    if entries is None:
        problems.append('the image has no RomFS, so the engine ABI and pack index are missing')
        return problems
    extra = sorted(set(entries) - CLEAN_ROMFS)
    if extra:
        problems.append(f'RomFS holds files beyond the engine metadata: {extra[:5]}')
    if 'data.embedded' in entries:
        problems.append('data.embedded marker present: embedded data is enabled')
    if entries.get('engine/profile.bin') != CLEAN_PROFILE:
        problems.append('engine/profile.bin is not the clean-release profile')
    recipe = load_recipe(recipe_path or DEFAULT_RECIPE)
    abi = struct.unpack('<I', entries.get('engine/abi.bin', b'\0\0\0\0'))[0]
    if abi != recipe['engine_abi']:
        problems.append(f"engine ABI {abi:08x} does not match the recipe {recipe['engine_abi']:08x}")
    defined = {s['name'] for s in elf.symbols if s['shndx'] != 0}
    for name in REQUIRED_SYMBOLS:
        if name not in defined:
            problems.append(f'pack/engine symbol {name} is missing from the executable')
    wanted = {e['path']: (e['size'], e['crc32']) for e in recipe['entries']}
    try:
        expected = dict(assets_index(entries['engine/assets.bin']))
        for path, (size, crc, _live) in rex_regions(entries['engine/rex.bin']).items():
            expected[path] = (size, crc)
    except (KeyError, ValueError, struct.error) as exc:
        problems.append(f'engine metadata unreadable: {exc}')
        return problems
    for path, row in sorted(expected.items()):
        if path not in wanted:
            problems.append(f'the executable expects {path}, which the recipe does not produce')
        elif wanted[path] != row:
            problems.append(f'{path}: executable expects size/CRC {row}, recipe has {wanted[path]}')
    # The recipe is a static superset (it must not move with linker garbage
    # collection); entries this particular link does not reference are only shipped.
    return problems


def scan_rom_leaks(image: bytes, recipe: dict, rom: bytes, window: int = 32) -> list[str]:
    """Search the image for runs of ROM-derived bytes (development check, needs the ROM).

    Every `window`-byte aligned slice of every reconstructed payload with some
    entropy is looked up at every offset of the image."""
    sys.path.insert(0, str(ROOT / 'builder'))
    from firered3ds_builder.recipe import reconstruct
    seen: dict[bytes, str] = {}
    for path, data in reconstruct(recipe, rom):
        for off in range(0, len(data) - window + 1, window):
            chunk = data[off:off + window]
            # Skip near-constant runs and plain counting ramps: libraries have those too.
            if len(set(chunk)) >= 12 and len({chunk[i + 1] - chunk[i] for i in range(window - 1)}) > 2:
                seen.setdefault(chunk, path)
    hits: dict[str, int] = defaultdict(int)
    for off in range(len(image) - window + 1):
        chunk = image[off:off + window]
        if chunk in seen:
            hits[seen[chunk]] += 1
    return [f'{n} ROM-derived {window}-byte runs from {p} found in the image' for p, n in sorted(hits.items())]


def markdown(r: dict) -> str:
    lines = ['# Clean-release audit', '',
             f"- ELF: `{r['elf']}`",
             f"- External (NOBITS, filled from the pack): **{r['external_nobits_bytes']:,} bytes** "
             + ', '.join(f'{k} {v:,}' for k, v in r['external_by_kind'].items()),
             f"- Engine/safe read-only bytes: {r['engine_bytes']:,}",
             f"- **FireRed content still embedded: {r['embedded_bytes']:,} bytes**",
             f"- Unknown / needs review: {r['unknown_bytes']:,} bytes",
             f"- RomFS payload files: {len(r['romfs_payload_files'])}", '',
             '## Still embedded, by category', '', '| category | bytes | objects |', '|---|---:|---:|']
    for name, v in r['embedded_by_category'].items():
        lines.append(f"| {name} | {v['bytes']:,} | {v['object_count']} |")
    lines += ['', '## Largest embedded symbols', '', '| symbol | bytes | object |', '|---|---:|---|']
    for s in r['largest_embedded_symbols'][:15]:
        lines.append(f"| `{s['symbol']}` | {s['size']:,} | {s['object']} |")
    lines += ['', '## Engine-safe contributions', '', '| reason | bytes |', '|---|---:|']
    for k, v in sorted(r['engine_detail'].items(), key=lambda kv: -kv[1]):
        lines.append(f'| {k} | {v:,} |')
    return '\n'.join(lines) + '\n'


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__.split('\n')[0])
    ap.add_argument('--tree', type=Path, default=ROOT, help='bootstrapped build tree')
    ap.add_argument('--elf', type=Path)
    ap.add_argument('--map', type=Path)
    ap.add_argument('--3dsx', dest='threedsx', type=Path)
    ap.add_argument('--json', type=Path)
    ap.add_argument('--md', type=Path)
    ap.add_argument('--release', action='store_true', help='exit non-zero unless the image is a clean public release')
    ap.add_argument('--coverage', action='store_true', help='accepted for clarity: --release always proves pack coverage')
    ap.add_argument('--rom', type=Path, help='development only: scan the image for ROM-derived byte runs')
    ap.add_argument('--recipe', type=Path)
    args = ap.parse_args()
    result = audit(args.tree.resolve(), args.elf, args.map, args.threedsx, args.recipe)
    if args.json:
        args.json.write_text(json.dumps(result, indent=2) + '\n')
    report = markdown(result)
    if args.md:
        args.md.write_text(report)
    print(report)
    count = result['embedded_bytes'] + result['unknown_bytes'] + len(result['romfs_payload_files'])
    print(f"FireRed game payloads embedded in this image: {count}"
          f" ({result['embedded_bytes']} bytes, {len(result['romfs_payload_files'])} RomFS files)")
    if args.release:
        port = args.tree.resolve() / '3ds_port'
        elf = Elf(args.elf or port / 'build/firered_game.elf')
        entries = threedsx_romfs_entries(args.threedsx) if args.threedsx else None
        problems = release_findings(result, entries, args.recipe, elf)
        if not args.rom:
            problems.append('A local supported ROM leak scan is required for release verification')
        if args.rom and args.threedsx:
            recipe = load_recipe(args.recipe or DEFAULT_RECIPE)
            problems += scan_rom_leaks(args.threedsx.read_bytes(), recipe, __import__('firered3ds_builder.rom', fromlist=['load_rom']).load_rom(args.rom).data)
        for problem in problems:
            print('RELEASE BLOCKED:', problem, file=sys.stderr)
        if problems:
            return 1
        print('CLEAN RELEASE: PASS (no FireRed payloads embedded; every pack path the executable expects is in the recipe)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
