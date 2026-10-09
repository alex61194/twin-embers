# Pokémon Storage on the 3DS lower screen — compiled private candidate

Base: `ea3b10219cd6b0885320acdeeeabbf263d37210a` on
`fix/field-mon-picture-and-battle-menu`. Independent working branch:
`feature/storage-bottom-touch`. Keep the validated base and `main` untouched.

Continuation checkpoint: `28c878069670a737819c7a9a26c0f4529fe5de76`.
The code corrections were committed as `1b4ee5323ff7` and `d6555b1`;
`33e2951` enables the production-controller regressions in CI. These are
ordinary descendants of the checkpoint, without rewriting any history.

## Scope

### Owner correction after testing `6eae647`

#### Follow-up after testing `9ef0337`: black TOP and mailbox Read freeze

`8d67fd7` retains TOP while the PC root task is not ready, including the
frame after its palette fade has ended but before it calls EnterPokeStorage.
The previous fade-active-only condition released TOP in that gap, presented
the black field frame, and held that black frame when the box started.
The lower session still begins only with the actual box controller; the
Withdraw/Deposit/Move choice screen stays on TOP.

`0bc7a53` adds the hash-pinned `0102-mail-init-vblank-wait.json` edit to
the real `CB2_InitMailView` in `src/mail.c`. Init step 9 waits for a background
DMA copy, but the original offline fast loop retries that step without ever
returning to the ARM11 VBlank boundary that completes it. A stalled step now
returns; once DMA completes, the same state resumes. The guard also covers
the link-queue wait at step 15. The original GBA branch and all mail data,
formatting, graphics, saved words and callbacks are retained.

The owner clarified that the failure was from the PC mailbox, immediately
on Read, with a frozen black screen and no message. This is consistent with
the identified unbounded initialization wait; no actual 3DS replay is claimed.
The regression executes the production patched callback with stalled DMA
and link steps, proves one blocked call per frame and eventual reader entry,
and checks that the GBA fast-loop/link exit still behave as before.
The TOP regression now includes fade=false while the root task is still
waiting to enter the box. Four storage-controller tests and the mail
native/GBA callback test pass. Both changed units were compiled with ARM,
the complete game relinked, and the 3DSX relocation/m4a checks passed.
The exact final private binary hash and CI outcome accompany the deliverable.
ROM, pack ABI `8f71cf3a`, `.pak`, save layouts and other branches are unchanged.

The Withdraw/Deposit/Move/Move Items/Exit choice menu stays on the original
TOP field presentation, both initially and on return from a box. HOME stays
below. Only entering the actual box controller starts the lower-screen storage
session; its viewport is revealed after setup. TOP retains its last choice
frame during the entry fade so that it is not frozen on a black frame.
This supersedes the earlier initial-PC-lower presentation described below.

The storage sidebar explicitly starts with `selected = PT_NONE` and
`pressed = PT_NONE`. A zeroed view previously selected `PT_POKEDEX` (ID zero)
even with all its controls disabled, leaving that single button red. It now
uses the same disabled state as the other sidebar buttons. Existing artwork,
button design, pack ABI, save layout and game rules are unchanged.

Production regressions cover choice-menu ownership on entry/return, the
entry-fade hold, box/child lifetime, and neutral sidebar initialization.
The four storage-controller tests and the production renderer regression pass.
The changed native unit was recompiled with the real ARM compiler, then the
complete game was relinked and its 3DSX relocation and m4a checks passed.
The privately delivered candidate is regenerated with the final clean Git
label; its exact SHA/hash and CI result accompany that output. No emulator
was used. Console appearance of these two corrections remains for the owner
to check; automated renderer checks do not replace hardware acceptance.

- The PC main menu, original box view and its GBA child screens use the
  existing PokeTouch lower GPU viewport, sampling the original FireRed
  240x160 tile/sprite composition 1:1 without rerasterizing Pokémon, box
  backgrounds, dialogs, fonts or palettes. Initial field PC uses its
  native bottom-aligned BG0 crop; box and children use the centred
  GBA viewport. TOP holds its previously presented field picture.
- A read-only bridge recognizes the original PC task, box task, stable
  cursor state, context menus, box chooser and item/summary transitions.
  It does not change game storage arrays or mutations.
- A tap is armed only on touch-down, cancels permanently when leaving
  the original region, and issues actions on release. Physical buttons
  cancel pending touch navigation. Navigational plans only send original
  FireRed D-pad/A/B/START input after game cursor animations are ready.
- Geometry includes 30 box slots, the original upper cursor/box arrows,
  party slots, contextual menu rows, 5 PC root options, and the deposit
  / jump box picker. The source-of-truth for every action remains
  FireRed's own controller, including full-party, last-Pokémon and
  MAIL/release safeguards.
- PC-owned Summary and Bag child screens retain their own game controls
  and view; they cannot activate unrelated PokeTouch field roots.

## Corrections after real source/build inspection

- Box icon centres are `(100 + 24*column, 44 + 24*row)`. Their row boundaries
  start at y32, with the first row clipped at y36 for the upper controls.
  The former y24 origin selected the next row in part of each icon and
  truncated the last row. The last visible row now ends at y152.
- Party centres are `(104,64)` and `(152,16 + 24*(slot-1))`; the close control
  is `(152,132)`. Hitboxes and routes from Party back to title/buttons now
  follow those real sprites and the original B-to-box navigation.
