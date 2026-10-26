"""Write the full game's linker script: devkitARM's 3dsx.ld with the game's RAM in one section.

The game's own variables, those the GBA keeps in RAM, are gathered in game_ram,
just before .bss, so that main_game_full.c can put them back for a soft reset,
as the GBA clears its RAM. ROM-derived data (.bss.ctr_*: REX tables, graphics,
songs) stays in .bss: the GBA keeps it in ROM. The Makefile passes the output
directory with -B, so gcc takes this 3dsx.ld before devkitARM's.
"""
import argparse
from pathlib import Path

# .bss.* except the .bss.ctr_* data loaded from the pack.
GAME_BSS = '.bss .bss.[!c]* .bss.c[!t]* .bss.ct[!r]* .bss.ctr[!_]* COMMON'
GAME_RAM = f'''\tgame_ram ALIGN(4) :
\t{{
\t\t__ctr_game_ram = .;
\t\t*build/game/*.o(.data .data.*)
\t\t*build/game_data/*.o(.data .data.*)
\t\t*(common_data)
\t\t*(ewram_data)
\t\t*build/game/*.o({GAME_BSS})
\t\t*build/game_data/*.o({GAME_BSS})
\t\t. = ALIGN(4);
\t\t__ctr_game_ram_end = .;
\t}} : data

'''
ANCHOR = '\t__bss_start__ = .;'

p = argparse.ArgumentParser()
p.add_argument('base', type=Path)
p.add_argument('output', type=Path)
a = p.parse_args()
script = a.base.read_text()
if script.count(ANCHOR) != 1:
    raise SystemExit(f'{a.base}: no single "__bss_start__ = .;" to put game_ram before')
a.output.parent.mkdir(parents=True, exist_ok=True)
a.output.write_text(script.replace(ANCHOR, GAME_RAM + ANCHOR))
