# Validation of the new source tree

Technical reference: `96f95e015af9f5f38e4a8aefca7cab29a78b030d`.
Date: 2026-10-07. This file records new-tree results, not the old binary's results.

## 2026-10-09: native Trainer ID and RNG initialization

Branch `fix/3ds-new-game-entropy` starts at verified main
`6c23965e7714a9bb1d29caa7501d481fd5004c6b`. Root cause, boundaries, entropy
fallback and reproduction commands are in [RNG-INITIALIZATION.md](RNG-INITIALIZATION.md).

| Check | Result |
|---|---|
| Fresh pinned bootstrap | PASS; corrected `main.c` exactly matches the ARM11-compiled source |
| Controlled entropy / original behavior | PASS: 66 vectors, PS success/open failure/generation failure, 4,096 fallback samples (3,964 distinct); 64 original zero-overwrite reproductions |
| Independent new games | PASS: 256 resets, preset and typed names, unchanged LCG/secret-ID generation; 600 subsequent RNG advances per run with no entropy requests |
| Save compatibility | PASS: original SaveBlock2 size/ID offset, four stored ID roundtrips (including zero), 12 real rotating/alternating sector/checksum function roundtrips with all synthetic blocks preserved |
| Existing host suite | PASS: all 22 `verify-host` executables |
| Builder | PASS: 80 tests, 2 own-ROM tests explicitly skipped |
| Source/binary audit rejection tests | PASS: 15 source + 14 binary tests |
| REX/actual ARM/C runtime | PASS: 3 tests |
| Save migration/fault injection and option layout | PASS: 2 + 2 tests |
| Full ARM11 build | PASS: devkitARM GCC 16.1.0; `CLEAN_RELEASE=1 verify-game-3dsx`; relocation simulation and ARM11 sound guard pass |
| Standalone entropy 3DSX | PASS: ARM11 compile/link plus relocation simulation; no game data/code required |
| Structural binary audit | PASS: embedded game payload 0, unknown bytes 0, RomFS game payload files 0; no own-ROM leak scan claimed |
| Source/history | PASS: 371 reviewed paths, all 125 reachable commits through `ac9ec966f121b3aec09479f7489413181b98fff2`; immutable Git reads cached/batched, original validation rules unchanged; later documentation commit rechecked before push |
| Isolated emulator | PASS: three independent diagnostic launches, each 32/32 distinct live seeds; first seeds 46898, 3609, 36380. Updated diagnostic reports PS random available for Old/New 3DS configuration runs |
| Full-game isolated boot | PASS: game candidate built at `ac9ec966f121`, no original save, existing local pack copied read-only into isolated SD; ABI `8f71cf3a`, engine/graphics loading and intro/title run with compositor `errors=0`; callback `CB2_TitleScreenRun` confirmed at frame 3,600; interactive initialization/Continue not claimed |
| Physical New 3DS family, owner-reported | PASS reported 2026-10-09: visible new-game ID 61398, varying IDs in subsequent new games, and the requested manual save/Continue checks all pass. Owner confirms physical hardware in the New 3DS/New 3DS XL/New 2DS XL family; exact model not specified. No independent hardware measurement or PS-versus-fallback service attribution claimed |
| Publication policy | UNCHANGED: `binary_release_approved: false`; release command exits 2 with `BINARY RELEASE BLOCKED` |

The installed emulator identifies itself as `AzaharPlus-931cfffb3-dirty` (MSYS2
build), although installed under Azahar. The portable test copy and its fresh
user/SD directories stayed entirely in the ignored workspace. Emulator stub
warnings are not hardware-service validation. Synthetic/standalone tests used no
real save, ROM or PAK. The additional full-game boot used a private copy of the
existing local pack; its original remained untouched, no original save was used,
and no pack or binary was uploaded. The owner subsequently reports successful
manual new-game ID variation and save/Continue checks on physical New 3DS-family
hardware, as recorded above. Exact model, independent hardware measurements,
physical PS-versus-fallback behavior and Old 3DS acceptance remain unverified.
Local game/diagnostic binaries
are test candidates only; the original binary-release block remains in force.

## Revision 2: the 336-byte game-derived exception group is externalized

