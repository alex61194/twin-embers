"""Synthetic-only tests for the optional HeartGold battle sprites (no network, no real sprite)."""
import hashlib
import struct
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
import zlib

from firered3ds_builder import gen4_sprites


def png(rows, palette, depth=4, filters=(0,)):
    """An indexed PNG of `rows`, each row written with the next filter of `filters`."""
    width, height = len(rows[0]), len(rows)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    lines, previous = [], None
    for y, row in enumerate(rows):
        if depth == 8:
            raw = bytes(row)
        else:
            raw = bytes((row[x] << 4) | (row[x + 1] if x + 1 < width else 0) for x in range(0, width, 2))
        previous = previous or bytes(len(raw))
        kind = filters[y % len(filters)]
        out = bytearray()
        for x, value in enumerate(raw):
            left = raw[x - 1] if x else 0
            up = previous[x]
            corner = previous[x - 1] if x else 0
            if kind == 0:
                guess = 0
            elif kind == 1:
                guess = left
            elif kind == 2:
                guess = up
            elif kind == 3:
                guess = (left + up) // 2
            else:
                p = left + up - corner
                guess = min((abs(p - left), 0, left), (abs(p - up), 1, up), (abs(p - corner), 2, corner))[2]
            out.append((value - guess) & 0xFF)
        lines.append(bytes([kind]) + bytes(out))
        previous = raw
    header = struct.pack(">IIBBBBB", width, height, depth, 3, 0, 0, 0)
    plte = b"".join(bytes(colour) for colour in palette)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"PLTE", plte)
            + chunk(b"IDAT", zlib.compress(b"".join(lines))) + chunk(b"IEND", b""))


def sprite(seed, colour):
    """Two 80x80 frames side by side, like pret's files."""
    rows = [[(x * 3 + y * 5 + seed) % 16 for x in range(160)] for y in range(80)]
    palette = [(colour, (i * 16) % 256, 255 - i * 8) for i in range(16)]
    return rows, palette


def row_for(dex, body, side, data):
    return (dex, body, side, len(data), hashlib.sha256(data).hexdigest())


class PngTest(unittest.TestCase):
    def test_every_filter_and_depth_reads_back(self):
        rows, palette = sprite(3, 40)
        for depth in (4, 8):
            for filters in ((0,), (1,), (2,), (3,), (4,), (0, 1, 2, 3, 4)):
                width, height, got_palette, got = gen4_sprites.read_png(png(rows, palette, depth, filters))
                self.assertEqual((width, height), (160, 80))
                self.assertEqual(got, rows)
                self.assertEqual(got_palette, palette)

    def test_not_a_png_or_not_indexed_is_refused(self):
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.read_png(b"GIF89a")
        data = bytearray(png([[0] * 8] * 8, [(0, 0, 0)] * 16))
        data[25] = 2  # colour type: truecolour
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.read_png(bytes(data))


