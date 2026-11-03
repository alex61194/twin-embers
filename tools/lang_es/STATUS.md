# Twin Embers – Spanish support: status (3 Nov 2026)

## Approach
Twin Embers is built from the pret/pokefirered decompilation. The game data (graphics, maps, music, scripts, text) is **not** in the repo. The Builder rebuilds it from the user's own ROM using a "recipe": a list of offsets and sizes, checked by SHA-1 and CRC. So we **don't translate anything**. The Spanish build takes its text straight from the official Spanish ROM (Pokémon Edición Rojo Fuego, Spain, BPRS rev 0, SHA-1 ab8f6bfe…). The plan is one .3dsx per language.

## Done
- **Lower-screen font** (PR #39, draft): the port's own UI now draws accents, Ñ/ñ, Ü, ¿, ¡, º and ª with FireRed's font.
- **Port UI strings**: `make TWIN_LANG=es` builds every port-only string in Spanish (options, battle, summary, Pokédex, bag, PC, save, Fly). Strings are written as `CTR_TR("English", "Español")`, chosen at compile time.
- **ROM alignment test** (USA 1.0 vs Spain): of about 7.5 MB of recipe data, about 6.4 MB is the same in both ROMs, apart from offsets and pointers. Our runtime already relocates pointers. About 0.76 MB really differs: almost all text, plus about 40 localized graphics (title logo, summary screen, trainer card…).
- **Text sizes**: the engine is compiled with fixed symbol sizes, so the Spanish build needs the Spanish string lengths. A tool follows pointers from the USA ROM to the Spanish ROM and reads only sizes and positions, never text. It has found 3,330 of the 3,945 standalone strings so far.
- **Event scripts** (421 KB of script bytecode with inline dialogue): we pulled all labels (9,832) and pointers (9,954) from the USA object built from pret. We now have a small script disassembler (opcode sizes generated from pret's `event.inc`). It matches 3,524 of 3,538 USA script blocks exactly. Some Spanish scripts are a few bytes longer than the English ones, so the Spanish layout has to be found by disassembling the Spanish ROM, not by assuming fixed sizes.

## Done since 2 Nov
- **Spanish event scripts object**: labels and pointer sites at their Spanish offsets (420,310 bytes, all 9,954 pointer sites).
- **Spanish recipe** (`builder/firered3ds_builder/recipes/firered-ab8f6bfe*`): every USA recipe operation mapped to the Spanish ROM, by text pointers, contiguous runs, 16-byte anchors and pointer-masked comparison, LZ headers for graphics, and pointer following for tables. all 6,734 files are placed with CRCs; 36 graphics are the Spanish (localized) versions.
- **Build**: `make TWIN_LANG=es` uses that recipe; text sections take their Spanish length; the Builder accepts the Spanish ROM and the 3DS side only accepts a Spanish pack.

## Remaining
1. First Spanish build and test (not compiled yet). Things to look at: Easy Chat (Spanish alphabetical lists), the credits' "FIN" picture and the Department Store tiles (both unpack to a new size), the trainer card ID position, Teachy TV's closing banner.
2. Code differences of the Spanish game beyond data. Handled in the recipe so far: plural suffixes (`sText_S`, `sText_IES`, `sPluralTxt`) are empty strings, as the Spanish game never appends them. Others may show up in testing.

## Overlap with a gen1recomp-based mod
gen1recomp is a different engine (Gen 1 Red/Blue) and, as far as we know, its Spanish mod replaces text inside that recompilation. Our pipeline never ships text and reads everything from the user's Spanish FireRed ROM. So the main work here is layout, sizes and pointers, not translation. Things that could still be shared: translations of the port's own UI strings, and knowledge of where the Spanish FireRed's code differs from the English one.
