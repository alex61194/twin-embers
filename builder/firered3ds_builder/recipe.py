"""Reconstruction recipes, schemas 1 and 2.

Operations (all bounds checked, deterministic, no literal bytes):

  ["C", offset, length]                         copy ROM bytes
  ["F", value, length]                          fill
  ["Z", offset]                                 GBA LZ77 decompress a ROM stream
  ["R", offset, src_stride, dst_stride, count]  record copy: the first dst_stride
        bytes of each of `count` consecutive src_stride-byte ROM records. The GBA
        compiler pads every struct to a multiple of 4 bytes; the 3DS build does not.
  ["B", offset, stride, count, [[src_bit, dst_bit, width], ...]]
        bit-field record remap: `count` consecutive `stride`-byte ROM records, each
        rebuilt by moving the listed bit ranges to their 3DS positions (little endian
        bit numbers inside the record); bits not listed are zero. agbcc packs
        adjacent bit-fields contiguously while the 3DS compiler never lets a field
        straddle its 16-bit storage unit. Only bit positions, never values.

Schema 2 adds the `units` list that the engine build uses to place pack bytes; the
Builder itself only reads `entries`. There is no literal pool in any schema.
"""
import hashlib
import re
import json
from pathlib import Path
import zlib
from .pak import normalise, engine_abi
from .rom import SUPPORTED_SHA1

MAX_FILE = 32 * 1024 * 1024


def number(value: object, maximum: int) -> int:
    if type(value) is not int or not 0 <= value <= maximum:
        raise ValueError('Recipe integer is out of range')
    return value


def lz77(src: bytes, offset: int, limit: int) -> bytes:
    if offset + 4 > len(src) or src[offset] != 0x10:
        raise ValueError('Invalid LZ77 header')
    size = int.from_bytes(src[offset + 1:offset + 4], 'little')
    if size > limit:
        raise ValueError('LZ77 output exceeds expected file size')
    out = bytearray()
    pos = offset + 4
    while len(out) < size:
        if pos >= len(src):
            raise ValueError('Truncated LZ77 flag')
        flags = src[pos]
        pos += 1
        for bit in range(8):
            if len(out) == size:
                break
            if flags & (0x80 >> bit):
                if pos + 2 > len(src):
                    raise ValueError('Truncated LZ77 reference')
                a, b = src[pos:pos + 2]
                pos += 2
                length, distance = (a >> 4) + 3, ((a & 15) << 8 | b) + 1
                if distance > len(out) or length > size - len(out):
                    raise ValueError('Invalid LZ77 reference')
                for _ in range(length):
                    out.append(out[-distance])
            else:
                if pos >= len(src):
                    raise ValueError('Truncated LZ77 literal')
                out.append(src[pos])
                pos += 1
    return bytes(out)


def bitfield_records(rom: bytes, op: list, room: int) -> bytes:
    offset, stride, count = (number(v, len(rom)) for v in op[1:4])
    ranges = op[4]
    if not 0 < stride <= 8 or count == 0 or count * stride > room or offset + count * stride > len(rom):
        raise ValueError('Bit-field record outside bounds')
    if not isinstance(ranges, list) or not 0 < len(ranges) <= 32:
        raise ValueError('Invalid bit-field ranges')
    moves, used = [], 0
    for row in ranges:
        if not isinstance(row, list) or len(row) != 3:
            raise ValueError('Invalid bit-field range')
        src, dst, width = (number(v, stride * 8) for v in row)
        if width == 0 or src + width > stride * 8 or dst + width > stride * 8:
            raise ValueError('Bit-field range outside the record')
        mask = ((1 << width) - 1) << dst
        if used & mask:
            raise ValueError('Overlapping bit-field destinations')
        used |= mask
        moves.append((src, dst, (1 << width) - 1))
    out = bytearray()
    for i in range(count):
        value = int.from_bytes(rom[offset + i * stride:offset + (i + 1) * stride], 'little')
        built = 0
        for src, dst, mask in moves:
            built |= ((value >> src) & mask) << dst
        out.extend(built.to_bytes(stride, 'little'))
    return bytes(out)


