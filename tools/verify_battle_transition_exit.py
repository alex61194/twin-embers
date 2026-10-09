#!/usr/bin/env python3
"""Regression check for the battle-transition exit lifetime bug (3DS abort).

Crash: IsBattleTransitionDone() freed sTransitionData while one of the
VBlankCB_* scanline callbacks (e.g. VBlankCB_ClockwiseWipe on the rival
battle) was still installed. The next VBlank read sTransitionData->vblankDma
through the NULL base (FAR 0x0). On GBA the stale reads hit BIOS bytes
benignly; on an MMU they abort. HBlankCB_Mugshots is the only HBlank
variant touching the state; DMA0 is left programmed with HBLANK repeat.

Invariant pinned here: IsBattleTransitionDone must retire the VBlank
callback, the HBlank callback and DMA channel 0 BEFORE
FREE_AND_SET_NULL(sTransitionData).
"""
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "battle_transition.c"


def extract_body(text, name):
    m = re.search(r"bool8 " + name + r"\(void\)\s*\{", text)
    if not m:
        raise AssertionError(f"{name} not found")
    depth = 0
    for i in range(m.end() - 1, len(text)):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[m.end():i]
    raise AssertionError(f"unbalanced braces in {name}")


def main():
    text = SRC.read_text(encoding="utf-8", errors="replace")
    body = extract_body(text, "IsBattleTransitionDone")
    free = body.find("FREE_AND_SET_NULL(sTransitionData)")
    if free < 0:
        raise AssertionError("FREE_AND_SET_NULL(sTransitionData) not found")
    before = body[:free]

    if "SetVBlankCallback(NULL)" not in before:
        raise AssertionError(
            "IsBattleTransitionDone must uninstall the VBlankCB_* callback "
            "before freeing sTransitionData")
    if "SetHBlankCallback(NULL)" not in before:
        raise AssertionError(
            "IsBattleTransitionDone must uninstall the HBlank callback "
            "before freeing sTransitionData")
    if "DmaStop(0)" not in before:
        raise AssertionError(
            "IsBattleTransitionDone must stop the transition's DMA0 "
            "before freeing sTransitionData")

    after = body[free + len("FREE_AND_SET_NULL(sTransitionData)"):]
    if "sTransitionData" in after:
        raise AssertionError("sTransitionData used after free")

    print("PASS battle transition exit: callbacks/DMA retired before free")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as e:
        print(f"FAIL battle transition exit: {e}")
        sys.exit(1)
