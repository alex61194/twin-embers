"""Compile synthetic C/ARM fixtures and exercise the real REX transformation/linker."""
import json, os, shutil, subprocess, tempfile, unittest, zlib
from pathlib import Path
import rex, rex_objects, rex_link
from firered3ds_builder.pak import engine_abi

ROOT=Path(__file__).resolve().parents[1]

class Runtime(unittest.TestCase):
    def test_pack_backed_font_tile_order(self):
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/('font-test.exe' if os.name=='nt' else 'font-test')
            subprocess.run([os.environ.get('CC','gcc'),'-std=c11','-Wall','-Wextra','-Werror',
                '-I'+str(ROOT/'3ds_port/include'),str(ROOT/'3ds_port/tests/twin_font_test.c'),'-o',str(output)],check=True)
            subprocess.run([str(output)],check=True)
    def test_actual_c_loader_with_synthetic_data(self):
        cc=os.environ.get('CC','gcc')
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/('rex-test.exe' if os.name=='nt' else 'rex-test')
            subprocess.run([cc,'-std=c11','-O2','-Wall','-Wextra','-Werror','-I'+str(ROOT/'3ds_port/include'),
                str(ROOT/'3ds_port/tests/rex_test.c'),str(ROOT/'3ds_port/src/3ds_rex_core.c'),
                str(ROOT/'3ds_port/src/3ds_pak.c'),'-o',str(output)],check=True)
            subprocess.run([str(output)],check=True)

class ObjectPipeline(unittest.TestCase):
    @unittest.skipUnless(Path(rex.tool('arm-none-eabi-gcc')).exists() or shutil.which(rex.tool('arm-none-eabi-gcc')),
                         'ARM compiler required; installed explicitly by CI')
    def test_real_nobits_symbols_roots_and_pointer_map(self):
        with tempfile.TemporaryDirectory() as tmp:
            port=Path(tmp); (port/'build/game').mkdir(parents=True)
            source=port/'fixture.c'
            source.write_text('void handler(void) {}\nvoid AgbMain(void) {}\n'
                              'const unsigned payload[4]={11,22,33,44};\n'
                              'void (*const callbacks[2])(void)={handler,AgbMain};\n')
            obj=port/'build/game/sample.o'
            cc=rex.tool('arm-none-eabi-gcc')
            subprocess.run([cc,'-march=armv6k','-ffunction-sections','-fdata-sections','-c',str(source),'-o',str(obj)],check=True)
            original=rex.Obj.load(obj)
            sections=[s for s in original.sections if rex.is_unit_candidate(s)]
            payload=b''.join(original.data(s) for s in sections)
            entry={'path':'gamedata/c/sample.bin','size':len(payload),'crc32':zlib.crc32(payload),'ops':[['C',0,len(payload)]]}
            units=[]; offset=0
            for sec in sections:
                units.append(['c/sample.o',sec.name,0,offset,sec.size]); offset+=sec.size
            recipe={'schema':2,'entries':[entry],'units':units,'engine_units':[],
                    'rom_sha1':'41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc',
                    'engine_abi':engine_abi([(entry['path'],entry['size'],entry['crc32'])])}
            rows=rex_objects.transform(obj,recipe)
            self.assertEqual(len(rows),len(sections))
            changed=rex.Obj.load(obj)
            for row in rows:
                self.assertEqual(changed.section(row['section']).type,rex.SHT_NOBITS)
                self.assertFalse(changed.relocs(changed.section(row['section'])))
            roots=rex_link.roots(port)
            self.assertIn('handler',roots)
            elf=port/'build/firered_game.elf'; mapfile=port/'build/firered_game.map'
            subprocess.run([cc,'-march=armv6k','-nostdlib','-Wl,--gc-sections','-Wl,--entry=AgbMain',
                '-Wl,-Ttext=0x100000','-Wl,-Map,build/firered_game.map','build/game/sample.o',
                *['-Wl,--undefined='+name for name in roots],'-Wl,--undefined=payload',
                '-Wl,--undefined=callbacks','-o','build/firered_game.elf'],cwd=port,check=True)
            regions,live_units,sites,live,reference=rex_link.build(port,recipe,elf,mapfile)
            self.assertEqual(len(sites),2)
            self.assertEqual(len(live_units),len(units))
            blob=rex_link.pack(regions,live_units,sites,live,reference)
            self.assertEqual(blob[:4],b'F3RX')
            self.assertIn(b'gamedata/c/sample.bin\0',blob)

if __name__=='__main__': unittest.main()
