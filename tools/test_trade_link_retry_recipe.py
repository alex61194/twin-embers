"""Recipe 0299: over the internet a link block takes round trips, not frames, so the trade's messages no longer get lost when the link is
still busy (they wait in order until SendBlock takes them) and a link standby asked for during a block send starts once the block is out
(the leader no longer leaves the trade-start standby alone). Checked by the text of the lines the recipe adds, and that it starts from
what 0298 and the earlier recipes leave."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / 'patches/pokefirered'
RECIPE = PATCHES / '0299-trade-messages-retry-and-standby-waits-for-block.json'


def last_after(path, before):
    """after_sha256 of the last recipe before `before` that touches `path`."""
    found = None
    for recipe in sorted(PATCHES.glob('*.json')):
        if recipe.name >= before:
            break
        for f in json.loads(recipe.read_text(encoding='utf-8'))['files']:
            if f['path'] == path:
                found = f['after_sha256']
    return found


class TradeLinkRetryTest(unittest.TestCase):
    def setUp(self):
        self.doc = json.loads(RECIPE.read_text(encoding='utf-8'))
        self.added = {f['path']: ''.join(l['port'] for l in f['lines'] if 'port' in l) for f in self.doc['files']}

    def test_it_starts_from_what_the_earlier_recipes_leave(self):
        for f in self.doc['files']:
            self.assertEqual(f['before_sha256'], last_after(f['path'], RECIPE.name), f['path'])

    def test_a_standby_asked_for_during_a_block_send_waits_for_it(self):
        added = self.added['src/link_rfu_2.c']
        self.assertIn('gRfu.callback == HandleBlockSend || gRfu.callback == SendNextBlock || gRfu.callback == SendLastBlock', added)
        self.assertIn('if (sCtrStandbyPending)\n        return FALSE;', added)
        self.assertIn('if (sCtrStandbyPending && gRfu.callback == NULL)', added)
        self.assertEqual(added.count('    sCtrStandbyPending = FALSE;\n'), 3)   # reset, shutdown, and when the standby starts

    def test_trade_menu_messages_wait_until_the_link_takes_them(self):
        added = self.added['src/trade.c']
        self.assertIn('if (IsLinkTaskFinished() && SendBlock(BitmaskAllOtherLinkPlayers(), sCtrTradeSend[sCtrTradeSendHead], 20))', added)
        self.assertGreaterEqual(added.count('CtrTrade_SendLinkData(sTradeMenu->linkData);'), 4)
        self.assertIn('CtrTrade_FlushLinkData();', added)
        self.assertIn('CTR_TRADE_SENT_ALL()', added)                           # the standbys wait for the queue

    def test_the_animation_messages_are_tried_until_sent(self):
        added = self.added['src/trade_scene.c']
        self.assertIn('sizeof(sTradeAnim->linkData)))\n            sTradeAnim->scheduleLinkTransfer = 0;\n        break;', added)
        self.assertIn('HandleLinkDataSend();', added)


if __name__ == '__main__':
    unittest.main()
