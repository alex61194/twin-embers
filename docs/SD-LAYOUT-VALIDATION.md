# Twin Embers SD layout validation

Base branch: fix/overworld-sprite-y-wrap at
3a23649963aed908bf7a5969ff0c0d0b4e5ac99d (latest branch tip verified).
All work is on fix/twinembers-sd-layout.

## Behavior

The install directory is /3ds/twinembers/. The game image, pack and save are
twinembers.3dsx, twinembers.pak and twinembers.sav respectively.
Data loading, development data, diagnostics and port.log use that directory.
Builder GUI SD selection, file dialogs, CLI defaults, private pack staging and
the normal game Makefile output agree with it. Format magic, ABI, Python package
names, source dependency paths and the development marker remain compatible.

An existing new save always wins. Otherwise migration prefers
/3ds/pokefirered/twinembers.sav over /3ds/pokefirered/firered.sav.
Both sources remain untouched. Exclusive destination creation prevents
overwriting a file that appears during migration.
Read, size, access, write, flush and close failures return false and stop startup
with an error. Only absence of all saves permits fresh-save creation.
Interrupted or failed copies may require manual removal of an invalid new file
if cleanup itself cannot complete; the original remains available.

Automatic ROM validation and the removal of the Verify ROM GUI button and
shortcut are preserved.

## Local verification (9 October 2026)

- Builder: 76 tests, 74 passed; 2 own-ROM tests skipped because no ROM was supplied.
  This includes actual GUI-method tests for SD selection and ROM-side defaults,
  existing automatic ROM-validation tests, no-button tests and native Tk checks.
- Production flash test compiled against pinned upstream headers: passed.
- Synthetic production migration harness: passed. Covers source priority,
  fallback, destination preservation, fresh saves, truncated/oversized sources,
  inaccessible paths, read/error-state/write/flush/source-close/destination-close
  failures, and a destination-created race.
- Runtime data tests using GNU stdio: passed, including renamed pack paths,
  missing/corrupt packs, streaming, CRC checks and clean-profile restrictions.
- Source audit tests: 13 passed; binary audit rejection tests: 14 passed;
  REX pipeline tests: 3 passed.
- make verify-host: all 19 host test programs passed.
- Full production source/history audit: 322 reviewed files and 67 reachable
  commits passed before this validation-only commit. Immutable Git blob reads
  were cached to avoid Windows per-process overhead; production check/history
  functions and every snapshot's manifest were used unchanged.
- CLEAN_RELEASE=1 verify-game-3dsx: passed with devkitPro/devkitARM, libctru,
  Citro2D/Citro3D and the pinned external upstream. The first attempt lacked
  libpng on the host-tool search path; the already installed MinGW libpng was
  used to build the local gbagfx tool, then compilation succeeded.
- Azahar loader-format/relocation simulation and ARM11 m4a checks: passed.
- Structural binary audit: 0 embedded/unknown game payload bytes and 0 game
  payload files in RomFS. This is not a ROM-assisted leak scan or release approval.

Local candidate source identity: ac902af1cc5310791d8fc12d10cc33a32130abc0.
The next commit added tests and validation documentation only.
A later UI correction replaces FireRed 3DS / FireRed3DS Builder with twinembers
on the data-error screen; the supported Pokemon FireRed ROM reference remains.
Candidate twinembers.3dsx: 3,765,180 bytes.
SHA-256: 0c3471dd45db6c9546cd459fa1b6cd229e39bdbac1dc47fb9467076a967a29a5.

No own ROM, generated pack or real save was used in tests or uploaded.
Hardware/emulator gameplay acceptance and the ROM-assisted binary scan remain
pending. Binary publication remains blocked; the candidate is local test access
only, not a GitHub release or CI binary artifact.
