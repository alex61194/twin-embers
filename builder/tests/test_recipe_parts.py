"""Partition integrity and the complete own-ROM pipeline (explicit local opt-in)."""
import hashlib, json, os, tempfile, unittest
from pathlib import Path
from firered3ds_builder.build import build, default_recipe
from firered3ds_builder.recipe import load_recipe
from firered3ds_builder.rom import SUPPORTED_SHA1
from firered3ds_builder.pak import PakReader

class Parts(unittest.TestCase):
    def test_corrupt_and_escaping_parts_are_rejected(self):
        source=default_recipe(SUPPORTED_SHA1)
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp); index=json.loads(source.read_text())
            for row in index['recipe_parts']:
                dest=root/row['path']; dest.parent.mkdir(parents=True,exist_ok=True)
                dest.write_bytes((source.parent/row['path']).read_bytes())
            path=root/source.name; path.write_text(json.dumps(index))
            self.assertEqual(len(load_recipe(path)['entries']),6734)
            part=root/index['recipe_parts'][0]['path']; part.write_bytes(part.read_bytes()+b' ')
            with self.assertRaises(ValueError): load_recipe(path)
            index['recipe_parts'][0]['path']='../elsewhere.json'; path.write_text(json.dumps(index))
            with self.assertRaises(ValueError): load_recipe(path)

    @unittest.skipUnless(os.environ.get('FIRERED_TEST_ROM'),'Own ROM required; never used in CI')
    def test_complete_own_rom_pipeline(self):
        rom=Path(os.environ['FIRERED_TEST_ROM'])
        before=hashlib.sha256(rom.read_bytes()).hexdigest()
        recipe=load_recipe(default_recipe(SUPPORTED_SHA1))
        with tempfile.TemporaryDirectory() as tmp:
            pack=Path(tmp)/'own.pak'
            result=build(rom,None,pack)
            self.assertEqual(result['entries'],6734)
            with PakReader(pack,recipe['engine_abi'],bytes.fromhex(SUPPORTED_SHA1)) as reader:
                self.assertEqual(reader.verify(),6734)
        self.assertEqual(hashlib.sha256(rom.read_bytes()).hexdigest(),before)
