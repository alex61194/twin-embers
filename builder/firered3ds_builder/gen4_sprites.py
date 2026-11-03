"""Optional HeartGold battle sprites for the pack: downloaded, checked, converted.

The sprites are not in the player's FireRed ROM and never ship with Twin
Embers. When the player asks for them, the Builder downloads the PNG files that
gen4_sources.py lists from pret/pokeheartgold at the pinned commit, keeps each
one only if its size and SHA-256 match the list, and converts it for the pack.
The pack stays valid without them: the engine ABI does not cover these
entries, a species without its file keeps FireRed's pictures, and a build with
no network writes the same pack as before.

Pack entries, one per species with a complete set, and the icons:

    graphics/gen4/battle/NNN.bin    NNN = national dex number, 001-386
    graphics/gen4/battle/201_FF.bin Unown's letters, FF = 01-27 (B to Z, !, ?), as FireRed numbers them
    graphics/gen4/battle/351_FF.bin Castform's weather forms, FF = 01-03 (sun, rain, ice), FireRed's order
    graphics/gen4/spinda.bin        where HeartGold draws Spinda's spots (from pret's pokepic.c)
    graphics/gen4/icons.bin         every menu icon, only when all of them could be had
                                    (their palettes replace FireRed's for every icon)
    graphics/gen4/trainer/NNN.bin   NNN = FireRed's trainer picture (TRAINER_PIC_*), the
                                    HeartGold class TRAINER_CLASSES pairs with it; a picture
                                    left out (Lorelei, Agatha, Oak, Leaf...) keeps FireRed's
    graphics/gen4/back/NNN.bin      NNN = 000 Red's, 001 Leaf's back (TRAINER_BACK_PIC_*): fan
                                    art in HeartGold's style (BACK_FILES), not pret's

Layout (little endian):

    0    b"G4B1"
    4    u8   flags: bit 0 = a female pair follows the male one
    5    u8   0
    6    u16  0
    8    u16 x 16  normal palette, BGR555 as the GBA's (colour 0 is transparent)
    40   u16 x 16  shiny palette
    72   pictures, 80 x 80 pixels each, 4 bits a pixel, rows top to bottom,
         the left pixel in the low nibble: front, back, then the female
         front and back when bit 0 is set (a female-only species has its
         one pair first, bit 0 clear)

icons.bin (little endian):

    0    b"G4I1"
    4    u16  icons, 414: key 0 the egg, 1-386 the national dex, 387-413 Unown's letters B to ?
    6    u16  palettes used (at most 6, FireRed's icon palette slots)
    8    u16 x 16 x 6  the palettes, BGR555, unused ones zero
    200  u8 x icons  each icon's palette, then zeros to a multiple of 4
    then 1024 bytes an icon: two 32x32 frames, each 16 8x8 4bpp tiles row by row,
         as FireRed's icons

trainer/NNN.bin (little endian): b"G4T1", u32 0, u16 x 16 the palette (BGR555, colour 0
transparent), then the picture's first frame, 80 x 80 pixels, 4 bits a pixel as above. HeartGold
keeps the trainer pictures in one NARC (files/a/0/5/8), five files a class: the fifth is the picture (an NCGR
of two 80x80 frames side by side, its pixels scrambled as Diamond and Pearl's are), its palette
(the second, an NCLR); the Builder does not need the other three.

back/NNN.bin: b"G4K1", u8 frames (5, FireRed's throw), three zero bytes, u16 x 16 the palette, then
each frame, 80 x 80 pixels, 4 bits a pixel. HeartGold has no Red or Leaf back: these come from the
fan-made HGSS-style backs that the gen1recomp HGSS Visual Overhaul mod (LucianoNeo/gen1recomp-mods)
keeps as 400x80 RGBA strips, Leaf's by setogabes, Red's from a public Red pack; the Builder turns
each into at most 15 colours (BGR555) and transparency.

bg/NNN.bin (NNN = FireRed's BATTLE_TERRAIN_*): b"G4G1", u16 tiles, u16 0, u16 x 16 the palette (BGR555, colour 0
unused), then the tiles (4 bits a pixel, FireRed's own layout, 32 bytes each) and the 64x32 tile map (two 32x32
screen blocks, entries as FireRed's BG3 reads them, palette 2), ready for the VRAM the battle scene is drawn from.
A scene is 240x112 pixels; the map repeats its first row above it and its last columns on either side.

spinda.bin: b"G4P1", then the four spots, each a u8 count and that many
(x, y) u8 pairs: the pixels of the spot on the front picture before the
personality moves it (HeartGold's RawChardata_PlaceSpindaSpots).
"""
from __future__ import annotations

import hashlib
import os
import queue
import re
import struct
import threading
import urllib.request
import zlib
from pathlib import Path
from typing import Callable

from . import gen4_sources

