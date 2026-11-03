"""Synthetic-only tests for the optional HeartGold battle sprites (no network, no real sprite)."""
import hashlib
import re
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
        for patcher in (patch.object(gen4_sprites.gen4_sources, "FILES", tuple(rows)),
                        patch.object(gen4_sprites, "BACK_FILES", ()), patch.object(gen4_sprites, "BG_FILES", ()),
                        patch.object(gen4_sprites, "NPC_FILES", ()), patch.object(gen4_sprites, "PLAYER_FILES", ())):
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
        self.assertEqual(info, {"species": 3, "missing": [], "icons": False, "trainers": 0, "backs": 0,
                                "backgrounds": 0, "npcs": 0, "player": 0, "downloaded": 8})
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


class IconTest(unittest.TestCase):
    def icon(self, seed, palette_colour):
        rows = [[(x + y * 3 + seed) % 16 for x in range(32)] for y in range(64)]
        return png(rows, [(palette_colour, i * 16, 0) for i in range(16)])

    def test_icons_entry_layout(self):
        icons = [self.icon(i, 10 if i % 2 else 20) for i in range(4)]
        entry = gen4_sprites.convert_icons(icons)
        self.assertEqual(entry[:4], b"G4I1")
        self.assertEqual(struct.unpack_from("<HH", entry, 4), (4, 2))
        self.assertEqual(list(entry[200:204]), [0, 1, 0, 1])
        self.assertEqual(len(entry), 200 + 4 + 4 * 1024)
        # Icon 1, frame 1, tile (1, 2), row 3: its pixels (8..15, 32 + 16 + 3) in the low nibble first.
        tile = 204 + 1024 + 512 + (2 * 4 + 1) * 32 + 3 * 4
        y = 32 + 16 + 3
        self.assertEqual(entry[tile] & 15, (8 + y * 3 + 1) % 16)
        self.assertEqual(entry[tile] >> 4, (9 + y * 3 + 1) % 16)

    def test_too_many_palettes_or_a_wrong_size_is_refused(self):
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_icons([self.icon(0, c * 30) for c in range(7)])
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_icons([png([[0] * 32] * 32, [(0, 0, 0)] * 16)])

    def test_icons_only_when_all_could_be_had(self):
        icons = {key: self.icon(key, 40) for key in (0, 1, 2)}
        rows = tuple((key, "icon", 600 + key, len(data), hashlib.sha256(data).hexdigest())
                     for key, data in icons.items())

        def get(url):
            for key, data in icons.items():
                if url.endswith("poke_icon_%08d.png" % (600 + key)):
                    return data
            raise OSError("not found")

        with patch.object(gen4_sprites.gen4_sources, "FILES", rows):
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(None, get=get))
            self.assertEqual([path for path, _ in files], ["graphics/gen4/icons.bin"])
            self.assertTrue(info["icons"])
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(
                None, get=lambda url: (_ for _ in ()).throw(OSError()) if "00000601" in url else get(url)))
            self.assertEqual((files, info["icons"]), ([], False))


class SpindaTest(unittest.TestCase):
    def test_spot_tables_from_a_c_file(self):
        source = b"""static const u8 sSpindaSpot1Coords[][2] = {
    { 0x1B, 0x0F },
    { 3, 4 },
    { 0xFF, 0xFF },
};
""" + b"".join(b"static const u8 sSpindaSpot%dCoords[][2] = {\n    { 1, 2 },\n    { 0xFF, 0xFF },\n};\n" % i
                for i in (2, 3, 4))
        self.assertEqual(gen4_sprites.convert_spinda(source),
                         b"G4P1" + bytes([2, 0x1B, 0x0F, 3, 4]) + bytes([1, 1, 2]) * 3)

    def test_a_missing_or_open_table_is_refused(self):
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_spinda(b"nothing here")
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_spinda(b"".join(b"static const u8 sSpindaSpot%dCoords[][2] = {\n    { 1, 2 },\n};\n" % i
                                                 for i in (1, 2, 3, 4)))


def nitro(magic, kind, body):
    """A Nitro graphics file with one block."""
    block = kind + struct.pack("<I", 8 + len(body)) + body
    return magic + struct.pack("<HHIHH", 0xFEFF, 0x0100, 16 + len(block), 16, 1) + block


