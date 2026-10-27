"""gDisplayedStringBattle holds every trainer's defeat and victory text (0155), checked against the pinned decompilation."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
UPSTREAM = ROOT/'build/upstream'
RECIPE = '0155-battle-string-room-for-brock.json'


def additions(path):
    doc = json.loads((ROOT/'patches/pokefirered'/RECIPE).read_text())
    row = next(row for row in doc['files'] if row['path'] == path)
    return ''.join(op['port'] for op in row['lines'] if 'port' in op)


def port_size():
    return int(re.search(r'#ifdef PLATFORM_3DS\n(?://.*\n)*#define DISPLAYED_STRING_BATTLE_SIZE (\d+)\n',
                         additions('include/battle_message.h')).group(1))


def charmap():
    table = {}
    for line in (UPSTREAM/'charmap.txt').read_text(encoding='utf-8').splitlines():
        line = line.split('@')[0].strip()
        match = re.match(r"^'(.+)'\s*=\s*([0-9A-Fa-f ]+)$", line) or re.match(r'^(\w+)\s*=\s*([0-9A-Fa-f ]+)$', line)
        if match:
            # '\'' is the apostrophe.
            table[match.group(1).replace("\\'", "'")] = len(match.group(2).split())
    return table


def encoded_length(text, table):
    """Bytes of a .string as the assembler writes it: a {CODE arg} is the code's bytes and one per argument."""
    size, i = 0, 0
    while i < len(text):
        if text[i] == '{':
            end = text.index('}', i)
            name, *args = text[i+1:end].split()
            size += table[name] + len(args)
            i = end + 1
        elif text[i] == '\\':
            size += table.get('\\' + text[i+1]) or table[text[i+1]]
            i += 2
        else:
            size += table[text[i]]
            i += 1
    return size


def trainer_texts():
    """Every text a trainerbattle script names, by label: its intro, defeat and victory texts."""
    texts, label = {}, None
    for path in sorted((UPSTREAM/'data').rglob('*.inc')):
        for line in path.read_text(encoding='utf-8').splitlines():
            match = re.match(r'^(\w+)::', line)
            if match:
                label = match.group(1)
                texts.setdefault(label, '')
                continue
            match = re.search(r'\.string "(.*)"', line)
            if match and label:
                texts[label] += match.group(1)
    used = set()
    for path in sorted((UPSTREAM/'data').rglob('scripts.inc')):
        for line in path.read_text(encoding='utf-8').splitlines():
            if 'trainerbattle' in line:
                used.update(word for word in re.findall(r'\w+', line) if word in texts)
    return {name: texts[name].split('$')[0] for name in used}


class BattleStringRoom(unittest.TestCase):
    def test_buffer_declared_once_with_port_size(self):
        self.assertIn('extern u8 gDisplayedStringBattle[DISPLAYED_STRING_BATTLE_SIZE];', additions('include/battle_message.h'))
        self.assertIn('EWRAM_DATA u8 gDisplayedStringBattle[DISPLAYED_STRING_BATTLE_SIZE] = {0};',
                      additions('src/battle_main.c'))
        self.assertGreater(port_size(), 300)

    @unittest.skipUnless((UPSTREAM/'charmap.txt').exists(), 'needs tools/bootstrap.py')
    def test_every_trainer_text_fits(self):
        table, size = charmap(), port_size()
        texts = trainer_texts()
        self.assertIn('PewterCity_Gym_Text_BrockDefeat', texts)
        for name, text in texts.items():
            # BattleStringExpandPlaceholders adds a pause until a press after a defeat or victory text, then EOS.
            with self.subTest(name):
                self.assertLess(encoded_length(text, table) + 3, size)
        self.assertGreater(encoded_length(texts['PewterCity_Gym_Text_BrockDefeat'], table) + 3, 300)


if __name__ == '__main__':
    unittest.main()
