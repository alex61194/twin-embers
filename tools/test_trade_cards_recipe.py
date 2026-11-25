"""Recipe 0280: the trade's cards stay on the lower screen while the menu waits for its next input (a link-slowed game thread can take more than
30 frames between two reads), and the rows of a menu that was up before are cleared while the trade is active. Checked by the text of the lines
the recipe adds, and that the recipe starts from what 0276 leaves."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / 'patches/pokefirered'
RECIPE = PATCHES / '0280-trade-cards-stay-up-while-the-link-is-slow.json'


class TradeCardsTest(unittest.TestCase):
    def setUp(self):
        self.doc = json.loads(RECIPE.read_text(encoding='utf-8'))
        self.added = {f['path']: ''.join(l['port'] for l in f['lines'] if 'port' in l) for f in self.doc['files']}

    def test_the_main_menu_keeps_the_cards_for_a_minute(self):
        self.assertIn('sCtrTradeFrame <= 60 * 60', self.added['src/trade.c'])
        self.assertNotIn('sCtrTradeFrame <= 30', self.added['src/trade.c'])

    def test_old_menu_rows_are_cleared_while_the_trade_is_active(self):
        self.assertIn('if (CtrTrade_Active())\n            sCtrMenuLabels = 0;', self.added['src/menu.c'])

    def test_it_starts_from_what_0276_leaves(self):
        last = json.loads((PATCHES / '0276-union-room-chat-for-lower-screen.json').read_text(encoding='utf-8'))
        after = {f['path']: f['after_sha256'] for f in last['files']}
        for f in self.doc['files']:
            self.assertEqual(f['before_sha256'], after[f['path']])


if __name__ == '__main__':
    unittest.main()
