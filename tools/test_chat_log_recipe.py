"""Recipe 0320: the Union Room chat keeps a log of its last events for the lower screen, and the upper screen keeps the room for the whole chat.
Checked by the text of the lines the recipe adds, and that it starts from what the earlier recipes leave."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
PATCHES = ROOT / 'patches/pokefirered'
RECIPE = PATCHES / '0320-union-room-chat-log-and-hold.json'


class ChatLogTest(unittest.TestCase):
    def setUp(self):
        self.doc = json.loads(RECIPE.read_text(encoding='utf-8'))
        self.added = {f['path']: ''.join(l['port'] for l in f['lines'] if 'port' in l) for f in self.doc['files']}

    def test_it_starts_from_what_the_earlier_recipes_leave(self):
        last = {}
        for path in sorted(PATCHES.glob('*.json')):
            if path == RECIPE:
                break
            for f in json.loads(path.read_text(encoding='utf-8'))['files']:
                last[f['path']] = f['after_sha256']
        for f in self.doc['files']:
            self.assertEqual(f['before_sha256'], last[f['path']], f['path'])

    def test_the_log_api(self):
        added = self.added['src/union_room_chat.c']
        self.assertIn('int CtrChat_LogCount(void)', added)
        self.assertIn('int CtrChat_LogEntry(int i, u8 *kind, u8 *name, u8 *text)', added)
        self.assertIn('#define CTR_CHAT_LOG_MAX 6', added)

    def test_the_log_is_filled_by_received_messages_and_cleared_on_enter(self):
        added = self.added['src/union_room_chat.c']
        self.assertIn('CtrChat_LogAdd((u8 *)gBlockRecvBuffer[tI]);', added)
        self.assertIn('    CtrChat_LogClear();\n', added)

    def test_the_chat_holds_the_upper_screen(self):
        added = self.added['src/union_room.c']
        self.assertIn('CtrTrade_HoldStart();', added)
        self.assertIn('EnterUnionRoomChat();', ''.join(l.get('port', '') for f in self.doc['files'] for l in f['lines']) + 'EnterUnionRoomChat();')


if __name__ == '__main__':
    unittest.main()
