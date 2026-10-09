#!/usr/bin/env python3
"""Regression check for the new-game black-screen hang on ARM11.

DoMapLoadLoop / DoLoadMap_QLPlayback run map-load steps in a tight loop.
One step waits for the DMA3 queue, which only VBlankIntr drains. The GBA's
VBlank IRQ fires underneath the loop; ARM11 has no asynchronous IRQ.

Invariant pinned here: on PLATFORM_3DS both loops call VBlankIntrWait()
when a step makes no progress, and the GBA branch keeps the original loop.
"""
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "overworld.c"


def body(text, name):
    m = re.search(r"static void " + name + r"\(u8 \*state\)\s*\{", text)
    if not m:
        raise AssertionError(f"{name} not found")
    depth = 0
    for i in range(m.end() - 1, len(text)):
        depth += {"{": 1, "}": -1}.get(text[i], 0)
        if depth == 0:
            return text[m.end():i]
    raise AssertionError(f"{name} unterminated")


def check(text, name, step):
    b = body(text, name)
    ds, _, rest = b.partition("#ifdef PLATFORM_3DS")
    arm, sep, gba = rest.partition("#else")
    if not sep or "#endif" not in gba:
        raise AssertionError(f"{name}: missing PLATFORM_3DS branch")
    for needle in ("u8 step = *state;", f"if ({step}(state", "if (*state == step)",
                   "VBlankIntrWait();"):
        if needle not in arm:
            raise AssertionError(f"{name}: ARM11 branch lacks {needle!r}")
    if not re.search(r"while \(!" + step + r"\(state", gba):
        raise AssertionError(f"{name}: GBA loop changed")


def main():
    text = SRC.read_text(encoding="utf-8")
    check(text, "DoMapLoadLoop", "LoadMapInStepsLocal")
    check(text, "DoLoadMap_QLPlayback", "LoadMap_QLPlayback")
    print("PASS map load: ARM11 steps without progress wait for VBlank")
    return 0


if __name__ == "__main__":
    sys.exit(main())
