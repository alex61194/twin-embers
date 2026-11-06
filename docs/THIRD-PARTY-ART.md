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

## Facets and Emerald Lens (ElvieBlooms)

Their authors' permission and each look's credit are in `CREDITS.md`.
