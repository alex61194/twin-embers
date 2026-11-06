"""Turns HD Dynamic Battlefields' 16:10 paintings into the battle scenes the HD option of the Builder downloads.

    python tools/dev_hd_battlefields.py --art DIR --frlg frlg.lua --map-groups map_groups.h --out DIR

The paintings (github.com/Bentley734/HD-Dynamic-Battlefields) have no licence; their author let us use them in this port
(docs/THIRD-PARTY-ART.md). Only this tool's 3DS-sized output is published, never the paintings. It reads
a local copy of the art (DIR/<environment>/<environment>-01-16x10.png and ...-night.png), the mod's FR/LG map table
(metadata/maps/frlg.lua) and pokefirered's include/constants/map_groups.h, and writes under --out the files the game
reads (add them to a pack with `firered3ds-builder build --extra-dir OUT`):

    graphics/gen4/hd/NNN.bin   NNN = environment * 2 (day) or * 2 + 1 (night): a battle scene as the Builder's convert_bg
                               makes it (272x112, one 15-colour palette: the whole width of the 3DS's battle view, nothing mirrored), the painting cut to the battle area
    graphics/gen4/hdn/NNN.bin  the same numbers, the scene at the 3DS's own resolution: b"G4H1", u16 400, u16 240, then the
                               picture as RGB565 (little endian) row by row, full colour, dithered, the painting's whole width
    graphics/gen4/hd/map.bin   b"G4M1", u16 count, then a (u16 map = group << 8 | number, u8 environment, u8 water
                               environment, 255: none) entry for each FR/LG map, sorted by map

Needs Pillow (only this tool does).
"""
import argparse
import io
import re
import struct
import sys
from pathlib import Path

from PIL import Image

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / 'builder'))
from firered3ds_builder import gen4_sprites  # noqa: E402

NW, NH = 400, 240   # the whole top screen, drawn 1:1 (hdn/): the message box covers its bottom
VISIBLE_H = 168     # the scene's own height on screen (112 GBA rows at 1.5): the framing kept from the first pictures
BAYER = ((0, 8, 2, 10), (12, 4, 14, 6), (3, 11, 1, 9), (15, 7, 13, 5))
W, H = 272, 112     # the battle view's width (400 / 1.5 GBA pixels, a little more) and the scene's height
# The mod's map names that pokefirered spells another way (ROUTE_1 is ROUTE1 there, handled below).
ALIASES = {'OAKS_LAB': 'PALLET_TOWN_PROFESSOR_OAKS_LAB'}


def scene(path: Path) -> bytes:
    image = Image.open(path).convert('RGB')
    width, height = image.size
    band = round(width * H / W)
    top = max(0, (height - band) // 2 - 60)
    image = image.crop((0, top, width, top + band)).resize((W, H), Image.LANCZOS)
    image = image.quantize(colors=15, method=Image.Quantize.MEDIANCUT, dither=Image.Dither.FLOYDSTEINBERG).convert('RGB')
    out = io.BytesIO()
    image.save(out, format='PNG')
    return gen4_sprites.convert_bg_wide(out.getvalue())


def native(path: Path) -> bytes:
    image = Image.open(path).convert('RGB')
    width, height = image.size
    # The horizon where it was when the picture stopped above the message box (a band 168 rows tall, raised 60 source
    # rows from the middle), the picture going on down from there; a painting too short for that ends behind the box,
    # its last row repeated (never seen).
    scale = NW / width
    top = max(0, (height - round(width * VISIBLE_H / NW)) // 2 - 60)
    rows = min(NH, round((height - top) * scale))
    part = image.crop((0, top, width, min(height, top + round(NH / scale)))).resize((NW, rows), Image.LANCZOS)
    image = Image.new('RGB', (NW, NH))
    image.paste(part, (0, 0))
    for y in range(rows, NH):
        image.paste(part.crop((0, rows - 1, NW, rows)), (0, y))
    pixels = image.load()
    out = bytearray(b'G4H1' + struct.pack('<HH', NW, NH))
    for y in range(NH):
        for x in range(NW):
            r, g, b = pixels[x, y]
            d = BAYER[y & 3][x & 3]      # 0..15: dithers the bits RGB565 drops (3, 2 and 3)
            r = min(255, r + (d >> 1)) >> 3
            g = min(255, g + (d >> 2)) >> 2
            b = min(255, b + (d >> 1)) >> 3
            out += struct.pack('<H', r << 11 | g << 5 | b)
    return bytes(out)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument('--art', type=Path, required=True)
    parser.add_argument('--frlg', type=Path, required=True)
    parser.add_argument('--map-groups', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()

    environments = {}
    for folder in sorted(p for p in args.art.iterdir() if p.is_dir()):
        days = sorted(f for f in folder.glob(folder.name + '-01-16x10*.png') if 'night' not in f.name)
        nights = sorted(f for f in folder.glob(folder.name + '-01-16x10*night*.png'))
        if days:
            environments[folder.name] = (days[0], nights[0] if nights else None)
    names = sorted(environments)
    index = {name: n for n, name in enumerate(names)}
    target = args.out / 'graphics' / 'gen4' / 'hd'
    target.mkdir(parents=True, exist_ok=True)
    native_target = args.out / 'graphics' / 'gen4' / 'hdn'
    native_target.mkdir(parents=True, exist_ok=True)
    for name in names:
        day, night = environments[name]
        for number, art in ((index[name] * 2, day), (index[name] * 2 + 1, night)):
            if art:
                (target / ('%03d.bin' % number)).write_bytes(scene(art))
                (native_target / ('%03d.bin' % number)).write_bytes(native(art))
        print('%-22s %3d %s' % (name, index[name], 'day+night' if night else 'day'))

    groups = {}
    for name, number, group in re.findall(r'#define MAP_(\w+)\s+\((\d+) \| \((\d+) << 8\)\)',
                                          args.map_groups.read_text(encoding='utf-8')):
        groups[name] = int(group) << 8 | int(number)
    rows, missing, unknown = [], [], set()
    for name, body in re.findall(r"\['FR_(\w+)'\]=\{([^}]*)\}", args.frlg.read_text(encoding='utf-8')):
        env = re.search(r"environment='(\w+)'", body)
        water = re.search(r"water='(\w+)'", body)
        key = groups.get(ALIASES.get(name, name)) or groups.get(re.sub(r'ROUTE_(\d)', r'ROUTE\1', name))
        if key is None:
            missing.append(name)
            continue
        e = env.group(1) if env else None
        w = water.group(1) if water else None
        if e and e not in index:
            unknown.add(e)
            e = None
        if w and w not in index:
            unknown.add(w)
            w = None
        if e or w:
            rows.append((key, index[e] if e else 255, index[w] if w else 255))
    rows.sort()
    (target / 'map.bin').write_bytes(b'G4M1' + struct.pack('<H', len(rows))
                                     + b''.join(struct.pack('<HBB', *row) for row in rows))
    print('%d maps in the table, %d names the game does not have, environments without a painting: %s'
          % (len(rows), len(missing), ', '.join(sorted(unknown)) or 'none'))
    if missing:
        print('not found:', ', '.join(missing[:12]))


if __name__ == '__main__':
    main()
