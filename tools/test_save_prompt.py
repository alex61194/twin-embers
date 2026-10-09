"""PokéTouch SAVE prompt on a blank card (patch 0114).

With --tree DIR (a workspace made by tools/bootstrap.py): compiles the patched
CtrSave_IsDifferentFile with the tree's own save constants and checks it follows the
original start menu: the "different game file" warning appears only when a new game is
saved over a readable existing file, never on a blank (EMPTY) or unreadable (INVALID) card.
"""
import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import sys
import tempfile
import unittest

HARNESS = r'''
#include <stdio.h>
#include "gba/defines.h"
typedef unsigned char u8;
typedef unsigned short u16;
typedef u8 bool8;
#define SAVE_STATUS_EMPTY    %(EMPTY)s
#define SAVE_STATUS_OK       %(OK)s
#define SAVE_STATUS_INVALID  %(INVALID)s
#define SAVE_STATUS_ERROR    %(ERROR)s
bool8 gDifferentSaveFile;
u16 gSaveFileStatus;
%(FUNC)s
int main(void)
{
    static const struct { u16 status; bool8 different; int expect; } cases[] = {
        { SAVE_STATUS_EMPTY, TRUE, 0 }, { SAVE_STATUS_INVALID, TRUE, 0 },
        { SAVE_STATUS_OK, TRUE, 1 }, { SAVE_STATUS_ERROR, TRUE, 1 },
        { SAVE_STATUS_EMPTY, FALSE, 0 }, { SAVE_STATUS_OK, FALSE, 0 },
    };
    for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
        gSaveFileStatus = cases[i].status;
        gDifferentSaveFile = cases[i].different;
        /* Original: SaveDialogCB_AskSaveHandleInput only shows the overwrite warning for a
         * new game when the status is neither EMPTY nor INVALID. */
        if (CtrSave_IsDifferentFile() != cases[i].expect) {
            printf("FAIL case %%u\n", i);
            return 1;
        }
    }
    puts("PASS save prompt");
    return 0;
}
'''


class SavePrompt(unittest.TestCase):
    def tree(self):
        if not os.environ.get('SAVE_PROMPT_TREE'):
            self.skipTest('needs --tree (a bootstrapped, patched workspace)')
        return Path(os.environ['SAVE_PROMPT_TREE']).resolve()

    def test_blank_card_asks_plain_save(self):
        tree = self.tree()
        source = (tree / 'src/start_menu.c').read_text(encoding='utf-8')
        func = re.search(r'^int CtrSave_IsDifferentFile\(void\)\n\{\n.*?^\}\n', source, re.M | re.S)
        self.assertIsNotNone(func, 'CtrSave_IsDifferentFile not found')
        consts = dict(re.findall(r'^#define SAVE_STATUS_(EMPTY|OK|INVALID|ERROR)\s+(\S+)',
                                 (tree / 'include/save.h').read_text(encoding='utf-8'), re.M))
        self.assertEqual(set(consts), {'EMPTY', 'OK', 'INVALID', 'ERROR'})
        cc = os.environ.get('CC') or shutil.which('gcc') or 'gcc'
        with tempfile.TemporaryDirectory() as tmp:
            c = Path(tmp) / 'save_prompt.c'
            c.write_text(HARNESS % dict(consts, FUNC=func.group(0)), encoding='utf-8')
            exe = Path(tmp) / ('save-prompt.exe' if os.name == 'nt' else 'save-prompt')
            subprocess.run([cc, '-std=gnu11', '-O2', '-Wall', '-Werror', '-iquote', str(tree / 'include'),
                            str(c), '-o', str(exe)], check=True)
            run = subprocess.run([str(exe)], capture_output=True, text=True)
            self.assertEqual(run.returncode, 0, run.stdout)
            self.assertIn('PASS save prompt', run.stdout)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--tree', type=Path)
    args, rest = parser.parse_known_args()
    if args.tree:
        os.environ['SAVE_PROMPT_TREE'] = str(args.tree)
    unittest.main(argv=[sys.argv[0]] + rest)
