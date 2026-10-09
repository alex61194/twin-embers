# Engine exceptions: 336-byte group externalized, 56-byte group port-authored

This is a provenance and engineering record for the local, unapproved candidate. The
repository remains PRIVATE and binary publication remains BLOCKED. Externalizing a
table changes where its bytes live; it is not a rights determination.

| group | before | now |
|---|---:|---:|
| game-derived (14 sections) | 336 bytes embedded as "engine exceptions" | **0 bytes** (EXTERNALIZED) |
| port routing, descriptors, state (9 sections) | 56 bytes | 56 bytes, unchanged, classified PORT-AUTHORED (owner-approved) |

Evidence: candidate ELF/map and audit of a fresh bootstrap of `pret/pokefirered@037335f4c725d7c9aecdac87066f2002b4bd7e14`
with the repository's edits, a pack built by the Builder from a supported ROM
(SHA-1 `41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc`, used read-only), and the recipe's
`engine_units`. The engine ABI moved from `bb510ffd` to `8f71cf3a`; REX units went
from 12,197 to 12,211 (the fourteen tables); pack entries stay 6,734.

## How the 336-byte group is reconstructed

The recipe is still only structure: ROM offsets, strides, bit positions and CRCs. The
operations used are copy (`C`), zero fill (`F`) and the new bit-field record remap (`B`).
No literal byte, hex dump, Base64 or copied initializer was added.

| section | bytes | operation | what differs between ROM and 3DS |
|---|---:|---|---|
| `diploma.c` `sBgTemplates` | 8 | `B` | bit-field layout |
| `easy_chat_3.c` `sEasyChatBgTemplates` | 16 | `B` | bit-field layout |
| `intro.c` `sBgTemplates_GameFreakScene` | 8 | `B` | bit-field layout |
| `pokemon_special_anim_scene.c` `sBgTemplates` | 8 | `B` | bit-field layout |
| `pokemon_storage_system_tasks.c` `sBgTemplates` | 16 | `B` | bit-field layout |
| `trainer_card.c` `sTrainerCardBgTemplates` | 16 | `B` | bit-field layout |
| `union_room_chat_display.c` `sBgTemplates` | 16 | `B` | bit-field layout |
| `overworld.c` `sOverworldBgTemplates` | 16 | `B` + port code | bit-field layout; 3DS map layout of BG1-3 |
| `intro.c` `sFightSceneSpritePalettes` | 48 | `C` + `F` | ROM has five descriptors; the BUGFIX build adds a zero terminator |
| `dodrio_berry_picking.c` `sUnsharedColumns` | 25 | `C` + port code | BUGFIX corrects one five-player column |
| `pokemon.c` `gFacilityClassToPicIndex` | 150 | `C` + port code | BUGFIX corrects two Elite Four pictures |
| `fame_checker.c` `sTextColor_Green` | 3 | `C` | none (tiny, explicit unit) |
| `oak_speech.c` `sTextColor_White` | 4 | `C` | none (tiny, explicit unit) |
| `field_player_avatar.c` `dot.0` | 2 | `C` | none (tiny, explicit unit) |

**Bit-field layout.** `struct BgTemplate` is `u16 bg:2, charBaseIndex:2, mapBaseIndex:5,
screenSize:2, paletteMode:1, priority:2, baseTile:10`. agbcc packs these 24 bits
contiguously (`baseTile` at bit 14); the 3DS compiler never lets a field straddle its
16-bit storage unit (`baseTile` at bit 16). Both are four bytes. The op
`["B", offset, 4, count, [[0,0,14],[14,16,10]]]` rebuilds each record from the ROM by
moving bit ranges; values are never stored.

**Game-derived values versus port/runtime values.** The pack carries what the ROM has.
Where the 3DS build deliberately differs, the difference is port code, not data, in
`patches/pokefirered/0097-final-game-table-reconstruction.json`: the source tables are
the original ones (`#undef BUGFIX` around the table only), and
`InitOverworldBgTemplates` (3DS map base/size of BG1-3), `GetUnsharedColumn` and
`CtrPokemon_ApplyBugfixes` (called once after the pack loads) apply the port's
behaviour. The pointer words of the palette descriptors are runtime addresses and are
patched by REX, as for every other pointer table.

**Tiny tables.** The three sections below eight bytes are listed by name in
`OBJECT_LOCAL_UNITS` of `tools/rex_manifest.py`. The proof is a unique match inside the
same object's ROM data. No size threshold decides what stays embedded, and a
coincidental match (a one-byte port variable, a compiler switch table) can never enter
the pack because only named units are eligible.

**Equivalence proof.** `tools/verify_rex_recipe.py` rebuilds every pack entry from the ROM
and compares every unit with the bytes the compiler produced, except at pointer words:
12,211 units and 6,522 graphics entries, 0 differences. For the three tables with port
code, the ROM-derived bytes plus that code were also compared with the previous
executable's table: identical. `tools/verify_rex_pointers.py`: 53,262 pointer sites,
0 site-set differences (the three documented out-of-array `gMonIconPaletteTable`
targets are the only mismatches).

## Remaining classifier limits

`tools/rex_manifest.py` still derives labels from failed proofs, and neither label proves
authorship or a license. Compiler jump tables, compiler constants and literal pools are
separate engine-owned categories. `BuildDateTime` (`__DATE__ __TIME__`) is compiler output,
not a game table.

