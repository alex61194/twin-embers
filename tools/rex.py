#!/usr/bin/env python3
"""Shared code for ROM-extracted data (REX).

REX moves FireRed's read-only game tables out of the executable. A *unit* is one
linker section of a game object (`.rodata.gItems` of item.o, or the whole
`.rodata` of the assembly object maps.o). The unit keeps its symbols and its size
in the executable, but as zero-filled NOBITS storage; at start-up the engine
copies the unit's bytes from the pack and patches its pointer words.

The pack holds ROM bytes exactly as the user's ROM has them (one *region* per
object, a plain copy of a ROM range), so the Builder only needs COPY. Pointer
words in a region are meaningless ROM addresses: the engine knows every pointer
site and its real target from the link and overwrites it.

This module is the object-file reader and the manifest format used by:
  tools/rex_manifest.py   development: derive the manifest from a reference build
  tools/rex_objects.py    build: turn manifest units into NOBITS, record pointer sites
  tools/rex_link.py       build: resolve sites after the link, write engine metadata
"""
from __future__ import annotations

import json
import os
import struct
import zlib
from dataclasses import dataclass, field
from pathlib import Path

SHT_PROGBITS, SHT_SYMTAB, SHT_NOBITS, SHT_REL = 1, 2, 8, 9
SHF_WRITE, SHF_ALLOC, SHF_EXEC, SHF_MERGE, SHF_STRINGS = 1, 2, 4, 0x10, 0x20
R_ARM_ABS32, R_ARM_ABS16 = 2, 5
RELOC_WIDTH = {R_ARM_ABS32: 4, R_ARM_ABS16: 2}   # bytes rewritten by each supported type
SHN_UNDEF, SHN_ABS, SHN_COMMON = 0, 0xfff1, 0xfff2
STB_LOCAL, STB_GLOBAL, STB_WEAK = 0, 1, 2
STT_NOTYPE, STT_OBJECT, STT_SECTION, STT_FILE = 0, 1, 3, 4

SCHEMA = 2
REGION_PREFIX = 'gamedata/'


@dataclass
class Section:
    index: int
    name: str
    type: int
    flags: int
    offset: int
    size: int
    link: int
    info: int
    align: int
    entsize: int


@dataclass
class Symbol:
    index: int
    name: str
    value: int
    size: int
    bind: int
    type: int
    shndx: int


@dataclass
class Reloc:
    offset: int
    type: int
    symbol: int


