"""The Cheats page's game glue (recipe 0278) is checked by its text: the host tests run the options and the shiny search, and this keeps the lines the
recipe adds to the game thin and safe (each cheat asks one accessor and does one small thing). Only the added lines are read (the recipe stores the
unchanged ones as spans)."""
import json
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
RECIPE = ROOT / 'patches/pokefirered/0278-cheats.json'


def added(path):
    doc = json.loads(RECIPE.read_text(encoding='utf-8'))
    row = next(f for f in doc['files'] if f['path'] == path)
    return ''.join(line['port'] for line in row['lines'] if 'port' in line)


class CheatsRecipeTest(unittest.TestCase):
    def test_files_and_guards(self):
        doc = json.loads(RECIPE.read_text(encoding='utf-8'))
        self.assertEqual(sorted(f['path'] for f in doc['files']),
                         ['src/battle_main.c', 'src/battle_script_commands.c', 'src/event_object_movement.c', 'src/pokemon.c', 'src/wild_encounter.c'])
        for f in doc['files']:
            text = added(f['path'])
            self.assertIn('#ifdef PLATFORM_3DS', text, f['path'])          # nothing changes outside the port
            self.assertEqual(text.count('#ifdef') + text.count('#if '), text.count('#endif'), f['path'])
            self.assertNotIn('malloc', text)

    def test_catch_exp_and_pp(self):
        text = added('src/battle_script_commands.c')
        self.assertIn('odds > 254 || CtrOptions_SureCatch()', text)
        self.assertIn('if (CtrOptions_InfinitePp() && GetBattlerSide(gBattlerAttacker) == B_SIDE_PLAYER)', text)   # only the player's moves
        self.assertIn('*exp = CtrOptions_ScaleExp(*exp);', text)
        self.assertIn('gExpShareExp = CtrOptions_ScaleExp(gExpShareExp);', text)

    def test_shiny_only_for_the_players_pokemon_before_the_substructures(self):
        text = added('src/pokemon.c')
        self.assertIn('otIdType == OT_ID_PLAYER_ID && species != SPECIES_UNOWN && CtrOptions_ShinyOdds()', text)
        self.assertIn('CtrOptions_ShinyPersonality(personality, value, Random32())', text)    # Random32 is only drawn when the option is on
        self.assertLess(text.index('CtrOptions_ShinyOdds()'), text.index('Random32()'))
        doc = json.loads(RECIPE.read_text(encoding='utf-8'))
        row = next(f for f in doc['files'] if f['path'] == 'src/pokemon.c')
        self.assertEqual(len([l for l in row['lines'] if 'port' in l and 'SetBoxMonData(boxMon, MON_DATA_PERSONALITY' in l['port']]), 1)

    def test_encounters_walls_and_flee(self):
        self.assertIn('if (CtrOptions_NoEncounters())\n        return FALSE;', added('src/wild_encounter.c'))
        walls = added('src/event_object_movement.c')
        self.assertIn('objectEvent->isPlayer && CtrOptions_WalkThroughWalls() && GetMapBorderIdAt(x, y) != CONNECTION_INVALID', walls)   # not off the map
        self.assertIn('return COLLISION_NONE;', walls)
        flee = added('src/battle_main.c')
        self.assertIn('CtrOptions_AlwaysFlee() && GetBattlerSide(battler) == B_SIDE_PLAYER', flee)
        self.assertIn('effect++;', flee)


if __name__ == '__main__':
    unittest.main()
