"""The horizontal fog covers the whole 400 pixel screen at every scroll position."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COLUMNS, WIDTH = 8, 400


def uncovered(scroll):
    """Pixels of 0..WIDTH-1 no fog sprite covers: sprites are 64 wide, x is their centre, the ring wraps at 512."""
    covered = [False] * WIDTH
    for column in range(COLUMNS):
        x = (scroll + 32 + column * 64) & 0x1FF
        for centre in (x, x - 512):
            for p in range(max(0, centre - 32), min(WIDTH, centre + 32)):
                covered[p] = True
    return covered.count(False)


class FogCoverage(unittest.TestCase):
    def test_patch_uses_the_columns_the_screen_needs(self):
        patch = json.loads((ROOT / 'patches/pokefirered/0123-fog-horizontal-native-width.json').read_text())
        text = ''.join(line.get('port', '') for f in patch['files'] for line in f['lines'])
        self.assertIn('NUM_FOG_HORIZONTAL_COLUMNS   %d' % COLUMNS, text)
        self.assertIn('& 0x1FF', text)

    def test_no_gap_at_any_scroll(self):
        self.assertEqual(max(uncovered(s) for s in range(256)), 0)


if __name__ == '__main__':
    unittest.main()
