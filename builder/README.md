# Twin Embers data Builder

User-owned ROM reconstruction, offline unless you tick the optional HeartGold
battle sprites (below). Python 3.11+ is required. The GUI uses only Python's
standard library: no SDK, online account, telemetry or game content is included.

## Optional HeartGold battle sprites

Ticking "Also download HeartGold battle sprites" (or `build --gen4-sprites` on
the command line) adds Generation 4 battle sprites to the pack for the 386
species FireRed has. They are not in your FireRed ROM and never ship with Twin
Embers: the Builder downloads the PNG files from
[pret/pokeheartgold](https://github.com/pret/pokeheartgold) at the commit that
`firered3ds_builder/gen4_sources.py` pins, keeps each file only if its size and
SHA-256 match that list, converts it and adds it to the pack. Nothing is
uploaded. Files already downloaded are kept in
`%LOCALAPPDATA%\TwinEmbers\gen4` for the next build.

The entries are optional: the engine ABI does not cover them, and a species
whose files could not be had (no network, a file that did not match) keeps
FireRed's pictures. The sprites remain Nintendo, Game Freak and Creatures'
work; pret's repository has no licence of its own.

## Graphical interface (Windows)

From the `builder` folder, use either:

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

The Windows `TwinEmbersBuilder.exe` is built by CI with PyInstaller (see
`.github/workflows/private-windows-gui.yml`):

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
needs no administrator rights; its only download is the optional sprites above. No EXE is published yet.
Game-binary publication remains deliberately BLOCKED by
`tools/source_audit.py --release`; the Builder does not change that policy.
