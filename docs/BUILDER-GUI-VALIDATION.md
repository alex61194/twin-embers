# Windows GUI builder candidate — source-only

## Remove the redundant Verify ROM action

Continuation from GUI HEAD `8a0580941ceaaa0d0d470c9bedce0b37dc73954e` on
`feature/builder-firered-ui`, in isolated branch `feature/builder-no-verify-rom`.
The owner requested removal of the standalone Verify ROM button from the
Windows application. Its button, Alt+V shortcut and unused GUI-only verify
worker/result path are removed. The ROM entry uses the available row width.

Build Data Pack still calls the unchanged gui_service.build_private_pack,
which checks the source ROM size/SHA-1 before reconstructing or writing a pack.
CLI verify-rom, standalone service verification, overwrite consent, safe
staging, cancellation, idle progress visibility and pack/ABI verification are
retained. No game code, ROM, `.pak`, save or previous branch is changed.

Nine simulated-window tests and fifteen synthetic GUI-service tests pass.
The Windows packaging workflow also runs the actual native Tk layout test
and validates the frozen executable with --self-test. Its final artifact,
source SHA, checksum and CI result accompany the private EXE delivery.

Base: `6207d2131cae6b877555aef9ccc960aca1f00d95` on
`feature/storage-bottom-touch`. Branch: `feature/builder-windows-gui`.
This GUI does not change the game/PC/audio code, ROM requirements, data recipe
or ABI `8f71cf3a`. No public release is approved.

## UI workflow

- Own ROM file selector and strict local verification (16 MiB and SHA-1).
- Destination file selector or 3DS SD root folder selection.
- Explicit Yes confirmation before any replacement of an existing pack.
- Background worker: stages report real operations, not fabricated percentages.
- Cooperative Cancel after stage boundaries, no live interruption of the
  established reconstruction engine; user is told to wait.
- Atomic staging of a fully verified pack within the destination filesystem.
  Existing files are preserved on failed builds. Newly created outputs use an
  exclusive file handle, then flush to disk; partial writes are removed.
- No external processes, network calls, credentials, telemetrics, SDK or assets.
  UI is authored entirely from Tkinter/ttk and native text.
- Uses existing `firered3ds_builder.build`, `rom`, `PakReader` and recipe
  loader. The CLI and pack reader/writer are unchanged.

## Validation and limits

`python -m unittest discover -s builder/tests -v` includes synthetic
GUI-service tests: output confirmation, non-destructive failure, rollback of
incomplete new output, rejection of wrong ROM and symlinks, cancellation,
explicit SD directory creation, and verified synthetic pack.

The initial GUI candidate is source-only. No Windows screenshot or packaged
EXE has been validated in this repository's Linux CI; tests cover the headless
service and existing builder, not OS-native Tk rendering. A Windows desktop
smoke test and permission/SD removal acceptance remain necessary before a
portable executable is offered. The builder still recognizes only the exact
SHA-1 documented in README. Do not distribute ROM, pack, game or unreviewed
runtime binaries. Binary game release stays blocked independently.

## Private Windows-only GUI executable test

A separate workflow can compile `TwinEmbersBuilder.exe` on an actual GitHub
Windows runner using Python 3.11, Tcl/Tk and PyInstaller. Its private workflow
artifact is provided to the repository owner for **testing**, not as a game
release. The frozen program uses `--self-test` in CI to load the full embedded
partitioned structural recipe and verify 6,734 entries / 12,211 REX units /
ABI `8f71cf3a`, without opening any ROM or Tk window.

The frozen application contains the original builder and its data-free recipe,
Python/Tcl/Tk runtime, PyInstaller bootloader and platform DLLs. It intentionally
contains **no ROM, Pokémon art, game executable, save, or .pak**. Legal attribution
for the bundler/runtime must accompany a future publicly distributed installer.
The private source-only repository policy and game-binary release block remain
unchanged. Windows interactive smoke tests (file selectors, SD write/overwrites,
high DPI and antivirus false positives) are still the owner's responsibility.

## Replacement rollback safeguard

An explicitly approved existing `.pak` is now copied to a private same-volume
backup before replacement. The generated pack is installed only after internal
validation, then the installed bytes are re-hashed. If a post-install check
fails, the old file is restored automatically. If restoration itself fails,
the previous bytes remain in a named backup alongside the output and the user
is instructed to recover them manually. New files are still created exclusively
and removed if a copy fails. Tests simulate a damaged installed pack and verify
that rollback restores the previous bytes without leaving temporary files.

A successful CLI-only build and synthetic checks do not substitute for a
Windows GUI smoke test with an actual removable SD card.

## FireRed-inspired GUI refresh

Based on verified builder branch HEAD `c2b749a84bc6fb162c68f9adcca8f0f84b2e3175`.
Isolated branch: `feature/builder-firered-ui`. The interface remains in English.
Original text and Tk/ttk widgets provide a red header, amber accents, cream
cards, large buttons, focus borders and explicit operation/result badges.
No proprietary artwork, fonts, icons or other game assets are added.

The indeterminate progress bar is unmapped at startup and occupies no layout
space until Build starts. Every terminal queue event stops and
unmaps it: build success, cancellation or failure. Cancellation
keeps the animation until the worker actually finishes safely. Inputs remain
disabled during work; Alt+B builds and Escape requests cancel.
The ROM entry receives initial focus and normal Tab navigation is preserved.
Status and checksum details wrap to the actual card width.

The word "Private" is removed from interface labels and dialogs. Local-only
processing, no-game-files notices, repository access, artifact permissions and
the game-binary publication block are preserved. No backend recipe, ROM hash,
CLI, pack format, game implementation or file-safety policy changes.

`test_gui_window.py` runs the actual GUI class with simulated Tk widgets on
hosts without a display. It covers startup visibility, build startup,
every terminal event, cancellation, safe closing, disabled shortcuts, input
validation and visible string checks. Windows also runs a native Tk smoke test
for layout, initial/working/finished progress visibility and entry focus.
The Windows workflow runs these plus the existing synthetic file-safety tests,
then validates the frozen EXE with `--self-test`. SHA256SUMS.txt accompanies the
seven-day owner testing artifact. Real-ROM/removable-SD and high-DPI acceptance
still require the user's hardware; CI uses synthetic inputs only.
