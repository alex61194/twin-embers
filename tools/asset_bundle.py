#!/usr/bin/env python3
"""Preserve INCBIN array symbols in NOBITS storage; emit private asset metadata.

The game compiler sees the original arrays, including sizeof and constant
initializers. Only proven, byte-identical, relocation-free payload sections
are changed after compilation. No game table or executable section qualifies.
"""
from __future__ import annotations
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import zlib
from rex import tool

ROOT = Path(__file__).resolve().parents[1]
GROUPS = {'fonts-common-ui': ('graphics/fonts/', 'graphics/text_window/'),
          'pokemon-battle': ('graphics/pokemon/', 'graphics/battle_anims/',
                             'graphics/battle_interface/', 'graphics/trainers/'),
          'overworld-tilesets': ('graphics/object_events/', 'graphics/field_effects/', 'data/tilesets/'),
          'intro-title': ('graphics/title_screen/', 'graphics/intro/'),
          'remaining-graphics': ('graphics/',)}
ARRAY = re.compile(r'\bconst\s+(?:u8|u16|u32|s8|s16|s32)\s+(\w+)\s*((?:\[[^\]]*\]\s*)+)\s*=\s*([^;]+);')
INCBIN = re.compile(r'INCBIN_[US](?:8|16|32)\s*\(([^)]*)\)')
SPOT = re.compile(r'\bconst\s+struct\s+SpindaSpot\s+(\w+)\s*\[\s*\]\s*=\s*\{(.*?)\};', re.S)
SPOT_ROW = re.compile(r'\{\s*\.x\s*=\s*(\d+)\s*,\s*\.y\s*=\s*(\d+)\s*,\s*\.image\s*=\s*INCBIN_U16\s*\(\s*"(graphics/[^"\n]+)"\s*\)\s*\}')
HEADER = struct.Struct('<4sIIIII')
RECORD = struct.Struct('<IIIII')


class Elf:
    def __init__(self, path):
        self.raw = Path(path).read_bytes()
        if self.raw[:7] != b'\x7fELF\x01\x01\x01':
            raise ValueError('Expected little-endian ELF32')
        header = struct.unpack_from('<16sHHIIIIIHHHHHH', self.raw)
        offset, size, count, names = header[6], header[11], header[12], header[13]
        if size != 40:
            raise ValueError('Unexpected ELF section header size')
        self.sections = [struct.unpack_from('<IIIIIIIIII', self.raw, offset + i * size) for i in range(count)]
        strings = self.data(self.sections[names])
        self.names = [self.string(strings, s[0]) for s in self.sections]
        self.symbols = {}
        self.symbol_rows = {}
        for s in self.sections:
            if s[1] == 2:
                strings = self.data(self.sections[s[6]])
                for at in range(s[4], s[4] + s[5], s[9]):
                    name, value, length, info, other, ndx = struct.unpack_from('<IIIBBH', self.raw, at)
                    symbol = self.string(strings, name)
                    row = (value, length, info, ndx)
                    self.symbols[symbol] = row
                    self.symbol_rows.setdefault(symbol, []).append(row)

    def data(self, section):
        return self.raw[section[4]:section[4] + section[5]]

    @staticmethod
    def string(strings, offset):
        return strings[offset:strings.index(0, offset)].decode('utf-8')


def declarations(text):
    for match in ARRAY.finditer(text):
        calls = list(INCBIN.finditer(match[3]))
        if not calls:
            continue
        # A pure array of INCBIN payloads; reject mixed numeric initializers.
        rest = INCBIN.sub('', match[3])
        if rest.strip(' \r\n\t{},'):
            continue
        paths = [path for call in calls for argument in call[1].split(',')
                 for path in [''.join(re.findall(r'"([^"\n]*)"', argument))]]
        if any(not p.startswith(('graphics/', 'data/')) or '..' in Path(p).parts for p in paths):
            raise ValueError('Invalid INCBIN path')
        yield match[1], paths
    for match in SPOT.finditer(text):
        rows = list(SPOT_ROW.finditer(match[2]))
        if rows and not SPOT_ROW.sub('', match[2]).strip(' \r\n\t,'):
            paths = [row[3] for row in rows]
            if any('..' in Path(p).parts for p in paths):
                raise ValueError('Invalid Spinda INCBIN path')
            yield match[1], paths


