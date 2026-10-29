"""Optional HeartGold battle sprites for the pack: downloaded, checked, converted.

The sprites are not in the player's FireRed ROM and never ship with Twin
Embers. When the player asks for them, the Builder downloads the PNG files that
gen4_sources.py lists from pret/pokeheartgold at the pinned commit, keeps each
one only if its size and SHA-256 match the list, and converts it for the pack.
The pack stays valid without them: the engine ABI does not cover these
entries, a species without its file keeps FireRed's pictures, and a build with
no network writes the same pack as before.

Pack entries, one per species with a complete set:

    graphics/gen4/battle/NNN.bin    NNN = national dex number, 001-386

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
"""
from __future__ import annotations

import hashlib
import os
import queue
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


def file_name(row: tuple) -> str:
    """The name a listed file has in the cache or in a folder given for offline use."""
    return "{:03d}_{}_{}.png".format(*row[:3])


def file_url(row: tuple) -> str:
    return URL.format(repo=gen4_sources.REPO, commit=gen4_sources.COMMIT, path=gen4_sources.PATH.format(*row[:3]))


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
    """The pack entries for every species whose files could be had, and a summary."""
    rows = gen4_sources.FILES
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
    for dex in sorted({row[0] for row in rows}):
        # The first listed pair is the species' own; a second one (female) differs from it.
        bodies = [body for body in ("male", "female") if any(row[:2] == (dex, body) for row in rows)]
        pairs = [(got.get((dex, body, "front")), got.get((dex, body, "back"))) for body in bodies]
        if any(None in pair for pair in pairs):
            missing.append(dex)
            continue
        files.append(("%s%03d.bin" % (PREFIX, dex), convert(*pairs[0], pairs[1] if len(pairs) > 1 else None)))
    return files, {"species": len(files), "missing": missing, "downloaded": downloaded}
