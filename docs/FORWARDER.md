# HOME Menu forwarder

`3ds_port/forwarder` builds `TwinEmbers-Forwarder.cia`: a HOME Menu icon that starts
`sdmc:/3ds/twinembers/twinembers.3dsx`. It is installed once (FBI or similar); an update of the
game still only replaces the 3DSX. It needs Luma3DS (it uses Luma's `hb:ldr`); without Luma it
shows a message and the game is started from the Homebrew Launcher as before.

Adapted from the forwarder of ZallaxDev's pokeemerald-3Ds-dualscreen (MIT, NOTICE.md), made for
Twin Embers by kwak8709, received as a ZIP and reviewed before use.

## How it works

1. The forwarder checks that `twinembers.3dsx` exists and that Luma's `hb:ldr` is on.
2. It sets `hb:ldr`'s target to the 3DSX, with two arguments: the 3DSX path (`argv[0]`, for its
   RomFS) and `twinembers-forwarder:hbldr-tid=<previous Luma 3DSX title>`.
3. It makes its own title Luma's selected 3DSX title and restarts itself; Luma loads the game in
   its place.
4. The game (`main_game_full.c`, `3ds_forwarder.h`) puts the previous title back as its first
   step. Luma applies it when the game exits: the next start of the icon runs the forwarder again,
   and the usual Homebrew Launcher entry keeps working.

What the ZIP lacked: step 4 existed only in Zallax's game. Without it, after one start from the
icon Luma's 3DSX title stayed the forwarder's until the console restarted: the usual Homebrew
Launcher entry stopped opening the launcher, and the icon itself no longer ran its own code.

Safeguards: the game restores a title only from that exact argument (16 hex digits, an
application title; `3ds_port/tests/forwarder_test.c` in `verify-host`), and only on Luma. The
forwarder never passes its own title as the one to restore (a previous start with an older game
could leave it selected); it falls back to Luma's configured 3DSX title.

## Old 3DS and New 3DS

The game runs in the forwarder's process, under its exheader (`forwarder-rsf.yml`):

- Old 3DS: `SystemMode 64MB`, the default mode, 268 MHz. The New 3DS settings are ignored.
- New 3DS: `CpuSpeed 804MHz` and `EnableL2Cache true`. The game also asks for them at start-up
  (`osSetSpeedupEnable`, which does nothing on an Old 3DS), and `ptm:sysm` is allowed for that.
- Memory stays the 64 MB layout on both (`SystemModeExt Legacy`): the game does not need more,
  and the 124 MB mode would take memory from the HOME Menu's applets.

## Artwork

No artwork is stored in the repository: its source audit admits only text, the rule that keeps
game data out. The pictures are made at build time:

- `ICON_SRC=… BANNER_SRC=…`: the owner's icon and logo pictures, kept outside the
  repository. The Twin Embers logo is by @xsh3d0r_ (Discord). `prepare_art.py` (Pillow) fits the icon into 48x48 and the logo into the 256x128
  banner, transparent around it (the HOME Menu theme shows through). The owner's CIA uses a
  WebP logo with real transparency, taken as it is. A JPEG logo with a painted grey
  checkerboard is cleaned instead (background, enclosed checkerboard and light fringe removed,
  the first logo's "3Ds Dual Screen" subtitle cut away: `--cut-subtitle`, `--base-column`). The art draws on Pokemon designs (a Poke Ball, Charizard-like wings): fine
  on one's own console; publishing it is a separate decision.
- Neither: `make_art.py` draws two flames and the name, with nothing from any game.

The game's 3DSX uses the same icon in the Homebrew Launcher, with the name "Twin Embers":
`make -C 3ds_port ICON_SRC=/path/to/icon.jpg` fits the owner's picture with `prepare_art.py`
(icon only, no banner), and without `ICON_SRC` it takes `make_art.py`'s two flames.

The ZIP's icon (official box art) and banner (the Pokemon logo, plus 576 KB of audio of unknown
origin) are not used. The banner sound is make_art.py's two-note chime in both cases.

## Build

```
make -C 3ds_port/forwarder MAKEROM=/path/to/makerom BANNERTOOL=/path/to/bannertool \
    ICON_SRC=/path/to/icon.jpg BANNER_SRC=/path/to/logo.jpg
```

Needs devkitARM with libctru, Python 3, makerom (3DSGuy/Project_CTR) and bannertool. Title ID
`0004000001086500`, product code `CTR-H-TWEM`.

## Checked here, and what needs a console

- The CIA's exheader was read back with ctrtool: title ID, product code, Old 3DS 64 MB prod,
  New 3DS 804 MHz and L2 cache, shared page writing, `ptm:sysm`.
- The game build contains the restore (its `main` reads Luma's shared page) and passes the clean
  release audit.
- Not run on a console or emulator. To check: start from the icon (the game starts); exit and
  start from the icon again (the game starts again, not the Homebrew Launcher); open the usual
  Homebrew Launcher entry (it opens); without the 3DSX on the card (the message); FPS on a New 3DS.
