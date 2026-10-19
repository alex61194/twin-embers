# Zallax upstream review

Twin Embers inherits its backend, compositor, input and pack code from
[ZallaxDev/pokeemerald-3Ds-dualscreen](https://github.com/ZallaxDev/pokeemerald-3Ds-dualscreen)
(revisions `1f3812d` and `6419a40`, see NOTICE.md). This review read the 148 commits after
`1f3812d` up to `e80ba61` (2026-10-10). The license file is unchanged in that range
(`LICENSE-PORT.md` blob `5f840bd`). Each change was checked against the Twin Embers code: does
the same defect exist here, and does the same fix fit.

## Applied (branch `fix/zallax-derived-fixes`)

| Twin Embers commit | Zallax | Defect in Twin Embers | Status |
|---|---|---|---|
| `85ca019` audio | `6da8434`, `1bc0a53` | FireRed's 81 fixed-pitch instruments (drums) played 2.36x too fast at the native 31536-Hz rate | Fixed (own implementation, patch 0116); reproduced and verified by running the real mixer in unicorn; NEEDS HARDWARE LISTENING |
| `36f95b1` battle | `c10e391` | Hyper Beam read anim 7 of a 7-entry table: an invalid pointer (`0x00010008`) in the 3DS image, a data abort for 1 orb in 8 | Fixed (patch 0117, as the GBA behaved: checked on the ROM); verified in the disassembly; NEEDS HARDWARE CHECK of a Hyper Beam with animations on |
| `e9c511e` battle | `76da700` | The level-up stats box was magnified with the scene and covered by the text box | Fixed (adapted code); compiles; NEEDS HARDWARE VALIDATION |
| `0963168` evolution | `7f0b1bf` | The evolution scene had no callback class and was drawn as the 400x240 field (picture off to the top-left) | Fixed by centring it; Zallax uses its battle class instead. Centred keeps it out of the zoomed battle compositor (the bottom screen is unaffected either way: it follows `CtrBottom_IsBattleMain`, not the compositor class). NEEDS HARDWARE VALIDATION |

## Already covered in Twin Embers

- **OBJ window** (`1bc0a53`): Twin Embers has its own per-pixel OBJ window mask, including the
  stat-change animation (`BuildObjWindow`).
- **INCBIN stubs: Mirage Tower crash and sixteen screens** (`c10e391`, `bfff7b0`): Zallax links
  an INCBIN as a one-byte stub; Twin Embers' REX keeps every table at its real size and resolves
  interior reads (`test_assets_runtime.py`). Mirage Tower is Emerald-only.
- **Poison step** (`7f0b1bf`): Twin Embers draws BG mosaic (`DrawMosaicBg`).
- **Field-move banner** (`6911a10`): `DISPLAY_WIN_RIGHT` already keeps the 8-bit window edge.
- **Shop BUY screen** (`04bcbef`, `7c5d2ab`, `d5c8870`, `572fcc2`): `shop` is a centred GBA screen
  and its metatile tables are real data.
- **Turbo with music at normal speed** (`15d1dfd`): Twin Embers' START turbo already runs the
  sound engine only on shown frames.
- **Log only with debug.txt** (`f1b1281`): already gated; Zallax also keeps the previous session
  in `port-prev.log`.
- **SHOW FPS option** (`48b71d5`): Twin Embers has FPS COUNTER.
- **CheckHeap on every map lookup** (`03e3392`): only Zallax's debug hooks called it.

## Could be adopted (needs a decision: features or larger changes)

- **Soft reset back to the title** (`f4d3aab`, `c6c7355`, by papacult): A+B+START+SELECT and the
  game's own DoSoftReset restart the game like the GBA instead of quitting. Twin Embers saves and
  quits to Homebrew Launcher, which is safe. Adopting it means grouping the game's `.data`/`.bss`
  in the linker script, keeping a copy, and re-entering `AgbMain` with setjmp/longjmp, checked
  against REX-loaded tables. Large; risk to the save path; hardware testing required.
- **Run-always toggle** (`42085a1`): run without holding B. Small, but needs a free option bit
  (the two reserved option bits may hold values from an earlier build, so not those).
- **Quick ball** (`06ed7d5`, `9fb696f`): throw the last ball used from the bottom screen or R.
- **ENHANCEMENTS options** (`1f518de`, `2ee8d0a`, `00b936e`, `364df03`, `7615382`, `297e010`):
  EXP for catching, modern Exp Share, trade evolutions at Lv. 40, field HMs without the move,
  visible wild Pokemon. Gameplay changes, each with a saved setting.
- **CHEATS options** (`c1f5e6d` to `d5281bf`): encounter rate, shiny odds, every ball catches,
  instant victory, fast eggs, infinite money, Pokedex, give items.
- **Spanish ROM** (`ceee732`, `2b76d23`): built from the player's own Spanish ROM. FireRed would
  need its own recipe for the Spanish revision; large.
- **ZL/ZR fast-forward on New 3DS** (`15d1dfd`): an extra binding beside START. Small.
- **HOME Menu forwarder** (`bafd82f`, `742c0ad`): a CIA that starts the 3DSX. Distribution
  only, and binary publication stays blocked.
- **Performance on Old 3DS** (`73ba4ad`, `e782474`, `eedd4f4`, `b60b47f`): cell-by-cell layer
  textures, kernel cache flushes, bottom-screen redraw budgets. Worth measuring on an Old 3DS
  before porting; no defect is known.

## Not applicable

Voxel world, 3D battles and stereo planes; PokeNav, Match Call, Mirage Tower, Trainer Hill,
contests and other Emerald-only content; Emerald's intro and title; the web builder and release
checklists; README and issue templates.
