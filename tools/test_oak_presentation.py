"""Run the production Oak drawing functions against synthetic VRAM/GPU calls."""
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    # Select the definition, then balance braces; no implementation is copied.
    import re
    match = re.search(r'static [\w *]+\b' + name + r'\([^;]+?\)\s*\{', source)
    if not match:
        raise AssertionError('missing production function: ' + name)
    start = match.start()
    depth = 1
    end = source.index('{', start) + 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[start:end]


class OakPresentation(unittest.TestCase):
    def test_production_sampling_clipping_and_stage_isolation(self):
        source = (ROOT/'3ds_port/src/3ds_video.c').read_text()
        functions = '\n'.join(function(source, name) for name in
                              ('LayerBrightness', 'OakSurroundColor', 'DrawOakSurround',
                               'DrawStageUiText', 'StageUnderlay'))
        harness = (ROOT/'3ds_port/tests/oak_presentation_test.c').read_text()
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            path = folder/'test.c'
            path.write_text(harness.replace('/* PRODUCTION FUNCTIONS */', functions))
            exe = folder/('test.exe' if os.name == 'nt' else 'test')
            subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-O2',
                            '-Wall', '-Wextra', '-Werror', str(path), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_oak_only_dispatch(self):
        source = (ROOT/'3ds_port/src/3ds_video.c').read_text()
        layers = function(source, 'Layers')
        self.assertIn('else if (sStageUi)\n', layers)
        self.assertIn('if (bg == 1) DrawOakSurround();\n                    DrawStageUiText(bg);', layers)
        self.assertIn('sStageUi = sStage && sStageUiRequested;', source)
        make = (ROOT/'3ds_port/Makefile').read_text()
        self.assertIn('CTR_GBA_STAGE_UI_OBJS := build/game/oak_speech.o\n', make)
        self.assertIn('CTR_GBA_STAGE_OBJS := build/game/credits.o\n', make)


if __name__ == '__main__':
    unittest.main()
