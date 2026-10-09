# Trainer ID and RNG initialization on 3DS

## Root cause and original behavior

Investigated against `pret/pokefirered@037335f4c725d7c9aecdac87066f2002b4bd7e14`
and port main `6c23965e7714a9bb1d29caa7501d481fd5004c6b`.

- [`src/main.c`](https://github.com/pret/pokefirered/blob/037335f4c725d7c9aecdac87066f2002b4bd7e14/src/main.c):
  `StartTimer1` enables the free-running GBA Timer 1. `SeedRngAndSetTrainerId`
  samples its 16-bit counter, passes that same value to `SeedRng`, stops the
  timer, and stores the lower Trainer ID in `gTrainerId`.
- [`src/title_screen.c`](https://github.com/pret/pokefirered/blob/037335f4c725d7c9aecdac87066f2002b4bd7e14/src/title_screen.c):
  title setup starts the timer; leaving the title after its cry/fade samples it
  before save pointers are set and the existing save is loaded.
- [`src/naming_screen.c`](https://github.com/pret/pokefirered/blob/037335f4c725d7c9aecdac87066f2002b4bd7e14/src/naming_screen.c):
  entering the **player** keyboard starts it again, and keyboard exit samples
  it again. Rival names, nicknames and box names do not seed the RNG. Choosing
  a preset player name retains the title sample.
- [`src/new_game.c`](https://github.com/pret/pokefirered/blob/037335f4c725d7c9aecdac87066f2002b4bd7e14/src/new_game.c):
  `InitPlayerTrainerId` combines the generated lower half with an upper half
  from `Random()`, then writes four little-endian bytes into SaveBlock2. The
  upper half is the secret ID; the visible ID is the lower half.
- [`src/random.c`](https://github.com/pret/pokefirered/blob/037335f4c725d7c9aecdac87066f2002b4bd7e14/src/random.c):
  the original 32-bit LCG is retained; `SeedRng` accepts a 16-bit seed and
  `Random` advances it and returns its upper half. VBlank also advances it.

On 3DS the register macros point into `gGbaShadow.regs`, not hardware. Reset
zeros the bank; writing the Timer 1 enable bit does not increment its counter.
Both sampling calls consequently overwrite the live RNG with zero and set
the visible Trainer ID to zero. A varying seed at native startup would still
be overwritten at title exit, and fixing only the title would still be
overwritten after typing a player name. The shared sampling function and its
two callers, rather than only the absent timer, are the complete failure path.
Automated tests reproduce both overwrites with original pinned functions.

## Correction

Recipe `0115-native-rng-entropy.json` substitutes `CtrRng_GetSeed` for the
counter read **only on PLATFORM_3DS**. It stores line references and verified
before/after hashes, not copied game-source context. The original GBA branch,
seed/ID relationship, timing of the two calls, LCG, VBlank calls, upper-ID
construction and all save logic remain unchanged.

The adapter opens libctru's PS service, requests eight random bytes with
[`PS_GenerateRandomBytes`](https://github.com/devkitPro/libctru/blob/master/libctru/include/3ds/services/ps.h),
and closes every successful open, including generation failures. It folds all
bits of the result, ARM11 system ticks, system milliseconds and a process-local
sample counter into a 16-bit value. On open/generation failure it discards any
partial random buffer and uses clocks plus the counter. These APIs are not
specific to New 3DS; there is no frame hook, HID scan, heap allocation, SD write,
retained service handle or gameplay reseeding. The fallback is for variation,
not cryptographic security. If all external entropy were frozen across process
restarts, it could not guarantee a different first seed.

Seeds remain in FireRed's 65,536-value domain. Collisions and a legitimate
`00000` remain possible; suppressing zero or promising unique IDs would change
the original distribution. Existing IDs, including buggy zeros, are not repaired
or regenerated. The title sampling only changes transient `gTrainerId`/RNG;
Continue loads the stored ID. Save format, encryption, pack ABI and existing
data pack remain compatible. Audio, PokeTouch and other game systems are untouched.

## Repeatable checks

Use a fresh, ignored tree when source inputs have changed:

```sh
python3 tools/bootstrap.py --dir build/rng-test-upstream
python3 tools/test_rng_initialization.py --tree build/rng-test-upstream -v
make -C build/rng-test-upstream/3ds_port verify-host
make -C build/rng-test-upstream/3ds_port CLEAN_RELEASE=1 verify-game-3dsx
make -C 3ds_port rng-test-3dsx
python3 tools/source_audit.py --history
python3 tools/source_audit.py --release
```

The final command must still exit 2 with **BINARY RELEASE BLOCKED**.
Windows needs the interpreter/devkitPro paths described in BUILDING.md.

Controlled tests cover 66 folding vectors (including high clock/random bits),
PS success, denied initialization, failed/partially written generation,
4,096 fallback samples, 64 reproductions of both original zero overwrites,
256 independent new-game resets with equal frame schedules and both naming
paths, the original LCG/secret-ID construction, 600 gameplay frames per
initialization with no entropy requests, and valid zero-seed behavior.

Save checks assert SaveBlock2 size `0xF24` and ID offset `0xA`, roundtrip four
stored IDs through the production 128 KiB flash implementation, and execute
the actual external save/checksum/slot functions for 12 rotating, alternating
slot roundtrips. On a 64-bit host, the other two serialized blocks are opaque
synthetic bytes with ARM/GBA sizes because host pointers enlarge SaveBlock1.
No actual player save or ROM is a fixture. Full overworld deserialization and
interactive Continue acceptance remain distinct from these slot tests.

The standalone `3ds_port/build/rng-test.3dsx` contains only the owned diagnostic,
entropy adapter and SDK code. It checks fixed vectors, reports PS availability,
samples 32 live seeds into `sdmc:/rng-test.txt`, and exits after 120 frames.
Use it locally; no executable is committed or uploaded. It tests platform
entropy rather than interactive game scenes. Hardware acceptance should cover
separate cold launches, typed and preset player names, Continue with an existing
ID (including zero), saving/relaunching, and normal gameplay on Old/New 3DS.

Original adapter, tests, diagnostic, recipe additions and documentation:
alex61194 with disclosed AI assistance, MIT within LICENSE-PORT.md's owned
contribution scope. External pret/libctru code retains its existing rights.
