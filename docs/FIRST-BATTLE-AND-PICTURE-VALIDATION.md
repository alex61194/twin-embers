# First rival battle and field portrait: private validation

Date: 2026-10-08. Branch: `fix/field-mon-picture-and-battle-menu`.
Base: `4ddc7c948113ddf114fa266bfa87acfbbd756ad9`, retaining the Oak fix
that the user reported fixed. Executable code: `6401b6e57937a71360a1280ba168e5f9bad83168`.

## Behavior

The first rival battle uses the Oak/Old Man controller rather than the ordinary
player controller. Previously the lower UI could not identify its waiting
handlers, and this controller scrolled BG0 to the original action/move pages.
Recipe 0099 registers its live action/move readiness with the existing lower
battle UI, keeps BG0 on the message page, suppresses upper menus through the
existing mechanism, and reports pending Bag/Party transitions. Touch and physical
keys share the port's existing navigation graph. The autonomous Old Man demo
is excluded. Battle decisions, restrictions, tutorial messages, and command
completion remain in the external controller.

Recipe 0100 positions the scripted Pokemon portrait at the same horizontal
centre and bottom-160-row offset as its BG0 window. At script coordinates
(10,3), the native 64x64 picture occupies [168,232) x [112,176), exactly the
8x8-tile window interior. Its size and game resource lifecycle are unchanged.
No camera, map renderer, battle compositor, red PokeTouch art, save/Continue,
turbo, FPS overlay, intro/title or Oak presentation code was changed here.

## Verified

- Actual ARM11 compilation of both controllers and script_menu.c; full link.
- All four changed external files match a replay from pinned upstream HEAD
  through all hash-checked recipes. Upstream remains inside ignored build/.
- 42 tools tests pass, including three new controller/portrait regressions.
- Actual production bridge: first-battle action/move readiness, non-ready
  setup and submenus, exclusion of ordinary battles and automatic demo,
  pending Bag/Party holds, and all sixteen action navigation routes.
- Actual portrait creation statement compiled and executed with captured
  synthetic coordinates, including the starter window and two boundary cases.
- 3DSX loader/relocation simulation and ARM11 m4a checks pass.
- Structural ELF/map/3DSX audit with coverage: 0 embedded game payload bytes,
  0 unknown contributions and 0 game payload files in RomFS.
- User-supplied existing pack: all 6,734 payloads verified; ABI `8f71cf3a`.
  No pack or ROM modification, generation or upload was needed.
- Existing 49 builder tests (two own-ROM skips), asset/data runtime checks and
  18 native host tests passed earlier in this isolated session.

## Private executable

`TwinEmbers-laboratorio-combate-bottom.3dsx`: 3,757,500 bytes.
SHA-256: `33e3f6cf886699cd2b1a45191beeead7166de75f4d9d5ff49824271530afb55b`.
The built version header identifies `6401b6e57937`, clean working tree.
Subsequent documentation-only commits do not change executable code.
RomFS contains engine ABI, graphics index, REX metadata and clean profile only.

Use the existing compatible pack at `/3ds/twinembers/twinembers.pak` on the
3DS SD card or emulator's virtual SD card. Launch this 3DSX. The pack stays
external; do not import it into Git or a release. Publication gates remain closed.

## User acceptance pending

The user explicitly requested no Azahar automation and will test the executable.
No completed emulator gameplay or visual success is claimed for this binary.
Hardware is untested. Check the starter portrait inside its frame, closing the
picture, first rival FIGHT/BAG/POKEMON/RUN choices on BOTTOM, move selection and
B return, Bag/Party return, tutorial voiceovers, and an ordinary later battle.

The room/map cropping visible in the laboratory screenshot is **not asserted
fixed** by this portrait alignment change. The map/camera path was not modified;
confirm whether it persists in this build before a further targeted correction.