SIZE = 80
PICTURE = SIZE * SIZE // 2
MAGIC = b"G4B1"
PREFIX = "graphics/gen4/battle/"
ICONS = "graphics/gen4/icons.bin"
SPINDA = "graphics/gen4/spinda.bin"
TRAINER_PREFIX = "graphics/gen4/trainer/"
TRAINER_MAGIC = b"G4T1"
BACK_PREFIX = "graphics/gen4/back/"
BACK_MAGIC = b"G4K2"
BACK_FRAMES = 5
# The back is enlarged 1.25 as pixel art (Scale3x, then every 2.4th pixel) to 100x100, on a 104x104 picture of
# 13x13 tiles (CTR_GEN4_BACK_SIZE): shown 1:1, it stands 1.25 times as large as the 80x80 art.
BACK_SHOWN, BACK_SIZE = 100, 104
BACK_PICTURE = BACK_SIZE * BACK_SIZE // 2
# The fan-made backs, pinned like pret's files: (FireRed's TRAINER_BACK_PIC_*, "back", path, size, SHA-256).
BACK_REPO = "LucianoNeo/gen1recomp-mods"
BACK_COMMIT = "768f60d9d0f097b9cc65e78099574c296a3f63d2"
BACK_FILES = (
    (0, "back", "hgss_sprites/assets/battle/back-animated/redplayer.png", 10199,
     "26448af5283a349c17016e920aa7c8a6b75e93791b1cad4147870db221ca3cc5"),
    (1, "back", "hgss_sprites/assets/battle/back-animated/leafplayer.png", 5524,
     "8c222cfdae83898370c60806bf1c9297207023da59a46ec1f0fce2e9acd8945d"),
)
# The battle backgrounds: Facets' SCENERY set (LibertyTwins' Battle Backgrounds Patch FR, converted to 240x112
# PNGs of 15 colours), pinned like the rest: (scene, "bg", path, size, SHA-256). BG_TERRAIN gives each of FireRed's
# BATTLE_TERRAIN_* (grass, long grass, sand, underwater, water, pond, mountain, cave, building, plain, link, gym, leader,
# indoor 2, indoor 1, Lorelei, Bruno, Agatha, Lance, champion) its scene, as Facets maps them.
BG_REPO = "ElvieBlooms/facets"
BG_COMMIT = "9b4940d54f8bad152746481b56d841698a548e64"
BG_PREFIX = "graphics/gen4/bg/"
BG_MAGIC = b"G4G1"
BG_W, BG_H = 240, 112
BG_MAP_COLUMNS, BG_MAP_ROWS = 64, 32
BG_TILES_MAX = 512
BG_FILES = (
    (0, "bg", "assets/battlebg/scenery/grass.png", 7203,
     "26a618485a29fa74b389439d3f730b3fc0e236f2399c99e907e07e6c5df7bf74"),
    (1, "bg", "assets/battlebg/scenery/forest.png", 8960,
     "62cd98d05159d1c15e9fb45d11457d6fa776307759eb2d1c5301e0b73e5e1e62"),
    (2, "bg", "assets/battlebg/scenery/beach.png", 6480,
     "5c800d2ae752b2e04cd4105df6ccdccf1943d083451bde17eb594b712d4ad25f"),
    (3, "bg", "assets/battlebg/scenery/underwater.png", 5485,
     "c7f3e9e468feaea6b63cde9c0664eca40ef4bbb73e68aa32f5bb4ab24cb44c3c"),
    (4, "bg", "assets/battlebg/scenery/sea.png", 3666,
     "8c10cea856f281511e3f7bded7f26624c7702f9eb4bcb988d1f64f380cae3611"),
    (5, "bg", "assets/battlebg/scenery/lake.png", 5866,
     "11592e3ed8ed148231078ba0756496e09b212b89d25e0a770e4f654f7ed0d770"),
    (6, "bg", "assets/battlebg/scenery/mountain.png", 9330,
     "c66d9d846c18593d0d4053ae6be0b8362e4b781cfc2858204276860ba0cf1591"),
    (7, "bg", "assets/battlebg/scenery/cave.png", 11799,
     "ab0dd0dfaaa2f9ecdf15cd2c3c71131b3e3d46b913692aca35faa8b2e84df44a"),
    (8, "bg", "assets/battlebg/scenery/lab.png", 3966,
     "95d12a070159cd58313309c189eab2d659eb942c218a6e03c545a3ea6010f415"),
    (9, "bg", "assets/battlebg/scenery/route.png", 9873,
     "9b16990868cdc3aa0bd60e71510999e252dfee08755d76875d571525e5829809"),
    (10, "bg", "assets/battlebg/scenery/indoors.png", 1586,
     "d21831b0da6d3f7c992de0474245c2b9d8dda184b6e9e34926785efeff393b24"),
    (11, "bg", "assets/battlebg/scenery/neutral.png", 2211,
     "3e99166d28c748adaabded65d486d3cf0ca4a86c8a1950a50030aef3c162a865"),
    (12, "bg", "assets/battlebg/scenery/gym.png", 4170,
     "edc34b6191b8da6f46c8488a40473ef6fc35a967e01807d54aab41e5253030b4"),
    (13, "bg", "assets/battlebg/scenery/space.png", 2205,
     "13ee861c57029aa50cbe6ff2f7d532c78e149188973db1ee04d54697681db72d"),
)
BG_TERRAIN = (0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 11, 12, 12, 10, 10, 12, 12, 12, 12, 13)
ICON_MAGIC = b"G4I1"
ICON_PALETTES = 6
URL = "https://raw.githubusercontent.com/{repo}/{commit}/{path}"
TIMEOUT = 30


