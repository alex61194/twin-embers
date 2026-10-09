# Real Nintendo 3DS acceptance checklist

For the owner, on real hardware, with the exact candidate `twinembers.3dsx` (record
its SHA-256) and a pack built by the Builder from your own supported ROM. Keep a
copy of every save you use. Record the model (Old/New 3DS, 2DS), system version and
Homebrew Launcher entry point. Mark each line PASS or FAIL with a short note; a
FAIL should include the screen text or `/3ds/twinembers/port.log` lines.

## Setup

- [ ] `/3ds/twinembers/` holds only `twinembers.3dsx` and `twinembers.pak`.
- [ ] Builder printed ABI `8f71cf3a` and `verify-pak` passed.
- [ ] `/3ds/dspfirm.cdc` exists (made once by DSP1); without it the game runs silently.

## Starting and saving

- [ ] New game (no save anywhere): intro, title, Oak speech and naming screen run.
- [ ] First save of that new game asks "Would you like to save your adventure?"
      (no "different game file" warning on a blank card).
- [ ] Save in the field, power off, restart: Continue resumes at the saved spot.
- [ ] Existing save: copy a known FireRed save to `/3ds/pokefirered/firered.sav`
      (no `twinembers.sav`); the game copies it once and shows your progress; the
      original file is unchanged.
- [ ] With both `/3ds/twinembers/twinembers.sav` and an old save present, the
      Twin Embers save is used.
- [ ] SAVE screen reports success only after writing; a write-protected card
      shows a save error instead.

## Interface

- [ ] PokéTouch lower screen: Party, Bag, Summary, Pokédex, Trainer Card, Town
      Map, OPTIONS and SAVE open, respond to touch and close cleanly.
- [ ] OPTIONS shows seven rows; every change survives a save and restart.
- [ ] PC storage: move, withdraw and deposit Pokémon by touch.

## Gameplay

- [ ] Several wild and trainer battles: menus, moves, switching, Pokémon cries.
- [ ] Fly to two towns; Surf across water; Flash inside Rock Tunnel.
- [ ] Enter and leave buildings, caves and route connections without glitches.
- [ ] Map edges at Pallet Town, Route 1 and a coastal route look continuous.

## Audio and performance

- [ ] Music and effects sound clean and stay in sync for 15+ minutes.
- [ ] FPS counter stays at about 60 in towns, routes and battles; no audio dropouts.
- [ ] Turbo speeds play up and returns to normal speed cleanly.
- [ ] Close the lid during play and reopen: game, audio and touch resume.

## Data pack errors

- [ ] Rename `twinembers.pak`: the missing-pack screen is readable (separated,
      whole-word lines) and A exits.
- [ ] Use a pack from a different release or a damaged copy: the mismatch screen
      appears and A exits; no save is touched.

Hardware results are evidence only when recorded with the candidate SHA-256. They do
not change `binary_release_approved` or the separate rights review.
