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

If the bootstrap says the inputs changed, preserve the old workspace and choose
a fresh folder under `build/`, for example:

```sh
"$PY" tools/bootstrap.py --dir build/upstream-new
make -C build/upstream-new/3ds_port CLEAN_RELEASE=1 PYTHON="$PY" verify-game-3dsx
```

The executable is then in `build/upstream-new/3ds_port/`. Add `-j4` (or your CPU's
core count) to `make` to build faster.

## 3. Install

Build the data pack with the Twin Embers Builder from your own ROM (see the README),
then copy both files to the SD card:

    /3ds/twinembers/twinembers.3dsx
    /3ds/twinembers/twinembers.pak

Never share your ROM, data pack or saves. Building an executable does not grant
rights to distribute its compiled game code. The published binary is covered by
the recorded owner decision for its exact hash; a locally built binary may differ.
See [DISTRIBUTION.md](DISTRIBUTION.md) for the publication policy.
