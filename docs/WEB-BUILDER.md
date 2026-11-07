# Web Builder and 3DS install codes

`web/` is a static page in two languages (`index.html` English, `es.html` Spanish, with an EN/ES selector on both; `app.js` picks its messages by the page's `lang`), published with GitHub Pages at <https://alex61194.github.io/twin-embers/>. It builds `twinembers.pak` in the player's browser and offers QR codes to install the game from the console. There is no server: Pages only hosts files.

## How the pack is built

- `web/worker.js` loads Pyodide 0.28.3 (CPython compiled to WebAssembly) from cdn.jsdelivr.net. The version is pinned and `pyodide.js` is checked against its SHA-384 before it runs.
- It then fetches the Builder modules and the recipe from the site, each checked against `builder/files.json`, and imports them as the desktop Builder does. `tools/build_web.py` copies them from `builder/firered3ds_builder/` when the site is assembled, so the page always runs the code in this repository; nothing is rewritten for the web.
- `firered3ds_builder/browser.py` is the entry point: it verifies the ROM, reconstructs every entry from the recipe, writes and re-reads the pack with `build.build`, the same calls the CLI makes. The page offers the result as a download.
- The ROM is read with the browser's file picker into the worker's memory. Nothing is uploaded or stored; closing the tab discards it. The page also checks the size and SHA-1 at once so a wrong file is reported before Pyodide starts working. It takes FireRed (USA) and Rojo Fuego (Spain), revision 0; the Builder picks the recipe for the ROM (`firered-41cb23d8` or `firered-ab8f6bfe`), and once a Spanish ROM is chosen the page's download code and button offer `twinembers-es.3dsx`, the game its pack is for.
- **Optional downloads** (six switches: HeartGold sprites, new people on the map, new battle backgrounds, Pokémon that walk behind you, HD battle scenes, more characters to be; the page passes the groups chosen to the worker, which fetches only their files): the worker fetches the files `gen4_sources.py` lists from raw.githubusercontent.com at the pinned commit, eight at a time, into a folder in Pyodide's memory. `gen4_sprites.pack_files` then keeps each file only if its size and SHA-256 match, exactly as for the desktop Builder's offline folder. Pyodide cannot start threads, so the page runs it with `threads=0`, which does the same work on the calling thread. A file that fails leaves its species on FireRed's pictures. The trainer pictures come in one 1.6 MB archive from the same commit; if it fails, every trainer keeps FireRed's. The player's two fan-made backs come from LucianoNeo/gen1recomp-mods at its own pinned commit, the 14 battle scenes from ElvieBlooms/facets at another and the player looks from ElvieBlooms/emerald_lens at a third, each checked the same way.

The result is the same pack, byte for byte, as `firered3ds-builder build [--gen4-sprites]` with the same ROM. `builder/tests/test_browser.py` checks that on synthetic data, and against the real recipe when `FIRERED_ROM_REFERENCE` names the supported ROM.

## Install codes

- **Universal-Updater**: `web/twinembers.unistore` (UniStore version 4). Its `twinembers.3dsx` script creates `sdmc:/3ds/twinembers/` and downloads `releases/latest/download/twinembers.3dsx` into it; its `twinembers.3dsx (Spanish)` script downloads `releases/latest/download/twinembers-es.3dsx` to `sdmc:/3ds/twinembers/twinembers-es.3dsx` (the forwarder starts either name, the newer when both are there); its `HOME Menu forwarder (CIA)` script downloads `TwinEmbers-Forwarder.cia`, installs it and deletes the download. `tools/build_web.py` fills in the release tag and date, and sets the store revision to the last commit's date and hour (YYYYMMDDHH) so Universal-Updater refreshes the store after a publication, the same day too. The store's icon is `twinembers.t3x` (`storeInfo.sheet` / `sheetURL`, `icon_index` 0 in both entries): `build_web.py` writes it from `docs/media/twin-embers-icon.png`, the game's 48x48 icon, as devkitPro's `tex3ds -f rgba8 -z none` would (one image in a 64x64 RGBA8 texture), so no binary is kept in the repository. `validate_unistore` checks what Universal-Updater itself requires (`storeInfo.version` 3 or 4, an integer `revision`, a bare `file` name, the fields each script action needs).
- **FBI**: the QR code holds `https://github.com/alex61194/twin-embers/releases/latest/download/TwinEmbers-Forwarder.cia` (Remote Install → Scan QR Code). It only installs the forwarder; the `.3dsx` still goes to `SD:/3ds/twinembers/`.

Both point at `releases/latest`, which never includes pre-releases, so the release assets must keep the names `twinembers.3dsx`, `twinembers-es.3dsx` and `TwinEmbers-Forwarder.cia`. The QR codes are drawn by the page with qrcode-generator 1.4.4 (MIT, Kazuhiko Arase) from cdnjs, loaded with a Subresource Integrity hash.

## Trying it locally

```bash
python tools/build_web.py --out build/web
python -m http.server 8000 --bind 127.0.0.1 --directory build/web
```

Open <http://127.0.0.1:8000/>. The page needs `localhost`/`127.0.0.1` or HTTPS (the browser's `crypto.subtle`). `build/` is ignored by Git; never put a ROM or a pack in the site folder.

## Publishing (maintainer)

The workflow `.github/workflows/pages.yml` runs the web tests, assembles the site and deploys it.

1. Once: in the repository's **Settings → Pages**, set **Source** to **GitHub Actions**.
2. After publishing a release, run **Actions → Web Builder (GitHub Pages) → Run workflow** on `main`, with `main` at the release's commit, so the page builds the pack for the `.3dsx` that `releases/latest` serves. The `github-pages` environment only accepts deployments from `main`: the run the workflow starts by itself when a release is published (from the release's tag) builds the site but its deploy job is refused, as happened for v0.5.0.
3. A page built from a `main` ahead of the latest release may make a pack whose engine ABI does not match the released `.3dsx`; run the workflow only when `main` matches the release, unless you are testing.

The UniStore URL (`https://alex61194.github.io/twin-embers/twinembers.unistore`) is written into the store itself; change `web/twinembers.unistore` if the site ever moves.
