"""Synthetic-only tests for the Windows GUI's safe builder workflow."""
import hashlib
import ast
from pathlib import Path
from types import SimpleNamespace
import tempfile
import threading
import unittest
from unittest.mock import patch
import zlib

from firered3ds_builder import gui_service
from firered3ds_builder.pak import engine_abi, write_pak
from firered3ds_builder.rom import SUPPORTED_SHA1


class GuiWorkflowTest(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.addCleanup(self.tmp.cleanup)
        self.root = Path(self.tmp.name)
        self.rom = self.root / "test.gba"
        self.rom.write_bytes(b"SYNTHETIC TEST FIXTURE, NOT A GAME")
        self.target = self.root / "twinembers.pak"
        self.payload = b"safe local fake pack content"
        self.entries = [("test/fixture.bin", self.payload)]
        self.abi = engine_abi((p, len(b), zlib.crc32(b)) for p, b in self.entries)
        self.events = []
        self.build_calls = 0

    def fake_build(self, rom, recipe, output):
        self.assertEqual(Path(rom), self.rom)
        self.assertIsNone(recipe)
        self.build_calls += 1
        write_pak(output, self.entries, self.abi, bytes.fromhex(SUPPORTED_SHA1))
        return {"abi": self.abi, "entries": 1}

    def run_build(self, **kwargs):
        with patch.object(gui_service, "load_rom", return_value=SimpleNamespace(sha1=SUPPORTED_SHA1)), \
             patch.object(gui_service, "build", side_effect=self.fake_build):
            return gui_service.build_private_pack(
                self.rom, self.target,
                on_progress=lambda stage, msg: self.events.append(stage), **kwargs)

    def test_synthetic_pack_created_verified_and_rom_unchanged(self):
        before = self.rom.read_bytes()
        result = self.run_build()
        self.assertEqual(self.build_calls, 1)
        self.assertEqual(result["abi"], self.abi)
        self.assertEqual(result["entries"], 1)
        self.assertEqual(result["sha256"], hashlib.sha256(self.target.read_bytes()).hexdigest())
        self.assertEqual(self.events, ["rom", "build", "verify", "publish", "done"])
        self.assertEqual(self.rom.read_bytes(), before)
        self.assertEqual(list(self.root.glob(".fire3ds-*")), [])

    def test_existing_pack_is_never_overwritten_without_approval(self):
        self.target.write_bytes(b"OLD USER PACK")
        with self.assertRaises(FileExistsError):
            self.run_build()
        self.assertEqual(self.target.read_bytes(), b"OLD USER PACK")
        self.assertEqual(self.build_calls, 0)

    def test_approved_replace_is_atomic_after_verification(self):
        self.target.write_bytes(b"OLD USER PACK")
        result = self.run_build(overwrite=True)
        self.assertEqual(result["entries"], 1)
        self.assertNotEqual(self.target.read_bytes(), b"OLD USER PACK")

    def test_failure_preserves_existing_pack_even_after_consent(self):
        self.target.write_bytes(b"OLD USER PACK")
        with patch.object(gui_service, "load_rom", return_value=SimpleNamespace(sha1=SUPPORTED_SHA1)), \
             patch.object(gui_service, "build", side_effect=ValueError("bad recipe")):
            with self.assertRaisesRegex(ValueError, "bad recipe"):
                gui_service.build_private_pack(self.rom, self.target, overwrite=True)
        self.assertEqual(self.target.read_bytes(), b"OLD USER PACK")
        self.assertEqual(list(self.root.glob(".fire3ds-*")), [])

    def test_cancel_before_start_never_writes(self):
        flag = threading.Event()
        flag.set()
        with self.assertRaises(gui_service.BuildCancelled):
            self.run_build(cancel=flag)
        self.assertFalse(self.target.exists())

    def test_cancel_after_reconstruction_never_publishes(self):
        flag = threading.Event()
        def notify(stage, _):
            self.events.append(stage)
            if stage == "build":
                flag.set()
        with patch.object(gui_service, "load_rom", return_value=SimpleNamespace(sha1=SUPPORTED_SHA1)), \
             patch.object(gui_service, "build", side_effect=self.fake_build):
            with self.assertRaises(gui_service.BuildCancelled):
                gui_service.build_private_pack(self.rom, self.target, cancel=flag, on_progress=notify)
        self.assertFalse(self.target.exists())
        self.assertEqual(list(self.root.glob(".fire3ds-*")), [])

    def test_copy_failure_removes_incomplete_new_output(self):
        with patch.object(gui_service, "shutil") as mocked:
            real_shutil = __import__("shutil")
            mocked.disk_usage.return_value = SimpleNamespace(free=100 * 1024 * 1024)
            mocked.copyfileobj.side_effect = OSError("removed SD card")
            with self.assertRaisesRegex(OSError, "removed SD card"):
                self.run_build()
        self.assertFalse(self.target.exists())

    def test_gui_source_has_valid_syntax_without_display(self):
        # CI does not need a Tk display; this still catches syntax errors.
        path = Path(__file__).resolve().parents[1] / "firered3ds_builder" / "gui.py"
        ast.parse(path.read_text(encoding="utf-8"))

    def test_destination_created_race_is_not_overwritten(self):
        def concurrent_writer(rom, recipe, output):
            result = self.fake_build(rom, recipe, output)
            self.target.write_bytes(b"ANOTHER USER FILE")
            return result
        with patch.object(gui_service, "load_rom", return_value=SimpleNamespace(sha1=SUPPORTED_SHA1)), \
             patch.object(gui_service, "build", side_effect=concurrent_writer):
            with self.assertRaises(FileExistsError):
                gui_service.build_private_pack(self.rom, self.target)
        self.assertEqual(self.target.read_bytes(), b"ANOTHER USER FILE")

    def test_corrupt_replacement_rolls_back_original_pack(self):
        """The previous pack survives an error after successful os.replace."""
        self.target.write_bytes(b"PREVIOUS USER DATA PACK")
        original = self.target.read_bytes()
        real_replace = gui_service.os.replace
        writes = []

        def broken_readback(source, target):
            real_replace(source, target)
            # The builder itself uses os.replace while staging the pack.
            # Corrupt only the final installation, not its earlier writes.
            if Path(target) == self.target:
                writes.append(str(source))
                if len(writes) == 1:
                    Path(target).write_bytes(b"CORRUPT AFTER INSTALL")
        with patch.object(gui_service.os, "replace", side_effect=broken_readback):
            with self.assertRaisesRegex(OSError, "did not match"):
                self.run_build(overwrite=True)
        self.assertEqual(self.target.read_bytes(), original)
        self.assertEqual(len(writes), 2, "The second replace restores the backup")
        self.assertEqual(list(self.root.glob(".fire3ds-*")), [])

    def test_successful_overwrite_cleans_up_backup(self):
        self.target.write_bytes(b"ORIGINAL USER DATA PACK")
        self.run_build(overwrite=True)
        self.assertFalse(list(self.root.glob(".fire3ds-previous-*")))

    def test_wrong_rom_is_rejected_without_build(self):
        with self.assertRaises(ValueError):
            gui_service.build_private_pack(self.rom, self.target)
        self.assertFalse(self.target.exists())

    def test_symlink_target_is_rejected(self):
        underlying = self.root / "private.pak"
        underlying.write_bytes(b"must survive")
        try:
            self.target.symlink_to(underlying)
        except (OSError, NotImplementedError):
            self.skipTest("symlinks unavailable")
        with self.assertRaises(ValueError):
            self.run_build(overwrite=True)
        self.assertEqual(underlying.read_bytes(), b"must survive")

    def test_refuse_same_input_output(self):
        with patch.object(gui_service, "load_rom", return_value=SimpleNamespace(sha1=SUPPORTED_SHA1)):
            with self.assertRaises(ValueError):
                gui_service.build_private_pack(self.rom, self.rom)

    def test_parent_created_only_when_user_requested(self):
        self.target = self.root / "3ds" / "twinembers" / "twinembers.pak"
        with self.assertRaisesRegex(ValueError, "does not exist"):
            self.run_build()
        self.assertFalse(self.target.parent.exists())
        self.run_build(create_directories=True)
        self.assertTrue(self.target.exists())


class DescribeErrorTest(unittest.TestCase):
    def test_common_file_errors_are_plain_language(self):
        import errno
        full = gui_service.describe_error(OSError(errno.ENOSPC, "No space left on device"))
        self.assertIn("drive is full", full)
        self.assertIn("32 MiB", full)
        locked = gui_service.describe_error(PermissionError(errno.EACCES, "Access is denied"))
        self.assertIn("lock switch", locked)
        missing = gui_service.describe_error(FileNotFoundError(errno.ENOENT, "missing", "E:/rom.gba"))
        self.assertEqual(missing, "File or folder not found: E:/rom.gba")
        self.assertEqual(gui_service.describe_error(ValueError("Unsupported ROM.")), "Unsupported ROM.")


if __name__ == "__main__":
    unittest.main()
