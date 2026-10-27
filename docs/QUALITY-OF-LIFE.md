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

## LeafGreen's wild Pokémon

The species LeafGreen has in the wild and FireRed does not can be caught here too. FireRed and LeafGreen share
their encounter tables slot for slot, so a slot holding a FireRed-only species gives LeafGreen's species instead
half of the time, at the slot's own level (`3ds_leafgreen.h`, patch 0130):

| FireRed | LeafGreen |
|---|---|
| Ekans, Arbok | Sandshrew, Sandslash |
| Oddish, Gloom | Bellsprout, Weepinbell |
| Psyduck, Golduck | Slowpoke, Slowbro |
| Growlithe | Vulpix |
| Shellder | Staryu |
| Scyther (Safari Zone) | Pinsir |
| Seadra, Weezing | Kingler, Muk |
| Wooper, Murkrow, Qwilfish, Delibird (Sevii Islands) | Marill, Misdreavus, Remoraid, Sneasel |

Two of LeafGreen's slots are not a counterpart of FireRed's and are named by place: Magmar on the Mt. Ember
exterior (the Spearow slots that are Magmar in LeafGreen, levels 38 and 40) and Mantine in the deep water of Tanoby
Ruins and Trainer Tower (the 35-40 Tentacool slot). It covers walking, surfing, Rock Smash, fishing and Sweet Scent.

- Not changed: species both games have at other rates (Krabby/Horsea, the Nidoran, Kakuna/Metapod,
  Koffing/Grimer, Ledyba/Spinarak), Electabuzz and Skarmory (LeafGreen has another species there, not an exclusive),
  the Game Corner prizes and in-game trades (script data), and the Pokédex area map, which shows FireRed's places.
- The table of counterparts is port-authored code in `build/3ds_leafgreen.o`; FireRed's encounter tables still
  come from the pack.

Checked: `tests/leafgreen_test.c` (in `verify-host`); every FireRed/LeafGreen slot difference in the pinned
`wild_encounters.json`; `verify-clean-release` reports 0 embedded bytes.

NEEDS HARDWARE VALIDATION (an encounter of each kind on a route with a counterpart).

## Trade evolutions by level

The Pokémon that evolve by trade also evolve on levelling up from level 40, with no trade (patch 0131): Kadabra,
Machoke, Graveler and Haunter, and the trade-with-item ones, without the item: Poliwhirl (Politoed), Onix (Steelix),
Scyther (Scizor), Seadra (Kingdra), Porygon (Porygon2) and Clamperl (Huntail).

- Holding the trade's item picks that branch: Clamperl with the Deep Sea Scale becomes Gorebyss.
- Slowpoke keeps evolving into Slowbro at 37. Holding a King's Rock when it levels up from 37 makes it Slowking.
- As in a trade, the targets from the second generation wait for the National Pokédex, and the Everstone stops it.
- A trade still evolves them as before. The held item is not used up by the level evolution.

NEEDS HARDWARE VALIDATION (Rare Candy on a level 39 Kadabra; a Slowpoke with a King's Rock).

## Night encounters

From 21:00 to 05:00 on the 3DS clock (the night the day/night tint draws), some wild slots give a night species,
always and at the slot's own level (`3ds_night_wild.h`, patch 0134). The day birds and bugs give way: Pidgey to
Hoothoot (Zubat or Oddish before the National Pokédex), Caterpie to Spinarak (Venonat), Spearow to Murkrow (Zubat),
Fearow and Pidgeotto to Noctowl (Golbat, Venomoth); Mt. Moon has more Clefairy, the Pokémon Tower more Haunter and
Misdreavus, the Pokémon Mansion Gastly and Houndour, Route 8 Gastly, the Lost Cave Misdreavus. With the Super Rod at
night in the waters of Five to Seven Island, Chinchou and Lanturn bite instead of Gyarados and Seadra. In Six Island's
Pattern Bush it goes both ways: Ledyba by day, Spinarak by night.

- Johto species wait for the National Pokédex, as in the Sevii Islands; before it a rule gives its Kanto species or
  leaves the slot alone.
- The LeafGreen half (above) applies after it, so a night Oddish can still be a Bellsprout.
- Eevee's friendship evolutions are on again with the same clock: Espeon by day, Umbreon at night (FireRed left them
  out for want of a clock).
- It follows the clock, not the DAY/NIGHT option, which only switches the tint. The Pokédex area map shows the day.

Checked: `tests/night_wild_test.c` (in `verify-host`); every rule's species is in its map's FireRed slot (pinned
`wild_encounters.json`).

NEEDS HARDWARE VALIDATION (Route 1 at 23:00 and at 12:00; Rare Candy on a friendly Eevee at night).

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

## The touch screen's ember look