class Gen4Error(ValueError):
    pass


# --- PNG (the 4- and 8-bit indexed images pret keeps) -------------------------------------------

def read_png(data: bytes) -> tuple[int, int, list[tuple[int, int, int]], list[list]]:
    """(width, height, palette as RGB, rows of palette indices) of an indexed PNG; of an RGBA or RGB one, no palette
    and rows of (r, g, b, a)."""
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise Gen4Error("not a PNG file")
    pos, idat, palette, header = 8, bytearray(), [], None
    while pos + 8 <= len(data):
        length, kind = struct.unpack(">I4s", data[pos:pos + 8])
        body = data[pos + 8:pos + 8 + length]
        pos += 12 + length
        if kind == b"IHDR":
            header = struct.unpack(">IIBBBBB", body)
        elif kind == b"PLTE":
            palette = [tuple(body[i:i + 3]) for i in range(0, len(body) - 2, 3)]
        elif kind == b"IDAT":
            idat += body
        elif kind == b"IEND":
            break
    if header is None:
        raise Gen4Error("PNG without a header")
    width, height, depth, colour, _, _, interlace = header
    rgba = colour == 6 and depth == 8
    rgb = colour == 2 and depth == 8
    if not (colour == 3 and depth in (4, 8) or rgba or rgb) or interlace:
        raise Gen4Error("only non-interlaced 4- or 8-bit indexed or 8-bit RGB or RGBA PNGs are read")
    step = 4 if rgba else 3 if rgb else 1
    stride = width * step if (rgba or rgb) else (width * depth + 7) // 8
    raw = zlib.decompress(bytes(idat))
    if len(raw) != (stride + 1) * height:
        raise Gen4Error("PNG data has the wrong size")
    rows, previous = [], bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            left = line[x - step] if x >= step else 0
            up = previous[x]
            corner = previous[x - step] if x >= step else 0
            if kind == 1:
                line[x] = (line[x] + left) & 0xFF
            elif kind == 2:
                line[x] = (line[x] + up) & 0xFF
            elif kind == 3:
                line[x] = (line[x] + (left + up) // 2) & 0xFF
            elif kind == 4:
                guess = left + up - corner
                near = min((abs(guess - left), 0, left), (abs(guess - up), 1, up), (abs(guess - corner), 2, corner))
                line[x] = (line[x] + near[2]) & 0xFF
            elif kind:
                raise Gen4Error("unknown PNG filter")
        previous = line
        if rgba:
            rows.append([tuple(line[x * 4:x * 4 + 4]) for x in range(width)])
        elif rgb:
            rows.append([tuple(line[x * 3:x * 3 + 3]) + (255,) for x in range(width)])
        elif depth == 8:
            rows.append(list(line[:width]))
        else:
            rows.append([(line[x // 2] >> (0 if x & 1 else 4)) & 0xF for x in range(width)])
    return width, height, palette, rows


def bgr555(palette: list[tuple[int, int, int]]) -> bytes:
    colours = (palette + [(0, 0, 0)] * 16)[:16]
    return b"".join(struct.pack("<H", (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10) for r, g, b in colours)


def first_frame(rows: list[list[int]]) -> bytes:
    """The sprite's first 80x80 frame (pret keeps two side by side) as 4-bit pixels."""
    if len(rows) != SIZE or len(rows[0]) < SIZE:
        raise Gen4Error("a battle sprite is 80 pixels high and at least 80 wide")
    out = bytearray()
    for row in rows:
        if max(row[:SIZE]) > 15:
            raise Gen4Error("a battle sprite uses 16 colours")
        out += bytes(row[x] | row[x + 1] << 4 for x in range(0, SIZE, 2))
    return bytes(out)


def icon_tiles(rows: list[list[int]]) -> bytes:
    """A 32x64 icon (two frames one above the other) as FireRed's 1024 bytes of tiles."""
    if len(rows) != 64 or len(rows[0]) != 32 or max(max(row) for row in rows) > 15:
        raise Gen4Error("an icon is 32x64 in 16 colours")
    out = bytearray()
    for frame in range(2):
        for ty in range(4):
            for tx in range(4):
                for r in range(8):
                    row = rows[frame * 32 + ty * 8 + r]
                    out += bytes(row[tx * 8 + x] | row[tx * 8 + x + 1] << 4 for x in range(0, 8, 2))
    return bytes(out)


def convert_icons(icons: list[bytes]) -> bytes:
    """icons.bin from the icons' PNG files in key order. Each PNG carries its own palette, one of a few shared ones."""
    palettes: list[bytes] = []
    indices, tiles = bytearray(), bytearray()
    for data in icons:
        _, _, palette, rows = read_png(data)
        colours = bgr555(palette)
        if colours not in palettes:
            palettes.append(colours)
        indices.append(palettes.index(colours))
        tiles += icon_tiles(rows)
    if len(palettes) > ICON_PALETTES:
        raise Gen4Error("more icon palettes than FireRed has slots")
    indices += bytes(-len(indices) % 4)
    return (ICON_MAGIC + struct.pack("<HH", len(icons), len(palettes))
            + b"".join(palettes) + bytes(32 * (ICON_PALETTES - len(palettes))) + bytes(indices) + bytes(tiles))


def convert_spinda(source: bytes) -> bytes:
    """spinda.bin from pret's pokepic.c: its four sSpindaSpotNCoords tables, up to their 0xFF end."""
    text = source.decode("utf-8")
    out = bytearray(b"G4P1")
    for spot in range(1, 5):
        match = re.search(r"sSpindaSpot%dCoords\[\]\[2\] = \{(.*?)\};" % spot, text, re.S)
        if not match:
            raise Gen4Error("no Spinda spot table %d" % spot)
        pairs = [(int(x, 0), int(y, 0)) for x, y in re.findall(r"\{\s*(\w+)\s*,\s*(\w+)\s*\}", match.group(1))]
        if pairs[-1:] != [(0xFF, 0xFF)] or len(pairs) > 256 or any(x > 0xFF or y > 0xFF for x, y in pairs):
            raise Gen4Error("Spinda spot table %d is not as expected" % spot)
        out.append(len(pairs) - 1)
        for x, y in pairs[:-1]:
            out += bytes((x, y))
    return bytes(out)


# FireRed's trainer pictures (include/constants/trainers.h, TRAINER_PIC_*) and the HeartGold trainer class
# (pret/pokeheartgold include/constants/trainer_class.h, TRAINERCLASS_*) whose front picture stands for each one.
# FireRed's player pictures (backs, the Oak intro) are not trainer front pictures and are never replaced.
TRAINER_CLASSES = {
    7: 44,     # RS_TUBER_M -> TUBER_M
    13: 32,    # RICH_BOY -> RICH_BOY
    18: 64,    # KINDLER -> FIREBREATHER
    29: 60,    # SCHOOL_KID_M -> SCHOOL_KID_M
    30: 61,    # SCHOOL_KID_F -> SCHOOL_KID_F
    32: 20,    # POKEFAN_M -> POKEFAN_M
    33: 21,    # POKEFAN_F -> POKEFAN
    39: 12,    # CYCLING_TRIATHLETE_M -> CYCLIST_M
    40: 13,    # CYCLING_TRIATHLETE_F -> CYCLIST_F
    41: 19,    # RUNNING_TRIATHLETE_M -> JOGGER
    45: 29,    # DRAGON_TAMER -> DRAGON_TAMER
    47: 28,    # NINJA_BOY -> NINJA_BOY
    48: 10,    # BATTLE_GIRL -> BATTLE_GIRL
    49: 80,    # PARASOL_LADY -> PARASOL_LADY
    54: 37,    # COLLECTOR -> COLLECTOR
    82: 2,     # YOUNGSTER -> YOUNGSTER
    83: 6,     # BUG_CATCHER -> BUG_CATCHER
    84: 3,     # LASS -> LASS
    85: 46,    # SAILOR -> SAILOR
    86: 4,     # CAMPER -> CAMPER
    87: 5,     # PICNICKER -> PICNICKER
    88: 68,    # POKEMANIAC -> POKE_MANIAC
    89: 78,    # SUPER_NERD -> SUPER_NERD
    90: 9,     # HIKER -> HIKER
    91: 65,    # BIKER -> BIKER
    92: 63,    # BURGLAR -> BURGLAR
    93: 59,    # ENGINEER -> WORKER
    94: 11,    # FISHERMAN -> FISHERMAN
    95: 42,    # SWIMMER_M -> SWIMMER_M
    96: 57,    # CUE_BALL -> ROUGHNECK
    98: 36,    # BEAUTY -> BEAUTY
    99: 43,    # SWIMMER_F -> SWIMMER_F
    100: 49,   # PSYCHIC_M -> PSYCHIC_M
    101: 52,   # ROCKER -> GUITARIST
    102: 31,   # JUGGLER -> JUGGLER
    104: 69,   # BIRD_KEEPER -> BIRD_KEEPER_GS
    105: 14,   # BLACK_BELT -> BLACK_BELT
    106: 110,  # RIVAL_EARLY -> LEADER_BLUE
    107: 113,  # SCIENTIST -> SCIENTIST_GS
    108: 124,  # LEADER_GIOVANNI -> ROCKET_BOSS
    109: 55,   # ROCKET_GRUNT_M -> TEAM_ROCKET
    110: 53,   # COOLTRAINER_M -> ACE_TRAINER_M_GS
    111: 54,   # COOLTRAINER_F -> ACE_TRAINER_F_GS
    113: 112,  # ELITE_FOUR_BRUNO -> ELITE_FOUR_BRUNO
    115: 86,   # ELITE_FOUR_LANCE -> CHAMPION (Lance)
    116: 98,   # LEADER_BROCK -> LEADER_BROCK
    117: 103,  # LEADER_MISTY -> LEADER_MISTY
    118: 104,  # LEADER_LT_SURGE -> LEADER_LT_SURGE
    119: 105,  # LEADER_ERIKA -> LEADER_ERIKA
    120: 89,   # LEADER_KOGA -> ELITE_FOUR_KOGA
    121: 108,  # LEADER_BLAINE -> LEADER_BLAINE
    122: 107,  # LEADER_SABRINA -> LEADER_SABRINA
    123: 34,   # GENTLEMAN -> GENTLEMAN
    124: 110,  # RIVAL_LATE -> LEADER_BLUE
    125: 110,  # CHAMPION_RIVAL -> LEADER_BLUE
    126: 82,   # CHANNELER -> MEDIUM
    127: 8,    # TWINS -> TWINS
    128: 121,  # COOL_COUPLE -> DOUBLE_TEAM
    129: 122,  # YOUNG_COUPLE -> YOUNG_COUPLE
    135: 109,  # RED -> PKMN_TRAINER_RED
    137: 62,   # ROCKET_GRUNT_F -> TEAM_ROCKET_F
    138: 50,   # PSYCHIC_F -> PSYCHIC_F
    139: 10,   # CRUSH_GIRL -> BATTLE_GIRL
    140: 45,   # TUBER_F -> TUBER_F
    141: 17,   # POKEMON_BREEDER -> PKMN_BREEDER_F
    142: 39,   # POKEMON_RANGER_M -> PKMN_RANGER_M
    143: 40,   # POKEMON_RANGER_F -> PKMN_RANGER_F
    144: 7,    # AROMA_LADY -> AROMA_LADY
    145: 48,   # RUIN_MANIAC -> RUIN_MANIAC
    146: 33,   # LADY -> LADY
    147: 15,   # PAINTER -> ARTIST
}
TRAINER_FILES = 5  # a class's files in the NARC: a cut picture, the palette, cells, animation, the whole picture
TRAINER_PICTURE, TRAINER_PALETTE = 4, 1


def narc_files(data: bytes) -> list[bytes]:
    """The members of a Nitro archive (NARC: BTAF offsets, BTNF names, GMIF data)."""
    if data[:4] != b"NARC" or len(data) < 28:
        raise Gen4Error("not a NARC archive")
    pos = struct.unpack_from("<H", data, 12)[0]
    if data[pos:pos + 4] != b"BTAF":
        raise Gen4Error("NARC without its file table")
    size, count = struct.unpack_from("<IH", data, pos + 4)
    if pos + 12 + 8 * count > len(data):
        raise Gen4Error("NARC file table is cut short")
    spans = [struct.unpack_from("<II", data, pos + 12 + 8 * i) for i in range(count)]
    pos += size
    if data[pos:pos + 4] != b"BTNF":
        raise Gen4Error("NARC without its name table")
    pos += struct.unpack_from("<I", data, pos + 4)[0]
    if data[pos:pos + 4] != b"GMIF":
        raise Gen4Error("NARC without its data")
    base = pos + 8
    if any(start > end or base + end > len(data) for start, end in spans):
        raise Gen4Error("NARC member outside the archive")
    return [data[base + start:base + end] for start, end in spans]


def nitro_block(data: bytes, magic: bytes, kind: bytes) -> bytes:
    """The first block of a Nitro graphics file (NCGR, NCLR...) after its block header."""
    if data[:4] != magic:
        raise Gen4Error("not a %s file" % magic[::-1].decode())
    pos = struct.unpack_from("<H", data, 12)[0]
    if data[pos:pos + 4] != kind:
        raise Gen4Error("%s without its %s block" % (magic[::-1].decode(), kind[::-1].decode()))
    return data[pos:pos + struct.unpack_from("<I", data, pos + 4)[0]]


def unscramble_dp(pixels: bytes) -> bytes:
    """Diamond and Pearl's scrambled pictures (HeartGold's UnscanPokepic_DP, used for its trainer pictures): each
    16-bit word from the last back is XORed with an LCG seeded by the last word."""
    words = list(struct.unpack("<%dH" % (len(pixels) // 2), pixels))
    seed = words[-1]
    for i in range(len(words) - 1, -1, -1):
        words[i] ^= seed & 0xFFFF
        seed = (seed * 1103515245 + 24691) & 0xFFFFFFFF
    return struct.pack("<%dH" % len(words), *words)


def trainer_entry(picture: bytes, palette: bytes) -> bytes:
    """One trainer picture's pack entry from HeartGold's NCGR (two 80x80 frames side by side) and NCLR."""
    block = nitro_block(picture, b"RGCN", b"RAHC")
    height, width, depth = struct.unpack_from("<HHI", block, 8)
    pixels = block[0x20:]
    if (height, width, depth) != (SIZE // 8, 2 * SIZE // 8, 3) or len(pixels) != 2 * PICTURE:
        raise Gen4Error("a trainer picture is two 80x80 frames in 16 colours")
    pixels = unscramble_dp(pixels)
    frame = b"".join(pixels[y * SIZE:y * SIZE + SIZE // 2] for y in range(SIZE))
    colours = nitro_block(palette, b"RLCN", b"TTLP")[0x18:0x18 + 32]
    if len(colours) != 32:
        raise Gen4Error("a trainer palette has 16 colours")
    return TRAINER_MAGIC + bytes(4) + colours + frame


def convert_trainers(archive: bytes) -> list[tuple[str, bytes]]:
    """The pack entries for FireRed's trainer pictures from HeartGold's archive (TRAINER_CLASSES)."""
    files = narc_files(archive)
    out = []
    for pic, cls in sorted(TRAINER_CLASSES.items()):
        if (cls + 1) * TRAINER_FILES > len(files):
            raise Gen4Error("the trainer archive has no class %d" % cls)
        first = cls * TRAINER_FILES
        out.append(("%s%03d.bin" % (TRAINER_PREFIX, pic),
                    trainer_entry(files[first + TRAINER_PICTURE], files[first + TRAINER_PALETTE])))
    return out


def quantize(pixels: list[tuple[int, int, int, int]], colours: int = 15) -> tuple[list[int], list[int]]:
    """RGBA pixels to at most `colours` BGR555 colours and transparency (alpha under 128): (palette with colour 0
    the transparent one, each pixel's index). The rarest colour is folded into its nearest one until they fit,
    so a few edge pixels of a strip with more colours change shade and nothing else does."""
    def key(pixel):
        r, g, b, _ = pixel
        return (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10
    counts: dict[int, int] = {}
    for pixel in pixels:
        if pixel[3] >= 128:
            counts[key(pixel)] = counts.get(key(pixel), 0) + 1
    merged = {c: c for c in counts}

    def channels(c):
        return c & 31, (c >> 5) & 31, (c >> 10) & 31

    while len(counts) > colours:
        rare = min(counts, key=lambda c: (counts[c], c))
        rest = [c for c in counts if c != rare]
        near = min(rest, key=lambda c: (sum((a - b) ** 2 for a, b in zip(channels(c), channels(rare))), c))
        counts[near] += counts.pop(rare)
        for c, into in merged.items():
            if into == rare:
                merged[c] = near
    order = sorted(counts, key=lambda c: (-counts[c], c))
    index = {c: order.index(merged[c]) + 1 for c in merged}
    return [0] + order, [index[key(pixel)] if pixel[3] >= 128 else 0 for pixel in pixels]


def scale3x(rows: list[list[int]]) -> list[list[int]]:
    """Scale3x (AdvMAME3x) on a grid of palette indices: three times as large, its stairs rounded off, no colour
    it did not have."""
    h, w = len(rows), len(rows[0])

    def at(x, y):
        return rows[min(max(y, 0), h - 1)][min(max(x, 0), w - 1)]
    out = [[0] * (w * 3) for _ in range(h * 3)]
    for y in range(h):
        for x in range(w):
            a, b, c = at(x - 1, y - 1), at(x, y - 1), at(x + 1, y - 1)
            d, e, f = at(x - 1, y), at(x, y), at(x + 1, y)
            g, hh, i = at(x - 1, y + 1), at(x, y + 1), at(x + 1, y + 1)
            cells = [e] * 9
            if b != hh and d != f:
                cells = [d if d == b else e,
                         b if (d == b and e != c) or (b == f and e != a) else e,
                         f if b == f else e,
                         d if (d == b and e != g) or (d == hh and e != a) else e,
                         e,
                         f if (b == f and e != i) or (hh == f and e != c) else e,
                         d if d == hh else e,
                         hh if (d == hh and e != i) or (hh == f and e != g) else e,
                         f if hh == f else e]
            for k, v in enumerate(cells):
                out[y * 3 + k // 3][x * 3 + k % 3] = v
    return out


def enlarge_back(frame: list[list[int]]) -> list[list[int]]:
    """An 80x80 frame of indices as the 104x104 picture the port shows (BACK_SHOWN pixels, centred)."""
    big = scale3x(frame)
    step = len(big) / BACK_SHOWN
    picture = [[0] * BACK_SIZE for _ in range(BACK_SIZE)]
    edge = (BACK_SIZE - BACK_SHOWN) // 2
    for y in range(BACK_SHOWN):
        source = big[int((y + 0.5) * step)]
        for x in range(BACK_SHOWN):
            picture[edge + y][edge + x] = source[int((x + 0.5) * step)]
    return picture


def convert_back(strip: bytes) -> bytes:
    """A fan-made back's pack entry from its 400x80 RGBA strip of five 80x80 frames, each enlarged to 104x104."""
    width, height, _, rows = read_png(strip)
    if (width, height) != (BACK_FRAMES * SIZE, SIZE) or not isinstance(rows[0][0], tuple):
        raise Gen4Error("a back is an RGBA strip of five 80x80 frames")
    palette, indices = quantize([pixel for row in rows for pixel in row])
    frames = b""
    for frame in range(BACK_FRAMES):
        grid = [indices[y * width + frame * SIZE:y * width + frame * SIZE + SIZE] for y in range(SIZE)]
        for line in enlarge_back(grid):
            frames += bytes(line[x] | line[x + 1] << 4 for x in range(0, BACK_SIZE, 2))
    colours = (palette + [0] * 16)[:16]
    return BACK_MAGIC + bytes([BACK_FRAMES, BACK_SIZE, 0, 0]) + struct.pack("<16H", *colours) + frames


def convert_bg(scene: bytes) -> bytes:
    """A battle background's pack entry from its 240x112 PNG: the 15-colour palette, the tiles (identical ones once)
    and the 64x32 tile map of BG3's two screen blocks."""
    width, height, _, rows = read_png(scene)
    if (width, height) != (BG_W, BG_H) or not isinstance(rows[0][0], tuple):
        raise Gen4Error("a battle background is a 240x112 RGB or RGBA PNG")
    palette, indices = quantize([pixel for row in rows for pixel in row])
    columns, tile_rows = BG_W // 8, BG_H // 8
    tiles: list[bytes] = []
    seen: dict[bytes, int] = {}
    grid = [[0] * columns for _ in range(tile_rows)]
    for ty in range(tile_rows):
        for tx in range(columns):
            tile = bytearray()
            for y in range(8):
                base = (ty * 8 + y) * width + tx * 8
                tile += bytes(indices[base + x] | indices[base + x + 1] << 4 for x in range(0, 8, 2))
            tile = bytes(tile)
            if tile not in seen:
                if len(tiles) == BG_TILES_MAX:
                    raise Gen4Error("a battle background has more than %d different tiles" % BG_TILES_MAX)
                seen[tile] = len(tiles)
                tiles.append(tile)
            grid[ty][tx] = seen[tile]
    # BG3 reads a 64x32 map (screen blocks 26 and 27, 32 columns each) with palette 2. Above the picture and beside
    # it, the map repeats the picture's edge: its first row on the rows above (the scene is zoomed past 240x112 on
    # the 3DS), its first column on the left (columns 63 and below) and its last on the right (columns 30 up).
    mapped = [[0] * BG_MAP_COLUMNS for _ in range(BG_MAP_ROWS)]
    for row in range(BG_MAP_ROWS):
        source = grid[0] if row >= tile_rows + 16 else grid[min(row, tile_rows - 1)]
        for column in range(BG_MAP_COLUMNS):
            tile = source[column] if column < columns else source[columns - 1] if column < 47 else source[0]
            mapped[row][column] = tile | 2 << 12
    tilemap = b"".join(struct.pack("<H", mapped[row][block * 32 + column]) for block in (0, 1)
                       for row in range(BG_MAP_ROWS) for column in range(32))
    colours = (palette + [0] * 16)[:16]
    return BG_MAGIC + struct.pack("<HH", len(tiles), 0) + struct.pack("<16H", *colours) + b"".join(tiles) + tilemap


def convert(front: bytes, back: bytes, female: tuple[bytes, bytes] | None = None) -> bytes:
    """One species' pack entry from its PNG files (male front and back, optional female pair)."""
    _, _, normal, front_rows = read_png(front)
    _, _, shiny, back_rows = read_png(back)
    pictures = first_frame(front_rows) + first_frame(back_rows)
    if female is not None:
        pictures += first_frame(read_png(female[0])[3]) + first_frame(read_png(female[1])[3])
    return MAGIC + struct.pack("<BBH", 1 if female else 0, 0, 0) + bgr555(normal) + bgr555(shiny) + pictures


# --- Download ---------------------------------------------------------------------------------

def cache_dir() -> Path:
    base = os.environ.get("LOCALAPPDATA") or str(Path.home() / ".cache")
    return Path(base) / "TwinEmbers" / "gen4" / gen4_sources.COMMIT[:12]


def http_get(url: str) -> bytes:
    with urllib.request.urlopen(url, timeout=TIMEOUT) as response:
        return response.read()


def source_path(row: tuple) -> str:
    if row[1] == "icon":
        return gen4_sources.ICON_PATH.format(row[2])
    if row[1] == "unown":
        return gen4_sources.UNOWN_PATH.format(gen4_sources.UNOWN_NAMES[row[0]], row[2])
    if row[1] == "castform":
        return gen4_sources.CASTFORM_PATH.format(gen4_sources.CASTFORM_NAMES[row[0]], row[2])
    if row[1] == "spinda":
        return gen4_sources.SPINDA_PATH
    if row[1] == "trainers":
        return gen4_sources.TRAINERS_PATH
    if row[1] in ("back", "bg"):
        return row[2]
    return gen4_sources.PATH.format(*row[:3])


def file_name(row: tuple) -> str:
    """The name a listed file has in the cache or in a folder given for offline use."""
    if row[1] == "icon":
        return "icon_{:08d}.png".format(row[2])
    if row[1] == "unown":
        return "unown_{:02d}_{}.png".format(row[0], row[2])
    if row[1] == "castform":
        return "castform_{:02d}_{}.png".format(row[0], row[2])
    if row[1] == "spinda":
        return "pokepic.c"
    if row[1] == "trainers":
        return "trainers_a058.narc"
    if row[1] == "back":
        return "back_{:03d}.png".format(row[0])
    if row[1] == "bg":
        return "bg_{:02d}.png".format(row[0])
    return "{:03d}_{}_{}.png".format(*row[:3])


def file_url(row: tuple) -> str:
    if row[1] == "back":
        return URL.format(repo=BACK_REPO, commit=BACK_COMMIT, path=source_path(row))
    if row[1] == "bg":
        return URL.format(repo=BG_REPO, commit=BG_COMMIT, path=source_path(row))
    return URL.format(repo=gen4_sources.REPO, commit=gen4_sources.COMMIT, path=source_path(row))


def all_files() -> tuple:
    """Every file the download may fetch: pret's (gen4_sources.FILES), the fan-made backs, then the battle scenes."""
    return tuple(gen4_sources.FILES) + BACK_FILES + BG_FILES


def matches(row: tuple, data: bytes | None) -> bool:
    return data is not None and len(data) == row[3] and hashlib.sha256(data).hexdigest() == row[4]


class Downloader:
    """Gets each listed PNG once: a folder given for offline use, then the cache, then GitHub.

    Anything that does not match its listed size and SHA-256 is thrown away. `threads` 0 fetches
    on the calling thread (the web Builder: Pyodide cannot start threads)."""

    def __init__(self, cache: Path | None = None, folder: Path | None = None,
                 get: Callable[[str], bytes] | None = http_get, threads: int = 8):
        self.cache, self.folder, self.get, self.threads = cache, folder, get, threads

    def fetch(self, row: tuple) -> tuple[bytes | None, bool]:
        """(the file or None, whether it came from the network)."""
        for place in (self.folder, self.cache):
            if place is not None:
                path = place / file_name(row)
                data = path.read_bytes() if path.is_file() else None
                if matches(row, data):
                    return data, False
        if self.get is None:
            return None, False
        try:
            data = self.get(file_url(row))
        except Exception:  # noqa: BLE001 - any failure leaves the species on FireRed's pictures
            return None, False
        if not matches(row, data):
            return None, False
        if self.cache is not None:
            try:
                self.cache.mkdir(parents=True, exist_ok=True)
                part = self.cache / (file_name(row) + ".part")
                part.write_bytes(data)
                part.replace(self.cache / file_name(row))
            except OSError:
                pass
        return data, True


def pack_files(downloader: Downloader, progress: Callable[[int, int], None] | None = None
               ) -> tuple[list[tuple[str, bytes]], dict]:
    """The pack entries for every species whose files could be had, the icons if all could, and a summary."""
    rows = list(all_files())
    got: dict[tuple, bytes] = {}
    downloaded = done = 0
    # Plain threads taking rows from a queue: concurrent.futures would bring multiprocessing (and ctypes, libffi,
    # pyexpat) into the frozen Builder for nothing.
    todo: queue.Queue = queue.Queue()
    results: queue.Queue = queue.Queue()
    for row in rows:
        todo.put(row)

    def work() -> None:
        while True:
            try:
                row = todo.get_nowait()
            except queue.Empty:
                return
            results.put((row, downloader.fetch(row)))

    workers = [threading.Thread(target=work, daemon=True) for _ in range(downloader.threads)]
    for worker in workers:
        worker.start()
    if not workers:
        work()
    for _ in rows:
        row, (data, fresh) = results.get()
        done += 1
        downloaded += fresh
        if data is not None:
            got[row[:3]] = data
        if progress is not None:
            progress(done, len(rows))
    for worker in workers:
        worker.join()
    files, missing = [], []
    icon_rows = sorted((row for row in rows if row[1] == "icon"), key=lambda row: row[0])
    form_rows = {kind: [row for row in rows if row[1] == kind] for kind in ("unown", "castform")}
    spinda = [row for row in rows if row[1] == "spinda"]
    trainers = [row for row in rows if row[1] == "trainers"]
    backs = [row for row in rows if row[1] == "back"]
    scenes = [row for row in rows if row[1] == "bg"]
    rows = [row for row in rows if row[1] in ("male", "female")]
    for dex in sorted({row[0] for row in rows}):
        # The first listed pair is the species' own; a second one (female) differs from it.
        bodies = [body for body in ("male", "female") if any(row[:2] == (dex, body) for row in rows)]
        pairs = [(got.get((dex, body, "front")), got.get((dex, body, "back"))) for body in bodies]
        if any(None in pair for pair in pairs):
            missing.append(dex)
            continue
        files.append(("%s%03d.bin" % (PREFIX, dex), convert(*pairs[0], pairs[1] if len(pairs) > 1 else None)))
    species = len(files)
    for kind, dex in (("unown", 201), ("castform", 351)):
        for form in sorted({row[0] for row in form_rows[kind]}):
            pair = (got.get((form, kind, "front")), got.get((form, kind, "back")))
            if None not in pair:
                files.append(("%s%03d_%02d.bin" % (PREFIX, dex, form), convert(*pair)))
    if spinda and spinda[0][:3] in got:
        files.append((SPINDA, convert_spinda(got[spinda[0][:3]])))
    icons = icon_rows and all(row[:3] in got for row in icon_rows)
    if icons:
        files.append((ICONS, convert_icons([got[row[:3]] for row in icon_rows])))
    trainer_files = convert_trainers(got[trainers[0][:3]]) if trainers and trainers[0][:3] in got else []
    files += trainer_files
    back_files = [("%s%03d.bin" % (BACK_PREFIX, row[0]), convert_back(got[row[:3]])) for row in backs
                  if row[:3] in got]
    files += back_files
    # Every terrain gets its scene, or none of them does (half a set would mix two looks).
    converted = {row[0]: convert_bg(got[row[:3]]) for row in scenes if row[:3] in got}
    bg_files = []
    if scenes and len(converted) == len(scenes):
        bg_files = [("%s%03d.bin" % (BG_PREFIX, terrain), converted[scene]) for terrain, scene in enumerate(BG_TERRAIN)]
    files += bg_files
    return files, {"species": species, "missing": missing, "icons": bool(icons), "trainers": len(trainer_files),
                   "backs": len(back_files), "backgrounds": len(bg_files), "downloaded": downloaded}
