"""The web Builder site (tools/build_web.py, web/): what goes in, the UniStore, pinned third-party scripts.

No ROM, no network: the site is assembled into a temporary folder and its Builder is imported from
there in a separate interpreter, as Pyodide imports it."""
import copy
import hashlib
import json
import re
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools'))
import build_web  # noqa: E402

RELEASES = 'https://github.com/alex61194/twin-embers/releases/latest/download/'


class SiteTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.TemporaryDirectory()
        cls.site = Path(cls.tmp.name) / 'site'
        cls.paths = build_web.assemble(cls.site, 'v9.9.9', '2026-10-29')

    @classmethod
    def tearDownClass(cls):
        cls.tmp.cleanup()

    def test_only_pages_store_and_builder_sources(self):
        on_disk = sorted(p.relative_to(self.site).as_posix() for p in self.site.rglob('*') if p.is_file())
        self.assertEqual(on_disk, self.paths)
        for path in on_disk:
            self.assertRegex(path, r'^(?:[a-z]+\.(?:html|css|js|unistore)|\.nojekyll|builder/files\.json|'
                                   r'builder/firered3ds_builder/(?:[a-z0-9_]+\.py|recipes/[-a-z0-9_/]+\.json))$')
        self.assertFalse(any(re.search(r'gui|cli|__main__', p) for p in on_disk))

    def test_builder_listing_matches_the_files(self):
        listing = json.loads((self.site / 'builder' / 'files.json').read_text())
        self.assertEqual(listing['version'], 'v9.9.9')
        listed = [row['path'] for row in listing['files']]
        self.assertEqual(sorted('builder/' + p for p in listed),
                         sorted(p for p in self.paths if p.startswith('builder/firered3ds_builder/')))
        for row in listing['files']:
            data = (self.site / 'builder' / row['path']).read_bytes()
            self.assertEqual(hashlib.sha256(data).hexdigest(), row['sha256'])
            self.assertEqual(data, (ROOT / 'builder' / row['path']).read_bytes())

    def test_the_site_builder_imports_on_its_own(self):
        code = ('import sys; sys.path.insert(0, sys.argv[1]); import firered3ds_builder.browser as b; '
                'from firered3ds_builder.build import default_recipe; '
                'from firered3ds_builder.recipe import load_recipe; '
                'from firered3ds_builder.rom import SUPPORTED_SHA1; '
                'r = load_recipe(default_recipe(SUPPORTED_SHA1)); print(len(r["entries"]), len(b.gen4_downloads())); '
                'print(sorted(m for m in sys.modules if m.startswith("firered3ds_builder")))')
        out = subprocess.run([sys.executable, '-I', '-c', code, str(self.site / 'builder')], check=True,
                             capture_output=True, text=True, cwd=self.tmp.name).stdout.splitlines()
        self.assertGreater(int(out[0].split()[0]), 1000)
        self.assertNotIn('gui', out[1])

    def test_store_has_the_release_filled_in(self):
        doc = json.loads((self.site / 'twinembers.unistore').read_text())
        build_web.validate_unistore(doc)
        self.assertEqual(doc['storeInfo']['revision'], 20261029)
        info = doc['storeContent'][0]['info']
        self.assertEqual((info['version'], info['last_updated']), ('v9.9.9', '2026-10-29'))

    def test_a_non_empty_folder_is_not_overwritten(self):
        with self.assertRaises(ValueError):
            build_web.assemble(self.site, 'v1', '2026-10-29')


