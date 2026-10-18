<h1 align="center"><img src="docs/media/twin-embers-banner.png" width="600" alt="Twin Embers"></h1>

<p align="center"><sub>Logo by <b>@xsh3d0r_</b> (Discord)</sub></p>

Pokémon FireRed, rebuilt to use both screens of a Nintendo 3DS.

Most of the work in this project comes down to one question: what should the second screen *do* in a game that was designed for one? The answer here is that the top screen stops being a postage stamp and the bottom screen stops being dead weight. Kanto fills the full 400 × 240 panel above, and everything you used to dig through the START menu for now sits under your thumb below.

It is a native ARM11 program, not a GBA emulator wrapped in a launcher. It has no ROM inside it and never will; you bring your own cartridge dump and a small Windows tool turns it into the data file the game reads at startup.

<img src="docs/media/02-overworld-pallet-town.png" width="400" alt="Pallet Town filling the top screen, with the touch menu underneath">

---

## The short version

- **Platform:** Nintendo 3DS / 2DS family through Homebrew Launcher, or from the HOME Menu with the optional shortcut (Luma3DS).
- **Game:** Pokémon FireRed, USA, Revision 0. Nothing else is accepted.
- **You provide:** your own ROM dump, an SD card, a Windows PC for a one-time setup step.
- **You get:** a wide overworld, a touch-driven lower screen, native battles, turbo, and a save file that lives next to the game.
- **Status:** alpha (v0.1.1). Author-tested on a New 3DS-family console and in the Azahar emulator. Other models are untested.

---

## A tour of the two screens

### Walking around

The overworld is drawn at the 3DS's native top-screen size instead of the GBA's 240 × 160, so you see more of every town and route before you reach its edge. Beyond a map's edge the game continues terrain such as shoreline and cliff where that looks natural, and otherwise keeps the original border.

The lower screen is **PokéTouch**, a home screen with tiles for your party, Bag, Pokédex, Trainer Card, Town Map, saving and options. Tapping a tile opens that screen directly. The original START menu is no longer needed.

<img src="docs/media/01-title.png" width="300" alt="Title screen"> <img src="docs/media/10-fly-two-island.png" width="300" alt="Flying to Two Island">

### Fighting

Battle commands move to the touch screen: FIGHT, BAG, POKéMON and RUN. Picking FIGHT shows each move as a card with its type, power and remaining PP, so you can choose without remembering the numbers.

<img src="docs/media/03-battle-menu.png" width="300" alt="Battle commands on the touch screen"> <img src="docs/media/04-battle-moves.png" width="300" alt="Move cards with type, power and PP">

### Looking after your team

The party list, Pokémon Summary and Trainer Card each get a full lower-screen page. PC storage is touch-driven as well: move, withdraw and deposit by tapping.

<img src="docs/media/05-party.png" width="300" alt="Party"> <img src="docs/media/06-summary.png" width="300" alt="Pokémon Summary">

<img src="docs/media/07-trainer-card.png" width="300" alt="Trainer Card"> <img src="docs/media/09-pokedex.png" width="300" alt="Pokédex">

### Settings

OPTIONS holds FireRed's own settings plus a switch for an on-screen FPS counter.

<img src="docs/media/08-options.png" width="300" alt="Options">

<sub>All images are real captures from Azahar and a New 3DS-family console. Native resolutions are 400 × 240 on top and 320 × 240 below. Game artwork belongs to its rightsholders.</sub>

---

## Set it up

Setup is a one-time job of about five minutes. You need three things on the SD card, and only one of them comes from you.