The lower screen's field menus wear the owner's modern design (DS/3DS-era raised buttons on a red ember
backdrop, from the mockups the owner approved): the START column as cream buttons with an icon
and FireRed's lettering (amber and glowing for the screen shown), the X and Y tiles with their badge, RUN
with a switch, the GBA picture and the HOME map in a slate frame, the bar as a note card with BACK (violet)
and PAGE beside it, OPTIONS as raised rows with the value in a pill (a switch for ON/OFF) and SAVE as a card
with YES and NO. The battle Summary keeps its layout on the same backdrop. The battle screens are unchanged.

- The art is the port's own: `tools/make_ember_skin.py` draws the backdrop and the six column icons
  (`ember_skin*.h`); the BAG icon is the port's battle Bag reduced. The text is FireRed's font at run time.
- Embers rise slowly over the backdrop (about twenty sparks, `3ds_touch_skin_impl.h`): one step a frame,
  drawn only on pixels that show the backdrop, so they pass behind every button and never over the GBA
  picture; each spark puts back what it painted. OPTIONS, page 2, EMBERS: OFF, LOW or HIGH, shown side
  by side with the chosen one lit (a touch picks one; Left and Right step through them). LOW keeps 7 of
  the sparks and moves them once every 3 frames, as fast as before. The default is HIGH on a New 3DS and
  LOW on an Old 3DS; once the player picks a level it is kept in `sdmc:/3ds/twinembers/settings.cfg`
  (`embers=0..2`), not in the game save.
- The old artwork skin (`poketouch_skin*.h`, `tools/make_touch_skin.py`) is removed.

Checked: `tests/poketouch_render_test.c` (new hit geometry, every incremental redraw equal to a full one);
`tests/poketouch_options_test.c` (the EMBERS row, its default and its values); the ARM build links.
NEEDS VALIDATION in Azahar and on a console (each field menu, SAVE, OPTIONS, the embers' cost per frame).

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
  stay dark. Glass that is not light blue (Lavender's grey panes, the Pokemon Tower's and the Pewter
  Museum's dark ones, Silph Co.'s bands, the Cinnabar Lab's white panes) is listed with
  `CTR_LIGHT_GLASS(entry, mask)`: the mask names the colour indices that are glass in that tile. Not lit:
  the Bike Shop skylight and the Pokemon Mansion.
- Not every window is lit all night: Pokemon Centers, Marts, Gyms and the route gatehouses
  (`CTR_LIGHT_PUBLIC`) are, the houses keep their own hours (`CtrDayNight_WindowOn`): on between 18:30
  and 20:00, off one by one between 21:45 and 02:15, about one in eight on all night, about a third on
  again from 05:00. The hours come from a hash of where the window stands on the map, so a window keeps
  them every night and walking past does not make them flicker. A window is the run of window tiles whose
  glass comes within 3 pixels of their shared edge (so panes split by a mullion, and the two halves of a
  glass door, still meet), so it switches as a whole.
- Where the tilemap stands on the map: `field_camera.c` keeps its tile offset to itself, so the bridge finds
  it by matching the tilemap buffers against the map's metatiles in view (`UpdateCamera`), then only checks
  it each frame; twice the camera position minus the offset stays the same while walking. Until it is
  found every listed window is lit.
- Lit windows bloom (`DrawDayNightGlow`): each lit tile has a 24x24 glow texture, its glass pixels blurred (`GlowSlot`,
  kept by the tile's 64-bit glass mask), added over the priority-2 backgrounds before that priority's sprites and
  only on the layers the current window shows (so not through an exit wipe). The light hugs the panes and fades
  within a few pixels: no shape of its own, nothing painted on walls, doors or steps.
- The night itself is a moonlit grade (`CtrDayNight_Moon`: a quarter of the way to grey, gamma 1.3, a
  blue multiply) that the hour's multiply goes over to as the night deepens, so dusk stays warm.
- Where: `3ds_game_bridge.c` asks for it each frame only while the field's own main callback runs
  (`CB1_Overworld`), outside a battle, on map types 1, 2, 3 and 6, passing the light lists of the map's
  primary and secondary tilesets. No game patch.
- Not yet: street lamps, fireflies and other night effects.

Checked: `tests/day_night_test.c` (in `verify-host`: the day is exactly white, the keys land at their hours,
no hour is darker than the night, no channel moves more than one step per second, the day wraps; the
shade never raises a colour's luma and the night is the moonlit grade, dusk stays warm, the lamp is full at
night and off by day, glass is told from sill, wall and grass; the windows' hours: none at noon, all at
21:00, fewer through the night, some all night and some at dawn; the light tables are valid tiles with no
duplicates, no Mart roof tile, and only General's non-house buildings public); `tests/poketouch_options_test.c`
(the DAY/NIGHT row, its default and its two values); the ARM build links. NEEDS VALIDATION in Azahar and on
a console (outdoors at night with the clock set, entering a house, a battle, a door fade, 3D on).

## Not added, and why

- ZL/ZR for the turbo: the port removed it on purpose (`3ds_fast_forward.h`).
- Run without holding B: the RUN button of the touch screen already does it.