class UniStoreTest(unittest.TestCase):
    def setUp(self):
        self.doc = json.loads((ROOT / 'web' / 'twinembers.unistore').read_text(encoding='utf-8'))

    def test_the_store_in_the_repository_is_valid(self):
        build_web.validate_unistore(self.doc)
        info = self.doc['storeInfo']
        self.assertEqual((info['version'], info['file']), (4, 'twinembers.unistore'))
        self.assertEqual(info['url'], 'https://alex61194.github.io/twin-embers/twinembers.unistore')

    def test_installs_the_latest_release_where_the_game_looks(self):
        entry = self.doc['storeContent'][0]
        game = entry['twinembers.3dsx']
        self.assertEqual(game[0], {'type': 'mkdir', 'directory': 'sdmc:/3ds/twinembers'})
        self.assertEqual(game[1], {'type': 'downloadFile', 'file': RELEASES + 'twinembers.3dsx',
                                   'output': 'sdmc:/3ds/twinembers/twinembers.3dsx'})
        cia = entry['HOME Menu forwarder (CIA)']
        self.assertEqual([a['type'] for a in cia], ['downloadFile', 'installCia', 'deleteFile'])
        self.assertEqual(cia[0]['file'], RELEASES + 'TwinEmbers-Forwarder.cia')
        self.assertEqual({cia[0]['output'], cia[1]['file'], cia[2]['file']}, {'sdmc:/TwinEmbers-Forwarder.cia'})
        # Universal-Updater's own UI font: keep the text plain ASCII.
        self.assertTrue((ROOT / 'web' / 'twinembers.unistore').read_bytes().isascii())

    def test_validator_refuses_what_universal_updater_refuses(self):
        def broken(change):
            doc = copy.deepcopy(self.doc)
            change(doc)
            return doc

        cases = [
            lambda d: d['storeInfo'].update(version=5),
            lambda d: d['storeInfo'].update(version='4'),
            lambda d: d['storeInfo'].pop('revision'),
            lambda d: d['storeInfo'].update(file='stores/twinembers.unistore'),
            lambda d: d['storeInfo'].update(sheet='twinembers.t3x'),
            lambda d: d.pop('storeContent'),
            lambda d: d['storeContent'][0]['info'].pop('title'),
            lambda d: d['storeContent'][0]['twinembers.3dsx'][1].pop('output'),
            lambda d: d['storeContent'][0]['twinembers.3dsx'][1].update(type='download'),
            lambda d: d['storeContent'][0]['twinembers.3dsx'][1].update(file='http://example.com/x.3dsx'),
            lambda d: d['storeContent'][0]['info'].update(category='game'),
        ]
        for change in cases:
            with self.assertRaises(ValueError):
                build_web.validate_unistore(broken(change))


class PageTest(unittest.TestCase):
    def read(self, name):
        return (ROOT / 'web' / name).read_text(encoding='utf-8')

    def test_third_party_scripts_are_pinned_and_checked(self):
        page = self.read('index.html')
        scripts = re.findall(r'<script src="(https://[^"]+)"\s+integrity="sha384-[A-Za-z0-9+/=]{64}"', page)
        self.assertEqual(scripts, ['https://cdnjs.cloudflare.com/ajax/libs/qrcode-generator/1.4.4/qrcode.min.js'])
        self.assertEqual(re.findall(r'<script src="([^"]+)"', page), scripts + ['app.js'])
        worker = self.read('worker.js')
        self.assertIn("const PYODIDE_VERSION = '0.28.3';", worker)
        self.assertIn('https://cdn.jsdelivr.net/pyodide/v${PYODIDE_VERSION}/full/', worker)
        self.assertRegex(worker, r"const PYODIDE_SHA384 = '[A-Za-z0-9+/]{64}';")

    def test_the_rom_is_never_sent_anywhere(self):
        for name in ('app.js', 'worker.js'):
            text = self.read(name)
            # The only requests: Pyodide, the site's own builder files, the listed sprite URLs.
            self.assertNotRegex(text, r'XMLHttpRequest|sendBeacon|WebSocket|method:\s*[\'"]POST')
            self.assertNotRegex(text, r'localStorage|indexedDB|sessionStorage')
        self.assertNotIn('fetch(', self.read('app.js'))

    def test_page_and_release_agree_on_the_supported_rom(self):
        sys.path.insert(0, str(ROOT / 'builder'))
        from firered3ds_builder.rom import SUPPORTED_SHA1, ROM_SIZE
        app = self.read('app.js')
        self.assertIn(f"const ROM_SHA1 = '{SUPPORTED_SHA1}';", app)
        self.assertIn('const ROM_SIZE = 16 * 1024 * 1024;', app)
        self.assertEqual(ROM_SIZE, 16 * 1024 * 1024)
        self.assertIn(f"const RELEASES = '{RELEASES}';", app)


if __name__ == '__main__':
    unittest.main()
