# German FireRed tools

Scripts that run **on the owner's PC** next to both ROMs (USA 1.0 and Germany BPRD rev 0).
They only print or write offsets and sizes, never ROM bytes. Their outputs stay on the PC.

- `align.py`, `align2.py`, `align3.py`: how much of the recipe matches between the two ROMs.
- `scrdis.py` + `script_ops.json`: bytecode disassembler for event scripts.
- `gen_recipe_de.py`: maps every operation of the USA recipe onto the German ROM (`firered-18a3758c`),
  by text pointers, contiguous runs, 16-byte anchors, pointer-masked comparison, and pointer tables.
- `finish_recipe_de.py`: packages the recipe and event script layout into the builder JSON structure
  (`builder/firered3ds_builder/recipes/firered-18a3758c.json` and its split partition files).

## Usage

1. Event scripts and string extraction:
   `python3 gen_recipe_de.py "<USA rom>" "<German rom>" ../../builder/firered3ds_builder/recipes/firered-41cb23d8 <sizes.json> <layout.json> recipe-de`

2. Finish and partition recipe:
   `python3 finish_recipe_de.py recipe-de/recipe-de.json <layout.json>`

All 6,734 files are placed with CRC32s and verified clean against the official German FireRed ROM
(SHA-1 `18a3758ceeef2c77b315144be2c3910d6f1f69fe`).
