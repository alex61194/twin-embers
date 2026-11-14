# Twin Embers data Builder

User-owned ROM reconstruction, offline unless you tick one of the three optional
downloads (below): HeartGold sprites, new people on the map and new battle
backgrounds, each on its own. Python 3.11+ is required. The GUI uses only Python's
standard library: no SDK, online account, telemetry or game content is included.

## Optional HeartGold battle sprites

Ticking "HeartGold sprites and icons" (or
`build --gen4-sprites` on the command line) adds Generation 4 battle sprites
and menu icons to the pack for the 386 species FireRed has (the icons only when
every one of them could be had, since their palettes replace FireRed's). They
are not in your FireRed ROM and never ship with Twin Embers: the Builder downloads the PNG files from
[pret/pokeheartgold](https://github.com/pret/pokeheartgold) at the commit that
`firered3ds_builder/gen4_sources.py` pins, keeps each file only if its size and
SHA-256 match that list, converts it and adds it to the pack. Nothing is
uploaded. Files already downloaded are kept in
`%LOCALAPPDATA%\TwinEmbers\gen4` for the next build.

Deoxys (386) is the exception to "the species' own file": HeartGold's `pokegra/0386` is the Normal Forme, but FireRed
and LeafGreen's Deoxys is the Attack Forme, so entry 386 is built from `otherpoke/deoxys/attack` (front: the normal
palette; back: the shiny one, as for every species). If those two files cannot be had, 386 is reported as missing
and the game keeps FireRed's own pictures.

The same download brings HeartGold's trainer front pictures (one archive,
`files/a/0/5/8`, also pinned by size and SHA-256): the trainers you battle show
HeartGold's picture of their class, 71 of FireRed's trainer pictures in all
(`TRAINER_CLASSES` in `gen4_sprites.py`), your rival included. Trainers
HeartGold has no picture of (Lorelei, Agatha, Professor Oak, Leaf and a few
classes) keep FireRed's.

HeartGold has no back of Red or Leaf, so your own back in battle comes from
fan-made HeartGold-style strips: two PNGs (80x80, five frames, FireRed's
throwing animation) from
[LucianoNeo/gen1recomp-mods](https://github.com/LucianoNeo/gen1recomp-mods) at
a pinned commit, checked by size and SHA-256 like the rest (`BACK_FILES` in
`gen4_sprites.py`). The Builder enlarges each frame 1.25 times as pixel art
(Scale3x, then every 2.4th pixel: 100x100, no new colour), so your back stands
a little larger than the Pokémon while every pixel stays one screen pixel.
Without them you keep FireRed's back.

The entries are optional: the engine ABI does not cover them, and a species
whose files could not be had (no network, a file that did not match) keeps
FireRed's pictures. The sprites remain Nintendo, Game Freak and Creatures'
work; pret's repository has no licence of its own.

## Optional new people on the map

Ticking "New people on the map" (or `build --npc-sprites`) adds 74 characters redrawn in a DS style
(32x32, nine frames in FireRed's order) from
[ElvieBlooms/facets](https://github.com/ElvieBlooms/facets) at a pinned commit: the townsfolk, trainers,
gym leaders, Team Rocket, nurses, Professor Oak, Bill, Mr. Fuji... The Builder takes the community set
(KyleDove, Jext and Hyo) where it has a person, then DiegoWT's Black/White-style one, Taiga's ORAS-style one
and PurpleZaffre's, converts each sheet to one palette of 15 colours (the rarest fold into their nearest, which
changes about one pixel in a hundred) and writes `graphics/gen4/npc/NNN.bin` for each of FireRed's `OBJ_EVENT_GFX_*`.
The rival and everything that is not a person keep FireRed's pictures; Red and Leaf have an option of their own
(below). A sheet that could not be had
(no network, a file that did not match its size and SHA-256) leaves that person as FireRed draws them.

## Optional new battle backgrounds

Ticking "New battle backgrounds" (or `build --battle-backgrounds`) replaces FireRed's 20 battle settings with 14
hand-drawn 240x112 scenes (grass, forest, beach, underwater, sea, lake, mountain, cave, lab, route, indoors,
neutral, gym and space) of LibertyTwins' Battle Backgrounds Patch FR, as converted by Facets (the same repository
and pinned commit as the people). The Builder turns each into tiles, a tile map and a palette
(`graphics/gen4/bg/NNN.bin`) and gives every terrain its scene (the gym, its leaders and the Elite Four share the
gym's). The 3DS shows more than 240x112 of the scene, so the tile map mirrors the picture around its edges with
flipped tiles instead of stretching its last row and columns. It writes all of them or none, so a battle never mixes
two looks.

## Ash and Green

There is no switch for them any more: tick "More characters to be" and choose Ash, Green and the other looks of Red, Leaf and the
rival in the game's OPTIONS (RED LOOK, LEAF LOOK, RIVAL LOOK). The old `build --player-sprites` flag is gone; the group's code
(`GROUP_PLAYER`, `PLAYER_FILES`) stays in `gen4_sprites.py` for its tests but no entry point asks for it.

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

The supported ROMs are exactly 16 MiB: FireRed (USA) revision 0, SHA-1
`41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc`, for `twinembers.3dsx`, and
Edicion Rojo Fuego (Spain) revision 0, SHA-1
`ab8f6bfe0ccdaf41188cd015c8c74c314d02296a`, for the Spanish game
`twinembers-es.3dsx`. The Builder picks the recipe for the ROM it is given.
Rev 1, other languages and modified ROMs are not supported. This is a **data pack** builder and
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
