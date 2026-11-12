# Overworld wild Pokemon: research, plan and the final design

Status: research only, no game code written. Branch `agent/q5-research`.

## 0. Key finding: the reference is not a Gen 3 hack

`poooooby/wilds-of-kanto-gen-3` is a **Lua mod for Gen1Recomp** (a static
recompilation of Red/Blue/Yellow), forked from "Wilds of Kanto" by
YoDrehDenSwagAuf. It is not a pret/pokefirered patch and contains no GBA C or
assembly. Its tree is `main.lua`, `options.lua`, `lib/*.lua`, `assets/`, docs.
"gen-3" in the name refers to the mod generation, not to the GBA game. So
there is **no code to port**; it is only a design reference. Findings below
come from the README and `docs/ARCHITECTURE.md` (the Lua sources were not read
in detail, so some answers are "not documented").

### How the reference works (as documented)
- Spawn: encounter tables come from a provider layer (`encounters.lua`,
  `encounter_pick.lua`, weighted pick). `spawn_regions.lua` groups connected
  tiles into regions with a density target; `surface.lua` classifies tiles as
  GRASS / CAVE / WATER; `cell_occupancy.lua` reserves cells atomically so two
  mons never share a tile. Cave mons only on walkable tiles. No Pokedex gate.
- Despawn: owned by `spawn_logic.lua`; the rule is not documented.
- Movement: one tile per step (`movement.lua`), behaviour state machines
  (`behavior.lua`): idle/roam, town mons wander and never block, Safari mons
  flee, mons sink into tall grass.
- Battle trigger: walking into a visible mon starts the battle once
  (`startWildBattle(species, level)`). Optional "Classic Enc" keeps step
  encounters as well.
- Shiny: configurable rate (default 1/4096); shown on the overworld sprite
  before the battle, kept on catch. Gender/level handling, repel and saving are
  **not documented**.
- Licence: MIT (copyright YoDrehDenSwagAuf, plus poooooby for the fork); MIT
  covers only its own code and assets. Third-party art (GSC sprite packs,
  PMDCollab CC BY-NC 4.0, etc.) keeps its own licences. Requirement: keep the
  copyright and permission notice if substantial portions are copied.

### Licensing conclusion
Nothing can or needs to be copied: the code is Lua for another engine, and
the sprite packs include non-commercial or unclear terms. We reimplement the
idea from scratch in C on the FireRed decomp. Recommended courtesy: credit
"inspired by Wilds of Kanto" in the README. Do not ship any of its sprites.

## 1. What FireRed already gives us (pure decomp C, @037335f)
Checked in the patched chain tree:
- `OBJECT_EVENTS_COUNT 16` (hard cap, includes the player and all NPCs of the
  map), `OBJECT_EVENT_TEMPLATES_COUNT 64`, `MAX_SPRITES 64`.
- `SpawnSpecialObjectEvent(template)`, `TrySpawnObjectEvent`,
  `RemoveObjectEventByLocalIdAndMap`, `RemoveObjectEventsOutsideView`
  (event_object_movement.c): runtime spawn and removal exist.
- Table-driven movement types (wander, look around, walk in place) can be
  reused for roaming.
- wild_encounter.c: `StandardWildEncounter`, `DoWildEncounterRateTest`,
  per-map tables, `IsWildLevelAllowedByRepel` (party lead level vs wild
  level), roamer code. A battle can be started from C or via
  `setwildbattle` / `dowildbattle`.
- FireRed has no overworld sprites for most species; the real cost is
  graphics (see 3). Shininess is PID/TID based; an overworld preview needs a
  palette decision at spawn.

## 2. What it would take as Twin Embers recipes
Each is a `patches/pokefirered/NNNN-*.json` (before/after spans, numbers from
0245 up). Clean-release rule: game C files may add only symbols matching
`^s(?:Text_)?Ctr[A-Z]\w*$`; function-static locals and anonymous read-only
tables count as FireRed data. Port code (3ds_*.c, *_impl.h) may hold tables.

Needed hooks in game files (all small):
1. event_object_movement.c: a hook that creates an object event from a runtime
   template (graphicsId = wild sprite), marked so it is never saved.
2. field_control_avatar.c: when the player steps into a marked object, call a
   port function that builds the wild mon and starts the battle.
3. wild_encounter.c: expose the per-map table lookup to a port function so
   spawn picks use the same tables and levels; apply the same repel check.