class ConvertTest(unittest.TestCase):
    def test_layout_palettes_and_first_frame(self):
        front_rows, normal = sprite(1, 200)
        back_rows, shiny = sprite(2, 100)
        entry = gen4_sprites.convert(png(front_rows, normal), png(back_rows, shiny))
        self.assertEqual(entry[:4], b"G4B1")
        self.assertEqual(entry[4], 0)
        self.assertEqual(len(entry), 72 + 2 * gen4_sprites.PICTURE)
        r, g, b = normal[5]
        self.assertEqual(struct.unpack_from("<H", entry, 8 + 2 * 5)[0], (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10)
        r, g, b = shiny[5]
        self.assertEqual(struct.unpack_from("<H", entry, 40 + 2 * 5)[0], (r >> 3) | (g >> 3) << 5 | (b >> 3) << 10)
        # Pixel (x=3, y=7) of the front: the high nibble of byte 7*40 + 1.
        self.assertEqual(entry[72 + 7 * 40 + 1] >> 4, front_rows[7][3])
        self.assertEqual(entry[72 + gen4_sprites.PICTURE + 7 * 40 + 1] >> 4, back_rows[7][3])

    def test_female_pair_follows(self):
        files = [png(*sprite(i, 10 * i)) for i in range(4)]
        entry = gen4_sprites.convert(files[0], files[1], (files[2], files[3]))
        self.assertEqual(entry[4], 1)
        self.assertEqual(len(entry), 72 + 4 * gen4_sprites.PICTURE)

    def test_wrong_size_is_refused(self):
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert(png([[0] * 64] * 64, [(0, 0, 0)] * 16), png([[0] * 64] * 64, [(0, 0, 0)] * 16))


class DownloadTest(unittest.TestCase):
    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.data = {}
        rows = []
        # Species 1: a male pair; species 2: male and female pairs; species 3: female only.
        for dex, bodies in ((1, ("male",)), (2, ("male", "female")), (3, ("female",))):
            for body in bodies:
                for side in ("front", "back"):
                    data = png(*sprite(dex * 7 + len(body) + len(side), dex * 30))
                    self.data[(dex, body, side)] = data
                    rows.append(row_for(dex, body, side, data))
        patcher = patch.object(gen4_sprites.gen4_sources, "FILES", tuple(rows))
        patcher.start()
        self.addCleanup(patcher.stop)
        self.urls = []

    def get(self, url):
        self.urls.append(url)
        for (dex, body, side), data in self.data.items():
            if url.endswith(gen4_sprites.gen4_sources.PATH.format(dex, body, side)):
                return data
        raise OSError("not found")

    def test_downloads_converts_and_caches(self):
        cache = self.root / "cache"
        files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(cache, get=self.get, threads=2))
        self.assertEqual([path for path, _ in files], ["graphics/gen4/battle/%03d.bin" % d for d in (1, 2, 3)])
        self.assertEqual(info, {"species": 3, "missing": [], "downloaded": 8})
        entries = dict(files)
        self.assertEqual(entries["graphics/gen4/battle/002.bin"][4], 1)
        self.assertEqual(entries["graphics/gen4/battle/003.bin"][4], 0)
        self.assertTrue(all(u.startswith("https://raw.githubusercontent.com/pret/pokeheartgold/") for u in self.urls))
        self.urls.clear()
        again, info = gen4_sprites.pack_files(gen4_sprites.Downloader(cache, get=self.get))
        self.assertEqual(again, files)
        self.assertEqual((info["downloaded"], self.urls), (0, []))

    def test_a_changed_file_is_refused_and_its_species_left_out(self):
        good = self.get

        def get(url):
            data = good(url)
            return data + b"x" if "/0002/female/" in url else data

        files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(self.root / "cache", get=get))
        self.assertEqual([path for path, _ in files], ["graphics/gen4/battle/001.bin", "graphics/gen4/battle/003.bin"])
        self.assertEqual(info["missing"], [2])
        self.assertFalse(any("002_female" in p.name for p in (self.root / "cache").iterdir()))

    def test_offline_gives_nothing_without_failing(self):
        def offline(url):
            raise OSError("no network")

        files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(None, get=offline))
        self.assertEqual((files, info["species"], info["missing"]), ([], 0, [1, 2, 3]))

    def test_folder_is_used_before_the_network(self):
        folder = self.root / "folder"
        folder.mkdir()
        for (dex, body, side), data in self.data.items():
            (folder / "{:03d}_{}_{}.png".format(dex, body, side)).write_bytes(data)
        files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(None, folder, get=None))
        self.assertEqual((info["species"], info["downloaded"]), (3, 0))


class SourcesTest(unittest.TestCase):
    def test_pinned_list_covers_firered_species_only(self):
        from firered3ds_builder import gen4_sources
        self.assertRegex(gen4_sources.COMMIT, "^[0-9a-f]{40}$")
        species = {row[0] for row in gen4_sources.FILES}
        self.assertEqual(species, set(range(1, 387)))
        for dex in species:
            bodies = [row[1:3] for row in gen4_sources.FILES if row[0] == dex]
            self.assertIn(bodies, ([("male", "front"), ("male", "back")],
                                   [("female", "front"), ("female", "back")],
                                   [("male", "front"), ("male", "back"), ("female", "front"), ("female", "back")]))
        for row in gen4_sources.FILES:
            self.assertGreater(row[3], 0)
            self.assertRegex(row[4], "^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
