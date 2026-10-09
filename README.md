<div align="center">

# Twin Embers Port

### A new home for Kanto. Two screens, one adventure.

**A native Pokémon FireRed port for Nintendo 3DS, designed around its two displays.**

400 × 240 overworld · PokéTouch lower-screen controls · Native battles

[**Get the game**](#play-on-your-3ds) · [**See it in action**](#kanto-on-two-screens) · [**Controls**](#controls) · [**Support**](#help-and-known-issues)

**Developed with AI assistance** — including Claude (Anthropic) and OpenAI Codex, under the direction and review of the project maintainer. [How AI was used](#development-and-ai-assistance).

</div>

---

> [!IMPORTANT]
> **First beta: v0.1.0-beta.1.** Download the **3DS game** and **Windows Builder** as two separate files from the [latest GitHub Release](https://github.com/alex61194/twin-embers/releases/latest). You need a dump of your own supported FireRed cartridge to generate the game-data pack. Back up your saves before updating.

## What is Twin Embers?

Twin Embers brings Pokémon FireRed to the Nintendo 3DS as a **native ARM11 port**, not a GBA emulator.

The upper screen expands Kanto's overworld to **400 × 240**. The lower touch screen hosts **PokéTouch**: a home for your Pokémon, Bag, Pokédex, Trainer Card, Town Map, saving, options and battle commands. You can keep exploring without the original START-menu workflow.

The game executable and its assets are separate: the **Twin Embers Builder** reads your own compatible ROM locally and creates `twinembers.pak`. The project does not provide a ROM or a pre-generated data pack.

## Kanto on two screens

<table>
  <tr>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/media/02-overworld-pallet-town.png" width="320" alt="Pallet Town on the top screen and PokéTouch below"><br><sub><b>Exploration</b> — expanded overworld and PokéTouch</sub></td>
    <td align="center" width="50%"><img src="https://github.com/alex61194/twin-embers/releases/download/media/03-battle-menu.png" width="320" alt="Touchscreen battle commands"><br><sub><b>Battles</b> — touch-driven commands</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://github.com/alex61194/twin-embers/releases/download/media/04-battle-moves.png" width="320" alt="Move selection with type power and PP"><br><sub>Move selection</sub></td>
    <td align="center"><img src="https://github.com/alex61194/twin-embers/releases/download/media/05-party.png" width="320" alt="Pokémon party menu"><br><sub>Party</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://github.com/alex61194/twin-embers/releases/download/media/06-summary.png" width="320" alt="Pokémon Summary on the lower screen"><br><sub>Pokémon Summary</sub></td>
    <td align="center"><img src="https://github.com/alex61194/twin-embers/releases/download/media/07-trainer-card.png" width="320" alt="Trainer Card"><br><sub>Trainer Card</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://github.com/alex61194/twin-embers/releases/download/media/09-pokedex.png" width="320" alt="Pokédex"><br><sub>Pokédex</sub></td>
    <td align="center"><img src="https://github.com/alex61194/twin-embers/releases/download/media/08-options.png" width="320" alt="Options screen"><br><sub>Options</sub></td>
  </tr>
  <tr>
    <td align="center"><img src="https://github.com/alex61194/twin-embers/releases/download/media/01-title.png" width="320" alt="FireRed title screen"><br><sub>Title</sub></td>
    <td align="center"><img src="https://github.com/alex61194/twin-embers/releases/download/media/10-fly-two-island.png" width="320" alt="Fly sequence at Two Island"><br><sub>Travel</sub></td>
  </tr>
</table>

<sub>Genuine captures from Azahar and a New 3DS-family console. Native displays: 400 × 240 above and 320 × 240 below. Game imagery remains the property of its respective rightsholders.</sub>

## Play on your 3DS

### You will need

- A Nintendo 3DS or 2DS with **Homebrew Launcher**.
- A Windows computer and your console's SD card.
- Your own dump of **Pokémon FireRed (USA), Revision 0**. Other regions and revisions are not supported.

### Install in four steps

**1 — Download the two files separately** from [Twin Embers v0.1.0-beta.1](https://github.com/alex61194/twin-embers/releases/tag/v0.1.0-beta.1):

| Download | Use |
|---|---|
| [`twinembers.3dsx`](https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/twinembers.3dsx) | The Nintendo 3DS executable |
| [`TwinEmbersBuilder.exe`](https://github.com/alex61194/twin-embers/releases/download/v0.1.0-beta.1/TwinEmbersBuilder.exe) | Windows tool to make the data pack from your ROM |

**2 — Build your data pack.** Connect the SD card to your PC, open the Builder, select your FireRed ROM, click **SD folder…** and choose the **root of the SD card**. Click **Build Data Pack**. The tool checks the ROM and writes and verifies `/3ds/twinembers/twinembers.pak`.

**3 — Copy the executable.** Put `twinembers.3dsx` next to the pack in `SD:/3ds/twinembers/`.

**4 — Launch.** Return the card to your console, open Homebrew Launcher and select **Twin Embers**.

Your SD card will look like this:

```text
SD:/
└── 3ds/
    ├── dspfirm.cdc
    └── twinembers/
        ├── twinembers.3dsx
        ├── twinembers.pak
        └── twinembers.sav  (created by the game)
```

**Sound:** if the game is silent, check that `SD:/3ds/dspfirm.cdc` exists. You can generate it with the DSP1 homebrew. Most custom-firmware setups already have it.

**Windows warning:** the Builder does not require administrator privileges or an internet connection. If SmartScreen warns about the executable, check its SHA-256 against the [release notes](https://github.com/alex61194/twin-embers/releases/tag/v0.1.0-beta.1) and only run software you trust. A matching hash is not a safety certification.

The Builder **does not include, download or create the 3DSX**. No ROM or ready-made PAK is distributed.

## What you can do

| Feature | Experience |
|---|---|
| **Wider Kanto** | Explore FireRed's original story, towns and routes with a 400 × 240 upper-screen overworld |
| **PokéTouch** | Use the lower screen for Pokémon, Bag, Summary, Pokédex, Trainer Card, Town Map, SAVE and OPTIONS |
| **Touchscreen battles** | FIGHT, BAG, POKéMON and RUN, with move cards showing type, power and PP |
| **PC storage** | Move, withdraw and deposit Pokémon with touch controls |
| **Turbo** | Cycle **2× → 3× → 4× → normal** with START during exploration and battles |
| **FPS display** | Optional counter in OPTIONS |
| **Save continuity** | Use `twinembers.sav`; a legacy `/3ds/pokefirered/` save can be copied without modifying the original |
| **Pack checks** | Missing or incompatible data packs show a clear error screen instead of proceeding |

### Controls

| Button | Action |
|---|---|
| Circle Pad / D-Pad | Move and navigate |
| A | Interact / confirm |
| B | Cancel; hold to run after you obtain Running Shoes |
| START | Cycle turbo: 2×, 3×, 4×, off |
| SELECT, L, R | Original FireRed functions |
| X | Use registered item |
| Y | Bicycle |
| Touch screen | PokéTouch menus, battle commands and PC storage |

The lower-screen menu replaces the original START menu. Some original in-game hints have not yet been updated.

## Supported systems and ROM

| Requirement | Details |
|---|---|
| Hardware | Nintendo 3DS and 2DS family, via Homebrew Launcher |
| Console testing | Author-reported testing on a New 3DS-family console; other models are not yet covered |
| Emulator | Azahar: a new game through the first rival battle and a Route 1 wild battle, plus several menus, saving, Continue and turbo |
| Accepted ROM | **Pokémon FireRed (USA), Revision 0**, 16 MiB (16,777,216 bytes) |
| SHA-1 | `41cb23d8dccc8ebd7c649cd8fbb58eeace6e2fdc` |

The Builder rejects Revision 1, LeafGreen, other languages, modified ROMs and trimmed dumps. No ROM download links are provided.

Some UI systems have automated test coverage but have not been fully exercised end-to-end on all real console models. See [validation](docs/VALIDATION.md) and the [hardware checklist](docs/HARDWARE-ACCEPTANCE.md).

## Updating without losing your save

1. Back up `SD:/3ds/twinembers/twinembers.sav` to your PC.
2. Replace the old `twinembers.3dsx` with the one from the new release.
3. Use the **matching release's** Builder to regenerate the PAK if required. Confirm replacement when prompted; the old verified pack is kept until the new one is ready.
4. Leave the save in place and launch the game.

## Help and known issues

| Problem | What to check |
|---|---|
| The game is missing in Homebrew Launcher | Check `SD:/3ds/twinembers/twinembers.3dsx` |
| “Data pack missing” | Use **SD folder…** and choose the SD card's root in the Builder |
| Pack does not match the engine | Rebuild it with the Builder from the same release |
| “Unsupported ROM” | Verify FireRed USA Revision 0 and the SHA-1 above |
| Card is full or locked | Free at least 32 MiB and check the write-protect switch |
| No sound | Check `SD:/3ds/dspfirm.cdc` |
| Save read/write error | Check the SD card; preserve your backup |

**Known limitations:** only the listed FireRed revision is supported. Some original messages still instruct you to use START for the menu. Before receiving the Pokédex, its lower-screen button can appear active inside another menu; tapping it returns to the main lower screen.

To report a bug, [open an issue](https://github.com/alex61194/twin-embers/issues) with your 3DS model (or emulator), steps to reproduce and any exact on-screen errors. Creating an empty `/3ds/twinembers/debug.txt` enables `port.log` diagnostics. **Never attach ROMs, PAKs, save files or other game data.**

## Development and AI assistance

**Twin Embers Port was developed with substantial AI assistance.** Claude (Anthropic) and OpenAI Codex were used during development for code implementation and adaptation, debugging, tests, reviews and documentation. The project is directed and maintained by **[alex61194](https://github.com/alex61194)**; AI-assisted work is reviewed and tested as part of the project workflow.

AI assistance does **not** replace attribution to original human authors and does not confer rights to third-party code or Pokémon material. Details are recorded in [AI_DISCLOSURE.md](AI_DISCLOSURE.md).

### Source and technical documentation

- [Build the 3DSX from source](docs/BUILDING.md)
- [Windows Builder usage and tests](builder/README.md)
- [Technical details: pack format, ABI and saves](docs/TECHNICAL-NOTES.md)
- [Validation](docs/VALIDATION.md) · [Contributing](CONTRIBUTING.md) · [Security](SECURITY.md)

The source archives attached automatically to the `v0.1.0-beta.1` tag describe that tagged snapshot; documentation on `main` may be newer.

## Credits, licenses and copyright

This README has its **own presentation and organization**. Credits below acknowledge actual code, tools and rights, not a template for the README.

- **Original Twin Embers contributions:** [alex61194](https://github.com/alex61194), with [disclosed AI assistance](AI_DISCLOSURE.md). The [port's MIT license](LICENSE-PORT.md) applies only to original contributions the maintainer owns.
- **Inherited and adapted dual-screen implementation:** **ZallaxDev and the Pokémon Emerald 3Ds Dual Screen contributors** created MIT-licensed backend, compositor, input/decoding and data/pack components used in this port. Their original copyright notices and applicable license terms remain intact in [NOTICE.md](NOTICE.md), [CREDITS.md](CREDITS.md) and [licenses/](licenses/).
- **Game decompilation:** **pret/pokefirered contributors** provide the upstream code fetched at a pinned commit when building locally. No general redistribution license for the game's decompiled source was identified. The port's MIT license does not relicense that code.
- **Toolchains and runtimes:** **devkitPro, libctru, citro2d, citro3d, GCC/newlib, Python, Tcl/Tk and PyInstaller** retain their respective notices. See [NOTICE.md](NOTICE.md), [PROVENANCE.md](PROVENANCE.md) and the [Builder's notices](builder/Windows-GUI-THIRD-PARTY.txt).
- **Pokémon intellectual property:** Pokémon FireRed, its characters, artwork, music and trademarks belong to their respective rightsholders, including Nintendo, Game Freak, Creatures and The Pokémon Company.

For precise attribution and licensing scope, consult **[CREDITS.md](CREDITS.md)**, **[NOTICE.md](NOTICE.md)**, **[LICENSE-PORT.md](LICENSE-PORT.md)**, **[PROVENANCE.md](PROVENANCE.md)** and **[licenses/](licenses/)**. These notices are not optional.

### Unofficial-project disclaimer

Twin Embers Port is an independent, unofficial fan project and is **not affiliated with or endorsed by Nintendo, Game Freak, Creatures or The Pokémon Company**.

The repository does not include any ROM, extracted game graphics/audio/data pack or user save. The independently downloadable `twinembers.3dsx` contains compiled code from the `pret/pokefirered` decompilation, whose redistribution rights have **not been independently verified**. Publication records the maintainer's decision and does not imply rightsholder permission; see [distribution policy](docs/DISTRIBUTION.md). No license to the original game or its decompiled game code is granted or implied.