Local, private validation of branch `feature/externalize-final-game-exceptions`
(base `45f43ef38fb70029426c59950915ceb9627a26dd`). Fresh bootstrap of
`pret/pokefirered@037335f4c725d7c9aecdac87066f2002b4bd7e14`; all 96 edit recipes
applied. Nothing below was uploaded and no gate was lifted: the repository is PRIVATE
and `source_audit.py --release` still reports `BINARY RELEASE BLOCKED`
(`binary_release_approved: false`).

| Item | Result |
|---|---|
| Game-derived engine exceptions | 336 bytes / 14 sections -> **0 bytes** |
| Port-routing/state group | 56 bytes / 9 sections, unchanged, still under separate review |
| Engine ABI | `bb510ffd` -> `8f71cf3a` (propagated to the recipe, RomFS `engine/abi.bin`, README, tests) |
| Pack entries / REX units / engine-exception records | 6,734 -> 6,734 / 12,197 -> 12,211 / 613 -> 599 |
| Supported ROM | SHA-1 `41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc`, used read-only, unchanged before/after |
| Builder tests with the real ROM | 49 run, 49 pass, 0 skipped (`test_own_rom_recipe_pack_pipeline` and `test_complete_own_rom_pipeline` ran) |
| Pack built by the Builder | 6,734 entries, 7,796,864 bytes, ABI `8f71cf3a`, SHA-256 `797dc9d1a521a1be41998df3793eee56a90ab04dac5dc51f1b866fa35940a9e4`; a second build is byte-identical; `verify-pak` verified 6,734 payloads |
| Recipe equivalence (`verify_rex_recipe.py`) | 12,211 units and 6,522 graphics entries compared with the compiled objects: 0 differences |
| Pointer equivalence (`verify_rex_pointers.py`) | 53,262 sites, 0 site-set differences; only the three documented out-of-array `gMonIconPaletteTable` targets differ |
| Clean 3DSX candidate | 3,753,756 bytes, SHA-256 `8e58f7c54e90fc4dcaf93a49c64e40b75cda565c91aa62d8d07da4be0726f2a3` |
| Binary audit (`--release --coverage --rom`) | embedded game payload 0, unknown 0, RomFS game payload files 0, ROM leak scan 0 hits, pack coverage PASS, ABI PASS |
| Engine-owned bytes in the audit | port-authored 56 (the separate group), jump tables 11,312, compiler constants 710, literal pools, toolchain and port source |
| Fourteen symbols in the ELF | all in `.bss` (NOBITS) |
| Host/other tests | Builder, source/history (9), binary audit (14), REX/ARM/C runtime (3), graphics externalization, CtrData runtime, host `verify-host`, map-load wait, battle-transition exit, ARM11 m4a guard: PASS |

The ROM leak scan only sees 32-byte windows with enough variety, so it cannot judge the
fourteen small tables; their absence is established by the NOBITS placement, the zero
embedded count and the recipe equivalence above.

**Azahar (Linux, software OpenGL, scripted).** Clean 3DSX plus the locally built pack:
boot, Game Freak logo, the Gengar/Nidorino fight scene, title, Continue, Quest Log,
overworld, Pokédex, party, bag, Trainer Card, the lower-screen Town Map and a few steps; and a no-save
new game through the controls guide (white text) and the Oak speech to the naming screen.
The log shows `pack 6,734 entries, ABI 8f71cf3a`, `rex: game data loaded from the pack in
~1.2 s` (emulator time), 59.8 fps, `errors=0`, no data errors. **Not tested:** warps/map
transitions, fishing, the Dodrio minigame, Easy Chat, Pokémon Summary and Storage, battles,
save, and audio by ear. **Real 3DS: NOT TESTED.**

The BUGFIX corrections that are now port code (Dodrio column, Elite Four pictures) and the
3DS overworld BG layout were proven equal to the previous executable's table bytes by
computation (ROM-derived bytes plus that code), not by playing those scenes.

The older snapshot below is kept for history.

Validated starting HEAD for the private preparation pass:
`ed8796ffaca218d1d65dbe3dae7f6f408ac92f0c`.
Successful GitHub Actions run:
37686587063 (in the private development repository).
These counts describe that reviewed snapshot; later governance commits have
their own source/history counts and CI results.

| Check at the validated starting HEAD | Result |
|---|---|
| Reviewed files | 272 |
| Audited commits | 5 |
| Builder / pack / recipe | 41 tests: 39 PASS, 2 SKIP (own ROM required) |
| Source/history audit tests | 9/9 PASS |
| Clean-release audit tests | 14/14 PASS |
| REX / actual ARM / C runtime | 3/3 PASS |
| Graphics externalization | 8/8 PASS |
| CtrData runtime | PASS |
| Binary publication policy | BLOCKED |

