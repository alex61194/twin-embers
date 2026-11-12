"""Every OPTIONS row label, in all three languages, must fit the bottom-screen label buffer (PT_OPT_LABEL_LEN bytes
including the terminating NUL, so UTF-8 labels have at most PT_OPT_LABEL_LEN - 1 bytes) and be written in sentence case."""
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SRC = (ROOT / '3ds_port/src/3ds_poketouch_options.c').read_text(encoding='utf-8')
HDR = (ROOT / '3ds_port/include/3ds_poketouch.h').read_text(encoding='utf-8')
LIMIT = int(re.search(r'#define PT_OPT_LABEL_LEN (\d+)', HDR).group(1)) - 1
LIT = r'"(?:[^"\\]|\\.)*"'
STR = '((?:' + LIT + r'\s*)+)'
ROW = re.compile(r'^\s*\{CTR_TR\(' + STR + r',\s*' + STR + r',\s*' + STR + r'\)', re.M)
BS = chr(92)


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


class OptionLabels(unittest.TestCase):
    def test_labels_fit(self):
        rows = ROW.findall(SRC)
        self.assertGreaterEqual(len(rows), 28)
        labels = {decode(names[0]) for names in rows}
        for label in ('Wild Pokemon', 'Wild display', 'Aggressive'):      # the Wilds page: three rows in all three languages
            self.assertIn(label, labels)
        self.assertNotIn('Visible wilds', labels)
        for names in rows:
            for lang, lit in zip(('en', 'es', 'de'), names):
                label = decode(lit)
                self.assertLessEqual(len(label.encode('utf-8')), LIMIT, f'{lang}: "{label}" is cut at {LIMIT} bytes')

    def test_sentence_case(self):
        for names in ROW.findall(SRC):
            label = decode(names[0])
            self.assertFalse(label.isupper() and len(label) > 3, label)


if __name__ == '__main__':
    unittest.main()
