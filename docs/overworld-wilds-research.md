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