4. Save: do not persist; despawn all on map load or save. Never touch SaveBlock.
5. Graphics: new ids need entries in `gObjectEventGraphicsInfoPointers` and
   palettes. Game-file table additions break the clean-release rule, so keep
   a parallel table in port code and hook `GetObjectEventGraphicsInfo`.

Port code (new `3ds_wilds_impl.h`, no game edits): tile/region selection,
density, despawn by distance/time, weighted pick, shiny/gender/level rolls,
behaviour state machine, sprite loading.

## 3. 3DS-specific concerns
- **16 object-event cap**: large towns already use about 12-14 NPCs plus the
  player. Budget 3-4 wild mons, fewer or none in dense maps. Per-map cap
  `min(4, free slots - 2)`; never evict map NPCs.
- **Sprites/OAM**: `MAX_SPRITES 64` in the game; each wild mon costs one
  sprite, 1-2 OAM entries, an OBJ VRAM tile block and an OBJ palette (16
  slots, several taken by player/NPCs/weather). Needs a small palette pool
  with LRU eviction. Our port renderer (3ds_game_bridge.c iterates
  MAX_SPRITES) already handles 64, so no port limit change is needed.
- **Assets**: the biggest cost. 386 species x directions x frames do not exist
  in the base game and community packs carry licences we cannot verify.
  Option: generate a 32x32 sprite per species from the game's own battle front
  art with an asset tool using the owner's ROM at build time (nothing
  shipped), static frame plus bob animation.
- **Performance**: each extra object event adds movement work per frame; 3-4
  is negligible. Scan regions once on map load. Old 3DS is tight (52-59 fps):
  default cap 2 there.
- **Dual screen**: the top is the game; the bottom could show the facing mon's
  name/level with LlWindow/LlRibbon (3ds_ember2_impl.h). Optional, needs a
  mockup first.
- Triggers must not fire during scripts, cutscenes, or trainer sight.

## 4. Options
| Option | Scope | Effort | Risk |
|---|---|---|---|
| Minimal | Only mons that already have overworld graphics (Snorlax-type), placed with existing scripts; polish only. | 1-2 days | low |
| Medium | Runtime spawn of 2-4 mons per outdoor map from the real encounter table, generated sprite, walk-into = battle, repel obeyed, step encounters kept, no save data. | 2-3 weeks | medium |
| Full | Medium plus water/cave/grass-sink behaviour, flee AI, shiny preview, directional walk sprites, bottom-screen info, options menu. | 2-3 months | high |

## 5. Risks
- 16-object cap and OBJ palette pressure are hard limits.
- Generated sprites may look poor; needs review on both console models.
- Clean-release audit: every game-file edit must be a `Ctr` symbol; tables in
  port code.
- Save compatibility: no save changes; despawn on save/load.
- Fairness: keep Repel and abilities consistent, avoid double encounters.
- Use a separate PRNG for spawns so existing RNG seeds do not shift.

## 6. Recommendation
Go **Medium in two steps**, behind an option that defaults to Off:
1. Prototype (3-4 days): port-side module spawning 1-2 mons on one route,
   placeholder generated sprite, walk-into battle, repel check, no
   persistence. Judge sprite quality on a real console.
2. If acceptable: table-driven spawn for all outdoor routes, density cap by
   free slots, Old 3DS cap of 2, README credit.
Defer Full features until the prototype proves the slot/palette budget.

## 7. Owner decisions
- Is a sprite generated from battle art acceptable visually?
- Option default: Off, or On for New 3DS only?
- Keep step encounters alongside visible mons (recommended: yes)?

## 8. Final design: one system (recipes 0249 and 0250)

The earlier step 1 prototype (Route 1 only) and the step 2 version (every map, own recipe 0250) were folded together with the
ideas of PR #83 by mcl5500/Sh3ld0r (MIT; recipe 0237, port side `CtrWild_*`) into one system: one module, one picker, one launcher.
The PR was not merged or imported; its ideas were re-implemented (credit in `CREDITS.md`). Recipe 0249 is the overworld side
(`event_object_movement.c`, its header and `field_control_avatar.c`); recipe 0250 is the encounter side (`wild_encounter.c`). Both
are thin glue over a pure port module, `3ds_port/src/3ds_wilds_core.c` (`include/3ds_wilds_core.h`), which has no game dependency and
is run by the host tests.

### Settings and options
- OPTIONS, Graphics, third page "Wilds" (ES "Salvajes", DE "Wilde"): **Wild Pokemon** (Off / Few / Normal / Many), **Wild display**
  (Pokemon / Silhouette) and **Aggressive** (Off / Rare / Low / Moderate / High). Everything is off by default; with the option off
  FireRed behaves exactly as it did (no object event is made and no encounter changes).
