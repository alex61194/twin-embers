"""Recipe integrity, record copy and pack-failure checks. Synthetic data only; no ROM."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest
import zlib

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / 'builder'))
from firered3ds_builder.build import default_recipe, RECIPES
from firered3ds_builder.pak import PakError, PakReader, engine_abi, normalise, path_id, write_pak
from firered3ds_builder.recipe import execute, load_recipe
from firered3ds_builder.rom import SUPPORTED_SHA1, ROM_SIZE

REASONS = {'jump-table', 'compiler-constants', 'port-adapted', 'port-authored'}


class RecordOp(unittest.TestCase):
    ROM = bytes(range(64))

    def test_copies_the_leading_bytes_of_each_padded_record(self):
        # 3 records of 6 bytes laid out as 8-byte agbcc records
        out = execute([['R', 8, 8, 6, 3]], self.ROM, 18)
        self.assertEqual(out, self.ROM[8:14] + self.ROM[16:22] + self.ROM[24:30])

    def test_bounds(self):
        for op in (['R', 8, 4, 6, 3],        # destination larger than source stride
                   ['R', 8, 8, 0, 3],        # empty record
                   ['R', 8, 8, 6, 0],        # no records
                   ['R', 60, 8, 6, 2],       # runs past the ROM
                   ['R', 8, 8, 6, 3, 1]):    # malformed
            with self.assertRaises(ValueError):
                execute([op], self.ROM, 18)
        with self.assertRaises(ValueError):   # more output than declared
            execute([['R', 8, 8, 6, 3]], self.ROM, 17)

    def test_no_literal_pool(self):
        with self.assertRaises(ValueError):
            execute([['L', 'AAAA']], self.ROM, 4)


class BitfieldOp(unittest.TestCase):
    # agbcc packs `u16 a:14; u16 b:10` contiguously (b at bit 14); the 3DS compiler puts b at bit 16.
    LAYOUT = [[0, 0, 14], [14, 16, 10]]

    def test_moves_the_field_without_touching_its_value(self):
        rom = bytes(8) + ((0x155 << 14) | 0x2a5a).to_bytes(4, 'little') + ((0x3ff << 14) | 0x3fff).to_bytes(4, 'little')
        out = execute([['B', 8, 4, 2, self.LAYOUT]], rom, 8)
        self.assertEqual(int.from_bytes(out[:4], 'little'), (0x155 << 16) | 0x2a5a)
        self.assertEqual(int.from_bytes(out[4:], 'little'), (0x3ff << 16) | 0x3fff)

    def test_unlisted_bits_are_zero(self):
        out = execute([['B', 0, 4, 1, [[0, 0, 8]]]], b'\xff' * 8, 4)
        self.assertEqual(out, b'\xff\x00\x00\x00')

    def test_bounds(self):
        rom = bytes(range(32))
        for op in (['B', 0, 4, 1, []],                           # no ranges
                   ['B', 0, 4, 0, self.LAYOUT],                  # no records
                   ['B', 30, 4, 1, self.LAYOUT],                 # past the ROM
                   ['B', 0, 4, 1, [[0, 0, 0]]],                  # empty range
                   ['B', 0, 4, 1, [[0, 30, 8]]],                 # destination outside the record
                   ['B', 0, 4, 1, [[28, 0, 8]]],                 # source outside the record
                   ['B', 0, 4, 1, [[0, 0, 8], [8, 4, 8]]],       # overlapping destinations
                   ['B', 0, 16, 1, self.LAYOUT],                 # record larger than 8 bytes
                   ['B', 0, 4, 1]):                              # malformed
            with self.assertRaises(ValueError):
                execute([op], rom, 4)
        with self.assertRaises(ValueError):                      # more output than declared
            execute([['B', 0, 4, 2, self.LAYOUT]], rom, 4)


# The fourteen tables that used to stay in the executable as "engine exceptions"
# although they come from the game. Each now reconstructs from the player's ROM.
EXTERNALIZED = [
    ('c/diploma.o', '.rodata.sBgTemplates', 8),
    ('c/dodrio_berry_picking.o', '.rodata.sUnsharedColumns', 25),
    ('c/easy_chat_3.o', '.rodata.sEasyChatBgTemplates', 16),
    ('c/fame_checker.o', '.rodata.sTextColor_Green', 3),
    ('c/field_player_avatar.o', '.rodata.dot.0', 2),
    ('c/intro.o', '.rodata.sFightSceneSpritePalettes', 48),
    ('c/intro.o', '.rodata.sBgTemplates_GameFreakScene', 8),
    ('c/oak_speech.o', '.rodata.sTextColor_White', 4),
    ('c/overworld.o', '.rodata.sOverworldBgTemplates', 16),
    ('c/pokemon.o', '.rodata.gFacilityClassToPicIndex', 150),
    ('c/pokemon_special_anim_scene.o', '.rodata.sBgTemplates', 8),
    ('c/pokemon_storage_system_tasks.o', '.rodata.sBgTemplates', 16),
    ('c/trainer_card.o', '.rodata.sTrainerCardBgTemplates', 16),
    ('c/union_room_chat_display.o', '.rodata.sBgTemplates', 16),
]
# The separate group of port routing, descriptors and state. It stays engine-owned
# and under its own review; it must not be mixed with the game-derived tables.
PORT_GROUP = {
    ('c/battle_controller_player.o', '.rodata.kNext.0'),
    ('c/pokemon_summary_screen.o', '.rodata.ailments.0'),
    ('c/berry_pouch.o', '.data.sCtrPouchTask'),
    ('c/item_menu.o', '.data.sCtrBagTask'),
    ('c/overworld.o', '.data.lastStep.0'),
    ('c/overworld.o', '.data.sKey.2'),
    ('c/party_menu.o', '.data.sBottomTouchSlot'),
    ('c/start_menu.o', '.data.sPokeTouchPick'),
    ('c/tm_case.o', '.data.sCtrTMCaseTask'),
}


class NoGameDerivedExceptions(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.recipe = load_recipe(default_recipe(SUPPORTED_SHA1))
        cls.units = {(k, s): (entry, off, size) for k, s, entry, off, size in cls.recipe['units']}
        cls.engine = {(k, s): (size, reason) for k, s, size, reason in cls.recipe['engine_units']}

    def test_fourteen_tables_are_reconstructed_from_the_rom(self):
        self.assertEqual(sum(size for *_k, size in EXTERNALIZED), 336)
        for key, section, size in EXTERNALIZED:
            with self.subTest(table=f'{key} {section}'):
                self.assertEqual(self.units[(key, section)][2], size)
                self.assertNotIn((key, section), self.engine)

    def test_game_derived_engine_exceptions_total_zero(self):
        derived = {(k, s): v for (k, s), v in self.engine.items()
                   if v[1] in ('port-adapted', 'port-authored') and (k, s) not in PORT_GROUP}
        # Whatever remains is the compiler's own build-time string, never a game table.
        self.assertEqual(set(derived), {('c/main.o', '.rodata.BuildDateTime')})

    def test_port_group_is_unchanged_and_separate(self):
        self.assertEqual({k for k, v in self.engine.items() if k in PORT_GROUP}, PORT_GROUP)
        self.assertEqual(sum(self.engine[k][0] for k in PORT_GROUP), 56)
        self.assertFalse(PORT_GROUP & set(self.units))
        self.assertFalse(PORT_GROUP & {(k, s) for k, s, _ in EXTERNALIZED})

    def test_no_literal_operation_was_added(self):
        for entry in self.recipe['entries']:
            for op in entry['ops']:
                self.assertIn(op[0], 'CFZRB')
        self.assertNotIn('L', {op[0] for e in self.recipe['entries'] for op in e['ops']})

    def test_new_operations_stay_structural(self):
        # The sentinel is a zero fill, the layout change is a bit move: neither can carry game bytes.
        used = {(e['path'], op[0]) for e in self.recipe['entries'] for op in e['ops']}
        self.assertIn(('gamedata/c/intro.bin', 'F'), used)
        self.assertIn(('gamedata/c/diploma.bin', 'B'), used)
        for entry in self.recipe['entries']:
            for op in entry['ops']:
                if op[0] == 'F':
                    self.assertEqual(op[1], 0)


class CommittedRecipe(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.path = default_recipe(SUPPORTED_SHA1)
        cls.recipe = load_recipe(cls.path)

    def test_is_bundled_and_pinned(self):
        self.assertEqual(self.path.parent, RECIPES)
        self.assertEqual(self.recipe['rom_sha1'], SUPPORTED_SHA1)
        self.assertEqual(self.recipe['schema'], 2)

    def test_entries_are_only_structure(self):
        seen, ids = set(), {}
        for entry in self.recipe['entries']:
            path = normalise(entry['path'])
            self.assertNotIn(path, seen)
            seen.add(path)
            self.assertNotIn(path_id(path), ids, 'pack id collision')
            ids[path_id(path)] = path
            self.assertEqual(set(entry), {'path', 'size', 'crc32', 'ops'})
            total = 0
            for op in entry['ops']:
                self.assertIn(op[0], 'CFZRB')
                if op[0] == 'B':
                    self.assertTrue(all(type(v) is int for v in op[1:4]))
                    self.assertTrue(all(type(v) is int for r in op[4] for v in r))
                else:
                    self.assertTrue(all(type(v) is int for v in op[1:]))
                if op[0] == 'C':
                    self.assertLessEqual(op[1] + op[2], ROM_SIZE)
                    total += op[2]
                elif op[0] == 'R':
                    self.assertLessEqual(op[1] + (op[4] - 1) * op[2] + op[3], ROM_SIZE)
                    total += op[3] * op[4]
                elif op[0] == 'B':
                    self.assertLessEqual(op[1] + op[2] * op[3], ROM_SIZE)
                    total += op[2] * op[3]
            if all(op[0] in 'CRB' for op in entry['ops']):
                self.assertEqual(total, entry['size'], entry['path'])

    def test_abi_matches_entries(self):
        abi = engine_abi((e['path'], e['size'], e['crc32']) for e in self.recipe['entries'])
        self.assertEqual(abi, self.recipe['engine_abi'])

    def test_units_tile_their_region(self):
        entries = self.recipe['entries']
        by_entry = {}
        for key, section, entry, offset, size in self.recipe['units']:
            self.assertTrue(entries[entry]['path'].startswith('gamedata/'))
            by_entry.setdefault(entry, []).append((offset, size, key, section))
        self.assertTrue(by_entry)
        for entry, rows in by_entry.items():
            cursor = 0
            for offset, size, key, section in sorted(rows):
                self.assertEqual(offset, cursor, f'{key} {section} leaves a gap or overlaps')
                cursor += size
            self.assertEqual(cursor, entries[entry]['size'], entries[entry]['path'])
        self.assertEqual(len({(k, s) for k, s, *_ in self.recipe['units']}), len(self.recipe['units']))

    def test_every_gamedata_entry_has_units(self):
        used = {entry for _k, _s, entry, _o, _z in self.recipe['units']}
        regions = {i for i, e in enumerate(self.recipe['entries']) if e['path'].startswith('gamedata/')}
        self.assertEqual(used, regions)

    def test_engine_exceptions_are_declared_and_small(self):
        rows = self.recipe['engine_units']
        self.assertTrue(all(reason in REASONS for *_x, reason in rows))
        # Nothing the ROM can provide may be declared engine-owned by accident.
        self.assertLess(sum(size for _k, _s, size, reason in rows if reason in ('port-adapted', 'port-authored')), 2048)
        units = {(k, s) for k, s, *_ in self.recipe['units']}
        self.assertFalse(units & {(k, s) for k, s, *_ in rows})

    def test_no_game_bytes_in_the_recipe(self):
        # A recipe is ROM offsets and CRCs: its size is the structure, never the payload.
        self.assertLess(self.path.stat().st_size, 4 * 1024 * 1024)
        text = self.path.read_text()
        self.assertNotIn('"data"', text)
        self.assertNotIn('base64', text)


class PackFailures(unittest.TestCase):
    FILES = [('gamedata/a.bin', b'alpha' * 20), ('graphics/b.4bpp', bytes(range(200)))]

    def setUp(self):
        self.dir = tempfile.TemporaryDirectory()
        self.path = Path(self.dir.name) / 'twinembers.pak'
        self.abi = engine_abi((p, len(b), zlib.crc32(b)) for p, b in self.FILES)
        write_pak(self.path, self.FILES, self.abi, bytes.fromhex(SUPPORTED_SHA1))

    def tearDown(self):
        self.dir.cleanup()

    def open(self):
        return PakReader(self.path, self.abi, bytes.fromhex(SUPPORTED_SHA1))

    def test_complete_pack_reads_every_entry(self):
        with self.open() as pak:
            self.assertEqual(pak.verify(), 2)
            for path, data in self.FILES:
                self.assertEqual(pak.read(path), data)

    def test_missing_entry_is_reported_by_path(self):
        with self.open() as pak:
            with self.assertRaises(KeyError):
                pak.read('gamedata/not-in-the-pack.bin')

    def test_corrupt_index(self):
        raw = bytearray(self.path.read_bytes())
        raw[64 + 20] ^= 0x40  # inside the first index entry
        self.path.write_bytes(raw)
        with self.assertRaises(PakError):
            self.open()

    def test_wrong_rom_identity(self):
        with self.assertRaises(PakError):
            PakReader(self.path, self.abi, bytes(20))

    def test_wrong_abi(self):
        with self.assertRaises(PakError):
            PakReader(self.path, self.abi ^ 1, bytes.fromhex(SUPPORTED_SHA1))

    def test_truncated(self):
        raw = self.path.read_bytes()
        self.path.write_bytes(raw[:-40])
        with self.assertRaises(PakError):
            self.open()

    def test_missing_pack_file(self):
        with self.assertRaises(OSError):
            PakReader(self.path.with_name('absent.pak'), self.abi, bytes.fromhex(SUPPORTED_SHA1))

    def test_cli_reports_missing_pack_cleanly(self):
        out = subprocess.run([sys.executable, '-m', 'firered3ds_builder', 'verify-pak', str(self.path.with_name('absent.pak')),
                              '--abi', f'{self.abi:x}'], cwd=ROOT / 'builder', capture_output=True, text=True)
        self.assertEqual(out.returncode, 1)
        self.assertIn('builder:', out.stderr)
        self.assertNotIn('Traceback', out.stderr)


if __name__ == '__main__':
    unittest.main()
