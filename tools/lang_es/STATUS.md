# Twin Embers – Spanish support: status (2 Nov 2026)

## Approach
Twin Embers is built from the pret/pokefirered decompilation. The game data (graphics, maps, music, scripts, text) is **not** in the repo. The Builder rebuilds it from the user's own ROM using a "recipe": a list of offsets and sizes, checked by SHA-1 and CRC. So we **don't translate anything**. The Spanish build takes its text straight from the official Spanish ROM (Pokémon Edición Rojo Fuego, Spain, BPRS rev 0, SHA-1 ab8f6bfe…). The plan is one .3dsx per language.

## Done
- **Lower-screen font** (PR #39, draft): the port's own UI now draws accents, Ñ/ñ, Ü, ¿, ¡, º and ª with FireRed's font.
- **Port UI strings**: `make TWIN_LANG=es` builds every port-only string in Spanish (options, battle, summary, Pokédex, bag, PC, save, Fly). Strings are written as `CTR_TR("English", "Español")`, chosen at compile time.
- **ROM alignment test** (USA 1.0 vs Spain): of about 7.5 MB of recipe data, about 6.4 MB is the same in both ROMs, apart from offsets and pointers. Our runtime already relocates pointers. About 0.76 MB really differs: almost all text, plus about 40 localized graphics (title logo, summary screen, trainer card…).
- **Text sizes**: the engine is compiled with fixed symbol sizes, so the Spanish build needs the Spanish string lengths. A tool follows pointers from the USA ROM to the Spanish ROM and reads only sizes and positions, never text. It has found 3,330 of the 3,945 standalone strings so far.
- **Event scripts** (421 KB of script bytecode with inline dialogue): we pulled all labels (9,832) and pointers (9,954) from the USA object built from pret. We now have a small script disassembler (opcode sizes generated from pret's `event.inc`). It matches 3,524 of 3,538 USA script blocks exactly. Some Spanish scripts are a few bytes longer than the English ones, so the Spanish layout has to be found by disassembling the Spanish ROM, not by assuming fixed sizes.

## Remaining
1. Finish the Spanish event-script layout, then generate a synthetic object with Spanish label offsets and pointer sites.
2. Compile the engine with the Spanish sizes and generate the Spanish recipe.
3. Patch the few places where the Spanish game's code differs, for example plural suffixes (`sText_S` / `sText_IES`).
4. Easy Chat words. They are in a different (alphabetical) order in Spanish, about 600 left.
5. Make the Builder accept the Spanish ROM and use its recipe.

## Overlap with a gen1recomp-based mod
gen1recomp is a different engine (Gen 1 Red/Blue) and, as far as we know, its Spanish mod replaces text inside that recompilation. Our pipeline never ships text and reads everything from the user's Spanish FireRed ROM. So the main work here is layout, sizes and pointers, not translation. Things that could still be shared: translations of the port's own UI strings, and knowledge of where the Spanish FireRed's code differs from the English one.
