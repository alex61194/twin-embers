# Private PokeTouch red-chrome candidate — 2026-10-08

Base: `579726e8ea8fee69380f0b83160996f1cd29fcaf`.
Branch: `feature/poketouch-firered-chrome`. Executable source commit: `d6e9e72`;
subsequent changes add optional real-pack tests and this record only.

Exterior frame/bar/sidebar/back/save-dialogue controls use FireRed-inspired red
surfaces and crisp integer highlights. Shared viewport/battle theme is unchanged.
All rectangles, label positions, player-name text, ordering and input routing
are preserved. Partial redraw restores the pressed button background and refreshes
the bar when entering/leaving SAVE. Original 12x12 running-shoe art replaces the
silhouette in the same canvas and location; it is not copied from the reference.
The reference image is not committed. Runtime item graphics remain in the pack:
fame_checker, poke_ball, berry_pouch, card_key, coin_case, teachy_tv.
X/Y still use the live game's item table. No pack/ROM/media/binary is committed.

## Executed checks

- Source/history gate: 282 reviewed files and 16 reachable commits passed after
  the retrospective-review fix. Eleven source-audit rejection tests passed.
  The missing review is scoped to full commit
  `1252120687d5122522b31155bf6b65c8dffab776` and exact blob SHA-256
  `168e4dc1739e58864d43cb237ff7f4b845acb85c44033fc82a74035127ed743c`.
  Historical manifests cannot be overwritten; content/path/mode/ROM checks remain.
  Manifest indentation was compacted to remain below the unchanged size gate.
- Production renderer compiled with warnings as errors; synthetic LZ77
  truncation/backreferences/overrun, tiled 4bpp 24/32-pixel pitch, odd-pixel
  coverage, index-zero transparency, opaque black, RGB565 white, invalid assets
  and unchanged fallbacks passed. All touch coordinates including edges checked.
- Synthetic HOME/GBA/OPTIONS/SAVE viewport bytes match the exact base renderer.
  Incremental drawing equals full drawing for pressed/selected/disabled buttons,
  X/Y item changes, RUN, dynamic player name, SAVE dialogue and BACK.
  The base comparison intentionally does not assert the base's known stale
  pressed-face pixels; the new renderer does assert complete pixel equality.
- Six production menu icon decoders passed with the user's actual pack and
  header/index/payload CRC verification.
- Builder: 49/49 tests passed with the supported own ROM, including both optional
  ROM-to-pack pipelines; binary-audit fixtures 14/14, actual ARM/REX/font 3/3,
  graphics externalization 8/8, assets runtime and data runtime passed.
  Data runtime requires GNU-stdio MSYS GCC on Windows, not MinGW GCC.
- Fresh pinned pret bootstrap and `CLEAN_RELEASE=1 verify-game-3dsx` passed.
  3DSX relocations and ARM11 m4a checks passed. ROM-assisted technical binary
  audit/coverage passed: embedded game payload 0, unknown 0, RomFS game files 0;
  7,394,252 bytes of game/graphics storage externalized. This technical result
  does not approve public distribution; the publication gate remains blocked.

## Private artifacts (not in Git)

- 3DSX: 3,756,016 bytes; SHA-256
  `446f6ffe8445610b8c87506f446228e1e8e1f059b656369a76c537a2b1a5f724`.
- Own-ROM pack: 6,734 entries, 7,796,864 bytes, ABI `8f71cf3a`; SHA-256
  `797dc9d1a521a1be41998df3793eee56a90ab04dac5dc51f1b866fa35940a9e4`.
  All 6,734 payloads verified. Supported ROM SHA-1
  `41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc`; original ROM was read-only.

## Remaining acceptance

Azahar control was denied and then its internal app approval timed out despite
the user's chat authorization. No emulator gameplay is claimed for this binary.
User requested delivery to test it themselves. Hardware: NOT TESTED.

Pending in gameplay: upper/lower touch, D-pad/A/B navigation, disabled START
field menu, X/Y registered-item use, RUN switching, return to world, save and
Continue on empty-event maps. Their production controller/bridge code and patch
0098 are unchanged from the base. A further host boot/save-sector test was
attempted but could not link the external game dependencies on Windows; no
passing save/Continue execution is claimed from that attempt.

Keep a backup of an existing save before testing. Place the pack at
`/3ds/twinembers/twinembers.pak`; launch the standalone 3DSX through Homebrew
Launcher. The executable needs this external pack.
