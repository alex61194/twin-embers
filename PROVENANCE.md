# Provenance

Technical source: `alex61194/pokemon-firered-3ds-clean` at
`96f95e015af9f5f38e4a8aefca7cab29a78b030d`, inspected read-only through pinned
file/blob requests. Selected source bytes were checked against their Git blob
hashes before adaptation. No laboratory checkout, Git objects or ancestry entered
the distribution repository.

Legal notices, collective credits and license text were retained from
`alex61194/pokemon-firered-3ds-port` at
`4c2f244c1881fae2f7ba2629d53aec27a95ee56a`. None of that repository's technical
code, tests, workflows or history was adopted. Common code inherited by the
technical reference is credited according to its actual lineage; a fresh history
does not erase authorship.

The source manifest records every distributed file's SHA-256, source repository,
pinned commit/blob when relevant, authorship scope and adaptation status.
`docs/import-exclusions.json` records excluded media, generated files, derivative
components and unrelated laboratory tooling. Automatic scans supplement review;
they do not independently establish legal clearance.

## What belongs to the port, the engine and the player

| Component | Origin | License / rights scope | Repository | Local binary candidate |
|---|---|---|---|---|
| Original Twin Embers platform code, tooling and documentation | alex61194 | MIT for owned contributions only, `LICENSE-PORT.md` | Reviewed source | Port code compiled into the 3DSX; Builder packaged separately |
| Inherited or adapted dual-screen port portions | ZallaxDev and Pokemon Emerald 3Ds Dual Screen contributors; revisions in `NOTICE.md` | Original MIT scope and notices in `licenses/` | Reviewed inherited/adapted source | Compiled or packaged where used, retaining attribution |
| FireRed game engine and interpreters, including upstream sound logic | `pret/pokefirered@037335f4c725d7c9aecdac87066f2002b4bd7e14` | No general redistribution license identified; not relicensed by the port | Fetched externally; context-free port edits only | **Compiled game logic is included in `twinembers.3dsx`** |
| FireRed graphics, fonts, maps, text, music, samples, scripts and game tables | Player's supported ROM | Original game rights remain with their respective rightsholders | No ROM or extracted payloads | Reconstructed locally as `twinembers.pak`; excluded from the clean executable's stored game payloads |
| ABI, asset index, REX relocation metadata and clean-profile marker | Build-generated structural metadata | Classification does not grant rights in game code or content | Reconstruction instructions and tooling | Engine-only RomFS metadata in the 3DSX |
| Windows Builder and its runtime | Port code; Python, Tcl/Tk, PyInstaller and other bundled dependencies | Port MIT scopes plus each dependency's own terms and exact bundle notices | Builder source; runtime fetched separately | Separate `TwinEmbersBuilder.exe`; no embedded 3DSX, ROM or pre-generated pack |
| Linked 3DS libraries and toolchain runtime | devkitPro and respective dependency authors | Component-specific terms; not covered by the port MIT grant | External dependencies | Linked into the 3DSX where used; their notices must accompany distribution as required |

No prebuilt game executable is currently published by this repository. A local
candidate's technical audits establish its payload separation, not copyright
authorization. In particular, "no ROM or game data pack is distributed" must
not be read as "no original game logic is present in the executable."

## Zallax distribution precedent

The comparison uses release `v0.3.0`, commit
`88b7dda3cf750f3bffae9b23c89789d71e4a01de`, rather than an unspecified latest tree:

