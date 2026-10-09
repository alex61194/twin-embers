"""Fill REX units in an emulated memory image, exactly as src/3ds_rex_core.c does.

The ARM11 harnesses load the ELF's PT_LOAD segments, which leave every REX table
zero (it is NOBITS). Call load_rex() after loading them to place the pack bytes and
patch the pointer words. Needs the game data: build/twinembers.pak (make private-pack)
or a ROM (FIRERED_ROM, reconstructed with the recipe).
"""
import os
from pathlib import Path
import struct
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'builder'))
from firered3ds_builder.pak import PakReader
from firered3ds_builder.recipe import load_recipe, reconstruct
from firered3ds_builder.rom import load_rom, SUPPORTED_SHA1

RECIPE = ROOT / 'builder/firered3ds_builder/recipes/firered-41cb23d8.json'


def region_data(port: Path, rom: Path | None, wanted: set[str]) -> dict[str, bytes]:
    pack = port / 'build/twinembers.pak'
    recipe = load_recipe(RECIPE)
    if pack.is_file():
        with PakReader(pack, recipe['engine_abi'], bytes.fromhex(SUPPORTED_SHA1)) as reader:
            return {path: reader.read(path) for path in wanted}
    rom = rom or (Path(os.environ['FIRERED_ROM']) if os.environ.get('FIRERED_ROM') else None)
    if rom is None:
        raise SystemExit('the REX tables need game data: run `make private-pack FIRERED_ROM=...` or set FIRERED_ROM')
    return {path: data for path, data in reconstruct(recipe, load_rom(rom).data) if path in wanted}


def load_rex(write, port: Path, rom: Path | None = None, delta: int = 0) -> int:
    """write(address, bytes) puts bytes into the emulated memory. Returns the unit count."""
    if not (port / 'romfs/engine/rex.bin').is_file():
        return 0  # an ELF from before REX: its tables are still in the segments
    blob = (port / 'romfs/engine/rex.bin').read_bytes()
    magic, version, _ref, regions, units, sites, string_bytes, _crc = struct.unpack_from('<8I', blob, 0)
    assert magic == 0x58523346 and version == 1
    region_at, unit_at = 32, 32 + 16 * regions
    site_at = unit_at + 24 * units
    strings = blob[site_at + 8 * sites:site_at + 8 * sites + string_bytes]
    paths = []
    for n in range(regions):
        off, _size, _crc, live = struct.unpack_from('<4I', blob, region_at + 16 * n)
        paths.append((strings[off:strings.index(0, off)].decode(), live))
    data = region_data(port, rom, {p for p, live in paths if live})
    for n in range(units):
        region, source, destination, size, first, count = struct.unpack_from('<6I', blob, unit_at + 24 * n)
        write(destination, data[paths[region][0]][source:source + size])
        for s in range(first, first + count):
            address, target = struct.unpack_from('<2I', blob, site_at + 8 * s)
            if address & 0x80000000:
                write(address & 0x7fffffff, struct.pack('<H', target & 0xffff))
            else:
                write(address, struct.pack('<I', (target + delta) & 0xffffffff))
    return units
