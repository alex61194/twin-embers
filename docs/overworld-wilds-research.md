# Overworld wild Pokemon: research, plan and the final design

Status: implemented. Sections 0 to 7 are the original research and plan, kept as history; section 8 is the design as built
(recipes 0249 and 0250, `3ds_port/src/3ds_wilds_core.c`) and its validation status.

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
  game keeps object events in (left, right and below; no pop-in), at the pace of the table below while under the target.
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
  Nothing is switched off for good: the attempts go on at 1 to 1.5 seconds instead of a third to two thirds of a second, a success hides
  the step encounters again, a new map or the option going off starts the count over. No work is added per frame. The quick retries do not
  delay the game's encounters: they come back after the 6th failure in a row, at most about 6 x 40 frames (3 to 4 seconds; 190 frames
  measured in `TestFastRetry`) after the first failure, and the first success hides them again.
- **A saved game**: object events are saved with the game, and the table is not. A wild's object event carries a mark (the Boy, no
  movement type, `trainerRange_berryTreeId` = `CTRW_MARK`, a byte the Boy does not use). When the field is rebuilt from a save
  (`SpawnObjectEventsOnReturnToField`, before the sprites are made) every object event with a wild's local id (0xF0 to 0xF5), the mark
  and no matching table entry is deactivated (`CtrWildsCore_IsOrphan`); a map's own object events, the companion (0xF7) and anything
  without the mark are never touched. A new map does the same sweep (it already removed them all).

### When the next wild comes (real frames at 60 fps)
Three numbers are kept apart:

- (a) the **configured wait**: `spawnTimer`, `MIN + Rand() % SPAN` of the table's own generator (never the game's), named in `3ds_wilds_core.h`;
- (b) the **real frames between two attempts**: the operative frames from one attempt to the next, the later attempt's frame included. A
  wait of T gives exactly T of them: `CtrWildsCore_SpawnDue` counts a frame down and answers TRUE on the frame the wait reaches 0 (it used
  to answer on the next one, one frame late; a wait that is already 0 gives the attempt on the same frame);
- (c) the **real frames from a map change to the first wild**: the change frame is frame 1 (its 30-frame pass has just made the target),
  and every operative frame up to the one that makes the wild counts.

An operative frame is one where `CtrWilds_Update` reaches `SpawnDue`: the option on, not blocked, the map able to show a wild and the
field's controls free. Frames with the controls locked (a door, a script, a menu) or the system blocked do not count, and neither do the
frames at the target: the wait only runs down while a wild is wanted (`active < target`). So the real time on screen can be longer than
the table when the player is locked, never shorter.

| After | Real frames (what the tests measure) | Configured wait | Constant |
| --- | --- | --- | --- |
| a new map, to the first wild (c) | 24 to 36 (0.4 to 0.6 s) | 24 to 36 | `CTRW_FIRST_*` |
| a success with fewer than half of the target out (`active * 2 < target`, `active` counting the wild just made) | 45 to 90 | 45 to 90 | `CTRW_FILL_*` |
| a success with half of the target or more out | 90 to 150 | 90 to 150 | `CTRW_TOPUP_*` |
| a failed attempt | 20 to 40 | 20 to 40 | `CTRW_FAIL_*` |
| a failed attempt while stalled (6 in a row, nothing out) | 60 to 90 | 60 to 90 | `CTRW_STALLED_*` |
| a wild went away (`CtrWildsCore_Forget`: shed, stale, out of view, battle) and the wait was longer | 60 to 119 at most (the removal frame counts as frame 1) | cut to 60 to 119 | `CTRW_REMOVED_*` |

`wilds_core_test` runs the glue's order every frame (frame setting, 30-frame pass, spawn) over 4000 seeds and checks that the observed
minimum and maximum are exactly these numbers, and that each real gap equals the configured wait. A removal never lengthens a wait (a
wait of 0 to 119 gives the attempt in max(wait, 1) frames) and does nothing when it is already 119 or less, so several wilds removed in
one frame cut it once. Turning the option off removes the wilds too (the wait is irrelevant then: nothing counts).