def narc(members):
    """A NARC archive of `members`, without names."""
    data, spans = b"", []
    for member in members:
        spans.append((len(data), len(data) + len(member)))
        data += member + bytes(-len(member) % 4)
    fat = b"BTAF" + struct.pack("<IHH", 12 + 8 * len(spans), len(spans), 0) + b"".join(
        struct.pack("<II", *span) for span in spans)
    fnt = b"BTNF" + struct.pack("<I", 16) + struct.pack("<IHH", 4, 0, 1)
    img = b"GMIF" + struct.pack("<I", 8 + len(data)) + data
    return b"NARC" + struct.pack("<HHIHH", 0xFFFE, 0x0100, 16 + len(fat) + len(fnt) + len(img), 16, 3) + fat + fnt + img


def scramble_dp(plain, seed):
    """The inverse of HeartGold's UnscanPokepic_DP: the last word is the seed (and its pixels read back as 0)."""
    words = list(struct.unpack("<%dH" % (len(plain) // 2), plain))
    words[-1] = 0
    out, value = [0] * len(words), seed
    for i in range(len(words) - 1, -1, -1):
        out[i] = words[i] ^ (value & 0xFFFF)
        value = (value * 1103515245 + 24691) & 0xFFFFFFFF
    return struct.pack("<%dH" % len(out), *out)


class TrainerTest(unittest.TestCase):
    def picture(self, seed):
        """Two 80x80 frames side by side, rows of 80 bytes, as HeartGold keeps them before scrambling."""
        return bytes((x * 7 + y * 3 + seed) % 256 for y in range(80) for x in range(80))

    def files(self, cls, seed):
        plain = self.picture(seed)
        picture = nitro(b"RGCN", b"RAHC", struct.pack("<HHIIIII", 10, 20, 3, 0, 0, 6400, 0x18)
                        + scramble_dp(plain, 0xBEEF + seed))
        palette = nitro(b"RLCN", b"TTLP", struct.pack("<IIII", 3, 0, 32, 0x10)
                        + struct.pack("<16H", *[(cls * 16 + i) & 0x7FFF for i in range(16)]))
        return [b"cut", palette, b"cells", b"anim", picture], plain

    def archive(self, classes):
        members, plains = [], {}
        for cls in range(classes):
            files, plains[cls] = self.files(cls, cls)
            members += files
        return narc(members), plains

    def test_entry_layout_palette_and_first_frame(self):
        files, plain = self.files(3, 5)
        entry = gen4_sprites.trainer_entry(files[4], files[1])
        self.assertEqual(entry[:8], b"G4T1" + bytes(4))
        self.assertEqual(struct.unpack_from("<16H", entry, 8), tuple(3 * 16 + i for i in range(16)))
        self.assertEqual(len(entry), 40 + gen4_sprites.PICTURE)
        # Row 7 of the first frame is bytes 7*80 .. 7*80+39 of the unscrambled picture.
        self.assertEqual(entry[40 + 7 * 40:40 + 8 * 40], plain[7 * 80:7 * 80 + 40])

    def test_every_firered_picture_gets_its_class(self):
        classes = max(gen4_sprites.TRAINER_CLASSES.values()) + 1
        archive, plains = self.archive(classes)
        out = dict(gen4_sprites.convert_trainers(archive))
        self.assertEqual(sorted(out), ["graphics/gen4/trainer/%03d.bin" % pic
                                       for pic in sorted(gen4_sprites.TRAINER_CLASSES)])
        brock = out["graphics/gen4/trainer/116.bin"]
        self.assertEqual(brock[40:80], plains[gen4_sprites.TRAINER_CLASSES[116]][:40])

    def test_player_pictures_are_never_replaced(self):
        # FireRed's Leaf, Oak and the elite four without a HeartGold picture keep their own.
        for pic in (112, 114, 132, 136):
            self.assertNotIn(pic, gen4_sprites.TRAINER_CLASSES)

    def test_a_short_or_wrong_archive_is_refused(self):
        archive, _ = self.archive(3)
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_trainers(archive)
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_trainers(b"NOPE" + bytes(40))
        files, _ = self.files(0, 0)
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.trainer_entry(nitro(b"RGCN", b"RAHC", bytes(64)), files[1])

    def test_trainers_come_with_the_download(self):
        classes = max(gen4_sprites.TRAINER_CLASSES.values()) + 1
        archive, _ = self.archive(classes)
        rows = ((0, "trainers", "front", len(archive), hashlib.sha256(archive).hexdigest()),)
        with patch.object(gen4_sprites.gen4_sources, "FILES", rows):
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(None, get=lambda url: archive))
            self.assertEqual(info["trainers"], len(gen4_sprites.TRAINER_CLASSES))
            self.assertEqual(len(files), len(gen4_sprites.TRAINER_CLASSES))
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(None, get=lambda url: archive + b"x"))
            self.assertEqual((files, info["trainers"]), ([], 0))


def rgba_png(rows):
    """An RGBA PNG of rows of (r, g, b, a), every row with filter 1 (left), as image editors often write."""
    width, height = len(rows[0]), len(rows)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    lines = b""
    for row in rows:
        raw = bytes(v for pixel in row for v in pixel)
        lines += b"\x01" + bytes((raw[x] - (raw[x - 4] if x >= 4 else 0)) & 0xFF for x in range(len(raw)))
    header = struct.pack(">IIBBBBB", width, height, 8, 6, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(lines))
            + chunk(b"IEND", b""))