**1. Get the downloads** from the [latest release](https://github.com/alex61194/twin-embers/releases/latest). They are separate files:

- `twinembers.3dsx`, the game itself.
- `TwinEmbersBuilder.exe`, the Windows tool.
- `TwinEmbers-Forwarder.cia`, optional: a HOME Menu shortcut (see below).

**2. Make the data pack.** Put the SD card in your PC and start the Builder. Choose your FireRed ROM, press **SD folder…** and pick the *root* of the card, then press **Build Data Pack**. The Builder checks the ROM, writes `3ds/twinembers/twinembers.pak` and verifies what it wrote. It needs no internet and no administrator rights.

**3. Copy the game next to it.** Place `twinembers.3dsx` in `3ds/twinembers/`.

Put the card back in the console and launch **Twin Embers** from Homebrew Launcher. The first run creates the save. When you are done the card looks like this:

```text
SD:/3ds/
├── dspfirm.cdc            (needed for sound; most custom firmware already has it)
└── twinembers/
    ├── twinembers.3dsx    you copied this
    ├── twinembers.pak     the Builder made this from your ROM
    └── twinembers.sav     the game creates this
```

### HOME Menu shortcut (optional)

`TwinEmbers-Forwarder.cia` puts a **Twin Embers** icon on the HOME Menu. Install it once with FBI or another CIA installer. It contains no game: it starts `SD:/3ds/twinembers/twinembers.3dsx`, so updating the game only ever means replacing the `.3dsx`. It needs Luma3DS; on a New 3DS it runs the game at 804 MHz with the L2 cache, and on an Old 3DS in the default mode. The Homebrew Launcher entry keeps working as before.

If Windows SmartScreen warns about the Builder, compare its SHA-256 with the one in the release notes before running it. A matching hash proves it is the published file, not that the file is safe; only run what you trust.

### The ROM it accepts

| | |
|---|---|
| Game | Pokémon FireRed, USA, **Revision 0** |
| Size | 16,777,216 bytes |
| SHA-1 | `41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc` |

Revision 1, LeafGreen, other languages, modified ROMs and trimmed dumps are refused. No ROM links are provided here or anywhere in this project.

### Coming from an older version

Copy `twinembers.sav` somewhere safe first. Replace the `.3dsx`, rebuild the pack with the Builder *from the same release* if asked (the old pack is kept until the new one is verified), and leave the save where it is. A save from the earlier `/3ds/pokefirered/` layout is copied across on first launch; the original is not modified.

---

## Using it

| Input | What it does |
|---|---|
| Circle Pad, D-Pad | Move, navigate |
| A / B | Confirm, interact / cancel, and hold B to run once you have the Running Shoes |
| START | Turbo: each press steps 2×, 3×, 4×, back to normal |
| X | Use your registered item |
| Y | Get on or off the Bicycle |
| SELECT, L, R | As in the original game |
| Touch | PokéTouch screens, battle commands, PC storage |

Turbo works while exploring and in battle. It skips drawing frames, not game logic, so the game still plays correctly at speed.

---

## When something goes wrong

- **Not listed in Homebrew Launcher.** The file must be exactly `SD:/3ds/twinembers/twinembers.3dsx`.
- **"Data pack missing."** In the Builder, press **SD folder…** and choose the root of the card, not the `3ds` folder.
- **Pack does not match the engine.** Rebuild it with the Builder from the same release as the `.3dsx`.
- **"Unsupported ROM."** Check it is FireRed USA Revision 0 against the SHA-1 above.
- **Builder cannot write.** Free at least 32 MiB and check the card's write-protect switch.
- **No sound.** Look for `SD:/3ds/dspfirm.cdc`.
- **Save error.** Leave the card alone, keep your backup, and report it.

Known gaps: some original dialogue still tells you to use START for a menu that no longer exists, and before you own the Pokédex its lower-screen tile can look active inside another menu (tapping it takes you back home).

To report a bug, [open an issue](https://github.com/alex61194/twin-embers/issues) with your console model or emulator, the steps that trigger it and any text the screen showed. Creating an empty `/3ds/twinembers/debug.txt` makes the game write `port.log`. **Never attach a ROM, a pack, a save or any other game data.**

---

## What is verified, and what is not

Automated tests cover the pack format, the Builder, save handling and its failure cases, the rendering paths for the touch screens and the map edge, and the audio mixer. A fresh build is also checked to confirm it embeds no game data.

Real-hardware coverage is narrower. One New 3DS-family console and Azahar have been used to play through a new game to the first rival battle, a Route 1 wild battle, saving, Continue and turbo. Older 3DS and 2DS models, long play sessions and late-game content have not been systematically tested. The [validation notes](docs/VALIDATION.md) and [hardware checklist](docs/HARDWARE-ACCEPTANCE.md) list exactly what has and has not been exercised.

## How it is put together

The pret/pokefirered decompilation is fetched at a pinned commit when you build from source and is patched by small, structured edit recipes kept in `patches/`, so this repository holds the changes and not the upstream code. The 3DS side (`3ds_port/`) supplies the display compositor, input, audio and file access. The game's graphics, sound and data are not compiled in; they are read at runtime from the pack the Builder produces from your ROM.

- [Build the 3DSX from source](docs/BUILDING.md)
- [Windows Builder](builder/README.md)
- [Pack format, ABI and saves](docs/TECHNICAL-NOTES.md)
- [Contributing](CONTRIBUTING.md) · [Security](SECURITY.md)

---

## Who made this, and under what terms

Twin Embers is directed and maintained by [alex61194](https://github.com/alex61194). **A large share of the code, tests and documentation was written with AI assistance** from Claude (Anthropic) and OpenAI Codex, then reviewed and tested by the maintainer; see [AI_DISCLOSURE.md](AI_DISCLOSURE.md). AI assistance does not stand in for the original authors' credit and gives no rights to anyone else's code or to Pokémon material.

Credit where the work is not ours:

- **ZallaxDev and the Pokémon Emerald 3Ds Dual Screen contributors** wrote the MIT-licensed backend, compositor, input and pack components this port builds on. Their notices are preserved in [NOTICE.md](NOTICE.md), [CREDITS.md](CREDITS.md) and [licenses/](licenses/).
- **@xsh3d0r_** (Discord) drew the Twin Embers logo used for this README and the HOME Menu banner.
- **kwak8709** made the HOME Menu forwarder for Twin Embers.
- **pret/pokefirered contributors** wrote the decompilation. No general redistribution licence for that code was identified, and this project's MIT licence does not cover it.
- **devkitPro, libctru, citro2d, citro3d, GCC/newlib, Python, Tcl/Tk and PyInstaller** keep their own notices ([NOTICE.md](NOTICE.md), [PROVENANCE.md](PROVENANCE.md), [Builder notices](builder/Windows-GUI-THIRD-PARTY.txt)).
- **Pokémon FireRed**, its characters, art, music and trademarks belong to Nintendo, Game Freak, Creatures and The Pokémon Company.

The [port's MIT licence](LICENSE-PORT.md) applies only to original contributions the maintainer owns. Full scope is in [CREDITS.md](CREDITS.md), [PROVENANCE.md](PROVENANCE.md) and [licenses/](licenses/); those notices are not optional.

Twin Embers is an unofficial fan project, not affiliated with or endorsed by any of the companies above. This repository contains no ROM, extracted graphics or audio, data pack or save. The separately downloadable `twinembers.3dsx` does contain compiled code derived from the pret/pokefirered decompilation, whose redistribution rights have not been independently verified; publishing it records the maintainer's decision, not anyone's permission. See the [distribution policy](docs/DISTRIBUTION.md). No licence to the original game or its decompiled code is granted or implied.
