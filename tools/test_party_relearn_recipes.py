"""Recipes 0257 (the Party's action menu walked in two columns by the D-pad) and 0258 (every state of the move relearner has a panel for the
lower screen) are checked by their text: the grid rule itself is host-tested (3ds_port/tests/party_grid_test.c), this keeps the game glue thin
and inside the clean-release rules. Only the lines a recipe adds are read (it stores the unchanged ones as spans)."""
import json
import re
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
R0257 = ROOT / 'patches/pokefirered/0257-party-action-menu-dpad-two-columns.json'
R0258 = ROOT / 'patches/pokefirered/0258-relearner-native-states.json'


def added(recipe, path):
    doc = json.loads(recipe.read_text(encoding='utf-8'))
    row = next(f for f in doc['files'] if f['path'] == path)
    return ''.join(line['port'] for line in row['lines'] if 'port' in line)


class PartyGridRecipe(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = added(R0257, 'src/party_menu.c')

    def test_only_party_menu_is_touched(self):
        doc = json.loads(R0257.read_text(encoding='utf-8'))
        self.assertEqual([f['path'] for f in doc['files']], ['src/party_menu.c'])

    def test_everything_is_port_only(self):
        # every added line sits between #ifdef PLATFORM_3DS and its #endif, or is the else-branch glue inside one
        self.assertEqual(self.text.count('#ifdef PLATFORM_3DS'), 2)
        self.assertEqual(self.text.count('#endif'), 2)

    def test_rule_comes_from_the_shared_header(self):
        self.assertIn('#include "3ds_party_grid.h"', self.text)
        self.assertIn('CtrPartyGrid_Move(count, Menu_GetCursorPos(), dir)', self.text)
        # no layout arithmetic of its own
        self.assertNotIn('/ 2', self.text)
        self.assertNotIn('% 2', self.text)
        self.assertNotIn('+ 1) / 2', self.text)

    def test_grid_only_beyond_four_actions_and_a_b_go_first(self):
        self.assertIn('sPartyMenuInternal->numActions > 4 && CtrPartyMenuGridInput(sPartyMenuInternal->numActions)', self.text)
        self.assertIn('JOY_NEW(A_BUTTON | B_BUTTON) ? -1', self.text)
        # the list input of four or fewer is the original, reached through the else
        self.assertRegex(self.text, r'input = MENU_NOTHING_CHOSEN;\s+else\n#endif')

    def test_clean_release_rules(self):
        self.assertNotRegex(self.text, r'\bswitch\b')
        self.assertNotRegex(self.text, r'\bstatic\s+(?!bool8 CtrPartyMenuGridInput)')
        self.assertNotIn('"', self.text.replace('#include "3ds_party_grid.h"', ''))        # no string literal
        self.assertTrue(re.search(r'static bool8 CtrPartyMenuGridInput\(u8 count\)', self.text))


class RelearnerRecipe(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.text = added(R0258, 'src/learn_move.c')

    def test_only_learn_move_is_touched(self):
        doc = json.loads(R0258.read_text(encoding='utf-8'))
        self.assertEqual([f['path'] for f in doc['files']], ['src/learn_move.c'])

    def test_list_is_open_in_every_state_but_ready_only_when_idle(self):
        self.assertIn('return sMoveRelearner != NULL && gMain.callback2 == CB2_MoveRelearner', self.text)
        self.assertNotIn('sMoveRelearner->state <= MENU_STATE_IDLE_BATTLE_MODE', self.text)
        # CtrBottomRelearn_Ready is left as it was (its lines are kept by the recipe): the list's keys only in the idle state

    def test_questions_are_the_states_that_print_and_await_a_yes_no(self):
        body = self.text[self.text.index('static bool8 CtrRelearnAsking'):]
        body = body[:body.index('}')]
        for state in ('PRINT_TEACH_MOVE_PROMPT', 'TEACH_MOVE_CONFIRM', 'PRINT_GIVE_UP_PROMPT', 'GIVE_UP_CONFIRM',
                      'PRINT_TRYING_TO_LEARN_PROMPT', 'WAIT_FOR_TRYING_TO_LEARN', 'CONFIRM_DELETE_OLD_MOVE',
                      'PRINT_STOP_TEACHING', 'WAIT_FOR_STOP_TEACHING', 'CONFIRM_STOP_TEACHING'):
            self.assertIn('MENU_STATE_' + state, body)

    def test_yes_no_takes_input_only_in_the_confirm_states(self):
        body = self.text[self.text.index('int CtrBottomRelearn_Input(void)'):]
        body = body[:body.index('\n}\n')]
        for state in ('TEACH_MOVE_CONFIRM', 'GIVE_UP_CONFIRM', 'CONFIRM_DELETE_OLD_MOVE', 'CONFIRM_STOP_TEACHING'):
            self.assertIn('MENU_STATE_' + state, body)
        self.assertIn('MENU_STATE_WAIT_FOR_A_BUTTON', body)
        self.assertIn('gPaletteFade.active', body)

    def test_two_states_print_their_text_a_frame_late(self):
        body = self.text[self.text.index('void CtrBottomRelearn_Message'):]
        body = body[:body.index('\n}\n')]
        self.assertIn('MENU_STATE_PRINT_TRYING_TO_LEARN_PROMPT', body)
        self.assertIn('gText_MonIsTryingToLearnMove', body)
        self.assertIn('MENU_STATE_PRINT_STOP_TEACHING', body)
        self.assertIn('gText_StopLearningMove', body)
        self.assertIn('CtrTouchText_PartyMessage(src, out, (unsigned)size)', body)

    def test_clean_release_rules(self):
        self.assertNotRegex(self.text.replace('no-tree-switch-conversion', ''), r'switch\s*\(')
        # new functions: no function-local static data, no table, jump tables off where several states are compared
        self.assertNotRegex(self.text, r'\n\s+static\s')
        self.assertNotRegex(self.text, r'\[\]\s*=')
        self.assertEqual(self.text.count('optimize("no-jump-tables", "no-tree-switch-conversion")'), 2)
        for name in re.findall(r'^(?:static )?(?:int|void|bool8) (\w+)\(', self.text, re.M):
            self.assertRegex(name, r'^Ctr', name)


if __name__ == '__main__':
    unittest.main()