- `settings.cfg` (not the game save): `visiblewilds` is the density (0 off, 1 few = 2 wilds, 2 normal = 4, 3 many = 6), `wildsil` 0 or 1,
  `wildaggro` 0 to 4 (0, 10, 20, 30 or 40% of the wilds are aggressive). The old first-version file wrote `visiblewilds=1` for "on" and
  nothing else; a lone 1 is read as NORMAL. The new code writes `wildsil` and `wildaggro` before `visiblewilds` (and with it, even at 0),
  so a 1 that has them is FEW, and a file cut anywhere never reads a wrong density. The file is 512 bytes, written as text then blank
  lines; the worst case with every key is about 260 bytes, and the looks and the ball come after the wild keys, so they are cut first.
  Unknown keys, out-of-range values and damaged lines are ignored (`tests/wilds_settings_test.c`).

### What is shown, and the limits
- Old 3DS: at most 2 wilds, for every density (`CtrWilds_Max`, which honours `force_old3ds`). New 3DS: at most 6.
- Object events: FireRed has 16. A wild is made only while 4 stay free, and the farthest ones are removed when fewer than 4 are free
  (checked every 30 frames, the only place the free ones are counted), so the map's NPCs and objects never lose their place to one. The
  count is lowered by the free object events, never raised above the density or the console.
- Land wilds appear on the tiles the game rolls land encounters on, water wilds on sea tiles of a map with a water table (only when the
  player surfs, or a third of the cells on foot when the map also has land). They appear in the band just outside the screen that the
  game keeps object events in (left, right and below; no pop-in), one every 1.5 to 4 seconds while below the target.
- Not in the Safari Zone, Randomlocke, the Union Room, the quest log playback, with encounters disabled, nor on a map whose tables have no
  species with a picture in the pack (Unown never). A Pokemon without a picture is not picked at all (the picker rolls a few times).
- Pictures come from the pack's follower set (`CtrFollower_Has`), the female picture when the Pokemon is female and the pack has it.
  Silhouettes are one combiner pass (`HudNpcTint`, `CTR_NPC_SILHOUETTE`), also forced in the Pokemon Tower without the Silph Scope.
  A shiny sparkles now and then (not as a silhouette).

### Behaviour: one state machine per wild
Idle (still, turns) and wander (one to three steps) alternate. A wild is **aggressive** or **calm**, rolled at spawn from the AGGRESSIVE
share. An aggressive wild that notices the player within 4 tiles (same height) shows "!" (with the game's `SE_PIN`), pauses for half a
second and then chases at walking pace (a running player escapes) for up to 12 steps while the player stays within 6 tiles; next to the
player it asks for the encounter, which the field input starts when the player is not mid-step (A or walking into it start it too). If
the lead Pokemon is much stronger than the wild (level x base stats under 70%) the aggressive wild is alarmed and **flees** instead.
A calm wild never chases; it only flees from a much stronger lead, with a chance of 10% just under the limit to 50% at 30%, when the
player is within 2 tiles (4 running or biking). A runner far away (6+ tiles) is gone with a 30% chance. Rest times after a miss, a
flight or a chase are 5, 10 and 15 seconds. Steps use the game's collision on land and a free sea tile on the water, and stay on what
the wild is made for. A water wild is at the sea's height, below a player on the shore, so it ignores the height difference (it can
notice, be walked into or be pressed with A from the shore); a land wild needs the same height as the player.

### One Pokemon, rolled once
The picker (`CtrWilds_Roll`, wild_encounter.c) rolls the whole Pokemon from the map's land or water table with the game's own routines
(so the LeafGreen and night variants are the ones a grass step would give): species, level, personality (nature, gender, shininess)
and whether it is the roamer. A roamer on its map shows as the game's own 1 in 4 would pick it and starts the battle through the
roamer's own functions. A Repel blocks exactly as the step encounter does (a level under the lead's), when the wild is picked, when
it is touched and, with a Repel started later, by removing the wilds it keeps off. The battle starts with that very Pokemon
(`CreateMon` with the stored personality, ordinary IVs at battle time; `CtrWilds_Start`). The picker runs the game's routines on the
module's own generator and puts the game's `Random()` state back as it was, so showing a wild never changes what the game rolls.

