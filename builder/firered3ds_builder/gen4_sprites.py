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
ICON_MAGIC = b"G4I1"
ICON_PALETTES = 6
URL = "https://raw.githubusercontent.com/{repo}/{commit}/{path}"
TIMEOUT = 30


class Gen4Error(ValueError):
    pass


# --- PNG (the 4- and 8-bit indexed images pret keeps) -------------------------------------------

def read_png(data: bytes) -> tuple[int, int, list[tuple[int, int, int]], list[list[int]]]:
    """(width, height, palette as RGB, rows of palette indices) of an indexed PNG."""
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
    if colour != 3 or depth not in (4, 8) or interlace:
        raise Gen4Error("only non-interlaced 4- or 8-bit indexed PNGs are read")
    stride = (width * depth + 7) // 8
    raw = zlib.decompress(bytes(idat))
    if len(raw) != (stride + 1) * height:
        raise Gen4Error("PNG data has the wrong size")
    rows, previous = [], bytearray(stride)
    for y in range(height):
        kind = raw[y * (stride + 1)]
        line = bytearray(raw[y * (stride + 1) + 1:(y + 1) * (stride + 1)])
        for x in range(stride):
            left = line[x - 1] if x else 0
            up = previous[x]
            corner = previous[x - 1] if x else 0
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
        if depth == 8:
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
    return "{:03d}_{}_{}.png".format(*row[:3])


def file_url(row: tuple) -> str:
    return URL.format(repo=gen4_sources.REPO, commit=gen4_sources.COMMIT, path=source_path(row))


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
    rows = list(gen4_sources.FILES)
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
    return files, {"species": species, "missing": missing, "icons": bool(icons), "trainers": len(trainer_files),
                   "downloaded": downloaded}
