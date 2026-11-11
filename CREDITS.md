# Credits

- **alex61194** — FireRed-specific port work, maintenance and migration from
  the private laboratory. MIT applies only to contributions the maintainer owns,
  explicitly authorized on 2026-10-07.
- **ZallaxDev and Pokémon Emerald 3Ds Dual Screen contributors** — original
  dual-screen architecture, compositor, decode/input interfaces, bridge concepts,
  audio model, bundle system, and data/pack code adapted for FireRed.
  Their collective copyright notice is preserved without replacing it with a
  claim that all port code is original. See NOTICE.md for exact revisions.
- **pret/pokefirered contributors** — external decompilation and reconstruction
  tools. Credit is not a license grant for the game source.
- **pret/pokeheartgold contributors** — external Pokémon HeartGold decompilation
  (https://github.com/pret/pokeheartgold). The optional Generation 4 battle sprites,
  trainer pictures and menu icons are downloaded by the user's Builder from it at the commit pinned
  in `gen4_sources.py`, verified by size and SHA-256, and are not stored in this
  repository. The artwork belongs to Nintendo, Game Freak and Creatures; credit is
  not a license grant.
- **Fan-made HeartGold-style player backs** — Leaf's by `setogabes` (Discord),
  Red's from the Battle Art Voxel Fork collection
  (https://github.com/absol89/DramaticShapeVoxelMod), both as bundled in
  https://github.com/LucianoNeo/gen1recomp-mods. Downloaded by the user's Builder
  at the commit pinned in `gen4_sprites.py`, verified by size and SHA-256, and not
  stored in this repository. They depict Nintendo, Game Freak and Creatures'
  characters; credit is not a license grant.
- **Facets, by ElvieBlooms** (https://github.com/ElvieBlooms/facets, MIT for its code; its art is free to use with
  credit) — the optional new people on the map and battle backgrounds, downloaded by the user's Builder at the
  commit pinned in `gen4_sprites.py`, verified by size and SHA-256, and not stored in this repository. People:
  **KyleDove**, **Jext** and **Hyo** (with some by **DiegoWT** and **Aqua**) for the community set, **DiegoWT**
  (Black/White style), **Taiga** (ORAS style) and **PurpleZaffre** (Agatha, Lorelei, Mr. Fuji and Giovanni).
  Backgrounds: **LibertyTwins**, **princess-phoenix**, **carchagui**, **aveontrainer**, **WesleyFG**,
  **kWharever**, **worldslayer608** and **knizz** (Battle Backgrounds Patch FR). The characters belong to
  Nintendo, Game Freak and Creatures; credit is not a license grant.
- **HD Dynamic Battlefields, by Bentley734** (https://github.com/Bentley734/HD-Dynamic-Battlefields, no licence
  file; used with the author's permission, see [docs/THIRD-PARTY-ART.md](docs/THIRD-PARTY-ART.md)) — the optional HD
  battle scenes: his paintings cut and reduced to the 3DS's resolution by `tools/dev_hd_battlefields.py`, downloaded
  by the user's Builder at the commit pinned in `gen4_hd.py`, verified by size and SHA-256, and not stored in this
  repository. The characters and settings belong to Nintendo, Game Freak and Creatures; credit is not a license grant.
- **Emerald Lens, by ElvieBlooms** (https://github.com/ElvieBlooms/emerald_lens, MIT for its code; its looks are
  shared with credit) — the optional player looks, downloaded by the user's Builder at the commit pinned in
  `gen4_sprites.py`, verified by size and SHA-256, and not stored in this repository. Ash (HGSS style) by
  **PKMNTrainerRick**; Green (HGSS style) compiled by **ChromeValiant**, art by **Poltergeist/Coffee Cup**,
  **Mid117**, **Wolfang62** and **aethestode**; Green's back, front, intro picture and map icon from her Gen 3 look there,
  compiled from **virdiangrass** and Team Aqua's asset repo, with edits by **Elvie**; Ash's map icon from his Gen 3
  look, shared by **kalarie** on PokéCommunity. The characters belong to Nintendo,
  Game Freak and Creatures; credit is not a license grant.
  The alternative looks (the Builder's `--alt-looks`, for the player to pick in the game: Ash, Gary, Silver, Gold, Brendan, May,
  Green and Kris in their versions) are downloaded the same way; each is by the artists named in its credit in
  `builder/firered3ds_builder/gen4_looks.py` (kalarie, PKMNTrainerRick, Alan_Flygon, lichenprincess, FourLeafSunny,
  GregarLink10, DiegoWT, LouLilie, richardPT, eykav, ChromeValiant and others), and Team Aqua's Asset Repo for some. They
  are used with Elvie's permission (see [docs/THIRD-PARTY-ART.md](docs/THIRD-PARTY-ART.md)).
- **YamaArashi** — MIT-licensed external conversion/preprocessing tools listed
  individually in NOTICE.md.
- **ipatix** — MIT-licensed external wav2agb tool.
- **devkitPro and its library/tool contributors** — external 3DS SDK, libctru,
  citro2d and citro3d; **GNU GCC and newlib contributors** — external toolchain.
- **Python, pyelftools and Unicorn contributors** — external local development
  and optional test dependencies; no runtime is bundled.
- **Pyodide contributors** (MPL-2.0) and **Kazuhiko Arase**, qrcode-generator
  (MIT) — external libraries the web Builder page loads from public CDNs at
  pinned versions; neither is copied into this repository.
- **Sh3ld0r/mcl5500** — overworld wild Pokémon feature (spawn controller, GPU
  silhouette tinting, OPTIONS page 4 settings and `patch 0237`), contributed
  under MIT with disclosed AI assistance. Concept inspired by **Wilds of Kanto
  Revival** by **poooooby** (https://github.com/poooooby/wilds-of-kanto-gen-3, a
  fork of **Wilds of Kanto** by **YoDrehDenSwagAuf** and collaborators for
  `Gen1Recomp`); original implementation, with no code or assets reused from
  that mod.
- **Nintendo, Game Freak, Creatures and The Pokémon Company** — relevant game
  and trademark rightsholders; no affiliation or authorization is asserted.

These credits cover identifiable component authors from checked source/license
notices. The upstream collective contributor notices are retained; no invented
list of individual authors is supplied. Any additional reused component must be
identified and reviewed before import. Assistance used in this migration is
disclosed in AI_DISCLOSURE.md.

Twin Embers Port uses only technical reference
`alex61194/pokemon-firered-3ds-clean@96f95e015af9f5f38e4a8aefca7cab29a78b030d`.
The earlier distribution contributes legal notices and credits only. New
distribution tools, the pack-backed font accessor/decoder, neutral geometric
icons and the asset-free intro fallback are owned port contributions with
disclosed AI assistance. These changes preserve third-party authorship and
do not expand the license on external game code.
