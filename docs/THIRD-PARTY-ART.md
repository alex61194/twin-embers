# Third-party art the Builder can download

None of this art is in this repository or in a release of the game: the user's Builder downloads it, checks each
file's size and SHA-256 against the list pinned in `builder/firered3ds_builder/`, and puts it in the user's own pack.

## HD battle scenes (Bentley734)

- **Work:** HD Dynamic Battlefields, by Bentley734 (https://github.com/Bentley734/HD-Dynamic-Battlefields). The
  repository carries no licence file.
- **Permission:** on 2026-11-06 the author answered alex61194 on Discord, after being asked to use his
  backgrounds in a FireRed port for the 3DS: "go ahead, but u should probably compress and optimize them for 3ds since
  they're pretty big". alex61194 replied that this was already done. That is the only permission there is: it is not a
  licence, and it does not cover other uses of the art.
- **What is distributed:** only the scenes made for the 3DS by `tools/dev_hd_battlefields.py`: 62 scenes of 400x240
  (`graphics/gen4/hdn/`), the same as 272x112 tile scenes (`graphics/gen4/hd/`) and the table of maps that picks one
  (`graphics/gen4/hd/map.bin`), for day and for night. The original paintings are not distributed.
- **Where they are:** the public repository named in `builder/firered3ds_builder/gen4_hd.py`, at the commit pinned
  there, with the same paths as in the pack (`tools/make_gen4_hd.py` writes that list).
- **Credit:** shown in the Builder (desktop and web), in the README and in `CREDITS.md`.

## Player looks (Emerald Lens, ElvieBlooms)

- **Work:** the looks of Emerald Lens (https://github.com/ElvieBlooms/emerald_lens), each by the artists named in its
  `meta.json` (listed in `builder/firered3ds_builder/gen4_looks.py` and in `CREDITS.md`).
- **Permission:** the author, Elvie, gave alex61194 her permission to use them in Twin Embers (reported by alex61194,
  2026-11-06). It is not a licence, and the looks' own artists are credited, not replaced. Harmony (D/P), Peach (HGSS)
  and Aang were added on 2026-11-16 from Emerald Lens 1.1.1; their artists are named in their credit, and Elvie's
  permission covers these three too (reported by alex61194, 2026-11-17).
  Hilbert, Hilda, Nate and Rosa (HGSS style, by DiegoWT) and Red RGBY (by hyo, based on Poffin_Case's overworlds) were added on
  2026-11-23 from Emerald Lens 1.2.0 (commit 67a25e2); nothing already pinned changed. **Elvie's permission for these five is not
  recorded yet: it has to be asked and written here before a release carries them.** The two LIBERTY looks of 1.2.0 (by tebited15, from
  LibertyTwins) are not included.
- **What the Builder does:** with `--alt-looks` it downloads each look's FireRed/LeafGreen sheets at the pinned commit,
  checks them by size and SHA-256 and puts them in the user's own pack (`graphics/gen4/look/<look>/`), for the player to
  pick in the game.

## Surf Pokemon (Emerald Lens, ElvieBlooms)

- **Work:** the surf blobs of Emerald Lens' `assets/surfmons` (each by the artist named in its `meta.json`, listed in
  `builder/firered3ds_builder/gen4_looks.py` and in `CREDITS.md`).
- **Permission:** as for the looks above; it covers the surf blobs too (reported by alex61194, 2026-11-17).
- **What the Builder does:** with `--alt-looks` it downloads each blob's FireRed/LeafGreen sheet at the pinned commit, checks it by
  size and SHA-256 and puts it in the user's own pack (`graphics/gen4/surfmon/<name>/blob.bin`), for the player to pick in the game.

## Facets (ElvieBlooms)

Their authors' permission and each look's credit are in `CREDITS.md`.
