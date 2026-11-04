# Twin Embers – German support: status (4 Nov 2026)

## Approach
Twin Embers is built from the pret/pokefirered decompilation. The game data (graphics, maps, music, scripts, text) is **not** in the repo. The Builder rebuilds it from the user's own ROM using a "recipe": a list of offsets and sizes, checked by SHA-1 and CRC.
No ROM bytes are stored in the repository. The German build takes all game content straight from the official German ROM (Pokémon Feuerrote Edition, Germany, BPRD rev 0, SHA-1 18a3758ceeef2c77b315144be2c3910d6f1f69fe).

## Completed
- **Clean-room German recipe** (`builder/firered3ds_builder/recipes/firered-18a3758c*`):
  All 6,734 payload files placed with exact CRC32 verification (engine ABI `0072be2a`).
  No ROM bytes or copyright material committed.
- **Builder integration**:
  `rom.py` and `test_recipe_parts.py` recognize the German ROM SHA-1 (`18a3758c...`).
  All builder test suites pass (133/133 tests).
- **Port UI strings**:
  Extended `CTR_TR` macro to support 3 languages: `CTR_TR("EN", "ES", "DE")`.
  Complete German translations added for bottom screen UI, battle HUD, menus, and map name popups.
- **Port engine**:
  Added German ROM SHA-1 to `3ds_data.c`.
  Healthbox level indicator set to `"Lv"`, HP to `"KP"`.
  Party screen TM/HM compatibility updated for German (`O.K.`, `NEIN`, `KENNT ATT.`).
- **Build system**:
  `3ds_port/Makefile` updated for `TWIN_LANG=de`, generating `twinembers-de.3dsx`.
- **HOME Menu Forwarder**:
  Target list updated to check `/3ds/twinembers/twinembers-de.3dsx`.