class BackTest(unittest.TestCase):
    def strip(self, colours):
        """Five 80x80 frames of `colours` opaque colours in stripes, a transparent corner and a half-transparent
        pixel; frame f's top-left pixel is colour f."""
        rows = []
        for y in range(80):
            row = []
            for x in range(400):
                frame, fx = divmod(x, 80)
                if fx < 4 and y < 4:
                    row.append((0, 0, 0, 0))
                elif (fx, y) == (10, 10):
                    row.append((255, 255, 255, 60))
                elif (fx, y) == (5, 5):
                    row.append((frame * 40, 8, 8, 255))
                else:
                    c = (fx + y) % colours
                    row.append((c * 8, 255 - c * 8, 128, 255))
            rows.append(row)
        return rows

    def test_rgba_png_reads_back(self):
        rows = self.strip(4)
        width, height, palette, got = gen4_sprites.read_png(rgba_png(rows))
        self.assertEqual((width, height, palette), (400, 80, []))
        self.assertEqual(got, rows)

    def test_entry_layout_transparency_and_frames(self):
        # Each frame its own colour, with a transparent corner and a half-transparent pixel.
        rows = [[(0, 0, 0, 0) if fx < 4 and y < 4 else (255, 255, 255, 60) if (fx, y) == (10, 10)
                 else (f * 40, 8, 8, 255) for f in range(5) for fx in range(80)] for y in range(80)]
        entry = gen4_sprites.convert_back(rgba_png(rows))
        self.assertEqual(entry[:8], b"G4K2" + bytes([5, 104, 0, 0]))
        self.assertEqual(len(entry), 40 + 5 * gen4_sprites.BACK_PICTURE)
        palette = struct.unpack_from("<16H", entry, 8)

        def pixel(frame, x, y):
            return (entry[40 + frame * gen4_sprites.BACK_PICTURE + y * 52 + x // 2] >> (4 * (x & 1))) & 15

        for frame in range(5):
            self.assertEqual(pixel(frame, 0, 0), 0)        # the picture's edge
            self.assertEqual(pixel(frame, 103, 103), 0)
            self.assertEqual(pixel(frame, 3, 3), 0)        # transparent, enlarged
            self.assertEqual(pixel(frame, 14, 14), 0)      # alpha under 128, enlarged
            r = frame * 40
            self.assertEqual(palette[pixel(frame, 52, 52)], (r >> 3) | (8 >> 3) << 5 | (8 >> 3) << 10)
            self.assertEqual(pixel(frame, 101, 101), pixel(frame, 52, 52))

    def test_scale3x_rounds_a_corner_and_keeps_its_colours(self):
        self.assertEqual(gen4_sprites.scale3x([[1, 1], [1, 0]])[3:6], [[1, 1, 1, 1, 1, 0],
                                                                     [1, 1, 1, 1, 0, 0],
                                                                     [1, 1, 1, 0, 0, 0]])
        self.assertEqual(gen4_sprites.scale3x([[2] * 3] * 3), [[2] * 9] * 9)

    def test_enlarged_back_is_centred_on_its_picture(self):
        picture = gen4_sprites.enlarge_back([[5] * 80 for _ in range(80)])
        self.assertEqual((len(picture), len(picture[0])), (104, 104))
        self.assertEqual(sum(v == 5 for row in picture for v in row), 100 * 100)
        self.assertEqual((picture[1][1], picture[2][2], picture[101][101], picture[102][102]), (0, 5, 5, 0))

    def test_too_many_colours_fold_into_fifteen(self):
        palette, indices = gen4_sprites.quantize([(c * 8, 0, 0, 255) for c in range(20) for _ in range(c + 1)]
                                                 + [(0, 0, 0, 0)])
        self.assertEqual(len(palette), 16)
        self.assertEqual(palette[0], 0)
        self.assertEqual(indices[-1], 0)
        self.assertTrue(all(1 <= i <= 15 for i in indices[:-1]))
        # The commonest colour keeps its own entry, first after transparency.
        self.assertEqual(palette[1], 19)
        # Black is a colour like any other, never transparency.
        palette, indices = gen4_sprites.quantize([(0, 0, 0, 255), (8, 8, 8, 255)])
        self.assertEqual(indices, [1, 2] if palette[1] == 0 else [2, 1])

    def test_a_wrong_size_or_indexed_strip_is_refused(self):
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_back(rgba_png([[(0, 0, 0, 255)] * 80] * 80))
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_back(png([[0] * 400] * 80, [(0, 0, 0)] * 16))

    def test_backs_come_with_the_download_from_their_own_repository(self):
        strip = rgba_png(self.strip(3))
        rows = ((0, "back", "a/red.png", len(strip), hashlib.sha256(strip).hexdigest()),)
        urls = []

        def get(url):
            urls.append(url)
            return strip

        with patch.object(gen4_sprites.gen4_sources, "FILES", ()), patch.object(gen4_sprites, "BACK_FILES", rows), \
                patch.object(gen4_sprites, "BG_FILES", ()), patch.object(gen4_sprites, "NPC_FILES", ()):
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(None, get=get), groups=("pokemon",))
        self.assertEqual([path for path, _ in files], ["graphics/gen4/back/000.bin"])
        self.assertEqual(info["backs"], 1)
        self.assertEqual(urls, ["https://raw.githubusercontent.com/%s/%s/a/red.png"
                                % (gen4_sprites.BACK_REPO, gen4_sprites.BACK_COMMIT)])

    def test_pinned_backs_are_red_and_leaf(self):
        self.assertEqual([row[:2] for row in gen4_sprites.BACK_FILES], [(0, "back"), (1, "back")])
        self.assertRegex(gen4_sprites.BACK_COMMIT, "^[0-9a-f]{40}$")
        for row in gen4_sprites.BACK_FILES:
            self.assertRegex(row[4], "^[0-9a-f]{64}$")


def rgb_png(rows):
    """An RGB PNG of rows of (r, g, b), filter 0."""
    width, height = len(rows[0]), len(rows)

    def chunk(kind, body):
        return struct.pack(">I", len(body)) + kind + body + struct.pack(">I", zlib.crc32(kind + body))

    lines = b"".join(b"\x00" + bytes(v for pixel in row for v in pixel) for row in rows)
    header = struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)
    return (b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", header) + chunk(b"IDAT", zlib.compress(lines))
            + chunk(b"IEND", b""))


