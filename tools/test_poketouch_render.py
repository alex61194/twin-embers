"""Exercise production renderer with synthetic data and compare base viewports."""
import os, subprocess, tempfile, unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASE = '579726e8ea8fee69380f0b83160996f1cd29fcaf'

class Renderer(unittest.TestCase):
    def test_runtime_icons_geometry_and_redraw(self):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            command = [os.environ.get('CC', 'gcc'), '-std=c11', '-O2', '-Wall', '-Wextra', '-Werror',
                       '-ffunction-sections', '-fdata-sections', '-Wl,--gc-sections',
                       '-I'+str(ROOT/'3ds_port/include'), '-I'+str(ROOT/'3ds_port/src'),
                       str(ROOT/'3ds_port/tests/poketouch_render_test.c'),str(ROOT/'3ds_port/src/3ds_pak.c')]
            current = folder/('current.exe' if os.name=='nt' else 'current')
            subprocess.run(command+['-o',str(current)],check=True)
            synthetic_env=dict(os.environ); synthetic_env.pop('FIRERED_TEST_PAK',None)
            subprocess.run([str(current),str(folder/'current.ppm'),str(folder/'current.bin')],check=True,env=synthetic_env)
            if os.environ.get('FIRERED_TEST_PAK'):
                subprocess.run([str(current)],check=True)
            # Baseline header and renderer from the exact authorized commit, when this
            # checkout has it in its history (a fresh source export does not).
            known = subprocess.run(['git','-C',str(ROOT),'cat-file','-t',BASE],
                                   capture_output=True,text=True).stdout.strip()=='commit'
            if not known:
                self.skipTest(f'baseline {BASE[:7]} is not in the history of this checkout')
            for path in ('3ds_port/src/3ds_poketouch.c','3ds_port/include/poketouch_assets.h'):
                raw = subprocess.check_output(['git','-C',str(ROOT),'show',BASE+':'+path])
                (folder/Path(path).name).write_bytes(raw)
            baseline = folder/('baseline.exe' if os.name=='nt' else 'baseline')
            subprocess.run(command+['-I'+str(folder),'-DPT_BASELINE',
                           '-DPT_SOURCE="'+str(folder/'3ds_poketouch.c').replace('\\','/')+'"',
                           '-o',str(baseline)],check=True)
            subprocess.run([str(baseline),str(folder/'base.ppm'),str(folder/'base.bin')],check=True)
            cur=(folder/'current.bin').read_bytes(); base=(folder/'base.bin').read_bytes()
            frame=240*160*2
            self.assertEqual(len(cur),len(base))
            # HOME and GBA viewports are pixel-identical to the base.
            self.assertEqual(cur[:2*frame],base[:2*frame],'HOME and GBA viewport pixels must match the base')
            # OPTIONS and SAVE changed colour (navy to red), not geometry: the same pixels are the backdrop
            # in both, and nothing blue is left.
            for n,name in ((2,'OPTIONS'),(3,'SAVE')):
                a=cur[n*frame:(n+1)*frame]; b=base[n*frame:(n+1)*frame]
                px=lambda data:[int.from_bytes(data[i:i+2],'little') for i in range(0,len(data),2)]
                a,b=px(a),px(b)
                bg=lambda p:max(set(p),key=p.count)
                abg,bbg=bg(a),bg(b)
                self.assertEqual([x==abg for x in a],[x==bbg for x in b],name+' geometry must match the base')
                self.assertNotEqual(a,b,name+' must be recoloured')
                blue=sum(1 for x in a if (x&31)*8>((x>>11)*8)+40)
                self.assertEqual(blue,0,name+' still has %d blue pixels'%blue)
                self.assertGreater((abg>>11)*8,(abg&31)*8,name+' backdrop must be red')
            self.assertNotEqual((folder/'current.ppm').read_bytes(),(folder/'base.ppm').read_bytes())

if __name__=='__main__': unittest.main()
