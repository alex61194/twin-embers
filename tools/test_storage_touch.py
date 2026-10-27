"""Storage touch geometry; no emulator or real game data."""
import os
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

class StorageTouch(unittest.TestCase):
    def test_geometry(self):
        program = r'''
#include <assert.h>
#include <stdio.h>
#include "3ds_storage_touch.h"
int main(void)
{
    for (int i = 0; i < 30; ++i) {
        int x = 100 + i % 6 * 24, y = 44 + i / 6 * 24;
        assert(CtrStorage_BoxHit(x, y, 0) == i);
        /* Every visible row pixel belongs to its icon's Voronoi cell. */
        int top = i / 6 == 0 ? 36 : 32 + i / 6 * 24;
        for (int py = top; py < 56 + i / 6 * 24; ++py)
            assert(CtrStorage_BoxHit(x, py, 0) == i);
    }
    assert(CtrStorage_BoxHit(-1, 0, 0) == ST_HIT_NONE);
    assert(CtrStorage_BoxHit(240, 100, 0) == ST_HIT_NONE);
    /* Live FireRed OAM cursor/arrow coordinates: upper controls first. */
    assert(CtrStorage_BoxHit(162, 12, 0) == ST_HIT_TITLE);
    assert(CtrStorage_BoxHit(92, 28, 0) == ST_HIT_PREV);
    assert(CtrStorage_BoxHit(228, 28, 0) == ST_HIT_NEXT);
    assert(CtrStorage_BoxHit(120, 14, 0) == ST_HIT_BUTTON_PARTY);
    assert(CtrStorage_BoxHit(208, 14, 0) == ST_HIT_BUTTON_CLOSE);
    assert(CtrStorage_BoxHit(110, 151, 0) == 24);
    assert(CtrStorage_BoxHit(110, 152, 0) == ST_HIT_NONE);
    assert(CtrStorage_MenuHit(170, 25, 160, 16, 70, 4) == ST_HIT_MENU_BASE);
    assert(CtrStorage_MenuHit(170, 47, 160, 16, 70, 4) == ST_HIT_MENU_BASE + 1);
    assert(CtrStorage_MenuHit(170, 90, 160, 16, 70, 4) == ST_HIT_NONE);
    assert(CtrStorage_PcHit(20, 25, 5) == ST_HIT_PC_BASE + 1);
    assert(CtrStorage_ChooseHit(124, 88) == ST_HIT_PREV);
    assert(CtrStorage_ChooseHit(160, 96) == ST_HIT_TITLE);
    assert(CtrStorage_ChooseHit(196, 88) == ST_HIT_NEXT);
    assert(CtrStorage_ChooseHit(124, 120) == ST_HIT_NONE);
    assert(CtrStorage_PcHit(20, 88, 5) == ST_HIT_NONE);
    assert(CtrStorage_BoxHit(145, 58, 1) != ST_HIT_NONE);
    assert(CtrStorage_BoxHit(95, 10, 1) == ST_HIT_NONE);
    assert(CtrStorage_BoxHit(104, 64, 1) == ST_HIT_PARTY_BASE);
    for (int slot = 1; slot < 6; ++slot)
        assert(CtrStorage_BoxHit(152, 16 + 24 * (slot - 1), 1) == ST_HIT_PARTY_BASE + slot);
    assert(CtrStorage_BoxHit(152, 132, 1) == ST_HIT_PARTY_BASE + 6);
    puts("storage-touch: all box slots and menu bounds PASS");
    return 0;
}
'''
        with tempfile.TemporaryDirectory() as temp:
            src = Path(temp) / 'test.c'
            exe = Path(temp) / ('test.exe' if os.name == 'nt' else 'test')
            src.write_text(program)
            subprocess.run([os.environ.get('CC', 'gcc'),'-std=c11','-O2','-Wall','-Wextra','-Werror',
                            '-I',str(ROOT/'3ds_port/include'),str(src),'-o',str(exe)],check=True)
            subprocess.run([str(exe)],check=True)

if __name__ == '__main__':
    unittest.main()
