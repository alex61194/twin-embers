# Fly and field invocation presentation validation

Historical initial validation: see [follow-up](FLY-FOLLOWUP-VALIDATION.md) for
the remaining 256px BG walker cap and Fly handoff defects found in the next
recording. The original strip-placement fixture did not execute DrawTextBg.

## Recording and correction

The supplied 8.53-second recording shows the invoked Pokemon crossing at the
old 120,80 point, without its streak strip, and the Fly bird appearing through
the bottom edge on arrival.

Field-move invocation now shares a native centre of 200,120 with its 80px-high
streak strip. BG0 keeps its pixel scale, repeats across all 400px of the screen
and uses the same vertical window as the Pokemon (80..160). The opening,
closing, indoor, outdoor and full-field restored bounds retain their roles.
The invocation starts fully beyond the right edge and finishes beyond the left.

Fly bird trajectories now use the screen width for their horizontal radius.
The bird and the carried player are translated together from authored field
effect coordinates. Negative Y and offscreen positive X retain their exact
signed positions instead of being decoded as wrapped native-world actors.

The sprite builder captures positions after sprite sorting, including emitted
subsprites. Pending positions are published only when the matching OAM buffer
is transferred. Raw attribute matching protects against stale entries.
Only invocation actors, Fly callbacks and the carried player are classified.
NPCs, ordinary world sprites, other scenes and the dismounted player keep their
existing coordinate path. No map camera, save layout or game artwork was changed.

## Local verification (9 October 2026)

- Fly presentation: 5 production-function synthetic tests passed. They cover
  matching-frame publication, signed offscreen coordinates, actual renderer
  selection, actor isolation, dismount cleanup, subsprites, strip/mon centring,
  and viewport-width-aware trajectory expressions.
- Field/Flash/child-session presentation: 3 tests passed, including new invocation
  window checks and previous full-field restoration checks.
- Storage controller/lifecycle: 4 tests passed.
- Oak presentation: 2 tests passed.
- First-battle controller/portrait: 3 tests passed.
- REX pipeline: 3 tests passed.
- Full local CLEAN_RELEASE=1 verify-game-3dsx rebuild: passed.
- Azahar loader-format relocation simulation and ARM11 m4a checks: passed.
- Native source/Makefile and the two new recipe outputs match the candidate
  byte-for-byte.
- Structural binary audit: zero embedded/unknown game payload bytes and zero
  game payload files in RomFS.
- Source/history audit uses the unchanged production checks with cached immutable
  Git blobs; final results are reported alongside this change.

Candidate code identity: 595e130139c2d264612775ec3f70d4d92422ac42, clean.
The subsequent commit adds test coverage and this report only.
Candidate bytes: 3768120
SHA-256: 1be0b514a3e0721e968628288991ba264da8f03ec2b58fc6dd0d002043641c2d

This is a local test candidate, not a release. No ROM, pack, save or binary is
committed or uploaded. A new emulator/hardware gameplay run was not performed:
repeat the recorded flight with the user's own locally generated pack to confirm
the visual result. The binary was validated through compilation, structural
checks and synthetic execution of its presentation code.
