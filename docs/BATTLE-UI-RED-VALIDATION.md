# Battle menu and Summary in PokeTouch's red

The lower screen during a battle (the action menu, moves, targets, Safari Zone, Bag and
Party with their menus and messages) and the Pokemon Summary (INFO, STATS, MOVES with its move details, TRAINER, which share its components and its Party icon cache) now wear the red chrome of PokeTouch instead of the
navy of the shared theme. Sizes, positions, touch areas and behaviour are untouched.

## What changed

- `3ds_ui_theme_impl.h` gains a surface table (`UiSurface`): `sUiNavy`, the previous look
  and still the default for every other screen, and `sUiRed`, PokeTouch's chrome
  (`PT_RED_BG`, `PT_RED_PANEL`, `PT_RED_EDGE`, `PT_RED_LIGHT`, `sChrome` and `sChromeOn`
  values). The components read their surface colours from it (`UIS_*`); the battle drawing
  selects `sUiRed` while it draws and restores `sUiNavy`.
- Backdrop, strip of Poke Balls (with the chrome panel's light and shadow lines), cards,
  Bag list and detail, Party panels, modals, text shadow and secondary ink are the red
  family. Buttons are the chrome's: FIGHT the bright one (`sChromeOn`), BAG, POKeMON, RUN
  and the Safari four the deeper one (`sChrome`), CANCEL a darker red.
- Kept as they are in PokeTouch: the blue of a selection and the white focus ring, white
  text, the type colours of move cards, the red/blue of foe/ally target cards, HP colours.
- Polish within the same rectangles: each action button carries a small coloured tick on
  its left edge like PokeTouch's side buttons (FIGHT amber, BAG green, POKeMON blue, RUN
  yellow; BALL blue, BAIT pink, ROCK grey, RUN green), so the red buttons stay apart; a
  fainted Pokemon's panel is darkened instead of tinted red; panel text shadows are dark
  red instead of navy.

## Icons and the first-build errors

Seen on a real screen after the first build: the left-edge tick overlapped the "POKeMON" label
(it is 96 px wide in a 98 px button), the RUN label sat higher than BAG's and POKeMON's because
its shoe was drawn at 12 px beside 24 px icons, and the icons were the 12x12 menu samples
doubled (blocky) with a plus-shaped primitive for FIGHT. Now:

- The tick moved to the top edge, centred, on every action and Safari button.
- Every icon is 24x24 so all labels share one height. BAG and POKeMON (and BALL) draw the
  game's own Bag and Poke Ball icons at their full size, read from the user's pack; without
  the pack the 12x12 samples are doubled as before (same geometry, tested both ways).
- Hand-made 24x24 port art for what has no game art to draw from: a running shoe (RUN), the
  impact burst of FIGHT, the Safari rock and bait. Drawn from simple shapes by script and
  checked by eye; not game pixels (`battle_ui_art.h`).

## Side menu icons and RUN

The font stays the game's. The six side-menu icons (POKeDEX, POKeMON, BAG, the Trainer Card, SAVE,
OPTIONS) were the pack's 24x24 item art cut to 12x12 by taking the first opaque pixel of each 2x2,
which lost shading and rounded shapes. They are now reduced by area to 16x16 with alpha: each
output pixel averages the drawn pixels it covers (so outlines stay dark and transparent edges do
not tint) and keeps a coverage that blends into the button's gloss. They sit at the same place
(left tick, then icon, then label) and the label does not move. Without the pack the 12x12 samples
draw as before. The RUN button draws the port's 24x24 running shoe (the same art as the battle's
RUN) instead of the 12x12 silhouette. `3ds_icons.h` holds the shared code; the tests decode a
synthetic pack icon through it (full coverage, an edge pixel a third covered, an empty icon, a
missing pack).

## Legibility (WCAG 2.x, the usual recommendation for game UIs)

- White labels sit on button glosses held to 3:1 or better for the 2x and 3x labels (FIGHT, BAG,
  POKeMON, RUN, the Safari four) and 4.5:1 for 1x labels (menu rows, tabs, CANCEL); the first
  restyle's brighter chrome gloss (about 1.8:1 under FIGHT) was toned down for this.
- White text and the warm secondary ink hold 4.5:1 on the red backdrop, insets and panels, and
  white holds it on the selection blues.
- A move card's name and a badge's caption use dark ink on the light type colours (Electric,
  Ground, Ice, Normal...) and white on the dark ones, chosen by luminance, so every one of the
  18 types reads at 3:1 or better.
- Touch areas were already large (the smallest battle target is 98x84 px) and are unchanged.

## Font

Searched: open fonts that could replace FireRed's own (CC0: m5x7, m6x11 by Daniel Linssen;
OFL: pixeldroid Console and Menu, Pixelify Sans, Press Start 2P). The game's font stays: its
capitals are 9 px tall, the CC0 and OFL pixel fonts' are 5 to 7 px, so in the same boxes the
text would get smaller and thinner, and Press Start 2P is too wide for the 98 px buttons.
Adding another font would also add a file and a licence to the repository. If one is
wanted anyway, m5x7 (CC0, https://github.com/boringcactus/m5x7) is the candidate; at 2x it
would make 14 px capitals where FireRed's give 18.

## Checks (9 October 2026)

`python tools/test_battle_ui.py` (`3ds_port/tests/battle_ui_render_test.c`, the production
renderer with synthetic bridges and no game data):

- The Summary's four tabs and the move details are drawn the same way (presses on the arrows, BACK and tabs included), with no navy left and hit areas and silhouettes identical to the renderer of `587c748`.
- 16 battle states drawn (idle, action, moves, targets, Safari, Bag with tab, row, menu and message,
  Party with slots, menu, yes/no, message and forced) with presses and cursor moves; after
  each change the incremental update equals a full redraw.
- No colour of the shared navy theme is left on any state.
- Against the renderer of `587c748` (the battle, summary and theme sources before this change;
  re-run on 2026-10-09): the touch areas answer to
  the same points on every state, and the silhouette of every state (which pixels are not
  the backdrop) is identical, so no size or position moved. Modal states are excluded from
  the silhouette comparison because a modal halves every colour behind it.
- The contrast figures above are asserted on the production colour tables.
- `tools/test_poketouch_render.py` still passes: HOME, GBA, OPTIONS and SAVE are
  pixel-identical to the approved base.

Not tested: a real 3DS. The renderer was only seen through a stand-in font; the game's
own font and icons are used on the console.