def execute(ops: list, rom: bytes, size: int) -> bytes:
    out = bytearray()
    for op in ops:
        if op[0] == 'C' and len(op) == 3:
            offset, length = number(op[1], len(rom)), number(op[2], size)
            if length > len(rom) - offset or length > size - len(out):
                raise ValueError('ROM copy outside bounds')
            out.extend(rom[offset:offset + length])
        elif op[0] == 'F' and len(op) == 3:
            value, length = number(op[1], 255), number(op[2], size)
            if length > size - len(out):
                raise ValueError('Fill exceeds output size')
            out.extend(bytes([value]) * length)
        elif op[0] == 'Z' and len(op) == 2:
            out.extend(lz77(rom, number(op[1], len(rom)), size - len(out)))
        elif op[0] == 'R' and len(op) == 5:
            offset, src, dst, count = (number(v, len(rom)) for v in op[1:])
            if not 0 < dst <= src or count == 0 or count * dst > size - len(out) \
                    or offset + (count - 1) * src + dst > len(rom):
                raise ValueError('Record copy outside bounds')
            for i in range(count):
                out.extend(rom[offset + i * src:offset + i * src + dst])
        elif op[0] == 'B' and len(op) == 5:
            out.extend(bitfield_records(rom, op, size - len(out)))
        else:
            raise ValueError('Unsupported recipe operation; literal pools are not permitted')
    return bytes(out)


def reconstruct(recipe: dict, rom: bytes) -> list[tuple[str, bytes]]:
    if recipe.get('schema') not in (1, 2) or recipe.get('rom_sha1') != SUPPORTED_SHA1:
        raise ValueError('Recipe schema/ROM mismatch')
    entries = recipe.get('entries')
    if not isinstance(entries, list) or not 0 < len(entries) <= 65536:
        raise ValueError('Invalid recipe entry count')
    files = []
    seen = set()
    total = 0
    for entry in entries:
        path = normalise(entry['path'])
        if path in seen:
            raise ValueError('Duplicate recipe path')
        seen.add(path)
        size = number(entry['size'], MAX_FILE)
        total += size
        if total > 128 * 1024 * 1024:
            raise ValueError('Recipe output exceeds private development limit')
        out = execute(entry['ops'], rom, size)
        if len(out) != size or zlib.crc32(out) != number(entry['crc32'], 0xffffffff):
            raise ValueError(f'Reconstructed output size/CRC mismatch: {path}')
        files.append((path, bytes(out)))
    abi = engine_abi((p, len(b), zlib.crc32(b)) for p, b in files)
    if abi != recipe['engine_abi']:
        raise ValueError('Recipe engine ABI does not match outputs')
    return files


def load_recipe(path: Path) -> dict:
    if path.stat().st_size > 16 * 1024 * 1024:
        raise ValueError('Recipe exceeds supported size')
    recipe = json.loads(path.read_text(encoding='utf-8'))
    parts = recipe.pop('recipe_parts', None)
    if parts is None:
        return recipe
    if recipe.get('schema') != 2 or any(k in recipe for k in ('entries', 'units', 'engine_units')):
        raise ValueError('Invalid partitioned recipe index')
    if not isinstance(parts, list) or not 0 < len(parts) <= 64:
        raise ValueError('Invalid recipe part count')
    seen = set()
    for item in parts:
        name = item['path']
        if not re.fullmatch(r'firered-[0-9a-f]{8}/(?:entries|units|engine_units)-[0-9]{2}\.json', name) or name in seen:
            raise ValueError('Unsafe or duplicate recipe part')
        seen.add(name)
        target = path.parent / name
        if target.is_symlink() or path.parent.resolve() not in target.resolve().parents or target.stat().st_size > 256 * 1024:
            raise ValueError('Unsafe recipe part file')
        raw = target.read_bytes()
        if hashlib.sha256(raw).hexdigest() != item['sha256']:
            raise ValueError('Recipe part hash mismatch')
        part = json.loads(raw)
        if len(part) != 1 or next(iter(part)) not in ('entries', 'units', 'engine_units'):
            raise ValueError('Unsupported recipe part')
        key, rows = next(iter(part.items()))
        if not isinstance(rows, list):
            raise ValueError('Recipe part is not a list')
        recipe.setdefault(key, []).extend(rows)
    if set(recipe) != {'schema', 'rom_sha1', 'engine_abi', 'entries', 'units', 'engine_units'}:
        raise ValueError('Incomplete recipe index')
    return recipe
