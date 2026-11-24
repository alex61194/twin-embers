# Building twinembers.3dsx yourself (Windows)

A prebuilt `twinembers.3dsx` is attached to the releases. If you prefer to build the executable on your
own computer, use this source and the `pret/pokefirered` decompilation, which the bootstrap
downloads from its official GitHub repository at a pinned commit. Game data is not
compiled in: it comes from the data pack the Builder makes from your own ROM.

Allow about 3 GB of disk space and 10–20 minutes.

## 1. Install the tools

1. **devkitPro** — run the official graphical installer from
   <https://devkitpro.org/wiki/Getting_Started> and select the **3DS Development**
   component. Use the default `C:\devkitPro` folder.
2. **Python 3.11 or newer** from <https://www.python.org/downloads/windows/>.
   Note where `python.exe` is installed, for example
   `C:\Users\YOU\AppData\Local\Programs\Python\Python313\python.exe`.
3. **This source**: download or clone this repository into a folder **without
   spaces**, for example `C:\twinembers\src`.

## 2. Build

Open **devkitPro → MSYS2** from the Start menu. The devkitPro shell does not see
the Windows `PATH`, so give it the full path of `python.exe` once (write `C:\` as
`/c/` and use forward slashes):

```sh
PY=/c/Users/YOU/AppData/Local/Programs/Python/Python313/python.exe
cd /c/twinembers/src
"$PY" tools/bootstrap.py
make -C build/upstream/3ds_port CLEAN_RELEASE=1 PYTHON="$PY" verify-game-3dsx
```

The first command downloads the pinned decompilation and applies the edit recipes;
the second compiles, links and checks the executable. The result is:

    build/upstream/3ds_port/twinembers.3dsx

If the bootstrap says the inputs changed, delete the `build/upstream` folder and
run it again. Add `-j4` (or your CPU's core count) to `make` to build faster.

## Release builds (maintainers)

A public release is built from a clean checkout with the real art and the relay it is meant to use; the build stops when either is missing:

```sh
make -C build/upstream/3ds_port CLEAN_RELEASE=1 TWIN_EMBERS_VERSION=X.Y.Z PYTHON="$PY" \
     TWIN_RELAY=PUBLIC-ADDRESS:47311 REQUIRE_RELAY=1 \
     ICON_SRC=/path/to/icon48.png ICON_CROP=0 REQUIRE_ART=1 \
     FIRERED_ROM_REFERENCE=/path/to/your-rom.gba verify-clean-release
```

- `TWIN_RELAY=host:port` puts the relay of [RELAY-SERVER.md](RELAY-SERVER.md) into the build, so *Online* works with nothing set up on the console; `REQUIRE_RELAY=1` refuses to build without it (a build without a relay has nobody to reach online). Check that the relay answers (step 5 of RELAY-SERVER.md) before publishing.
- `ICON_SRC` with `REQUIRE_ART=1`: the Homebrew Launcher icon; without them the placeholder flames ship. `TWIN_LANG=es` or `de` makes the Spanish and German 3DSX (`twinembers-es.3dsx`, `twinembers-de.3dsx`).
- `verify-clean-release` ends with `CLEAN RELEASE: PASS` when the executable carries no game data. The `BINARY RELEASE BLOCKED` line that follows only reports that `docs/release-decision.json` names v0.1.0-beta.1 (see [DISTRIBUTION.md](DISTRIBUTION.md)). `python3 tools/source_audit.py` must pass; `--history` is a separate gate.
- Keep `twinembers.3dsx`, `SHA256SUMS.txt` and the notes together; the CHANGELOG's *Unreleased* section becomes the version's section.

## 3. Install

Build the data pack with the Twin Embers Builder from your own ROM (see the README),
then copy both files to the SD card:

    /3ds/twinembers/twinembers.3dsx
    /3ds/twinembers/twinembers.pak

Do not share the executable you built, the ROM or the data pack: they contain or are
derived from copyrighted game material.
