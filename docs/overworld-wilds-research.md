# Overworld wild Pokemon: research and plan

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

## 8. Step 1 prototype: what was built, how to test, known gaps

Branch `agent/wilds`, recipe `patches/pokefirered/0249-visible-wilds.json`. Nothing was compiled for a console.

### What exists
- Setting `visiblewilds=1` in `settings.cfg` (default off, written only when on): `CtrSettings_VisibleWilds`.
- OPTIONS: a sixth page "Wilds" (ES "Salvajes", DE "Wilde") with one row, "Visible wilds" (On/Off, EN/ES/DE with a
  two-line description). Page 2 is full (7 rows) and page 3 would have shifted every later row index, so a new last
  page was used; the PAGE button already draws up to 6 dots.
- Game side (all functions; the only new data are `sCtrWild*` statics, so the clean-release rule holds):
  - `wild_encounter.c`: `CtrWilds_Pick` (species and level from the map's land table through the game's own
    `ChooseWildMonIndex_Land`, `CtrWild_VersionSlot` and `ChooseWildMonLevel`; so the LeafGreen/night variants and
    levels match a grass step; the game's Repel test is applied), `CtrWilds_Start` (`GenerateWildMon` +
    `StartWildBattle`, the ordinary wild battle path, so Nuzlocke and the rest see it as a normal encounter),
    `CtrWilds_EncountersOn` (honours `DisableWildEncounters`).
  - `event_object_movement.c`: up to 2 object events (local ids 0xF0-0xF1) on Route 1 only, spawned on tall grass 4+
    tiles from the player inside the view, wander movement (range 2), removed by the game when out of view. Called
    from `CtrFollower_Update` each field frame. Own LCG, so `Random()` is not drawn on for spawn choices (the game's
    wander movement still uses it, as every NPC does). Graphics: the object is the Boy's (his palette, no new OBJ
    palette), and the existing companion compositor draws the pack's Pokemon picture (`CTR_NPC_FOLLOWER + dex`) when the
    pack has it; otherwise the Boy stays visible (placeholder). No new sprites were generated (that is step 2).
  - `field_control_avatar.c`: in `ProcessPlayerFieldInput`, walking into the tile in front (held direction) or pressing
    A there calls `CtrWilds_TryBattle`; it starts the battle and removes the object.
- Guards: never in quest log playback or the Union Room, never while surfing/diving, never with Randomlocke on (its
  species swap would not match the picture), never when fewer than 5 object events are free, none while field controls
  are locked (scripts/cutscenes), trainers' sight is checked before the hook. With a Repel active, a Pokemon the Repel
  would block is neither picked at spawn nor started on touch (it stays and blocks the way).
- Nothing is persisted: they are removed on any other map, with the option off, and any one a save kept (object events
  are saved) is removed on the next frame because its slot has no species.
- Tests updated: `poketouch_options_test` (26 rows, 6 pages, the new row), `settings_test` (key written only when on).

### How to test (console or Azahar)
1. OPTIONS, last page ("Wilds"), "Visible wilds" On. 2. Go to Route 1 (Pallet Town north exit) and walk in the tall
grass. After a few seconds up to 2 Pokemon (Pidgey/Rattata at levels 2-5) appear in the grass near you and wander.
3. Walk into one or press A facing it: the normal wild battle starts. 4. Use a Repel with a lead Pokemon of a higher
level than 5: no wilds appear. 5. Leave the route or switch the option off: they vanish. Step encounters are unchanged.

### Known gaps and risks (unverified: needs a console)
- Only syntax-checked (`-fsyntax-only`, ARM flags) and the recipe chain applies (246 recipes); no run-time test.
- Pop-in: they appear inside the view (4+ tiles away), because the game culls object events outside it.
- Placeholder look: without a pack picture (Pidgey/Rattata normally have one) the Boy is drawn.
- The wander movement can step off the grass (range 2). No flee/behaviour, shiny preview, gender, or bottom screen info.
- Cap is 2 on every console (the doc's Old 3DS cap); a New 3DS cap of 3-4 is a one-line change (`CTR_WILD_MAX`, the
  local-id range 0xF0-0xF1 and the override share it).
