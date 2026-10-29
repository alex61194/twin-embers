# Web Builder and 3DS install codes

`web/` is a static page, published with GitHub Pages at <https://alex61194.github.io/twin-embers/>. It builds `twinembers.pak` in the player's browser and offers QR codes to install the game from the console. There is no server: Pages only hosts files.

## How the pack is built

- `web/worker.js` loads Pyodide 0.28.3 (CPython compiled to WebAssembly) from cdn.jsdelivr.net. The version is pinned and `pyodide.js` is checked against its SHA-384 before it runs.
- It then fetches the Builder modules and the recipe from the site, each checked against `builder/files.json`, and imports them as the desktop Builder does. `tools/build_web.py` copies them from `builder/firered3ds_builder/` when the site is assembled, so the page always runs the code in this repository; nothing is rewritten for the web.
- `firered3ds_builder/browser.py` is the entry point: it verifies the ROM, reconstructs every entry from the recipe, writes and re-reads the pack with `build.build`, the same calls the CLI makes. The page offers the result as a download.
- The ROM is read with the browser's file picker into the worker's memory. Nothing is uploaded or stored; closing the tab discards it. The page also checks the size and SHA-1 at once so a wrong file is reported before Pyodide starts working.
- **HeartGold battle sprites** (optional switch): the worker fetches the files `gen4_sources.py` lists from raw.githubusercontent.com at the pinned commit, eight at a time, into a folder in Pyodide's memory. `gen4_sprites.pack_files` then keeps each file only if its size and SHA-256 match, exactly as for the desktop Builder's offline folder. Pyodide cannot start threads, so the page runs it with `threads=0`, which does the same work on the calling thread. A file that fails leaves its species on FireRed's pictures.

The result is the same pack, byte for byte, as `firered3ds-builder build [--gen4-sprites]` with the same ROM. `builder/tests/test_browser.py` checks that on synthetic data, and against the real recipe when `FIRERED_ROM_REFERENCE` names the supported ROM.

## Install codes

- **Universal-Updater**: `web/twinembers.unistore` (UniStore version 4). Its `twinembers.3dsx` script creates `sdmc:/3ds/twinembers/` and downloads `releases/latest/download/twinembers.3dsx` into it; its `HOME Menu forwarder (CIA)` script downloads `TwinEmbers-Forwarder.cia`, installs it and deletes the download. `tools/build_web.py` fills in the release tag and date, and sets the store revision to the date (YYYYMMDD) so Universal-Updater refreshes the store after a release. `validate_unistore` checks what Universal-Updater itself requires (`storeInfo.version` 3 or 4, an integer `revision`, a bare `file` name, the fields each script action needs).
- **FBI**: the QR code holds `https://github.com/alex61194/twin-embers/releases/latest/download/TwinEmbers-Forwarder.cia` (Remote Install → Scan QR Code). It only installs the forwarder; the `.3dsx` still goes to `SD:/3ds/twinembers/`.

Both point at `releases/latest`, which never includes pre-releases, so the release assets must keep the names `twinembers.3dsx` and `TwinEmbers-Forwarder.cia`. The QR codes are drawn by the page with qrcode-generator 1.4.4 (MIT, Kazuhiko Arase) from cdnjs, loaded with a Subresource Integrity hash.

## Trying it locally

```bash
python tools/build_web.py --out build/web
python -m http.server 8000 --bind 127.0.0.1 --directory build/web
```

Open <http://127.0.0.1:8000/>. The page needs `localhost`/`127.0.0.1` or HTTPS (the browser's `crypto.subtle`). `build/` is ignored by Git; never put a ROM or a pack in the site folder.

## Publishing (maintainer)

The workflow `.github/workflows/pages.yml` runs the web tests, assembles the site and deploys it.

1. Once: in the repository's **Settings → Pages**, set **Source** to **GitHub Actions**.
2. It runs by itself whenever a release is **published** (not for pre-releases), from that release's tag, so the page always builds the pack for the `.3dsx` that `releases/latest` serves.
3. To publish without a release, run **Actions → Web Builder (GitHub Pages) → Run workflow** and choose the branch or tag. Use the tag of the latest release unless you are testing: a page built from another branch may make a pack whose engine ABI does not match the released `.3dsx`.

The UniStore URL (`https://alex61194.github.io/twin-embers/twinembers.unistore`) is written into the store itself; change `web/twinembers.unistore` if the site ever moves.