- [LICENSE-PORT.md](https://github.com/ZallaxDev/pokeemerald-3Ds-dualscreen/blob/88b7dda3cf750f3bffae9b23c89789d71e4a01de/LICENSE-PORT.md)
  grants MIT rights for original port work and excludes the game and decompilation.
- [docs/PROVENANCE.md](https://github.com/ZallaxDev/pokeemerald-3Ds-dualscreen/blob/88b7dda3cf750f3bffae9b23c89789d71e4a01de/docs/PROVENANCE.md)
  explicitly identifies compiled pret game logic in the 3DSX and states that the
  upstream has no license stated.
- [3ds_port/full.mk](https://github.com/ZallaxDev/pokeemerald-3Ds-dualscreen/blob/88b7dda3cf750f3bffae9b23c89789d71e4a01de/3ds_port/full.mk)
  compiles upstream game translation units;
  [3ds_port/Makefile](https://github.com/ZallaxDev/pokeemerald-3Ds-dualscreen/blob/88b7dda3cf750f3bffae9b23c89789d71e4a01de/3ds_port/Makefile)
  packages its release executable with engine-only RomFS.
- The [published release](https://github.com/ZallaxDev/pokeemerald-3Ds-dualscreen/releases/tag/v0.3.0)
  provides standalone 3DSX files and a Windows Builder package. Its Windows
  package also carries the 3DSX in its payload; Twin Embers keeps the Builder
  and game executable separate.

Both projects therefore compile pret game logic and externalize game resources
for reconstruction from the player's ROM. The compared Zallax license,
provenance and release documents do not establish permission from the game
rightsholders or a legal exception authorizing that engine distribution.
No additional clearance unavailable to Twin Embers was identified in those
materials; undisclosed permissions cannot be ruled out. Zallax's publication
does not grant Twin Embers permission, and the original FireRed and Emerald
games remain distinct copyrighted works.

The maintainer approves adopting this technical arrangement. That project
approval is distinct from authorization by the relevant game rightsholders.
The precise unresolved issue is reproduction, adaptation and distribution of
the compiled decompiled game logic. Resolving it requires a rights grant that
covers that code and binary distribution, or a qualified legal assessment of
an applicable exception in the relevant jurisdictions. Attribution, ownership
of a cartridge and the port's MIT grant do not establish that clearance.

## External source and edit instructions

`pret/pokefirered` stays external at
`037335f4c725d7c9aecdac87066f2002b4bd7e14`. The reference's 94 patches are
represented as context-free JSON edit recipes. Local line/span references supply
existing upstream lines; only added port lines are stored. Complete before/after
hashes guard each step, including new files and deletions. Recipes are preflighted
in memory before applying changes. One further recipe adds an original font
accessor; it carries no glyph pixels or widths.

This format avoids distributing upstream patch context. It does not relicense
the private source tree produced locally or resolve the rights in compiled game
logic. The actual original patches remain outside this repository.

## Reconstruction recipe

The reference recipe is preserved semantically: 6,734 entries, 12,211 units,
599 engine-exception records (the 336-byte game-derived group was externalized). A small index names SHA-256-verified partitions;
the loader reassembles the original lists without reordering. All game entries
use C/F/R/B operations (ROM ranges, zero fills, record strides, bit-field remaps); there is no literal pool.
Compiler and port exceptions are metadata, not an automatic binary approval.

## Explicit adaptations

- The original 3DS entropy adapter and controlled synthetic tests are owned
  port work by alex61194 with disclosed AI assistance, MIT. The hash-pinned
  recipe changes only FireRed's shared entropy read, preserving its two
  sampling boundaries, RNG mechanics and saves. External game source is read
  only from the ignored pinned tree. See docs/RNG-INITIALIZATION.md.

- Re-authored intro images, their manifests and bitmap renderer are excluded.
  A small asset-free fallback keeps the original intro supplied by the pack.
- The generated lower-screen font header is excluded. Glyphs and widths are
  accessed from the already externalized font/table at runtime. The ASCII bridge
  contains encoding logic, not font artwork. Neutral icons are generated in RAM
  from original rectangular geometry.
- The C rewrite of upstream's DirectSound mixer is excluded because the
  reference describes it as a translation of game assembly. The sound worker
  calls the external upstream implementation instead; no mixer source is
  relicensed here.
- Include-only port algorithm fragments use `.h` instead of `.inc`; algorithms
  retain their original credit. Bootstrap, edit application, source audit and
  governance are new distribution work.
- The clean build cannot embed ROM payloads. A mandatory ROM-assisted binary
  check and a separate always-closed publication policy protect the release path.

No results measured on the old laboratory binary are claimed for this adapted
tree. The new binary needs its own coverage, leak scan, linked-license inventory
and hardware acceptance before release can be considered.