- The game and the port share `3ds_storage_bridge.h`. The real ARM compiler
  checks both definitions and callers. Inactive task slots, invalid IDs,
  missing storage state and palette fades reject input.
- A finger and its navigation plan retain the live input-controller identity.
  Replacing a popup cancels the old gesture, including wallpaper set/theme
  changes at the same coordinates. Cursor animations wait between key pulses.
  ERROR_MSG and HANDLE_INPUT retain one root identity, so dismissing a
  full-party/last-mon error with an arrow does not abandon navigation.
- The original release, carried-item and close/continue Yes/No windows accept
  touch through their actual menu cursor and native A/B handling. No storage
  mutation, save format or release safeguard is reimplemented.
- Bag, TM Case and Berry Pouch can belong to the PC session. Setup/fade/null
  callback frames retain the preceding lower picture; the original Summary,
  box-name keyboard and box controller are displayed after their setup.
  BACK presentation uses the same readiness gate as its actual input path.
- `3ds_video.c` required no new composition or artwork. Its production GPU
  sampling call is tested: field-PC source `(80,80)` and centred child source
  `(80,40)`, both 240x160 at 1:1, into the existing `(4,4)` lower viewport.
  This checks coordinates and dimensions, not a hardware-rendered picture.

## Automated checks

- GitHub source/history audit, review manifests, synthetic ARM toolchain,
  pack/REX fixtures and clean publication block are exercised by CI.
- `python tools/test_storage_touch.py` compiles a portable header under
  `gcc -Wall -Wextra -Werror` and checks the 30 box cells, popup bounds,
  every visible row pixel, original party/arrow/control coordinates, and
  all 900 box cursor routes.
- `python tools/test_storage_controller.py` executes the authored game bridge,
  production touch transformation/controller, screen ownership/lifecycle and
  GPU crop selection with synthetic state. It checks all 30 slots, 7 party
  targets, 7 popup rows, 5 PC options, box-picker arrows and all four Yes/No
  controllers. Holding for 90 samples does not act; release confirms once;
  drag-out, changed popup and physical input cancel. These are host C tests,
  not gameplay execution or an emulator.
- Local Windows devkitPro / devkitARM GCC 16.1.0 successfully compiled and
  linked the complete pinned FireRed ARM11 game at the original checkpoint
  and again after the corrections. `CLEAN_RELEASE=1 verify-game-3dsx` passed,
  including every 3DSX relocation stream and the ARM11 m4a address/mixer gate.
- The four real storage/menu/data/naming translation units also passed
  `-Werror=implicit-function-declaration`, `-Werror=incompatible-pointer-types`,
  `-Werror=int-conversion` and `-Werror=return-type`. Existing volatile-DMA
  qualifier warnings in the inherited game bridge remain; no claim of a
  warning-free full upstream build is made.
- Local suites: Builder 49 tests (47 PASS, 2 explicit own-ROM SKIP), tools
  46/46 PASS, and the separate production asset/data runtime gates PASS.
- The structural ELF/map/3DSX audit reports 0 classified embedded game bytes,
  0 unknown bytes and 0 payload files in RomFS; 7,394,252 bytes use external
  NOBITS storage. RomFS contains exactly the four engine metadata files.
  ABI, required symbols and expected pack path/size/CRC coverage pass the
  release-audit checks. Its sole technical blocker is the unperformed local
  supported-ROM leak scan. The independent publication policy stays blocked.
- A compatible private pack is required to run the game on a 3DS. Building
  this candidate required no ROM or pack. This branch does NOT alter ABI `8f71cf3a`.
  Keep `/3ds/twinembers/twinembers.pak` untouched and private.

## Explicitly not claimed

- **Azahar was not launched or used**, per the owner's request.
- There has been NO completed console screenshot comparison or end-to-end
  3DS gameplay acceptance of this branch. A green CI cannot prove that
  sprites and every UI transition are free of graphic errors.
- A standalone private 3DSX has now been built locally. It is supplied outside
  Git in the owner's output directory with its final source SHA, binary hash
  and logs. No GitHub binary artifact/release is uploaded. Reproduce with
  `python tools/bootstrap.py --dir build/storage-touch` and
  `make -C build/storage-touch/3ds_port CLEAN_RELEASE=1 verify-game-3dsx`.
- Complex multi-mon grab/drag/group-release, the marking selector and touch
  keyboard entry for renaming boxes have not been separately integrated;
  physical FireRed controls remain required for these. Thus complete touch-only
  PC acceptance is NOT claimed. This milestone covers Pokémon Storage; the
  preceding field PC-owner selector and the player's Item PC are separate
  controllers and are not claimed as newly ported here.

## Manual acceptance requested (without Azahar)

Back up the save. On 3DS, open Bill's PC, choose Move/Deposit/Withdraw;
tap a box Pokémon twice, inspect its real menu, use Summary then B to
return to the same box; change boxes via either arrow; move and withdraw,
deposit through the real box picker; verify the final party and box state
after save/reload. Also check wallpaper, naming, moving an item, releasing
with NO, all 30 slots, open/close transitions and absence of black
frames, duplicated text or TOP menu flashes. Capture any visual bug
before claiming that the presentation is validated.

No public binary upload or release policy has been enabled.