- A wild standing in front of a door/ledge path can block the player until it wanders off.
- Step 2 would replace the single-route check with a table-driven map list and the density rule.

## 9. Design adopted from Wilds of Hoenn

Source studied: https://github.com/poooooby/wilds-of-hoenn (MIT; README, `lib/spawn_manager.lua`, `wild_reactions.lua`,
`behavior.lua`, `config.lua`, `spawn_filters.lua`, `shiny.lua`). It is a Lua mod that runs inside a recompiled Gen 3 game and
draws its own actors, so no code is reused; only the design is. Recipe `patches/pokefirered/0250-visible-wilds-everywhere.json`
(plus `CtrWilds_Max` in `3ds_port/src/3ds_game_bridge.c`) implements it.

### What the mod does (own words)
- Spawning: reads the map's own encounter tables at run time (no tables of its own); land species only where the player can
  walk, water species on every water tile. Count per terrain = 1 + one per 20 eligible tiles, capped (Spawn Amount: low 4,
  normal 8, high 12). They arrive by rising out of grass, surfacing from water, or fading in; they leave the same way.
- Movement: a 50/50 mix of idle and roam, an action every 1-2 s, one tile per step, legality through the engine's own
  collision; the wild stays on its terrain unless chasing.
- Reactions (option "Wild Reacts"): only when the player runs or bikes close (radius 2 tiles, 4 on a bike); walking calmly
  disturbs nobody. The wild is judged by level x base-stat total against the lead Pokemon: under 0.7 it is startled ("!"),
  hops and flees 1-3 steps at running pace (30% vanish into the grass; small birds take off); over 1.3 it gets angry and
  chases a bit faster than a runner, with a leash of 2-4 tiles and 1.2 s per tile, then gives up ("?") and walks home; an even
  match ignores the player. Whether it reacts at all is one roll when the player comes in range (10% just past the limit,
  rising to 50% at 0.4 / 2.0); a miss rests 5 s, a reaction 10-15 s. Nothing reacts during cutscenes, trainers, messages,
  battles or the Safari Zone.
- Encounter: walking into a wild starts the normal wild battle (a chaser that reaches the player does too); the classic step
  encounters can stay on or be turned off; Sweet Scent calls 2-3 wilds for about a minute; the Safari Zone uses its own battle.
- Shiny/gender/level: species and level come from the game's roll; the shiny chain builds a "constructed personality" that
  keeps nature and gender (needs its modern-spawns bridge). The mod does not show gender itself.
- Out of scope for us: partner roles (Follow, Forage, Battler), water life, sky flyers, local scenes, silhouettes, portraits,
  five languages, a 15-row options menu, HD sprite sets.
- Performance: no hard limits stated; only the count caps above and culling of what is outside the view.

### Adopted (built in 0250)
- Run-time tables only: the game's land table of whatever map the player is on (`CtrWilds_Pick`), through the game's own slot,
  version and level routines; Repel test kept; not in the Safari Zone; no Unown (their letter depends on the chamber slot).
- Spawn outside the screen: the game only keeps object events inside its view (`OBJECT_EVENT_VIEW_*`, a band a little larger than
  the screen on the left, right and bottom, none above), so a wild appears in that band, 1+ tile past the screen edge. The
  pop-in of step 1 is gone for left, right and bottom; they walk in (wander range 3) or the player walks up to them.
- Pacing: one new wild every 1.5-4 s while a slot is free; one nobody can see is retired after 25-45 s (by slot); the game
  removes any that leave its view.
- Per-console cap (`CtrWilds_Max`, via `CtrPlatform_IsNew3DS`, which already honours `force_old3ds`/CTR_FORCE_OLD3DS): Old 3DS
  2, New 3DS 4 (local ids 0xF0-0xF3; 4 object events always left free so the 16-object limit holds; 1 sprite per wild).
- Reaction rule: the mod's level x base stats ratio against the lead, same thresholds (0.7 / 1.3) and chances (10-50%), one roll
  when it comes in range (2 tiles on foot, 4 running or biking), rest 5 s after a miss, 10 s after a flight, 15 s after a chase.
  Weaker: runs 2-4 grass tiles at running pace; 30% vanish when 6+ tiles from the player. Stronger: walks up (normal speed, 6-9
  steps, so a runner escapes) and starts the ordinary wild battle when next to a player who has stopped. Even: ignores the
  player. Every step uses the game's own collision and stays on grass.
