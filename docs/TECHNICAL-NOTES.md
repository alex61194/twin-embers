# Technical notes

Details moved out of the README. For installation, start with the
[README](../README.md).

## Data pack

The Builder reconstructs `twinembers.pak` from your own ROM. The reference recipe
describes 6,734 files and 12,211 REX units using only ROM copies, zero fills, record
and bit-field reconstruction. Its partitions contain offsets, lengths, identifiers
and CRCs, never game bytes. The pack header records the engine ABI (`8f71cf3a` for
this version) and the ROM's SHA-1. The game refuses a pack whose ABI or checksums do
not match.

Command line, from the `builder` folder:

```sh
python -m firered3ds_builder verify-rom /path/to/your-own.gba
python -m firered3ds_builder build --rom /path/to/your-own.gba --output /private/twinembers.pak
python -m firered3ds_builder verify-pak /private/twinembers.pak --abi 8f71cf3a
```

The GUI builds into a temporary folder on the destination drive and verifies every
record before installing. It writes a new pack with exclusive creation. It replaces
an existing pack only after an explicit **Yes**, keeping the previous file until the
new one is verified on the destination. It needs at least 32 MiB free, and it never
touches saves or the game executable.

## Saves and migration

The game uses `/3ds/twinembers/twinembers.sav`, a standard 128 KiB FireRed flash
image. On a fresh card it creates a blank (erased) image. If no save exists in the
new folder, the first existing source is copied once:

1. `/3ds/pokefirered/twinembers.sav`
2. `/3ds/pokefirered/firered.sav`

The game handles saves as follows:

- An existing destination always wins, and the original files are never modified.
- Copy, read, flush, close and permission errors stop startup with a save error.
  They never fall through to a new game or an older fallback.
- Invalid save sizes also stop startup.
- A save is reported as written only after the SD card confirms the write.
- The save layout is FireRed's own, including two reserved option bits.
- This is a byte-preserving path migration, not a converter for other emulators'
  save formats.

Logs (`port.log`, enabled by an empty `debug.txt`) and other diagnostic switches use
the same folder. Packs from older builds are not migrated; build a new one.

## How the executable is built

`tools/bootstrap.py` downloads `pret/pokefirered` at the commit pinned in
`upstream.lock` and applies the context-free edit recipes in
`patches/pokefirered/`. `make … verify-game-3dsx` then compiles and links the 3DSX
with devkitARM, libctru, citro2d and citro3d.

The resulting executable has these properties:

- Game data sections are NOBITS and are filled from the pack by REX before the game
  starts.
- The RomFS contains only the engine ABI, the asset index, REX metadata and
  clean-profile metadata.
- The intro uses the pack-backed original assets.
- The lower-screen font comes from the pack, and the icons are original geometry.
- The upstream m4a sound engine is compiled locally from the decompilation.

## Source checks

```sh
python tools/source_audit.py --history
python -m unittest discover -s builder/tests -v
python -m unittest discover -s tools -p test_source_audit.py -v
python -m unittest discover -s tools -p test_audit_clean_release.py -v
python -m unittest discover -s tools -p test_rex_pipeline.py -v
```

CI uses synthetic fixtures only. Own-ROM tests are enabled locally with
`FIRERED_TEST_ROM`. CI has no ROM secrets and uploads no game binaries. Tests that
need a bootstrapped tree take `--tree build/upstream`, for example
`python tools/test_save_prompt.py --tree build/upstream`.

`tools/source_audit.py --release` is the publication gate. It reports
`BINARY RELEASE BLOCKED` unless `docs/release-decision.json` records the owner's decision for the exact
binaries; the decision never claims that third-party rights are verified. See
[DISTRIBUTION.md](DISTRIBUTION.md) and [VALIDATION.md](VALIDATION.md).

## Provenance

This repository has its own root commit. An earlier, private distribution repository
supplied legal notices and credits only. `pret/pokefirered` stays an external,
commit-pinned dependency and is not vendored. See [PROVENANCE.md](../PROVENANCE.md).
