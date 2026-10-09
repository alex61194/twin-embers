<div align="center">

# Twin Embers Port

**A native Pokémon FireRed experience for Nintendo 3DS.**

Full-width 400×240 overworld · touch menus on the lower screen · native battle interface

[Install](#installation) · [Features](#features) · [Controls](#controls) · [Troubleshooting](#troubleshooting) · [Build from source](#building-from-source)

</div>

---

Twin Embers Port runs Pokémon FireRed natively on the Nintendo 3DS ARM11 processor.
It is built from the `pret/pokefirered` decompilation, not emulated. The top screen
shows the overworld at the 3DS's full 400×240 width, and the lower screen replaces
the START menu with **PokéTouch**, a set of touch menus.

No game data ships with this project. The **Twin Embers Builder** makes the data
pack on your own computer from **your own** FireRed cartridge dump.

> [!NOTE]
> **Beta.** The Builder is available from the [latest release](https://github.com/alex61194/twin-embers/releases/latest).
> The game executable `twinembers.3dsx` is not distributed: you build it yourself with
> [docs/BUILDING.md](docs/BUILDING.md) ([details](docs/DISTRIBUTION.md)). Back up your save before updating.

## Screenshots

<table>
  <tr>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/01-title.png" width="320" alt="Title screen"><br><sub>Title screen</sub></td>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/02-overworld-pallet-town.png" width="320" alt="Pallet Town with the PokéTouch lower screen"><br><sub>400×240 overworld and PokéTouch</sub></td>
  </tr>
  <tr>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/03-battle-menu.png" width="320" alt="Battle menu"><br><sub>Native battle interface</sub></td>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/04-battle-moves.png" width="320" alt="Move selection"><br><sub>Move cards with type, power and PP</sub></td>
  </tr>
  <tr>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/05-party.png" width="320" alt="Party screen"><br><sub>Party</sub></td>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/06-summary.png" width="320" alt="Pokémon Summary"><br><sub>Pokémon Summary</sub></td>
  </tr>
  <tr>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/07-trainer-card.png" width="320" alt="Trainer Card"><br><sub>Trainer Card</sub></td>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/08-options.png" width="320" alt="OPTIONS screen"><br><sub>OPTIONS</sub></td>
  </tr>
  <tr>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/09-pokedex.png" width="320" alt="Pokédex"><br><sub>Pokédex</sub></td>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/10-fly-two-island.png" width="320" alt="Flying to Two Island"><br><sub>Fly</sub></td>
  </tr>
</table>

<sub>Captured in the Azahar emulator and on a New 3DS-family console (400×240 top screen, 320×240 bottom).</sub>

## Download

| What | Where |
|---|---|
| `TwinEmbersBuilder.exe` (makes the data pack from your own ROM) | [Latest release](https://github.com/alex61194/twin-embers/releases/latest) |
| `twinembers.3dsx` (the game) | Not distributed. Build it yourself: [docs/BUILDING.md](docs/BUILDING.md) |
| Source code (this repository) | **Code → Download ZIP**, or `git clone` |
| Pokémon FireRed ROM | **Not provided.** Use a dump of your own cartridge |

The game and the Builder are always two separate programs. The Builder only makes
the data pack: it does not contain, download or create the game executable.

## Features

Everything below is implemented in the current source. It has been played in the
Azahar emulator and covered by automated host tests. It was also tested on a New 3DS-family
console by the author (see [Compatibility](#compatibility)).

| | |
|---|---|
| **Native ARM11 game** | The FireRed game code compiled for the 3DS, with the original story, maps and progression |
| **400×240 overworld** | The top screen shows the field at the full 3DS width instead of a scaled 240×160 image |
| **PokéTouch** | Lower-screen menu with a Town Map and buttons for POKéDEX, POKéMON, BAG, your Trainer Card, SAVE and OPTIONS |
| **Party and Bag** | Touch-driven Party and Bag screens, the Pokémon Summary, and the TM Case and Berry Pouch |
| **Pokédex and Trainer Card** | The game's Pokédex and Trainer Card, opened from the lower screen |
| **Town Map** | Shown on the lower screen while you explore |
| **SAVE and OPTIONS** | Save from the lower screen. Seven options: text speed, battle scene, battle style, sound, button mode, frame and FPS counter |
| **Battles** | Native lower-screen battle interface: FIGHT, BAG, POKéMON and RUN, plus move cards with type, power and PP |
| **PC storage** | Touch PC boxes: move, withdraw and deposit Pokémon |
| **Turbo** | START cycles 2×, 3×, 4× and normal speed in the field and in battle |
| **FPS counter** | Optional; switch it on or off in OPTIONS |
| **Saves** | A standard FireRed save in `twinembers.sav`, written only after the SD card confirms it. An older `/3ds/pokefirered/` save is copied once and never modified |
| **Audio** | The game's own music and sound engine running on the 3DS, with output through the system DSP |
| **Safe data checks** | A missing or mismatched data pack shows a clear error screen instead of crashing |

## Compatibility

- **Systems:** the Nintendo 3DS and 2DS family (3DS, 3DS XL, 2DS, New 3DS, New 3DS XL,
  New 2DS XL) through the **Homebrew Launcher**.
- **Real hardware:** tested by the author on a New 3DS-family console. Other models are not reported yet.
- **Emulator:** played in Azahar from a new game through the first rival battle and a
  wild battle on Route 1. This covered Party, Summary, Bag, the Trainer Card,
  OPTIONS, SAVE, Continue and turbo. PC storage is covered by host tests. The
  Pokédex screen was not reached in this run and still needs a check on hardware.
- **Audio** needs the 3DS DSP firmware file (see [Installation](#installation)).

## Installation

You need a 3DS with the **Homebrew Launcher**, a Windows PC, and a dump of your own
**Pokémon FireRed (USA)** cartridge ([supported ROM](#supported-rom)).

1. **Build the game.** Follow [docs/BUILDING.md](docs/BUILDING.md) once. It produces
   `twinembers.3dsx`. This takes about 15 minutes, most of it waiting.
2. **Make the data pack.** Start the Builder ([builder/README.md](builder/README.md)),
   choose your ROM, click **SD folder…** and select the **root** of your SD card,
   then click **Build Data Pack**. It checks your ROM, then creates and verifies
   `/3ds/twinembers/twinembers.pak`.
3. **Copy the game.** Put `twinembers.3dsx` into the same folder, `/3ds/twinembers/`.
4. **Play.** Insert the SD card, open the Homebrew Launcher and start **Twin Embers**.

Your SD card should then look like this:

```
SD card
├── 3ds/
│   ├── dspfirm.cdc          ← sound firmware (see below)
│   └── twinembers/
│       ├── twinembers.3dsx  ← the game
│       ├── twinembers.pak   ← made by the Builder
│       └── twinembers.sav   ← created by the game
```

**Sound:** if the game is silent, your SD card has no `/3ds/dspfirm.cdc` yet. Run the
**DSP1** homebrew once on your 3DS to create it. Most custom-firmware setups already
have this file.

## Controls

| Button | Action |
|---|---|
| Circle Pad / D-Pad | Move, choose menu items |
| A | Talk, check, confirm |
| B | Cancel; hold to run once you have the Running Shoes |
| START | Turbo: 2×, 3×, 4×, back to normal (field and battle) |
| SELECT, L, R | As in FireRed |
| X | Use the registered item |
| Y | Get on or off the Bicycle |
| Touch screen | PokéTouch menus, battle commands, PC boxes |

The lower-screen buttons replace FireRed's START menu.

## Supported ROM

Only **Pokémon FireRed (USA), revision 0** works:

| | |
|---|---|
| Size | 16 MiB (16,777,216 bytes) |
| SHA-1 | `41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc` |

The Builder checks this automatically. Revision 1, LeafGreen, other languages,
trimmed dumps and ROM hacks are rejected. No ROM is provided or linked: dump your own
cartridge.

## Updating the game

1. Build the new `twinembers.3dsx` and replace the old one in `/3ds/twinembers/`.
2. Run the new Builder again, because each version needs its matching pack. When it
   asks to replace the existing pack, choose **Yes**; the old pack is kept until the
   new one is verified.
3. Leave `twinembers.sav` where it is. Your progress carries over.

Back up `twinembers.sav` to your PC before updating, just in case.

## Troubleshooting

| What you see | What to do |
|---|---|
| "twinembers data pack missing" | `twinembers.pak` is not in `/3ds/twinembers/`. Run the Builder with **SD folder…** and select the SD card's root. |
| "The data pack does not match this engine" | The pack is damaged or from another version. Run this version's Builder again. |
| Builder says "Unsupported ROM" | Your file is not FireRed (USA) revision 0 or has been modified. Check the SHA-1 above. |
| Builder says the drive is full or cannot be written | Free at least 32 MiB, and check that the SD card's lock switch is not on LOCK. |
| No sound | Create `/3ds/dspfirm.cdc` with DSP1 (see [Installation](#installation)). |
| Save error at startup | The SD card could not be read or written. Check the card; your original saves are not modified. |
| The game does not appear in the Homebrew Launcher | `twinembers.3dsx` must be inside `/3ds/twinembers/`, not in the SD card's root. |

To report a problem, see [Reporting problems](#reporting-problems).

## Known issues

- Only FireRed (USA) revision 0 is supported.
- Saves from other emulators are not converted. The game reads standard 128 KiB
  FireRed flash saves in its own folder.
- In-game hints still say "Press START to open the MENU". In this port the menu is
  on the lower screen, and START controls turbo.
- Before you receive the Pokédex, its lower-screen button can look active while
  another menu is open. Tapping it just returns to the main lower screen.
- No prebuilt executable is distributed (see the status note at the top).

## Building from source

- **Game:** [docs/BUILDING.md](docs/BUILDING.md) gives step-by-step Windows
  instructions with devkitPro and Python.
- **Builder:** [builder/README.md](builder/README.md) covers the GUI, the command line
  and the tests.
- **Technical notes:** [docs/TECHNICAL-NOTES.md](docs/TECHNICAL-NOTES.md) covers the
  pack format, save migration and source checks.
- **Release checks:** [docs/VALIDATION.md](docs/VALIDATION.md) and
  [docs/DISTRIBUTION.md](docs/DISTRIBUTION.md).

Never share the executable you built, your ROM or your data pack: they contain or
are derived from copyrighted game material.

## Reporting problems

Open an issue with your 3DS model (or emulator), the exact screen text and the steps
that led to it. With an empty `/3ds/twinembers/debug.txt` on the SD card, the game
writes a diagnostics log, `port.log`, that you can attach. **Never attach or link a
ROM, data pack, save or other game files.** For security reports see
[SECURITY.md](SECURITY.md); for contributions see [CONTRIBUTING.md](CONTRIBUTING.md).

## Credits and licenses

- **Twin Embers Port** code by alex61194, with disclosed AI assistance
  ([AI_DISCLOSURE.md](AI_DISCLOSURE.md)): MIT, see [LICENSE-PORT.md](LICENSE-PORT.md).
- **ZallaxDev and the Pokémon Emerald 3Ds Dual Screen contributors** wrote the
  dual-screen code this port builds on: MIT, see [NOTICE.md](NOTICE.md) and
  [licenses/](licenses/).
- **pret** made the `pokefirered` decompilation. It is downloaded at a pinned commit
  during the build and is not part of this repository; it carries no license, and
  none is granted here.
- **devkitPro** provided libctru, citro2d, citro3d and devkitARM, which are used to
  build the game under their own licenses.
- **The Builder** bundles Python, Tcl/Tk and PyInstaller. Their license notices ship
  with it ([builder/Windows-GUI-THIRD-PARTY.txt](builder/Windows-GUI-THIRD-PARTY.txt)).

The full list is in [CREDITS.md](CREDITS.md), with the provenance record in
[PROVENANCE.md](PROVENANCE.md).

## Disclaimer

Twin Embers Port is an unofficial fan project. It is not affiliated with, endorsed by
or sponsored by Nintendo, Game Freak, Creatures or The Pokémon Company. Pokémon,
Pokémon FireRed and all related names, characters and content are the property of
their respective owners. Nintendo 3DS is a trademark of Nintendo. These names are used
only to identify compatibility.

This repository contains no ROM, game graphics, audio, game tables, data pack or game
executable. No license to the game, the ROM or the decompiled game source is granted
or implied.