**The ring is drawn in proportion.** A spawn attempt looks at up to 24 cells (`CTRW_PICK_TRIES`, one tile lookup each). Each cell is
drawn uniformly over the three zones outside the screen taken together (left, right, below): a zone weighs its number of cells, an
empty one 0, and the two bottom corners that the side zones also hold weigh twice. An empty ring gives no cell and no lookup. The ring
itself (the screen's half size plus a tile, the object event view) did not change.

### Diagnostics: the summary line
`CtrWildsCore.stats` (reset by every new map) counts plain integers: attempts, successes, failures by reason (`CTRW_FAIL_NOCELL`: the
ring had no cell the game accepts; `ROLL`: the map's table gave no pictured species; `NOOBJ`: no free object event), the frames until
the first wild and until the target was first reached, and the times it stalled. They work only while `CtrWildsCore.statsOn` is set,
which the glue takes from `CtrWilds_LogOn` (`CtrLog_Enabled`: port.log is on) at a new map and in the 30-frame pass, never per frame; any
change of that state starts the counters from zero, and the count is from the moment it began. With port.log off nothing is counted and no
line is formatted. The frame count is one byte tested (and, with the log on, one increment) inside `CtrWildsCore_SpawnDue`, after the
operative check: with the option off, a blocked state or a map with nothing to show, it never runs. Nothing is written per frame. `port.log`
(Old 3DS builds, or with `sdmc:/3ds/twinembers/debug.txt`) gets one line when the map changes, if the map had attempts, and one every 30 s on a map:

    wilds: map=0102 att=9 ok=6 nocell=2 roll=1 noobj=0 first=31 fill=420 stalls=0 t=900

`map` is the map group and number in hex; `att`, `ok` and the three failure reasons add up (`att = ok + nocell + roll + noobj`);
`first` and `fill` are operative frames from the map change to the first wild and to the target (0: not reached); `stalls` is how
often it gave the step encounters back; `t` is the operative frames counted. Many `nocell` with `stalls` above 0 means the ring is
too narrow there; `roll` means the map's table has no species with a picture; `noobj` means the object events were full.

### Cost per frame
With the option off and nothing out the per-frame update compares the map id and returns (a few instructions, 1 to 2 ns on a host). With
it on: a table of 6 wilds (object event index, state) is walked, no scan of the 16 object events, no free-slot search and no Pokemon
generation per frame; spawn attempts every 0.3 to 2.5 seconds only below the target (up to 24 tile lookups each); the free object events, the lead Pokemon and the
Repel are read once per 30 frames. Host micro-benchmarks (`wilds_core_test`) print the numbers (a computer's time, not a console's: the counters' byte test is lost in the noise and the increment is a fraction of a nanosecond there);
there is no console or emulator timing.

### Tests
`wilds_core_test` (budget, density and console cap, the 16 object events with the reserve, almost-full maps, spawn rate and ring,
Repel, the state machine and its ranges, every spawn interval over 4000 seeds, the refill after a removal, the quick retries against the
stall and the recovery, the weighted ring (histograms), the lookup limit and the counters with their summary line, the Pokemon's round trip, water and land, roamer, guards, map changes, fallback to the game's
encounters, the option turned off before any wild appeared, spawn failures in small and narrow maps with the recovery, the orphans of a
loaded game against NPCs, the companion and the Berry Blender's local ids), `tools/test_wilds_recipe.py` (the glue's text), `wilds_settings_test` (old, new, unknown, truncated at every byte, longer than the file), `settings_test`,
`poketouch_options_test`, `battle_gen4_test` (the silhouette flag), the render test's measured fit check for the three rows, and
`tools/test_option_labels.py`.

### Validation status
- **Host (a computer, gcc):** the pure core, the settings file, the OPTIONS text fit and the recipe glue's text are covered by the
  tests listed above, and the recipe chain applies and the three game files compile with the game's flags. A passing host test is
  not a result of the game running; no scenario below is counted as verified because a test covers it.
- **Real hardware:** the owner played the system on real hardware (report of 2026-11-13) and its general operation is correct.
  Which of the points below were looked at one by one was not recorded, so none is marked as verified here.
- **Emulator and Old 3DS timing:** no per-point result is recorded. The Old 3DS frame time needs `port.log` on a console.

Still to check, one by one (none recorded as done): (1) the spawn band is off screen on the top screen and nothing pops in; (2) held movements of a
wild do not wedge its object event (a wild that stops after a flight); (3) a chaser starts the battle without a double step of the
player; (4) frame time on an Old 3DS with 2 wilds and a New 3DS with 6; (5) caves and Viridian Forest (land encounter tiles that are not
tall grass); (6) the picture's gender, nature and shininess match the battle; (7) a map with many NPCs (Celadon, Saffron): the NPCs
all appear and no wild takes the last 4 object events; (8) water wilds while surfing and on foot at a shore, and silhouettes in the
Pokemon Tower; (9) a roamer on its map; (10) Repel on and off; (11) step encounters come back in a Safari Zone, with Randomlocke and on a map
with a table of Unown only; (12) option Normal then Off before any wild appears: the grass encounters are the game's at once;
(13) a place with no room for a wild (a small indoor grass patch): encounters come back after some seconds; (14) save with a wild on
screen, reload in the same session and from a cold start: no leftover Boy-looking object, no blocked tile; (15) the real time to the
first wild and to the target on a route (the `first` and `fill` of the summary line in `port.log`), and that 24 to 36 real frames is not
too abrupt; (16) frame time with the quicker retries in a place with no room.

### Credit
Design after Wilds of Hoenn by poooooby (MIT; `NOTICE.md`), and the ideas of PR #83 by mcl5500/Sh3ld0r (MIT): silhouettes, the "!" alert
with `SE_PIN`, water wilds, roamers, personality kept into the battle, configurable aggression. No code of either is copied.
