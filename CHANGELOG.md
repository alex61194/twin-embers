# Changelog

Twin Embers is a port of Pokémon FireRed (pret/pokefirered) to the Nintendo 3DS. This file lists what changed for the player in each release. It does not repeat the detail of the [documentation](docs/).

## v0.6.0

### Added

- **Visible wild Pokémon (optional, off by default).** OPTIONS, Graphics, page Wilds: wild Pokémon walk in the grass and on the water of the map instead of appearing at random steps. WILD POKEMON sets how many (Off, Few 2, Normal 4, Many 6; an Old 3DS shows at most 2 at once, a New 3DS up to 6), WILD DISPLAY chooses the Pokémon or a silhouette, and AGGRESSIVE sets how many of them notice you and chase (Off, 10%, 20%, 30%, 40%); a much stronger lead makes them flee. The battle starts with the Pokémon you saw (species, level, gender, nature, shininess, or the roamer), and Repel works as it does for step encounters. The game's own step encounters stay where wilds cannot appear (Safari Zone, Randomlocke, no free room on the map). Ideas from PR #83 by mcl5500/Sh3ld0r and the design of Wilds of Hoenn by poooooby; credited, and none of their code is used.
- **Nuzlocke options, version 2.** One wild catch per area, fainted Pokémon released after the battle (announced in the game's message box), a new game clears the areas used, DUPLICATES and SHINY clauses, and LEVEL CAPS, on their own NUZLOCKE page of OPTIONS.
- **RANDOMLOCKE.** Wild and trainer Pokémon become random species, fixed by the save's Trainer ID; the starter in Oak's lab is random too.
- **REUSABLE TMS** (a TM is not used up when it teaches a move) and **FREE HMS** (an HM in the Bag works in the field with no Pokémon that knows it) in OPTIONS.
- **REMEMBER** in the party menu: FireRed's own move relearner, free and from anywhere, now drawn on the lower screen in every step (the move list, the Yes and No questions and the messages).
- **A world map on the lower screen's HOME page** built at run time from your own Region Map art: three zoom levels with + and - buttons, drag to pan, and a ribbon that cycles Kanto and the Sevii Islands. Fly works from the map shown, so Kanto and the Sevii destinations you have visited can be picked from either.
- **Native lower-screen screens for the mail and the Easy Chat**: the letter (with the stationery's name and who wrote it, and a tap on the letter closes it) and the phrase picker (phrase chips, groups, letters, the word list, Yes and No).
- **Pokédex page buttons** AREA and CRY on the entry's card, and an **EXP bar** on the Summary's STATS page.
- **RUN is greyed** when fleeing is impossible (trainer battles), and holding it keeps trying to flee.

### Improved

- **OPTIONS is organised in sections** (Settings, Graphics, Quality of life, Nuzlocke); the page button names the section. Switches have no arrows, each page has a colour edge and the page dots carry the page's name. Texts were shortened to fit their notes, and a test now measures that every label fits in English, Spanish and German.
- **Battle menu:** the lead's type badges in the FIGHT banner, a framed lead ball, crossed-out fainted balls, a colour for each condition, POKEMON showing how many are able, and the active Pokémon centred on its plate. The healthboxes lose the ball on the player's cap.
- **Bag:** the scroll rails are sliders; a tap jumps to where it was pressed and a drag follows the finger, in the field and the battle Bag.
- **Wording:** names of species, items, moves, types and maps, and the English and Spanish interface words, are in proper case like German; the player's name is shown as typed.
- **Shorter messages on the Save screen** that fit their note.
- **Builder and web Builder:** a failed download no longer hangs or reports success, Cancel works during a download and the free-space check is real. The old Ash and Green switch is gone from both Builders (the looks are picked in the game's OPTIONS).

### Fixed

- **Field people and Pokémon that share a picture** (for example the three Bird Keepers of Route 18, or two wilds of one species) were drawn with each other's frame: a walker slid without steps, or a trainer was drawn looking another way than it really looked. Each frame now has its own tiles. Checked on a computer only; it has not been run in the game yet.
- **A battle animation** that moved a sprite through an address read with sign extension (issue #85) now builds the address from two 16-bit halves.
- **The evolution scene:** a B held since before the scene no longer stops the evolution, and the scene uses FireRed's plain background.
- **German text:** a double-encoded u-umlaut in the Summary's and the PC's German text.
- **Party action menu:** with more than 4 actions the menu is drawn in two columns and the D-pad now moves through them (up and down inside a column, left and right between columns). Checked on a computer only.
- **REMEMBER** no longer falls back to the GBA screen on its questions and messages. Checked on a computer only; it has not been run in the game yet.
- **Labels that did not fit** their room on several lower-screen menus.
- **Menus:** the touch state of a closed screen no longer survives into the next one, and an empty Pokédex list is not scrolled.
- **Battle scene:** the HD battle picture follows the screen's fades, so the end of a battle shows no black silhouettes.
- **Saving:** the save is committed with fsync, and a pack with a truncated back picture, or a follower asked for before the pack opens, no longer breaks the game.
- **Looks:** an HGSS look whose own walk sheet did not download is left out instead of being made from its sibling's.

### Performance

- **No stall at doors.** The day and night camera search was one frame of about 47 ms (Old 3DS) at each door; it now takes 16 frames of about 3 ms.
- **Cheaper lower screen:** fewer redrawn rectangles for the embers, less text work per frame, one Summary read a frame, and cached battle types. The FPS and fast-forward counters and the battle HUD draw runs of pixels, and the pack CRC32 uses a table.
- **The HOME map image** is built on a low-priority worker thread, so changing region no longer slows the game.

### Compatibility

- **German:** a 1-byte alignment fix in the German recipe (ViridianCity_Mart_Items and SayHiToOak), by mcl5500 (PR #84).
- **Settings file:** `settings.cfg` has a 512-byte block and still reads old 256-byte files. An old `visiblewilds=1` reads as Normal.
- **Packs:** build `twinembers.pak` again with the Builder of this release; it is made from your own ROM and no ROM or game data is distributed. The English, Spanish and German games are `twinembers.3dsx`, `twinembers-es.3dsx` and `twinembers-de.3dsx`.

### Interface

- **Sharper START-column icons** (Pokédex, Poké Ball, Bag, Trainer Card, Save, Options) redrawn pixel by pixel at 16x16 with colour ramps and an outline in each icon's own dark tone, with a test that every word fits beside its icon in English, Spanish and German.
- **A new look for the lower screen** (the ember look, raised cream cards, flame accents and slate edges) on OPTIONS, HOME, the field and battle Bag, SAVE, the PC box and naming keyboard, the Pokédex, the Trainer Card, the Summary, the move-learn screens, the mail and the battle menus. The top screen's healthboxes and dialog panel match it.

### Known Issues

- **Visible wilds on an Old 3DS:** the frame time with 2 visible wilds has not been measured on a console (`port.log` `wilds:` lines are pending).
- **Visible wilds, edge cases not individually verified:** caves and Viridian Forest, the Pokémon Tower silhouettes, the Safari Zone, Randomlocke (step encounters should stay or come back), save and load with a wild on screen, and crowded maps (Celadon, Saffron) with 6 wilds. The owner played the visible wilds on real hardware and their general operation is correct; the list is in `docs/HARDWARE-ACCEPTANCE.md`, Overworld.
- **The 7 largest follower Pokémon** (Steelix, Lugia, Ho-Oh, Wailord, Kyogre, Groudon, Rayquaza) are drawn at their HeartGold size and cover neighbouring tiles.
- **The shared-picture fix, the party menu D-pad and the native REMEMBER screens** have only been tested on a computer, not yet in the game or on a console.
- **Day and night and the companion** are implemented but still have no per-console result recorded; Old 3DS and 2DS models, long sessions and late-game content are not systematically tested (see `docs/VALIDATION.md`).
- **Real hardware coverage** is narrow: one New 3DS-family console and Azahar. Nothing else listed as unchecked in `docs/HARDWARE-ACCEPTANCE.md` should be taken as working.
- **The Safari Zone sign** still mentions START.
- **Repository CI:** the `source_audit.py --history` step currently fails on historical commits. It is not a game issue and does not change the release files.

## Earlier versions

Older releases and their notes are on the [GitHub releases page](https://github.com/alex61194/twin-embers/releases).
