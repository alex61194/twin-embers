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

    def test_the_trade_menu_pointer_is_cleared_when_freed_and_asks_are_only_for_the_main_menu(self):
        doc = json.loads((PATCHES / '0281-trade-menu-pointer-cleared-when-freed.json').read_text(encoding='utf-8'))
        self.assertEqual([f['path'] for f in doc['files']], ['src/trade.c'])
        added = ''.join(l['port'] for l in doc['files'][0]['lines'] if 'port' in l)
        self.assertEqual(added.count('FREE_AND_SET_NULL(sTradeMenu);'), 4)          # the four places FireRed frees the menu
        self.assertIn('sTradeMenu->callbackId != CB_MAIN_MENU', added)
        self.assertEqual(doc['files'][0]['before_sha256'], self.doc['files'][0]['after_sha256'] if self.doc['files'][0]['path'] == 'src/trade.c' else None)

    def test_the_age_of_a_frame_is_read_signed(self):
        doc = json.loads((PATCHES / '0282-link-menu-frame-age-read-signed.json').read_text(encoding='utf-8'))
        added = {f['path']: ''.join(l['port'] for l in f['lines'] if 'port' in l) for f in doc['files']}
        self.assertEqual(added['src/trade.c'].count('(s32)(gMain.vblankCounter2 - sCtrTradeFrame) <= 60 * 60'), 2)
        self.assertIn('(s32)(gMain.vblankCounter2 - sCtrLeaderFrame) <= 30', added['src/union_room.c'])

    def test_it_starts_from_what_0276_leaves(self):
        last = json.loads((PATCHES / '0276-union-room-chat-for-lower-screen.json').read_text(encoding='utf-8'))
        after = {f['path']: f['after_sha256'] for f in last['files']}
        for f in self.doc['files']:
            self.assertEqual(f['before_sha256'], after[f['path']])


if __name__ == '__main__':
    unittest.main()
