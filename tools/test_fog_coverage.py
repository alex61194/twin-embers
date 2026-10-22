"""The horizontal fog covers the whole 400x240 screen at every scroll position, and no two sprites overlap (a
semi-transparent sprite blends with whatever is under it, the fog included)."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
COLUMNS, ROWS, BOX, WIDTH, HEIGHT = 8, 5, 64, 400, 240


def boxes(ox, oy):
    """The sprites' screen boxes (left, top) for a scroll of (ox, oy) modulo 64."""
    return [(ox + c * BOX - BOX, oy + r * BOX - BOX) for r in range(ROWS) for c in range(COLUMNS)]


class FogCoverage(unittest.TestCase):
    def test_patch_uses_the_sprites_the_screen_needs(self):
        patch = json.loads((ROOT / 'patches/pokefirered/0123-fog-horizontal-native-width.json').read_text())
        text = ''.join(line.get('port', '') for f in patch['files'] for line in f['lines'])
        self.assertIn('NUM_FOG_HORIZONTAL_COLUMNS   %d' % COLUMNS, text)
        self.assertIn('NUM_FOG_HORIZONTAL_ROWS      %d' % ROWS, text)
        self.assertIn('CtrWeather_FogSprite', text)
        self.assertIn('& 63', text)

    def test_every_pixel_is_covered_exactly_once(self):
        for ox in range(0, 64, 5):
            for oy in range(0, 64, 7):
                count = [[0] * WIDTH for _ in range(HEIGHT)]
                for left, top in boxes(ox, oy):
                    for y in range(max(0, top), min(HEIGHT, top + BOX)):
                        row = count[y]
                        for x in range(max(0, left), min(WIDTH, left + BOX)):
                            row[x] += 1
                self.assertTrue(all(v == 1 for row in count for v in row), (ox, oy))


if __name__ == '__main__':
    unittest.main()
