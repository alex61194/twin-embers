# Third-party notices

Each component retains its own terms. No license for a game, ROM or decompilation
is inferred from the port's MIT license. No executable or third-party runtime is shipped.

## Reused port code

ZallaxDev and Pokémon Emerald 3Ds Dual Screen contributors:
https://github.com/ZallaxDev/pokeemerald-3Ds-dualscreen .
The architecture/backend/bundle adaptation uses revision
`1f3812d2fe4e53e76bbd528ccb8426e880dfc76b`; data/pack adaptations use
`6419a40038055d4ebe11f3b74a13cafd34a0435c`.
Both revisions have the same LICENSE-PORT.md Git blob
`5f840bd2c5ec00b69d1e993655247376b17834ed`.
The complete original scope and MIT text are in
[licenses/ZallaxDev-LICENSE-PORT.md](licenses/ZallaxDev-LICENSE-PORT.md).
The inherited MIT notice remains in
[licenses/ZallaxDev-MIT.txt](licenses/ZallaxDev-MIT.txt).
Attribution covers inherited/adapted backend, headers, tools and tests, not just
files with a Zallax comment. File-level treatment is recorded in the review manifest.

## External game upstream

pret/pokefirered:
https://github.com/pret/pokefirered/tree/037335f4c725d7c9aecdac87066f2002b4bd7e14 .
Fetched locally at this exact commit; never vendored or added as a Git submodule.
No general license for its game/decompilation source was found at that revision.
Local modified source and compiled game logic retain their existing rights.
The public edit recipes refer to local source line numbers and hashes rather than
distributing upstream context/deleted lines. This reduces copied material; it is
not permission to redistribute the resulting modified source or executable.

## External build tools from pret

These are fetched with upstream and executed locally; their source/binaries are
not imported. Original license notices are retained here for reference:

- YamaArashi, preproc, mid2agb, bin2c and ramscrgen: MIT, copyright 2016.
  [Original text](licenses/pret-preproc-MIT.txt).
- YamaArashi, gbagfx and scaninc: MIT, copyright 2015.
  [Original text](licenses/pret-gbagfx-MIT.txt).
- YamaArashi, rsfont: MIT, copyright 2015–2016.
  [Original text](licenses/pret-rsfont-MIT.txt).
- ipatix, wav2agb: MIT, copyright 2016.
  [Original text](licenses/pret-wav2agb-MIT.txt).

The external mapjson/jsonproc tools and any bundled dependencies retain their own
file-specific notices. They are not represented as covered by the above MIT notices.
Any future redistribution requires a separate inventory of those exact tools.

## Other external dependencies

devkitPro libctru, citro2d and citro3d are external build/link dependencies
(the upstream ports identify their licenses as zlib). devkitARM/GCC/newlib and
3dsxtool/smdhtool are external tools with separate, component-specific terms.
No SDK, icon, linker template or library binary is copied into this repository.
Exact installed SDK versions and their full dependency/license inventory must be
recorded before any binary release is considered. No license is assigned to a
generated linker script by the port MIT license.

Python is an external interpreter. Optional local execution tests use pyelftools
and Unicorn; neither package is vendored. A frozen Builder, Python runtime,
GUI, PyInstaller bundle and their dependency notices are outside this distribution.
This repository grants no permission to omit those notices in a future package.

The gradenGnostic pokeemerald-multiplatform voxel code and the MPL-2.0 devkitARM
linker template mentioned by Zallax are not imported. Voxel source is absent and
disabled. Credits do not imply that a component is included.

## Game and branding

Pokémon FireRed and its game content remain the property of their respective
rightsholders, including Nintendo, Game Freak and Creatures; Pokémon marks retain
their existing rights. No rightsholder endorsement or permission is claimed.

## Twin Embers technical reference and exclusions

The 3DS entropy adapter, standalone entropy diagnostic, synthetic initialization
and save compatibility tests, and related recipe additions are original owned
port contributions by alex61194 with disclosed AI assistance (MIT). The linked
libctru services retain their external SDK license; no SDK source or binary is
vendored. External FireRed initialization and save functions used locally for
integration tests are not redistributed or relicensed.

The technical source is exclusively the reviewed selection from
`alex61194/pokemon-firered-3ds-clean@96f95e015af9f5f38e4a8aefca7cab29a78b030d`,
plus the documented clean distribution adaptations. No previous distribution
code or repository history is imported. The complete original notices above
continue to apply to their respective inherited portions.

The reference's re-authored intro media, generated font artwork and C translation
of the game mixer are excluded. The font is read from the user's pack; the intro
uses original pack-backed data; the mixer remains external upstream code in the
private local build. Credits do not license the pack or derivative game logic.
See PROVENANCE.md and docs/import-exclusions.json for exact scope.