class BackgroundTest(unittest.TestCase):
    COLOURS = [(255, 0, 0), (0, 255, 0), (0, 0, 255), (255, 255, 0)]

    def scene(self):
        """A 240x112 scene: a colour per 8x8 tile (so it holds four different tiles in a repeating pattern)."""
        return [[self.COLOURS[((x // 8) + (y // 8) * 3) % 4] for x in range(240)] for y in range(112)]

    def entry(self):
        return gen4_sprites.convert_bg(rgb_png(self.scene()))

    def test_an_rgb_png_is_read(self):
        width, height, palette, rows = gen4_sprites.read_png(rgb_png([[(1, 2, 3), (4, 5, 6)]]))
        self.assertEqual((width, height, palette, rows), (2, 1, [], [[(1, 2, 3, 255), (4, 5, 6, 255)]]))

    def test_entry_layout_and_identical_tiles_stored_once(self):
        entry = self.entry()
        self.assertEqual(entry[:4], b"G4G1")
        tiles = struct.unpack("<H", entry[4:6])[0]
        self.assertEqual(tiles, 4)
        self.assertEqual(len(entry), 8 + 32 + tiles * 32 + 64 * 32 * 2)

    def test_the_map_shows_the_scene_and_mirrors_it_around(self):
        entry = self.entry()
        tiles = struct.unpack("<H", entry[4:6])[0]
        entries = struct.unpack("<2048H", entry[40 + tiles * 32:])

        def at(column, row):
            return entries[(column // 32) * 1024 + row * 32 + column % 32]
        # Every entry takes palette 2, as BG3's three palettes start there.
        self.assertTrue(all(value >> 12 == 2 for value in entries))
        # The picture itself: the tile of each 8x8 square, in the colour that square has.
        for row in (0, 5, 13):
            for column in (0, 7, 29):
                colour = self.COLOURS[(column + row * 3) % 4]
                tile = at(column, row) & 0x3ff
                index = entry[40 + tile * 32] & 0xF
                palette = struct.unpack("<16H", entry[8:40])[index]
                self.assertEqual(((palette & 31) * 255 // 31, (palette >> 5 & 31) * 255 // 31,
                                  (palette >> 10 & 31) * 255 // 31), colour)
        H, V = 1 << 10, 1 << 11
        # The picture's own tiles are not flipped.
        self.assertFalse(any(at(column, row) & (H | V) for row in range(14) for column in range(30)))
        # Beside it, mirrored: its last columns reversed on the right, its first ones on the left (wrapping from 63).
        for row in range(14):
            self.assertEqual(at(30, row), at(29, row) | H)
            self.assertEqual(at(31, row), at(28, row) | H)
            self.assertEqual(at(46, row), at(13, row) | H)
            self.assertEqual(at(63, row), at(0, row) | H)
            self.assertEqual(at(62, row), at(1, row) | H)
            self.assertEqual(at(47, row), at(16, row) | H)
        # Above it (rows 31 down wrap to the top) its top rows upside down, below it its bottom ones.
        for column in range(30):
            self.assertEqual(at(column, 31), at(column, 0) | V)
            self.assertEqual(at(column, 30), at(column, 1) | V)
            self.assertEqual(at(column, 18), at(column, 13) | V)
            self.assertEqual(at(column, 14), at(column, 13) | V)
            self.assertEqual(at(column, 17), at(column, 10) | V)
        # The corners take both flips.
        self.assertEqual(at(63, 31), at(0, 0) | H | V)
        self.assertEqual(at(30, 30), at(29, 1) | H | V)

    def test_a_wrong_size_or_too_many_tiles_is_refused(self):
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_bg(rgb_png([[(0, 0, 0)] * 240] * 100))
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_bg(png([[0] * 240] * 112, [(0, 0, 0)] * 16, depth=8))
        # 30x14 = 420 different tiles fit; with a 512 limit made smaller, they do not.
        noisy = [[((x * 7 + y * 13) % 15 * 16, (x * 3 + y) % 15 * 16, 0) for x in range(240)] for y in range(112)]
        with patch.object(gen4_sprites, "BG_TILES_MAX", 10), self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_bg(rgb_png(noisy))

    def test_the_pinned_scenes_and_terrains(self):
        self.assertRegex(gen4_sprites.BG_COMMIT, "^[0-9a-f]{40}$")
        self.assertEqual([row[:2] for row in gen4_sprites.BG_FILES], [(i, "bg") for i in range(14)])
        self.assertEqual(len(gen4_sprites.BG_TERRAIN), 20)       # FireRed's BATTLE_TERRAIN_GRASS to CHAMPION
        self.assertEqual(set(gen4_sprites.BG_TERRAIN), set(range(14)))
        for row in gen4_sprites.BG_FILES:
            self.assertRegex(row[4], "^[0-9a-f]{64}$")

    def test_every_terrain_has_its_scene_or_none_has(self):
        data = rgb_png(self.scene())
        rows = tuple((i, "bg", "s/%d.png" % i, len(data), hashlib.sha256(data).hexdigest()) for i in range(14))
        urls = []

        def get(url):
            urls.append(url)
            return data

        with patch.object(gen4_sprites.gen4_sources, "FILES", ()), patch.object(gen4_sprites, "BACK_FILES", ()), \
                patch.object(gen4_sprites, "NPC_FILES", ()), patch.object(gen4_sprites, "BG_FILES", rows):
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(None, get=get))
        self.assertEqual([path for path, _ in files], ["graphics/gen4/bg/%03d.bin" % i for i in range(20)])
        self.assertEqual(info["backgrounds"], 20)
        self.assertEqual(files[0][1], files[4][1] if gen4_sprites.BG_TERRAIN[0] == gen4_sprites.BG_TERRAIN[4]
                         else files[0][1])
        self.assertEqual(files[11][1], files[12][1])            # the gym and its leader share a scene
        self.assertEqual(urls[0], "https://raw.githubusercontent.com/%s/%s/s/0.png"
                         % (gen4_sprites.BG_REPO, gen4_sprites.BG_COMMIT))
        # One scene that cannot be had leaves every terrain on FireRed's.
        with patch.object(gen4_sprites.gen4_sources, "FILES", ()), patch.object(gen4_sprites, "BACK_FILES", ()), \
                patch.object(gen4_sprites, "NPC_FILES", ()), patch.object(gen4_sprites, "BG_FILES", rows):
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(
                None, get=lambda url: b"" if url.endswith("/3.png") else data))
        self.assertEqual((files, info["backgrounds"]), ([], 0))


class PeopleTest(unittest.TestCase):
    COLOURS = [(255, 0, 0, 255), (0, 255, 0, 255), (0, 0, 255, 255), (255, 255, 0, 255)]

    def sheet(self):
        """Nine frames; in frame f, the pixel (x, y) takes colour (f + x // 4 + y // 4) % 4 inside a 24x24 body."""
        rows = []
        for y in range(32 * 9):
            frame, fy = divmod(y, 32)
            rows.append([self.COLOURS[(frame + x // 4 + fy // 4) % 4] if 4 <= x < 28 and 4 <= fy < 28 else (0, 0, 0, 0)
                         for x in range(32)])
        return rows

    def test_entry_layout_and_tile_order(self):
        entry = gen4_sprites.convert_npc(rgba_png(self.sheet()))
        self.assertEqual(entry[:8], b"G4N1" + bytes([9, 32, 0, 0]))
        self.assertEqual(len(entry), 40 + 9 * 512)
        palette = struct.unpack("<16H", entry[8:40])
        self.assertEqual(palette[0], 0)
        self.assertEqual(len(set(palette[1:5])), 4)
        # Frame 2, tile (1, 0), its first row (pixels 8 to 15 of the first row: the body starts at 4, in rows from 4):
        # the tile's own bytes are 32 from the frame's start plus a tile's 32.
        sheet = self.sheet()
        for frame, tx, ty, y in ((0, 0, 0, 5), (2, 1, 2, 3), (8, 3, 3, 7)):
            tile = entry[40 + frame * 512 + (ty * 4 + tx) * 32 + y * 4:][:4]
            row = sheet[frame * 32 + ty * 8 + y][tx * 8:tx * 8 + 8]
            for x in range(8):
                index = tile[x // 2] >> (4 * (x & 1)) & 15
                if row[x][3] == 0:
                    self.assertEqual(index, 0)
                else:
                    colour = struct.unpack("<H", entry[8 + index * 2:10 + index * 2])[0]
                    self.assertEqual(((colour & 31) * 255 // 31, (colour >> 5 & 31) * 255 // 31,
                                      (colour >> 10 & 31) * 255 // 31), row[x][:3])

    def test_a_wrong_sheet_is_refused(self):
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_npc(rgba_png([[(0, 0, 0, 255)] * 32] * 100))
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_npc(png([[0] * 32] * 288, [(0, 0, 0)] * 16, depth=8))

    def test_pinned_people(self):
        rows = gen4_sprites.NPC_FILES
        self.assertEqual(len(rows), 74)
        self.assertEqual(len({row[0] for row in rows}), 74)           # each graphics id once
        self.assertTrue(all(row[1] == "npc" and 16 <= row[0] < 256 for row in rows))
        self.assertTrue(all(re.match(r"^assets/npcs/(community|diego|taiga|zaffre)/gen3/frlg/[A-Z_0-9]+\.png$", row[2])
                            for row in rows))
        for row in rows:
            self.assertRegex(row[4], "^[0-9a-f]{64}$")
        # Red, Leaf and the rival are not among them: they keep FireRed's.
        self.assertFalse({0, 1, 2, 3, 4, 5, 6, 7, 8, 72} & {row[0] for row in rows})

    def test_the_groups_choose_the_files_and_the_entries(self):
        self.assertEqual(len(gen4_sprites.all_files(("npc",))), 74)
        self.assertEqual(len(gen4_sprites.all_files(("bg",))), 14)
        self.assertEqual(len(gen4_sprites.all_files(())), 0)
        self.assertEqual(len(gen4_sprites.all_files()), len(gen4_sprites.all_files(("pokemon",))) + 74 + 14
                         + len(gen4_sprites.PLAYER_FILES) - len(gen4_sprites.BACK_FILES))
        data = rgba_png(self.sheet())
        rows = tuple((16 + i, "npc", "n/%d.png" % i, len(data), hashlib.sha256(data).hexdigest()) for i in range(3))
        with patch.object(gen4_sprites, "NPC_FILES", rows):
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(None, get=lambda url: data), groups=("npc",))
            self.assertEqual([path for path, _ in files], ["graphics/gen4/npc/%03d.bin" % (16 + i) for i in range(3)])
            self.assertEqual((info["npcs"], info["species"], info["backgrounds"]), (3, 0, 0))
            # One sheet that cannot be had leaves that person on FireRed's, the others are drawn.
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(
                None, get=lambda url: b"" if url.endswith("/1.png") else data), groups=("npc",))
            self.assertEqual([path for path, _ in files], ["graphics/gen4/npc/016.bin", "graphics/gen4/npc/018.bin"])
            self.assertEqual(info["npcs"], 2)
            # Without the group nothing of them is fetched.
            urls = []
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(
                None, get=lambda url: urls.append(url) or data), groups=())
            self.assertEqual((files, urls, info["npcs"]), ([], [], 0))


class PlayerTest(unittest.TestCase):
    COLOURS = PeopleTest.COLOURS

    def column(self, frames, side=32):
        """`frames` square frames one under the other; in frame f, the pixel (x, y) of the body takes colour
        (f + x // 4 + y // 4) % 4."""
        edge = side // 8
        return [[self.COLOURS[(f + x // 4 + fy // 4) % 4] if edge <= x < side - edge and edge <= fy < side - edge
                 else (0, 0, 0, 0) for x in range(side)]
                for f, fy in (divmod(y, side) for y in range(side * frames))]

    def test_the_player_sheets_keep_their_frames(self):
        entry = gen4_sprites.convert_npc(rgba_png(self.column(20)), 20)
        self.assertEqual(entry[:8], b"G4N1" + bytes([20, 32, 0, 0]))
        self.assertEqual(len(entry), 40 + 20 * 512)
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_npc(rgba_png(self.column(20)))      # a person has nine

    def test_a_gba_back_is_enlarged_half_again(self):
        entry = gen4_sprites.convert_back(rgba_png(self.column(5, 64)))
        self.assertEqual(entry[:8], b"G4K2" + bytes([5, 104, 0, 0]))
        self.assertEqual(len(entry), 40 + 5 * 104 * 104 // 2)
        # The 64x64 frame stands 96 pixels tall, centred: rows 4 to 99 are drawn, the body 96 * 3 / 4 of them.
        frame = entry[40:40 + 104 * 52]
        drawn = [y for y in range(104) if any(frame[y * 52:(y + 1) * 52])]
        self.assertEqual((drawn[0], drawn[-1]), (4 + 12, 4 + 83))
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_back(rgba_png(self.column(4, 64)))

    def test_a_gba_front_fills_the_trainer_picture(self):
        entry = gen4_sprites.convert_front(rgba_png(self.column(1, 64)))
        self.assertEqual(entry[:8], b"G4T1" + bytes(4))
        self.assertEqual(len(entry), 40 + 80 * 40)
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_front(rgba_png(self.column(1, 32)))

    def test_the_intro_picture_takes_firereds_colours(self):
        tiles, colours = gen4_sprites.convert_intro(rgba_png(self.column(1, 64)[:96] + self.column(1, 64)[:32]))
        self.assertEqual((len(tiles), len(colours)), (6144, 64))
        self.assertEqual(set(tiles) - {0}, {64, 65, 66, 67})
        self.assertEqual(tiles[:8], bytes(8))                       # the first row of the frame is empty
        with self.assertRaises(gen4_sprites.Gen4Error):
            gen4_sprites.convert_intro(rgba_png(self.column(1, 64)))

    def test_pinned_looks(self):
        rows = gen4_sprites.PLAYER_FILES
        self.assertRegex(gen4_sprites.PLAYER_COMMIT, "^[0-9a-f]{40}$")
        self.assertEqual(len({gen4_sprites.file_name(row) for row in rows}), len(rows))
        for gender in ("boy", "girl"):
            sheets = {row[2].rsplit("/", 1)[1][:-4] for row in rows if row[0] == gender}
            self.assertEqual(sheets, set(gen4_sprites.PLAYER_SHEETS) | {"back", "front", "intro", "mapIcon", "flyBird"})
        for row in rows:
            self.assertEqual(row[1], "player")
            self.assertRegex(row[2], r"^assets/sprites/(ash_hgss|ash|green_hgss|green)/gen3/frlg/[A-Za-z]+\.png$")
            self.assertRegex(row[4], "^[0-9a-f]{64}$")

    def test_each_sheet_goes_where_firered_draws_it(self):
        sizes = {"walk": 20, "bike": 9, "surf": 12, "fishing": 12, "fieldMove": 9, "vsSeekerBike": 6}
        data = {name: rgba_png(self.column(count)) for name, count in sizes.items()}
        data["back"] = rgba_png(self.column(5, 64))
        data["front"] = rgba_png(self.column(1, 64))
        data["intro"] = rgba_png(self.column(1, 64)[:96] + self.column(1, 64)[:32])
        data["mapIcon"] = rgba_png(self.column(1, 16))
        data["flyBird"] = rgba_png(self.column(2, 64))
        rows = tuple((gender, "player", "x/%s/%s.png" % (gender, name), len(data[name]),
                      hashlib.sha256(data[name]).hexdigest()) for gender in ("boy", "girl") for name in data)

        def get(url):
            return data[url.rsplit("/", 1)[1][:-4]]
        with patch.object(gen4_sprites, "PLAYER_FILES", rows):
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(None, get=get), groups=("player",))
        paths = sorted(path for path, _ in files)
        npc = ["graphics/gen4/npc/%03d.bin" % gfx for gfx in range(14)]
        whole = ["graphics/gen4/player/%s_%s.%s" % (name, gender, kind) for gender in ("boy", "girl")
                 for name, kinds in (("front", ("4bpp", "gbapal")), ("intro", ("8bpp", "gbapal")),
                                     ("icon", ("4bpp", "gbapal"))) for kind in kinds]
        birds = ["graphics/gen4/npc/250.bin", "graphics/gen4/npc/251.bin"]
        self.assertEqual(paths, sorted(npc + whole + birds + ["graphics/gen4/back/000.bin", "graphics/gen4/back/001.bin",
                                                              "graphics/gen4/trainer/135.bin",
                                                              "graphics/gen4/trainer/136.bin"]))
        self.assertEqual(info["player"], 32)
        bird = dict(files)["graphics/gen4/npc/250.bin"]
        self.assertEqual((bird[:6], len(bird)), (b"G4N1" + bytes([2, 64]), 40 + 2 * 2048))
        # FireRed's own sizes: what the port checks before it puts one in the place of Red's or Leaf's.
        sizes = {path: len(entry) for path, entry in files}
        self.assertEqual([sizes["graphics/gen4/player/%s_boy.%s" % pair] for pair in (
            ("front", "4bpp"), ("front", "gbapal"), ("intro", "8bpp"), ("intro", "gbapal"), ("icon", "4bpp"),
            ("icon", "gbapal"))], [2048, 32, 6144, 64, 128, 32])
        frames = {path: entry[4] for path, entry in files if path.startswith("graphics/gen4/npc/")}
        self.assertEqual([frames[path] for path in npc], [20, 9, 12, 9, 12, 9, 6] * 2)
        # With HeartGold's trainer pictures too, the player's front pictures take the place of their Red, once.
        archive = b"narc"
        trainers = (rows + ((0, "trainers", "t", len(archive), hashlib.sha256(archive).hexdigest()),))
        heartgold = [("graphics/gen4/trainer/135.bin", b"hg red"), ("graphics/gen4/trainer/001.bin", b"hg")]
        with patch.object(gen4_sprites, "PLAYER_FILES", trainers),                 patch.object(gen4_sprites, "convert_trainers", lambda data: list(heartgold)):
            files, info = gen4_sprites.pack_files(gen4_sprites.Downloader(
                None, get=lambda url: archive if url.endswith(gen4_sprites.gen4_sources.TRAINERS_PATH) else get(url)),
                groups=("player",))
        named = [path for path, _ in files]
        self.assertEqual(len(named), len(set(named)))
        self.assertIn("graphics/gen4/trainer/001.bin", named)
        self.assertNotEqual(dict(files)["graphics/gen4/trainer/135.bin"], b"hg red")
        self.assertEqual(info["trainers"], 1)
        # With HeartGold's sprites too, the player's backs take the place of the fan-made ones.
        self.assertFalse(any(row[1] == "back" for row in gen4_sprites.all_files(("pokemon", "player"))))
        self.assertTrue(any(row[1] == "back" for row in gen4_sprites.all_files(("pokemon",))))


class SourcesTest(unittest.TestCase):
    def test_pinned_list_covers_firered_species_only(self):
        from firered3ds_builder import gen4_sources
        self.assertRegex(gen4_sources.COMMIT, "^[0-9a-f]{40}$")
        battle = [row for row in gen4_sources.FILES if row[1] in ("male", "female")]
        self.assertEqual(sorted({row[0] for row in gen4_sources.FILES if row[1] == "unown"}), list(range(1, 28)))
        self.assertEqual(sorted({row[0] for row in gen4_sources.FILES if row[1] == "castform"}), [1, 2, 3])
        self.assertEqual([row[1:3] for row in gen4_sources.FILES if row[1] == "spinda"], [("spinda", "spots")])
        self.assertEqual([row[1:3] for row in gen4_sources.FILES if row[1] == "trainers"], [("trainers", "front")])
        species = {row[0] for row in battle}
        self.assertEqual(species, set(range(1, 387)))
        for dex in species:
            bodies = [row[1:3] for row in battle if row[0] == dex]
            self.assertIn(bodies, ([("male", "front"), ("male", "back")],
                                   [("female", "front"), ("female", "back")],
                                   [("male", "front"), ("male", "back"), ("female", "front"), ("female", "back")]))
        icons = sorted(row[0] for row in gen4_sources.FILES if row[1] == "icon")
        self.assertEqual(icons, list(range(0, 414)))
        for row in gen4_sources.FILES:
            self.assertGreater(row[3], 0)
            self.assertRegex(row[4], "^[0-9a-f]{64}$")


if __name__ == "__main__":
    unittest.main()