## 56-byte group: port routing, descriptors and state

Nine sections total 56 bytes: 16 bytes of native action routing, 32 bytes of
status-caption pointer descriptors, and eight bytes of task/selection/prefetch
sentinels. Source evidence indicates these were added for the port. The status
descriptor uses game status semantics and points at separately counted literals.

The existing owned-port/Zallax license scopes and exact attribution must be
respected. Apparent originality is not a new authorship certification or binary
redistribution approval. **NOT APPROVED by this pass** also applies to this group.

| Source / linked object | Section and symbol | Bytes | Purpose |
|---|---|---:|---|
| `3ds_port/include/3ds_battle_nav.h (inlined into src/battle_controller_player.c)`; `build/game/battle_controller_player.o` | `.rodata.kNext.0` / `kNext.0` | 16 | Port-authored directional routing for the lower-screen battle action layout. |
| `src/pokemon_summary_screen.c`; `build/game/pokemon_summary_screen.o` | `.rodata.ailments.0` / `ailments.0` | 32 | Port summary bridge pointers to short status captions interpreted from game status fields. |
| `src/berry_pouch.c`; `build/game/berry_pouch.o` | `.data.sCtrPouchTask` / `sCtrPouchTask` | 1 | Tracks the berry-pouch input task for the native touch bridge. |
| `src/item_menu.c`; `build/game/item_menu.o` | `.data.sCtrBagTask` / `sCtrBagTask` | 1 | Tracks the bag input task for the native touch bridge. |
| `src/overworld.c`; `build/game/overworld.o` | `.data.lastStep.0` / `lastStep.0` | 1 | Suppresses duplicate field-restoration diagnostic logging. |
| `src/overworld.c`; `build/game/overworld.o` | `.data.sKey.2` / `sKey.2` | 2 | Tracks the destination key for port-side warp prefetch state. |
| `src/party_menu.c`; `build/game/party_menu.o` | `.data.sBottomTouchSlot` / `sBottomTouchSlot` | 1 | Tracks the selected party slot in the lower-screen input bridge. |
| `src/start_menu.c`; `build/game/start_menu.o` | `.data.sPokeTouchPick` / `sPokeTouchPick` | 1 | Tracks a pending native-menu selection and its inactive sentinel. |
| `src/tm_case.c`; `build/game/tm_case.o` | `.data.sCtrTMCaseTask` / `sCtrTMCaseTask` | 1 | Tracks the TM-case task for the native touch bridge. |

## Disposition

The [per-section review](engine-exceptions-review.json) records, for all 23 sections,
source/object, section/symbol, size, purpose, classification and redistribution status;
the fourteen externalized rows additionally carry `disposition: EXTERNALIZED` and their
reconstruction. It contains identifiers and review metadata only, no table contents.

The 56-byte group (table above) was not touched: same sections, same bytes, same
classification, still **NOT APPROVED by this pass**. Binary publication remains BLOCKED
(`binary_release_approved: false`); a release still needs an explicit rights/provenance
decision for the remaining group and the other gates in [DISTRIBUTION](DISTRIBUTION.md).

## 56-byte group review (2026-10-09)

Each of the nine symbols was checked against `src/` and `include/` of the pinned
`pret/pokefirered@037335f4c725d7c9aecdac87066f2002b4bd7e14`. None exists there (the only
textual match, for `ailments`, is the comment "Volatile status ailments" in
`include/constants/battle.h`). Each is introduced by port code:

| Symbol | Introduced by |
|---|---|
| `kNext` | `3ds_port/include/3ds_battle_nav.h` |
| `ailments` | `patches/pokefirered/0041-native-bottom-summary-bridge.json` |
| `lastStep` | `patches/pokefirered/0013-menu-teardown-and-pp-read-boundary.json` |
| `sBottomTouchSlot` | `patches/pokefirered/0036-bottom-party-input-lifecycle.json` |
| `sKey` | `patches/pokefirered/0053-prefetch-warp-destination.json` |
| `sPokeTouchPick` | `patches/pokefirered/0056-poketouch-menu-actions.json` |
| `sCtrBagTask` | `patches/pokefirered/0059-poketouch-bag.json` |
| `sCtrTMCaseTask` | `patches/pokefirered/0061-poketouch-tm-case.json` |
| `sCtrPouchTask` | `patches/pokefirered/0063-poketouch-berry-pouch.json` |

The contents are port routing indices, single-byte task/selection/prefetch sentinels
and pointers to status captions written in the port's own source ("OK", "PSN", "PRZ",
"SLP", "FRZ", "BRN", "PKR", "FNT"). The recipes are credited to alex61194 with inherited
ZallaxDev portions in the review manifest; both carry MIT terms whose notices are kept.
A fresh clean ARM11 build of `3b3b941` still links exactly 56 port-authored bytes, with
0 embedded game payload bytes and 0 ROM leak-scan hits.

**Disposition (owner-approved 2026-10-09):** port-authored material under the existing
owned-port MIT and inherited Zallax MIT scopes, with the Zallax notices shipped alongside
any binary. Each entry in [engine-exceptions-review.json](engine-exceptions-review.json)
records its value and this classification. The values are navigation indices, sentinels
and pointers to port-written strings; none is a ROM byte. The classification does not
change `binary_release_approved` and does not grant or imply rights in compiled
`pret/pokefirered` game logic or any Pokémon material.
