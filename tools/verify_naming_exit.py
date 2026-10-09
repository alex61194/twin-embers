#!/usr/bin/env python3
"""Regression check for the naming-screen exit lifetime bug (3DS data abort).

Crash: CB2_NamingScreen runs RunTasks() -> MainState_Exit() frees
sNamingScreen, then the SAME frame continues into AnimateSprites() whose
cursor callback reads sNamingScreen->currentPage (FAR 0x1e22 with a NULL
base). VBlankCB_NamingScreen dereferences it too until the next screen
installs its own VBlank callback. On GBA the stale reads hit BIOS bytes
benignly; on an MMU they abort.

Invariant pinned here: MainState_Exit must retire the screen's live
callbacks (VBlank uninstall + sprite teardown) BEFORE FREE_AND_SET_NULL.
"""
import re
import sys
from pathlib import Path

SRC = Path(__file__).resolve().parent.parent / "src" / "naming_screen.c"


def extract_body(text, name):
    m = re.search(r"static bool8 " + name + r"\(void\)\s*\{", text)
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
    body = extract_body(text, "MainState_Exit")
    free = body.find("FREE_AND_SET_NULL(sNamingScreen)")
    if free < 0:
        raise AssertionError("FREE_AND_SET_NULL(sNamingScreen) not found")
    before = body[:free]

    if "ResetVHBlank()" not in before and "SetVBlankCallback(NULL)" not in before:
        raise AssertionError(
            "MainState_Exit must uninstall VBlankCB_NamingScreen "
            "before freeing sNamingScreen")
    if "ResetSpriteData()" not in before:
        raise AssertionError(
            "MainState_Exit must tear down sprites before freeing "
            "sNamingScreen (AnimateSprites runs later in the same frame)")

    # No use of the freed state after the free within this function.
    after = body[free + len("FREE_AND_SET_NULL(sNamingScreen)"):]
    if "sNamingScreen" in after:
        raise AssertionError("sNamingScreen used after free in MainState_Exit")

    print("PASS naming exit: VBlank/sprite teardown precedes free")


if __name__ == "__main__":
    try:
        main()
    except AssertionError as e:
        print(f"FAIL naming exit: {e}")
        sys.exit(1)
