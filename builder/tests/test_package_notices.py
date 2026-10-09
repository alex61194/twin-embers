"""The Windows notice collector assigns every bundled file to a reviewed component."""
import importlib.util
import unittest
from pathlib import Path

SPEC = importlib.util.spec_from_file_location(
    'package_notices', Path(__file__).resolve().parents[1] / 'package_notices.py')
notices = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(notices)


class Classify(unittest.TestCase):
    def test_known_bundle_files(self):
        expected = {
            'launch_gui': 'twin-embers',
            'firered3ds_builder\\recipes\\firered-41cb23d8.json': 'twin-embers',
            'pyiboot01_bootstrap': 'pyinstaller',
            'pyimod02_importers': 'pyinstaller',
            'pyi_rth__tkinter': 'pyinstaller',
            'python311.dll': 'python',
            'PYZ.pyz': 'python',
            'base_library.zip': 'python',
            '_bz2.pyd': 'python',
            '_hashlib.pyd': 'python',
            '_tkinter.pyd': 'python',
            'libcrypto-3.dll': 'openssl',
            'tcl86t.dll': 'tcltk',
            '_tk_data\\ttk\\winTheme.tcl': 'tcltk',
            'tcl8\\8.6\\http-2.9.5.tm': 'tcltk',
            'VCRUNTIME140.dll': 'microsoft',
            'api-ms-win-crt-heap-l1-1-0.dll': 'microsoft',
            'ucrtbase.dll': 'microsoft',
        }
        for name, component in expected.items():
            self.assertEqual(notices.classify(name), component, name)

    def test_unreviewed_files_fail_closed(self):
        # Excluded from the build on purpose: their license texts are not in the
        # Python license file. Anything new must be reviewed before it can ship.
        for name in ('_decimal.pyd', '_lzma.pyd', 'sqlite3.dll', 'libffi-8.dll', 'game.gba', 'data.pak'):
            with self.assertRaises(ValueError, msg=name):
                notices.classify(name)

    def test_every_component_has_a_description(self):
        self.assertEqual({c for _, c in notices.RULES}, set(notices.COMPONENTS))


if __name__ == '__main__':
    unittest.main()
