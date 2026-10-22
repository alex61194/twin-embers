# Quality-of-life additions

Small changes that make the port easier to use or to debug. Each says what it does, what it does not,
and how it was checked. Nothing here changes the save format.

## Previous session's log

With diagnostics on (`debug.txt`, `log.sync`, `warp.trace` or `tm.trace` on the card), each start moves
the last `port.log` to `port-prev.log` before writing the new one (`3ds_platform_min.c`). Until now the
log was appended to forever, so a crash was buried under later sessions. Only the previous session is
kept. Without diagnostics nothing is written or moved.

NEEDS HARDWARE VALIDATION (a FAT rename on the card).

## Type matchup on the battle's move cards

In a single battle each move card with a damaging move shows how it hits the foe: `x2` (green), `x4` (lime),
`x1/2` or `x1/4` (orange), `x0` (red); the x4 is a brighter lime than the x2. A neutral hit shows nothing. The tag sits between the type badge
and the power.

- It reads FireRed's own `gTypeEffectiveness` (`3ds_type_chart.h`, patch 0118), against the foe's
  current types (`type1`, `type2`), with the table's rules: the entries after its Foresight marker
  apply, a dual type multiplies, and a repeated type counts once.
- Not shown: double battles (which foe a move would hit is not known when choosing), moves without power,
  Hidden Power and Weather Ball (their real type is not the table's).
- Not looked at: abilities (Levitate, Wonder Guard, the absorbing abilities), Foresight and Odor Sleuth,
  and moves with fixed damage. The tag is the type matchup, not a damage prediction.
- It is on for everyone: the OPTIONS screen has no room for another row (seven rows fill it), so an
  on/off setting waits for a second OPTIONS page.

Checked: `tests/type_chart_test.c` (the multiplier on a table laid out like FireRed's, in `verify-host`);
the whole 18 x 18 x 18 space against the real table read from the pinned source, with no difference to
a direct reimplementation of the game's loop; `tools/test_battle_ui.py` (tag colours read back from the
rendered card, incremental redraws equal to full ones, the card's silhouette unchanged where no tag is
drawn); the ARM build links `CtrBattle_MoveMatchup`.

NEEDS HARDWARE VALIDATION (a real battle: the tag against the foe's types, then after a switch).

## "Engine ABI is missing"

The 3DSX reads `romfs:/engine/abi.bin` first. Two causes look the same on screen, so the message now
tells them apart (`3ds_data.c`): `romfsInit` failed (the file was not started as the 3DSX it is, for
example from another folder or under another name, or by a loader that does not pass its RomFS), which
shows the error code and where to start it; or the RomFS opened but has no `engine/abi.bin` (a 3DSX
built without its RomFS), which says to download it again or build with
`make CLEAN_RELEASE=1 verify-game-3dsx`. The result of `romfsInit` is also in `port.log`.

## Player's PC, WITHDRAW ITEM froze with a black top screen

Reported on the QoL build: PC of the player's room (or a Pokemon Center), PLAYER'S PC, WITHDRAW ITEM:
the top screen went black and the bottom stayed blank, with the FPS counter still shown.

Cause (read in `src/item_pc.c`): `ItemPc_RunSetup` is a `while (1)` loop of setup steps, and the step
`ItemPc_LoadGraphics` waits for a queued graphics copy (`FreeTempTileDataBuffersIfPossible`) that only
the DMA manager finishes, at a VBlank. On the GBA that interrupt runs inside the loop; on the ARM11 it
runs after the callback returns, so the loop spun forever. It is the same defect already fixed for the
mailbox (patch 0102) and the Berry Pouch (patch 0062); the Item PC had not been covered.

Fix: patch 0119 makes the loop return while a copy is queued (`WaitDma3Request(-1) == -1`), so the VBlank
runs and the next call continues. Checked: `tools/test_item_pc_wait.py` runs the production yield text
from the recipe against a stalled copy (the unpatched loop never returns: it times out; the patched one
returns each time and finishes once the copy completes; the GBA build is unchanged) and is in CI; the
ARM build links. NOT reproduced in an emulator or on a console: NEEDS HARDWARE VALIDATION (open the
player's PC, WITHDRAW ITEM, withdraw and leave, also from a Pokemon Center).

## Day and night on the outdoor field

On outdoor maps (FireRed's town and route map types) the map and its sprites take the colour of the hour
of the 3DS clock: night from 21:00 to 05:00, dawn at 06:30, plain day from 09:00 to 17:30 and dusk at
19:00, blended second by second in between (`3ds_day_night.h`; the hours follow Merrp's day/night system
for pokeemerald, the colours are the port's own). Indoors, in caves, in battle and in every menu nothing
changes. OPTIONS, page 2, DAY/NIGHT: CLOCK (the default) or OFF; like the other page-2 options it is not
kept in the save and is CLOCK again after a restart.

- It is coloured, not played: the compositor shades each colour as it turns the palette into texels
  (`3ds_video.c`, `DayNightTexel`), for the map's palettes 0-12 and the sprite palettes. The game's
  palettes in RAM, the weather, the fades and the save are untouched. Palettes 13-15 (text boxes, the
  map-name popup, the menus) keep their colours. A change of tint (in steps of 16 seconds) re-converts
  the palettes once and the tile cache refreshes through the palette versions it already uses.
- Lit windows: at night, the glass of the windows listed per tileset in `3ds_day_night_lights.h`
  (`tile | palette << 12`) takes a warm lamp colour instead of the shade, fading in from dusk. Only the
  glass pixels of those tiles (light blue, `CtrDayNight_Glass`) are lit; frames and sills stay dark. The
  list is by tile, not by colour, because the Poke Mart roof uses the same blues as the windows and must
  stay dark. Not lit: Silph Co.'s glass front, the Bike Shop skylight, and the houses with grey windows.
- Where: `3ds_game_bridge.c` asks for it each frame only while the field's own main callback runs
  (`CB1_Overworld`), outside a battle, on map types 1, 2, 3 and 6, passing the light lists of the map's
  primary and secondary tilesets. No game patch.
- Not yet: glow around lamps and night effects (the third phase of the plan).

Checked: `tests/day_night_test.c` (in `verify-host`: the day is exactly white, the keys land at their hours,
no hour is darker than the night, no channel moves more than one step per second, the day wraps; the
shade never brightens, the lamp is full at night and off by day, glass is told from sill, wall and grass;
the light tables are valid tiles with no duplicates and no Mart roof tile); `tests/poketouch_options_test.c`
(the DAY/NIGHT row, its default and its two values); the ARM build links. NEEDS VALIDATION in Azahar and on
a console (outdoors at night with the clock set, entering a house, a battle, a door fade, 3D on).

## Not added, and why

- ZL/ZR for the turbo: the port removed it on purpose (`3ds_fast_forward.h`).
- Run without holding B: the RUN button of the touch screen already does it.
