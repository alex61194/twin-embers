"""The words on the PokeTouch's START column buttons (Pokedex, Pokemon, Bag, Save, Option) in English, Spanish and German must fit
beside the 16x16 icon, with the game's own glyph widths (text.c's sFontNormalLatinGlyphWidths, taken here for the letters used), and
the icons must be the size the buttons are laid out for. The room is read from the sources, so changing the layout changes the test."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / '3ds_port/src/3ds_poketouch.c').read_text(encoding='utf-8')
SKIN = (ROOT / '3ds_port/include/ember_skin.h').read_text(encoding='utf-8')
LIT = r'"(?:[^"\\]|\\.)*"'
STR = '((?:' + LIT + r'\s*)+)'
BS = chr(92)
# FireRed's latin_normal glyph widths for every letter of the words below (pret/pokefirered src/text.c).
WIDTH = {'A': 6, 'B': 6, 'G': 6, 'M': 6, 'O': 6, 'P': 6, 'S': 6, 'a': 6, 'c': 6, 'd': 6, 'e': 6, 'g': 6, 'h': 6, 'i': 4, 'j': 6,
         'k': 5, 'l': 5, 'm': 6, 'n': 5, 'o': 6, 'p': 6, 'r': 5, 's': 5, 't': 5, 'u': 6, 'v': 6, 'x': 6, 'é': 6}


def decode(lits):
    lit = ''.join(m[1:-1] for m in re.findall(LIT, lits))
    out = bytearray()
    i = 0
    while i < len(lit):
        if lit[i] == BS and lit[i + 1] == 'x':
            out.append(int(lit[i + 2:i + 4], 16))
            i += 4
        else:
            out += lit[i].encode('utf-8')
            i += 1
    return bytes(out).decode('utf-8')


class ColumnLabels(unittest.TestCase):
    def test_words_fit_beside_the_icon(self):
        block = SRC[SRC.index('const char *label = id == PT_POKEDEX'):SRC.index(': name;')]
        words = re.findall(r'CTR_TR\(' + STR + r',\s*' + STR + r',\s*' + STR + r'\)', block)
        self.assertEqual(len(words), 5)
        col = re.search(r'\[PT_POKEDEX\]\s*=\s*\{(\d+),\s*(\d+),\s*(\d+),\s*(\d+)\}', SRC)
        width = int(col.group(3))
        draw = re.search(r'UiText\(label, r->x \+ (\d+),.*?r->x \+ r->w - (\d+)\);', SRC)
        start, margin = int(draw.group(1)), int(draw.group(2))
        room = width - margin - start
        for names in words:
            for lang, lit in zip(('en', 'es', 'de'), names):
                word = decode(lit)
                w = sum(WIDTH[ch] for ch in word)
                self.assertLessEqual(w, room, f'{lang}: "{word}" is {w} px, the room beside the icon is {room}')

    def test_icon_size_matches_the_layout(self):
        size = int(re.search(r'#define EMBER_ICON (\d+)', SKIN).group(1))
        draw = re.search(r'EmIcon\(ColumnIcon\(id\), r->x \+ (\d+),', SRC)
        start = re.search(r'UiText\(label, r->x \+ (\d+),', SRC)
        gap = int(start.group(1)) - (int(draw.group(1)) + size)
        self.assertGreaterEqual(gap, 2, 'the icon must leave at least 2 px before the word')
        face = int(re.search(r'\[PT_POKEDEX\]\s*=\s*\{\d+,\s*\d+,\s*\d+,\s*(\d+)\}', SRC).group(1)) - int(re.search(r'#define COL_DEPTH (\d+)', SRC).group(1))
        self.assertLessEqual(size + 2, face, 'the icon must stay inside the button face and its border')


if __name__ == '__main__':
    unittest.main()
