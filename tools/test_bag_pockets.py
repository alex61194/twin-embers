"""Issue #16: the Bag's pocket order, checked from the sources. The pure rule (3ds_bag_pockets.h) is run by 3ds_port/tests/bag_pockets_test.c;
this keeps the places that must agree with it: the field Bag's tabs are in the game's pocket order, the battle Bag has no tab table of
its own that could disagree, the shoulder buttons are ignored in the battle's Bag by the game's own input (recipe 0261) and the
D-pad's LEFT / RIGHT are the port's router's there."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ROOT / 'patches/pokefirered/0261-bag-battle-no-shoulder-pockets.json'


def read(rel):
    return (ROOT / rel).read_text(encoding='utf-8')


def added(path):
    doc = json.loads(RECIPE.read_text(encoding='utf-8'))
    row = next(f for f in doc['files'] if f['path'] == path)
    return ''.join(line['port'] for line in row['lines'] if 'port' in line)


class BagPockets(unittest.TestCase):
    def test_field_tabs_are_in_the_games_pocket_order(self):
        src = read('3ds_port/src/3ds_field_bag_impl.h')
        labels = re.search(r'sFBTabLabel\[3\]\s*=\s*\{([^}]*)\}', src).group(1)
        self.assertEqual([x.strip() for x in labels.split(',')], ['UIS_BAG_ITEMS', 'UIS_BAG_KEY_ITEMS', 'UIS_BAG_BALLS'])
        head = read('3ds_port/include/3ds_bag_pockets.h')
        self.assertRegex(head, r'CTR_BAGP_ITEMS = 0, CTR_BAGP_KEY_ITEMS = 1, CTR_BAGP_POKE_BALLS = 2')
        poke = read('3ds_port/include/3ds_poketouch.h')
        self.assertIn('FireRed\'s pocket: 0 Items, 1 Key Items, 2 Poke Balls', poke)

    def test_the_battle_bag_has_no_second_pocket_order(self):
        # A table {Items, Poke Balls, Key Items} once named the tabs of the battle Bag; the game walks Items, Key Items, Poke Balls.
        for rel in ('3ds_port/src/3ds_battle_ui_impl.h', '3ds_port/include/3ds_poketouch.h'):
            self.assertNotIn('sBTabPocket', read(rel))
            self.assertNotIn('CtrBattleUi_TabPocket', read(rel))

    def test_the_game_ignores_the_shoulder_buttons_in_the_battle_bag(self):
        text = added('src/item_menu.c')
        self.assertIn('#include "3ds_bag_pockets.h"', text)
        self.assertIn('CtrBagPockets_Switch(pocketId, gBagMenuState.location == ITEMMENULOCATION_BATTLE, JOY_NEW(DPAD_LEFT) != 0,', text)
        self.assertIn('lrState == MENU_L_PRESSED, lrState == MENU_R_PRESSED);', text)
        self.assertIn('PlaySE(SE_BAG_POCKET);', text)
        self.assertEqual(text.count('#ifdef PLATFORM_3DS'), 1)
        self.assertEqual(text.count('#else'), 1)
        self.assertEqual(text.count('#endif'), 1)

    def test_the_header_is_free_of_tables_and_switches(self):
        head = read('3ds_port/include/3ds_bag_pockets.h')
        body = re.sub(r'/\*.*?\*/', '', head, flags=re.S)
        self.assertNotRegex(body, r'\bswitch\b')
        self.assertNotRegex(body, r'\[\s*\]|\]\s*=\s*\{|\bstatic const\b')

    def test_the_dpad_is_still_the_routers_in_the_battle_bag(self):
        src = read('3ds_port/src/3ds_bottom.c')
        filt = src[src.index('uint16_t CtrBottom_FilterGameKeys('):]
        filt = filt[:filt.index('/* The naming keyboard')]
        self.assertIn('CtrBottomBag_InBattle()', filt)
        self.assertIn('held &= ~(K_LEFT | K_RIGHT);', filt)
        self.assertIn('BattleBagVisiblePocketStep', src)             # the router is the only thing that walks the pockets there
        self.assertIn('sBatBag.pocketTarget = 1;', src)              # BERRIES: the Key Items pocket, where the Berry Pouch is


if __name__ == '__main__':
    unittest.main()