- Gender consistent with the battle: rolled at spawn with the game's ratio, the female picture is used when the pack has it, and
  the battle regenerates the Pokemon with the game's own generator until the gender matches.
- Guards kept: Repel, disabled encounters, quest log, Union Room, surfing, Randomlocke, locked field controls.

### Adapted
- Walking disturbs a wild from 2 tiles (the mod needs running); no "!" or "?" icon (step 3), the wild just turns and goes.
- Chasers stay on grass (the mod lets them leave it): safe for doors, warps and ledges without being able to run the game.
- Spawn count is slot based (the cap), not tile-count based: counting eligible tiles would need a per-map scan.

### Skipped, with reasons
- Partner/follower/Battler: out of scope (owner decision); the companion option already exists.
- Water wilds, flyers, whales, local scenes, Sweet Scent lure, shiny chain/modern spawns, silhouettes, portraits: more object
  events/sprites than the 16/64 limits and the Old 3DS frame budget allow, and not game-faithful.
- Fade/rise/ring effects and shiny sparkle: need new sprites or OAM effects (step 3).
- Shiny preview: the pack has no shiny pictures; the shiny roll stays the game's personality roll at battle time.
- PMDCollab art: CC BY-NC 4.0, non-commercial. Not shipped, not referenced, not needed (the pack's own pictures are used).

### Credit wording (MIT)
Added to `NOTICE.md` ("Design reference: Wilds of Hoenn", with the MIT permission notice and "Copyright (c) 2026 poooooby") and a
bullet in `CREDITS.md` (design reference, MIT, no code or assets copied, PMDCollab art not used). Repeat it in the release notes.

### Not verified (needs a console or Azahar)
Nothing was run: the recipe chain applies (247 recipes), the three game files pass an ARM `-fsyntax-only`, and the clean-release
and source-audit tests pass. To check: (1) the spawn band is really off screen on the top screen (it assumes the camera is 7
tiles left of / above the player; if wilds appear on screen or vanish at once adjust `halfW/halfH` in `CtrWild_RingCell`);
(2) a held flight/chase movement does not wedge the game's wander state (a wild that stops moving afterwards); (3) a chaser starts
the battle without a double step of the player; (4) Old 3DS frame time with 2 wilds, New 3DS with 4; (5) caves and Viridian Forest
(land encounter tiles that are not tall grass; the object is still the Boy or the Pokemon picture); (6) the picture's gender
matches the battle.

### Step 3 (not built)
"!" and "?" emotes (a field-effect sprite tied to the object event), fade/rustle effect on spawn and vanish, Surf wilds on water
tiles, a density rule from the number of eligible tiles, birds taking off, Sweet Scent luring visible wilds, a bottom-screen line
with the wild's name and level, a shiny preview if the pack ever gets shiny pictures.

### Review fixes folded into 0250 (against the step 1 recipe 0249)
- A (high): pressing A on a visible wild that did not start a battle (Repel, disabled encounters) reached the NPC script lookup, which
  has no script for it. `GetInteractedObjectEventScript` now treats the wild local ids (`CTR_IS_WILD_LOCALID`, in
  `event_object_movement.h`) like the companion, also in the counter branch, and returns NULL; its callers already accept NULL.
- B: the claim "the game's random numbers are not drawn on" was false: `CtrWilds_Pick` drew `Random()` on every attempt. The spawn
  now looks for a free grass cell first and picks the species only when it found one; the comment says what is true.
- C: `CtrWilds_Update` ran every frame from `CtrFollower_Update` with the option off. It now returns at once when the option is off,
  no slot is in use and the map did not change (one pass per new map removes anything a save kept); slots that are empty are not
  searched for.
- Spanish option label "Salvajes visibles" was cut to 15 bytes; it is "Salvajes vistos" now, the row text no longer says "Route 1
  only", and `tools/test_option_labels.py` checks every row label in EN/ES/DE against `PT_OPT_LABEL_LEN`.
