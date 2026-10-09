# Field menu, Flash and cave presentation validation

## Recorded symptoms and changes

The supplied 48-second recording shows stray tiles around the Rock Tunnel
entrance preview, move selection and TM/HM learning on TOP, and a persistent
left black band after using Flash (about 40 native pixels).

Move selection/forgetting, modal Party move/stat panels and the item-learning
animation now own the lower GPU viewport. They keep the existing controls and
disable sidebar routing while modal. The session remains active throughout a
long animation and releases TOP when stable field gameplay returns. Ordinary
Summary, storage and tutorial ownership remains separate.

Cave darkness and Flash now snapshot all 160 WIN0H HBlank rows and partition the
native 400x240 field by the complete mask. The circle uses normalized 120,80
coordinates; it no longer treats the first scanline as a static screen-wide
rectangle or combines a native center with saturated 8-bit bounds.
Window-cache invalidation includes changed/released scanline snapshots.
Disabled OBJ windows cannot override the cave mask.
At full brightness the Flash task restores full WIN0H/WIN0V bounds, so the last
circular window cannot leave a permanent clipped field region.

CB2_ChangeMapMain owns a strict authored 240x160 viewport during the cave
preview/transition. Field loading then restores the native viewport.
On 3DS the preview name uses the upstream BUGFIX branch's three-element color
buffer, avoiding the zero-length local array's stack overwrite.

## Local checks (9 October 2026)

- New production-function synthetic tests: 3 passed. They cover callback
  ownership, a 3,000-frame child session and field return, disabled sidebar
  controls, changing scanline widths, mask/cache release, inactive OBJ masks,
  full-light bounds, and isolated cave/field geometry.
- Storage controller/lifecycle tests: 4 passed.
- PokeTouch renderer: 1 passed.
- Oak presentation: 2 passed.
- First-battle controller/portrait tests: 3 passed.
- REX pipeline: 3 passed.
- CLEAN_RELEASE=1 verify-game-3dsx: passed, including Azahar loader-format
  relocation simulation and ARM11 m4a checks.
- The candidate's native source files and every new recipe output were compared
  byte-for-byte against the committed source/recipe hashes: passed.
- Structural binary audit: zero embedded/unknown game payload bytes and zero
  game payload files in RomFS.
- Source/history audit uses the unchanged production checks with cached immutable
  Git blobs; final results are reported alongside this change.

Candidate source: 555e172ce2e0716aaa3de592d432a688d848367e (clean build identity).
The following documentation commit does not change the runtime.
Candidate size: 3766680 bytes.
SHA-256: 0417112ca4a0d2ae0479333c5008c8c53933ded115ad8ee8e8403f5d765cea54

No ROM, pack, save or binary is committed or uploaded. The test candidate remains
local. No new emulator or hardware gameplay run was performed: the fixes were
validated with synthetic production-function tests, source/recipe consistency,
compilation and structural binary checks. Visual/gameplay acceptance still needs
the user to repeat the recorded sequence with their locally generated pack.
