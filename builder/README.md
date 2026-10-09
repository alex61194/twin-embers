# Twin Embers data Builder

Offline, user-owned ROM reconstruction. Python 3.11+ is required. The GUI uses
only Python's standard Tkinter, ttk, threading and pathlib modules: no SDK,
online account, telemetry or game content is included.

## Graphical interface (Windows)

Download `TwinEmbersBuilder.exe` from the
[latest release](https://github.com/alex61194/twin-embers/releases/latest) and open it.
The SHA-256 hash is listed in the release notes. No Python installation is needed
for this prebuilt application.

To run from source instead, from the `builder` folder use either:

```powershell
py -3 launch_gui.py
```

or:

```powershell
py -3 -m firered3ds_builder.gui
```

After installing the package with `py -3 -m pip install .`, you can also use
`firered3ds-builder-gui`; it is registered as a Windows GUI entry point
(no command-prompt window). Installation does not bundle any ROM or pack.

Copy twinembers.3dsx into the same /3ds/twinembers/ folder; the game creates
twinembers.sav there or safely copies an existing legacy save (see the root README).

Choose your **own** supported `.gba` ROM and where to save `twinembers.pak`,
then click **Build Data Pack**. The ROM is verified automatically before
building. **SD folder…** creates the normal
`3ds/twinembers/twinembers.pak` destination below the folder you select.
Select the actual SD root, not the `3ds` subdirectory.

Build runs in a worker thread so the window stays responsive. Progress
reports actual build stages (ROM checking, reconstruction, pack verification,
local installation); reconstruction does not claim an invented percentage.
Cancellation waits for the current reconstruction step before cleaning up.
An existing pack is never overwritten without an explicit Yes confirmation.
When making a new file the destination uses exclusive creation, and failed
partial writes are removed. Existing packs are preserved on failure. Do not
remove an SD card while writing.

The supported ROM is exactly 16 MiB, SHA-1
`41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc`. Rev 1, other languages
and modified ROMs are not supported. This is a **data pack** builder and
does not create a `.3dsx` executable.

## Command line (unchanged)

```sh
cd builder
python -m firered3ds_builder verify-rom /path/to/your-own.gba
python -m firered3ds_builder build --rom /path/to/your-own.gba --output /private/twinembers.pak
python -m firered3ds_builder verify-pak /private/twinembers.pak --abi 8f71cf3a
```

The embedded structural recipe has 6,734 entries and 12,211 REX units,
as hash-verified JSON partitions; it contains no game bytes. The local ROM
and generated pack are private and MUST NOT be shared or committed.

## Tests, packaging and rights

```sh
cd builder
python -m unittest discover -s tests -v
```

GUI-service tests are synthetic; no copyrighted game data is required.

The Windows `TwinEmbersBuilder.exe` can be packaged locally with PyInstaller.
The packaging workflow `.github/workflows/private-windows-gui.yml` is disabled
in this public repository; it does not currently produce public CI artifacts:

```powershell
# from the repository root
py -3 -m pip install --no-deps ./builder
py -3 -m pip install pyinstaller==6.16.0
py -3 -m PyInstaller --noconfirm --onefile --windowed --clean --name TwinEmbersBuilder --collect-data firered3ds_builder --exclude-module _decimal --exclude-module _lzma builder/launch_gui.py
py -3 builder/package_notices.py --exe dist/TwinEmbersBuilder.exe --out dist/notices
```

`package_notices.py` classifies every file bundled in the EXE and writes the
exact-version license notices (Python, Tcl/Tk, PyInstaller and this project),
failing on any unknown file; ship those notices with the EXE.
`TwinEmbersBuilder.exe --self-test` checks the embedded recipe without a display.
The EXE does not contain or download a game executable, ROM or `.pak`, and
needs no administrator rights. The prebuilt EXE is published with `v0.1.0-beta.1`.

The game-binary publication policy is separate from Builder operation.
`tools/source_audit.py --release` validates the owner decision record; add
`--asset PATH` for each binary to check its exact SHA-256 against that record.
Without a valid record, or with a mismatched supplied binary, publication is blocked.
An owner decision does not verify third-party rights; see
[docs/DISTRIBUTION.md](../docs/DISTRIBUTION.md).