### Step encounters
They are not switched off globally. A step on a land (or water) tile gives way to the visible wilds only where the system works
(option on, not blocked, a table with a pictured species for that kind of tile) and a wild is out or one can appear (free object
events, a cap above zero). A map where nothing can be shown, or where the free object events run out, keeps FireRed's encounters;
fishing, Rock Smash and Sweet Scent are untouched. Surfing and caves keep their visible wilds whenever the system can operate there.

Three cases give the game's encounters back (`wilds_core_test`: `TestIdleReleases`, `TestStall`):
- **Option off**: FireRed's step encounters are back at the very next step, even if no wild had appeared yet. The per-frame early out
  (`CtrWildsCore_Idle`, `CtrWildsCore_Unsupported`) clears `operative` and `spawnable` itself, as it never reaches the frame setting.
- **Nowhere to appear** (a map smaller than the screen, a narrow route, the player at an edge, nothing the table can use): after
  `CTRW_STALL_FAILS` (6) spawn attempts in a row that fail, with no wild out, the step encounters come back (`CtrWildsCore_Stalled`).
  Nothing is switched off for good: the attempts go on at 3 to 5 seconds instead of 1 to 2, a success hides the step encounters again,
  a new map or the option going off starts the count over. No work is added per frame.
- **A saved game**: object events are saved with the game, and the table is not. A wild's object event carries a mark (the Boy, no
  movement type, `trainerRange_berryTreeId` = `CTRW_MARK`, a byte the Boy does not use). When the field is rebuilt from a save
  (`SpawnObjectEventsOnReturnToField`, before the sprites are made) every object event with a wild's local id (0xF0 to 0xF5), the mark
  and no matching table entry is deactivated (`CtrWildsCore_IsOrphan`); a map's own object events, the companion (0xF7) and anything
  without the mark are never touched. A new map does the same sweep (it already removed them all).

### Cost per frame
With the option off and nothing out the per-frame update compares the map id and returns (a few instructions, 1 to 2 ns on a host). With
it on: a table of 6 wilds (object event index, state) is walked, no scan of the 16 object events, no free-slot search and no Pokemon
generation per frame; spawn attempts every 1.5 to 4 seconds only below the target; the free object events, the lead Pokemon and the
Repel are read once per 30 frames. Host micro-benchmarks (`wilds_core_test`) print the numbers; there is no console or emulator timing.

### Tests
`wilds_core_test` (budget, density and console cap, the 16 object events with the reserve, almost-full maps, spawn rate and ring,
Repel, the state machine and its ranges, the Pokemon's round trip, water and land, roamer, guards, map changes, fallback to the game's
encounters, the option turned off before any wild appeared, spawn failures in small and narrow maps with the recovery, the orphans of a
loaded game against NPCs, the companion and the Berry Blender's local ids), `tools/test_wilds_recipe.py` (the glue's text), `wilds_settings_test` (old, new, unknown, truncated at every byte, longer than the file), `settings_test`,
`poketouch_options_test`, `battle_gen4_test` (the silhouette flag), the render test's measured fit check for the three rows, and
`tools/test_option_labels.py`.

### Not verified (needs a console or Azahar)
Nothing of the game glue ran. To try: (1) the spawn band is off screen on the top screen and nothing pops in; (2) held movements of a
wild do not wedge its object event (a wild that stops after a flight); (3) a chaser starts the battle without a double step of the
player; (4) frame time on an Old 3DS with 2 wilds and a New 3DS with 6; (5) caves and Viridian Forest (land encounter tiles that are not
tall grass); (6) the picture's gender, nature and shininess match the battle; (7) a map with many NPCs (Celadon, Saffron): the NPCs
all appear and no wild takes the last 4 object events; (8) water wilds while surfing and on foot at a shore, and silhouettes in the
Pokemon Tower; (9) a roamer on its map; (10) Repel on and off; (11) step encounters come back in a Safari Zone, with Randomlocke and on a map
with a table of Unown only; (12) option Normal then Off before any wild appears: the grass encounters are the game's at once;
(13) a place with no room for a wild (a small indoor grass patch): encounters come back after some seconds; (14) save with a wild on
screen, reload in the same session and from a cold start: no leftover Boy-looking object, no blocked tile.

### Credit
Design after Wilds of Hoenn by poooooby (MIT; `NOTICE.md`), and the ideas of PR #83 by mcl5500/Sh3ld0r (MIT): silhouettes, the "!" alert
with `SE_PIN`, water wilds, roamers, personality kept into the battle, configurable aggression. No code of either is copied.