def payload_layout(text, symbol, payloads):
    """Preserve explicitly empty rows in otherwise pure file-backed arrays.

    Numeric/table initializers remain ineligible. Only an exact two-dimensional
    row shape with full file rows and explicit {} zero rows is accepted.
    The compiled-byte comparison remains authoritative for the complete array.
    """
    spot = next((m for m in SPOT.finditer(text) if m[1] == symbol), None)
    if spot:
        rows = list(SPOT_ROW.finditer(spot[2]))
        if len(rows) != len(payloads):
            raise ValueError('Spinda bitmap count mismatch')
        output, offsets, prefixes = bytearray(), [], []
        for row, data in zip(rows, payloads):
            x, y = int(row[1]), int(row[2])
            if x > 255 or y > 255 or len(data) != 32:
                raise ValueError('Unexpected Spinda coordinate/bitmap layout')
            output.extend(bytes((x, y)))
            offsets.append(len(output))
            prefixes.append((x, y))
            output.extend(data)
        return bytes(output), offsets, prefixes
    match = next(m for m in ARRAY.finditer(text) if m[1] == symbol)
    if not re.search(r'\{\s*\}', match[3]):
        offsets, offset = [], 0
        for data in payloads:
            offsets.append(offset)
            offset += len(data)
        return b''.join(payloads), offsets, [None] * len(payloads)
    dims = re.findall(r'\[([^\]]*)\]', match[2])
    if len(dims) != 2 or not re.fullmatch(r'\s*[0-9]+\s*', dims[1]):
        raise ValueError('Unsupported empty-row shape: ' + symbol)
    bits = int(re.search(r'const\s+[us](8|16|32)', match[0])[1])
    stride = int(dims[1]) * (bits // 8)
    if not stride:
        raise ValueError('Zero row stride: ' + symbol)
    calls = iter(INCBIN.finditer(match[3]))
    replaced = INCBIN.sub('@', match[3]).strip()
    if not replaced.startswith('{') or not replaced.endswith('}'):
        raise ValueError('Unsupported empty-row initializer: ' + symbol)
    tokens = replaced[1:-1].strip().rstrip(',').split(',')
    output, offsets, file_index = bytearray(), [], 0
    for token in tokens:
        if re.fullmatch(r'\s*\{\s*\}\s*', token):
            output.extend(bytes(stride))
        elif token.strip() == '@':
            call = next(calls)
            count = len(call[1].split(','))
            row = payloads[file_index:file_index + count]
            if sum(map(len, row)) != stride:
                raise ValueError('File row does not match declared stride: ' + symbol)
            for data in row:
                offsets.append(len(output))
                output.extend(data)
            file_index += count
        else:
            raise ValueError('Unsupported mixed empty-row initializer: ' + symbol)
    if file_index != len(payloads):
        raise ValueError('Empty-row file layout mismatch: ' + symbol)
    return bytes(output), offsets, [None] * len(payloads)


def transform(obj, preprocessed):
    obj, preprocessed = obj.resolve(), preprocessed.resolve()
    elf = Elf(obj)
    rows, command = [], [tool('arm-none-eabi-objcopy')]
    source = str(preprocessed.relative_to(ROOT / '3ds_port/build/game'))
    text = preprocessed.read_text(encoding='utf-8')
    for symbol, paths in declarations(text):
        groups = [next((g for g, prefixes in GROUPS.items() if p.startswith(prefixes)), None) for p in paths]
        if not any(groups):
            continue
        if None in groups or len(set(groups)) != 1:
            raise ValueError('Mixed selected asset categories: ' + symbol)
        group = groups[0]
        section = '.rodata.' + symbol
        if section not in elf.names:  # Completely eliminated by the compiler.
            continue
        ndx = elf.names.index(section)
        original = elf.sections[ndx]
        payloads = [(ROOT / path).read_bytes() for path in paths]
        expected, offsets, prefixes = payload_layout(text, symbol, payloads)
        value, length, info, sym_ndx = elf.symbols[symbol]
        if (original[1] != 1 or original[2] != 2 or sym_ndx != ndx or value != 0
            or length != len(expected) or elf.data(original) != expected
            or any(s[1] in (4, 9) and s[7] == ndx and s[5] for s in elf.sections)):
            raise ValueError(f'{source}:{symbol}: cannot preserve exact payload/array semantics')
        key = hashlib.sha256((source + ':' + symbol).encode()).hexdigest()[:24]
        alias, dest = '__ctr_asset_' + key, '.bss.ctr_asset.' + key
        command += ['--rename-section', section + '=' + dest + ',alloc',
                    '--add-symbol', alias + '=' + dest + ':0,global']
        for path, data, offset, prefix in zip(paths, payloads, offsets, prefixes):
            rows.append(dict(symbol=symbol, alias=alias, section=dest, source=source,
                path=path, offset=offset, size=len(data), array_size=length,
                alignment=original[8], crc=zlib.crc32(data), group=group, prefix=prefix))
    if rows:
        temp = obj.with_suffix('.asset-tmp.o')
        # Thousands of Pokémon arrays exceed Windows' process argument limit.
        response = obj.with_suffix('.asset-objcopy.rsp')
        response.write_text('\n'.join(json.dumps(arg.replace('\\', '/'))
            for arg in command[1:] + [str(obj), str(temp)]) + '\n')
        subprocess.run([command[0], '@' + str(response)], check=True)
        changed = Elf(temp)
        for row in rows:
            section = changed.sections[changed.names.index(row['section'])]
            old_symbol = elf.symbols[row['symbol']]
            new_symbol = changed.symbols[row['symbol']]
            if section[1] != 8 or section[5] != row['array_size'] or section[8] != row['alignment']:
                raise ValueError('NOBITS size/alignment changed')
            if old_symbol[:3] != new_symbol[:3] or changed.symbols[row['alias']][0] != 0:
                raise ValueError('Original symbol value/size/binding changed')
        temp.replace(obj)
    obj.with_suffix('.assets.json').write_text(json.dumps(rows, indent=2) + '\n')


def index(elf_path):
    elf_path = elf_path.resolve()
    elf = Elf(elf_path)
    ref = elf.symbols['AgbMain'][0]
    rows = []
    for manifest in sorted((ROOT / '3ds_port/build/game').rglob('*.assets.json')):
        for row in json.loads(manifest.read_text()):
            if row['alias'] in elf.symbols:  # Respect linker garbage collection.
                value, size, info, ndx = elf.symbols[row['alias']]
                if elf.sections[ndx][1] != 8:
                    raise ValueError('Asset not in linked NOBITS storage')
                row['address'] = value + row['offset']
                rows.append(row)
    rows.sort(key=lambda r: (r['address'], r['path']))
    strings = bytearray()
    records = bytearray()
    for row in rows:
        group = list(GROUPS).index(row['group']) + 1
        if row.get('prefix'):
            if group != 5: raise ValueError('Spinda metadata must remain in the graphics group')
            x, y = row['prefix']
            group |= (1 << 24) | (x << 8) | (y << 16)
        records += RECORD.pack(row['address'], row['size'], len(strings), row['crc'], group)
        strings += row['path'].encode() + b'\0'
    body = records + strings
    dest = ROOT / '3ds_port/romfs/engine'
    dest.mkdir(parents=True, exist_ok=True)
    (dest / 'assets.bin').write_bytes(HEADER.pack(b'F3AS', 1, ref, len(rows), len(strings), zlib.crc32(body)) + body)
    (ROOT / '3ds_port/build/graphics-assets.json').write_text(json.dumps(rows, indent=2) + '\n')
    reserved = sum({r['alias']: r['array_size'] for r in rows}.values())
    print(f'assets: {len(rows)} live ranges, {len({r["path"] for r in rows})} unique files, {reserved} reserved bytes')


def main():
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument('--object', type=Path)
    ap.add_argument('--preprocessed', type=Path)
    ap.add_argument('--index', type=Path)
    args = ap.parse_args()
    if args.index:
        index(args.index)
    else:
        transform(args.object, args.preprocessed)


if __name__ == '__main__':
    main()