## Verified on this new tree

- Selected technical source and all 94 original patch files matched their pinned
  Git blob hashes before adaptation. No previous repository ancestry was imported.
- A fresh external bootstrap at the locked pret commit succeeded. The 95 JSON
  edit recipes reproduced Git applying the 94 original patches plus the new font
  bridge: 78 touched paths, zero byte differences, including creations/deletions.
- The partitioned recipe reassembles the reference lists in the same order:
  6,734 entries, 12,197 REX units, 613 exception records, ABI `bb510ffd`.
  Only C/R operations occur in game entries; no literal bytes or fill pool.
- 41 Builder/pack/recipe tests: 39 passed, 2 own-ROM tests explicitly skipped.
  One of those opt-in tests reconstructs and verifies the complete 6,734-entry
  pack. No real ROM was supplied for this run.
- 14 binary-audit tests, 9 source/history rejection tests, 8 graphics
  externalization tests, and 3 actual C/ARM REX/font tests passed. The synthetic
  ARM test uses the real compiler, NOBITS conversion, linker roots and post-link
  pointer-map generation, not a mocked transform.
- Production CtrData tests passed: startup, access CRCs, streams, exact sizes,
  full pack CRC scan, clean-profile missing-pack behavior and refusal to use
  development/embedded data in clean mode. Production graphics-loader tests
  passed bounds, CRC, rebase, lazy reads, CPU/DMA/LZ consumers and resident cache.
- GitHub CI passed on `ed8796ffaca218d1d65dbe3dae7f6f408ac92f0c`:
  run 37686587063 in the private development repository.
  This includes the full new source history and the exact blocked release status.

Windows GNU-stdio runtime tests used devkitPro's MSYS compiler. Builder/C pack,
REX and font tests used its MinGW and ARM compilers; CI repeats the synthetic
suite on Linux, explicitly installing the ARM fixture toolchain.

## Local 3DSX candidate

A clean-profile candidate compiled from the fresh bootstrapped technical tree.
The 3DSX loader simulation and ARM11 m4a address/relocation checks passed. Generated
assets, objects, ELF, maps and the candidate remain in the ignored local build;
none were committed or uploaded.

Candidate size: **3,753,584 bytes**. SHA-256:
`8bcd380c21d8a90d609b65477e7031d87b7da24ddc5418fe23ce54f204df8391`.

The structural ELF/map/3DSX audit reports:

| Finding | Result |
|---|---:|
| Embedded game payload under the audit's classification | 0 bytes |
| Unknown read-only/data contributions | 0 bytes |
| Game payload files in embedded RomFS | 0 |
| Externalized NOBITS game/graphics storage | 7,393,924 bytes |
| Live graphics index entries | 6,392 |
| REX regions | 212 |

RomFS contains exactly `engine/abi.bin`, `engine/assets.bin`, `engine/rex.bin`
and `engine/profile.bin`. Required runtime symbols, clean profile, ABI and
coverage of every executable-expected pack path passed. The recipe is a static
superset, so unused entries are expected.

The classifier retains **392 bytes** of declared port-adapted/authored exceptions
from the technical reference (336 + 56). Its structural zero does not prove that
those bytes, game code, compiler literal pools or every derived use have legal
clearance. These exceptions need a separate documented rights decision before
binary publication. No binary ROM scan, actual pack-backed gameplay, emulator
acceptance or hardware acceptance is claimed.

The private preparation review identifies each linked exception in
[ENGINE_EXCEPTIONS.md](ENGINE_EXCEPTIONS.md). Some of the 336-byte group have
unchanged upstream declarations; `port-adapted` is a recipe-generation label,
not proof of original authorship. No exception is removed or newly approved.
The candidate remains a local, unapproved binary; this pass does not publish it.

Binary publication is blocked regardless of synthetic test success. Full-ROM
reconstruction, binary ROM leak scan, pointer equivalence, linked-license review
and hardware acceptance remain separate required checks.

The real candidate's `--release --coverage` audit exits nonzero with the specific
finding that a local supported-ROM leak scan is required. The independent
`source_audit.py --release` policy also remains blocked. Neither gate was lifted.