@dataclass
class Obj:
    """A relocatable ELF32 little-endian object file."""
    path: Path
    raw: bytes
    sections: list[Section] = field(default_factory=list)
    symbols: list[Symbol] = field(default_factory=list)

    @classmethod
    def load(cls, path: Path) -> 'Obj':
        raw = Path(path).read_bytes()
        if raw[:7] != b'\x7fELF\x01\x01\x01':
            raise ValueError(f'{path}: not a little-endian ELF32 file')
        e_shoff, = struct.unpack_from('<I', raw, 0x20)
        e_shentsize, e_shnum, e_shstrndx = struct.unpack_from('<HHH', raw, 0x2e)
        if e_shentsize != 40:
            raise ValueError(f'{path}: unexpected section header size')
        obj = cls(Path(path), raw)
        fields = [struct.unpack_from('<10I', raw, e_shoff + i * 40) for i in range(e_shnum)]
        names = raw[fields[e_shstrndx][4]:fields[e_shstrndx][4] + fields[e_shstrndx][5]]
        for i, f in enumerate(fields):
            obj.sections.append(Section(i, _cstr(names, f[0]), f[1], f[2], f[4], f[5], f[6], f[7], f[8], f[9]))
        for s in obj.sections:
            if s.type == SHT_SYMTAB:
                strs = obj.sections[s.link]
                blob = raw[strs.offset:strs.offset + strs.size]
                for n in range(s.size // 16):
                    nm, value, size, info, _other, shndx = struct.unpack_from('<IIIBBH', raw, s.offset + n * 16)
                    obj.symbols.append(Symbol(n, _cstr(blob, nm), value, size, info >> 4, info & 15, shndx))
        return obj

    def data(self, section: Section) -> bytes:
        if section.type == SHT_NOBITS:
            return bytes(section.size)
        return self.raw[section.offset:section.offset + section.size]

    def section(self, name: str) -> Section | None:
        return next((s for s in self.sections if s.name == name), None)

    def relocs(self, target: Section) -> list[Reloc]:
        out = []
        for s in self.sections:
            if s.type == SHT_REL and s.info == target.index:
                for at in range(s.offset, s.offset + s.size, 8):
                    off, info = struct.unpack_from('<II', self.raw, at)
                    out.append(Reloc(off, info & 0xff, info >> 8))
        return sorted(out, key=lambda r: r.offset)

    def rel_sections(self, target: Section) -> list[Section]:
        return [s for s in self.sections if s.type == SHT_REL and s.info == target.index]


def tool(name: str) -> str:
    """A devkitARM binutils program by name, falling back to PATH."""
    base = Path(os.environ.get('DEVKITARM', '/opt/devkitpro/devkitARM')) / 'bin'
    for candidate in (base / name, base / f'{name}.exe'):
        if candidate.exists():
            return str(candidate)
    return name


def _cstr(blob: bytes, off: int) -> str:
    return blob[off:blob.index(0, off)].decode('utf-8', 'replace')


def is_unit_candidate(s: Section) -> bool:
    """File-backed, non-code, non-merged data sections that could become a unit."""
    return (s.type == SHT_PROGBITS and s.flags & SHF_ALLOC and not s.flags & (SHF_EXEC | SHF_MERGE | SHF_STRINGS)
            and s.size > 0 and (s.name.startswith(('.rodata', '.data')) or s.name == 'script_data'))


def unit_symbol(obj: Obj, s: Section) -> Symbol | None:
    """The object symbol that names a `-fdata-sections` unit: value 0, whole section."""
    best = None
    for sym in obj.symbols:
        if sym.shndx == s.index and sym.value == 0 and sym.type == STT_OBJECT and sym.name:
            if sym.size == s.size:
                return sym
            best = best or sym
    return best


def object_key(path: Path | str) -> str:
    """`c/item.o`, `asm/maps.o`, `song/mus_x.o` from a build-tree object path."""
    p = str(path).replace('\\', '/')
    for marker, kind in (('/game_data/', 'asm'), ('/game_song/', 'song'), ('/game/', 'c')):
        if marker in p:
            return kind + '/' + p.split(marker, 1)[1]
    raise ValueError(f'not a game object: {path}')


def rom_object_path(key: str) -> str:
    """The reference-ROM linker-map name of a 3DS object key."""
    kind, name = key.split('/', 1)
    return {'c': 'src/', 'asm': 'data/', 'song': 'sound/songs/midi/'}[kind] + name


def region_path(group: str) -> str:
    return f'{REGION_PREFIX}{group}.bin'


def crc(data: bytes) -> int:
    return zlib.crc32(data) & 0xffffffff


def load_recipe(path: Path) -> dict:
    import sys
    sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'builder'))
    from firered3ds_builder.recipe import load_recipe as load
    recipe = load(Path(path))
    if recipe.get('schema') != SCHEMA or 'units' not in recipe:
        raise ValueError('not a schema-2 FireRed recipe with REX units')
    return recipe


def units_by_object(recipe: dict) -> dict[str, dict[str, tuple[int, int, int]]]:
    """object key -> section name -> (entry index, offset in the entry, size)."""
    table: dict[str, dict[str, tuple[int, int, int]]] = {}
    for key, section, entry, off, size in recipe['units']:
        table.setdefault(key, {})[section] = (entry, off, size)
    return table
