"""Write the map description the map-edge draw test reads, from a bootstrapped tree.

The tree is the pinned upstream workspace created by tools/bootstrap.py; nothing
derived from it is stored in the repository. Output (one line per record):
  MAP <index> <type> <w> <h> <bw> <bh> <map.bin> <border.bin> <prim meta> <prim attr> <sec meta> <sec attr> <name>
  CONN <direction S/N/W/E> <offset> <target index>
"""
import json
import os
import re
import sys
from pathlib import Path

DIRS = {'down': 'S', 'up': 'N', 'left': 'W', 'right': 'E'}


def tileset_dirs(tree):
    found = {}
    for p in (tree / 'data/tilesets').glob('*/*'):
        found[p.name.replace('_', '')] = p
    return found


def tileset(name, found):
    key = re.sub(r'(?<!^)(?=[A-Z])', '_', name.replace('gTileset_', '')).lower().replace('_', '')
    return found[key]


def build(tree):
    tree = Path(tree)
    layouts = {l['id']: l for l in json.loads((tree / 'data/layouts/layouts.json').read_text())['layouts'] if 'id' in l}
    found = tileset_dirs(tree)
    maps = []
    for p in sorted((tree / 'data/maps').glob('*/map.json')):
        doc = json.loads(p.read_text())
        if doc.get('layout') in layouts:
            maps.append(doc)
    index = {m['id']: i for i, m in enumerate(maps)}
    out = []
    for i, m in enumerate(maps):
        l = layouts[m['layout']]
        prim, sec = tileset(l['primary_tileset'], found), tileset(l['secondary_tileset'], found)
        kind = ['MAP_TYPE_NONE', 'MAP_TYPE_TOWN', 'MAP_TYPE_CITY', 'MAP_TYPE_ROUTE', 'MAP_TYPE_UNDERGROUND',
                'MAP_TYPE_UNDERWATER', 'MAP_TYPE_OCEAN_ROUTE', 'MAP_TYPE_UNKNOWN', 'MAP_TYPE_INDOOR',
                'MAP_TYPE_SECRET_BASE'].index(m['map_type'])
        conns = [c for c in m.get('connections') or [] if c['direction'] in DIRS and c['map'] in index]
        rel = lambda x: os.path.relpath(x, tree).replace(os.sep, '/')
        out.append('MAP %d %d %d %d %d %d %s %s %s %s %s %s %s %d' % (
            i, kind, l['width'], l['height'], l['border_width'], l['border_height'],
            l['blockdata_filepath'], l['border_filepath'],
            rel(prim / 'metatiles.bin'), rel(prim / 'metatile_attributes.bin'),
            rel(sec / 'metatiles.bin'), rel(sec / 'metatile_attributes.bin'), m['name'], len(conns)))
        for c in conns:
            out.append('CONN %s %d %d' % (DIRS[c['direction']], c['offset'], index[c['map']]))
    return '\n'.join(out) + '\n'


if __name__ == '__main__':
    sys.stdout.write(build(sys.argv[1]))
