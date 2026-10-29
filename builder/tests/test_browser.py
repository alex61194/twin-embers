"""The web Builder's entry points (browser.py): same pack as the desktop path, no threads, no network.

Synthetic data only. With FIRERED_ROM_REFERENCE set to the supported ROM, the real recipe is
also built both ways and compared byte for byte."""
import json
import os
import sys
import tempfile
import threading
import unittest
import zlib
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from firered3ds_builder import browser, build, gen4_sources, gen4_sprites
from firered3ds_builder.pak import PakReader, engine_abi
from firered3ds_builder.rom import Rom, SUPPORTED_SHA1

from test_gen4_sprites import png, row_for, sprite


def no_threads(*args, **kwargs):
    raise RuntimeError("can't start new thread")  # what Pyodide says


class Gen4DownloadsTest(unittest.TestCase):
    def test_every_listed_file_once_from_the_pinned_commit(self):
        rows = browser.gen4_downloads()
        self.assertEqual(len(rows), len(gen4_sources.FILES))
        self.assertEqual(len({name for _, name in rows}), len(rows))
        prefix = f"https://raw.githubusercontent.com/{gen4_sources.REPO}/{gen4_sources.COMMIT}/"
        for (url, name), row in zip(rows, gen4_sources.FILES):
            self.assertTrue(url.startswith(prefix))
            self.assertTrue(url.endswith(gen4_sprites.source_path(row)))
            self.assertEqual(name, gen4_sprites.file_name(row))
        json.dumps(rows)  # plain lists of strings: they cross into JavaScript as they are


class SyntheticBuildTest(unittest.TestCase):
    """browser.build against the desktop calls, on a made-up ROM and recipe."""

    def setUp(self):
        tmp = tempfile.TemporaryDirectory()
        self.addCleanup(tmp.cleanup)
        self.root = Path(tmp.name)
        self.rom = bytes((i * 7 + 3) & 0xFF for i in range(4096))
        (self.root / "rom.gba").write_bytes(self.rom)
        files = [("data/a.bin", self.rom[16:400]), ("maps/b.bin", self.rom[1000:1003] + bytes(5))]
        entries = [{"path": "data/a.bin", "size": 384, "crc32": zlib.crc32(files[0][1]), "ops": [["C", 16, 384]]},
                   {"path": "maps/b.bin", "size": 8, "crc32": zlib.crc32(files[1][1]),
                    "ops": [["C", 1000, 3], ["F", 0, 5]]}]
        self.abi = engine_abi((p, len(b), zlib.crc32(b)) for p, b in files)
        recipe = self.root / "recipe.json"
        recipe.write_text(json.dumps({"schema": 1, "rom_sha1": SUPPORTED_SHA1, "engine_abi": self.abi,
                                      "entries": entries}))
        rom = Rom(self.rom, SUPPORTED_SHA1, self.root / "rom.gba")
        for target, value in ((build, rom), (browser, rom)):
            patcher = patch.object(target, "load_rom", return_value=value)
            patcher.start()
            self.addCleanup(patcher.stop)
        patcher = patch.object(build, "default_recipe", return_value=recipe)
        patcher.start()
        self.addCleanup(patcher.stop)
        # Two species of sprites in a folder, as the page leaves them.
        rows, self.folder = [], self.root / "gen4"
        self.folder.mkdir()
        for dex in (1, 2):
            for side in ("front", "back"):
                data = png(*sprite(dex * 5 + len(side), dex * 40))
                row = row_for(dex, "male", side, data)
                rows.append(row)
                (self.folder / gen4_sprites.file_name(row)).write_bytes(data)
        patcher = patch.object(gen4_sources, "FILES", tuple(rows))
        patcher.start()
        self.addCleanup(patcher.stop)

    def desktop(self, gen4: bool) -> bytes:
        """What `firered3ds-builder build [--gen4-sprites]` writes, given the same sprite files."""
        extra = []
        if gen4:
            extra, _ = gen4_sprites.pack_files(gen4_sprites.Downloader(None, self.folder, get=None))
        out = self.root / ("desktop-gen4.pak" if gen4 else "desktop.pak")
        build.build(self.root / "rom.gba", None, out, extra)
        return out.read_bytes()

    def test_plain_pack_matches_the_desktop_builder(self):
        out = self.root / "web.pak"
        info = browser.build(str(self.root / "rom.gba"), str(out))
        self.assertEqual(out.read_bytes(), self.desktop(False))
        self.assertEqual((info["entries"], info["abi"], info["gen4"]), (2, self.abi, None))
        self.assertEqual(info["bytes"], out.stat().st_size)

    def test_sprites_from_the_folder_without_threads(self):
        out = self.root / "web.pak"
        with patch.object(threading.Thread, "start", no_threads):
            info = browser.build(str(self.root / "rom.gba"), str(out), str(self.folder))
        self.assertEqual(out.read_bytes(), self.desktop(True))
        self.assertEqual(info["gen4"], {"species": 2, "missing": [], "icons": False, "downloaded": 0})
        with PakReader(out, self.abi, bytes.fromhex(SUPPORTED_SHA1)) as reader:
            self.assertEqual(reader.verify(), 4)

    def test_a_missing_or_changed_file_leaves_its_species_out(self):
        (self.folder / "002_male_back.png").write_bytes(b"\x89PNG not the listed file")
        (self.folder / "001_male_front.png").unlink()
        info = browser.build(str(self.root / "rom.gba"), str(self.root / "web.pak"), str(self.folder))
        self.assertEqual(info["gen4"], {"species": 0, "missing": [1, 2], "icons": False, "downloaded": 0})
        self.assertEqual(info["entries"], 2)

    def test_never_uses_the_network(self):
        with patch.object(gen4_sprites, "http_get", side_effect=AssertionError("network")), \
             patch.object(gen4_sprites.urllib.request, "urlopen", side_effect=AssertionError("network")):
            (self.folder / "001_male_front.png").unlink()
            info = browser.build(str(self.root / "rom.gba"), str(self.root / "web.pak"), str(self.folder))
        self.assertEqual(info["gen4"]["missing"], [1])


class WrongRomTest(unittest.TestCase):
    def test_wrong_rom_is_refused_before_anything_is_written(self):
        with tempfile.TemporaryDirectory() as tmp:
            rom = Path(tmp) / "rom.gba"
            rom.write_bytes(bytes(16 * 1024 * 1024))
            with self.assertRaisesRegex(ValueError, "Unsupported ROM"):
                browser.build(str(rom), str(Path(tmp) / "twinembers.pak"))
            self.assertEqual(sorted(p.name for p in Path(tmp).iterdir()), ["rom.gba"])


@unittest.skipUnless(os.environ.get("FIRERED_ROM_REFERENCE"), "set FIRERED_ROM_REFERENCE to the supported ROM")
class RealRomTest(unittest.TestCase):
    def test_real_recipe_matches_the_desktop_builder(self):
        rom = Path(os.environ["FIRERED_ROM_REFERENCE"])
        with tempfile.TemporaryDirectory() as tmp:
            web, desktop = Path(tmp) / "web.pak", Path(tmp) / "desktop.pak"
            with patch.object(threading.Thread, "start", no_threads):
                info = browser.build(str(rom), str(web))
            build.build(rom, None, desktop)
            self.assertEqual(web.read_bytes(), desktop.read_bytes())
            self.assertEqual(info["rom_sha1"], SUPPORTED_SHA1)


if __name__ == "__main__":
    unittest.main()
