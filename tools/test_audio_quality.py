"""Production sound-mode selection, ring cadence and synthetic PSG spectrum, no emulator."""
import json,os,subprocess,tempfile,unittest
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]

class AudioQuality(unittest.TestCase):
    def test_native_rate_and_harmonic_bandwidth(self):
        recipe=json.loads((ROOT/'patches/pokefirered/0103-audio-native-bandwidth.json').read_text(encoding='utf-8'))
        call=''.join(op['port'] for op in recipe['files'][0]['lines'] if 'port' in op)
        harness=(ROOT/'3ds_port/tests/audio_quality_test.c').read_text(encoding='utf-8')
        with tempfile.TemporaryDirectory() as tmp:
            folder=Path(tmp); source=folder/'test.c'
            source.write_text(harness.replace('/* PRODUCTION MODE CALL */',call),encoding='utf-8')
            for platform in ('native','gba'):
                exe=folder/(platform+('.exe' if os.name=='nt' else ''))
                command=[os.environ.get('CC','gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',
                         '-I'+str(ROOT/'3ds_port/include')]
                if platform=='native': command+=['-DPLATFORM_3DS']
                subprocess.run(command+[str(source),str(ROOT/'3ds_port/src/3ds_cgb_audio.c'),'-lm','-o',str(exe)],check=True)
                subprocess.run([str(exe)],check=True)

if __name__=='__main__': unittest.main()
