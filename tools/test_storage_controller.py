"""Execute the authored PC bridge and production touch controller, without assets."""
import json
import os
import re
import subprocess
import tempfile
import unittest
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def additions(path):
    recipe = json.loads((ROOT/'patches/pokefirered/0101-storage-bottom-bridge.json').read_text(encoding='utf-8'))
    row = next(r for r in recipe['files'] if r['path'] == path)
    return ''.join(op['port'] for op in row['lines'] if 'port' in op)


def function(source, name):
    start = source.index(name + '(')
    start = source.rfind('\n', 0, start) + 1
    end = source.index('\n}', start) + 2
    return source[start:end]


class StorageController(unittest.TestCase):
    def compile_run(self, source):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            c = folder/'test.c'
            c.write_text(source, encoding='utf-8')
            exe = folder/('test.exe' if os.name == 'nt' else 'test')
            subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-O2',
                            '-Wall', '-Wextra', '-Werror', '-I'+str(ROOT/'3ds_port/include'),
                            str(c), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_bridge_and_single_release(self):
        source = (ROOT/'3ds_port/src/3ds_bottom.c').read_text(encoding='utf-8')
        start = source.index('static uint16_t StoreDirKey(')
        end = source.index('\nstatic int ProcessTouch(', start)
        production = source[start:end]
        structs = source[source.index('enum { ST_PLAN_NONE'):source.index('static void BeginSessionPending')]
        harness = (ROOT/'3ds_port/tests/storage_controller_test.c').read_text(encoding='utf-8')
        bridge = '\n'.join(additions('src/pokemon_storage_system_'+part+'.c') for part in ('tasks','menu','data'))
        poketouch = (ROOT/'3ds_port/src/3ds_poketouch.c').read_text(encoding='utf-8')
        self.compile_run(harness.replace('/* PRODUCTION STRUCTS */', structs)
                         .replace('/* PRODUCTION BRIDGE */', bridge)
                         .replace('/* PRODUCTION TOUCH TRANSFORM */', function(poketouch, 'CtrPokeTouch_ViewportToGba'))
                         .replace('/* PRODUCTION CONTROLLER */', production))

    def test_storage_lifecycle_and_field_crop(self):
        source = (ROOT/'3ds_port/src/3ds_bottom.c').read_text(encoding='utf-8')
        harness = (ROOT/'3ds_port/tests/storage_lifecycle_test.c').read_text(encoding='utf-8')
        # Execute the actual lifecycle and UV selection functions. GPU calls
        # are capture stubs, so this proves origins/dimensions, not rendering.
        video = (ROOT/'3ds_port/src/3ds_video.c').read_text(encoding='utf-8')
        self.compile_run(harness.replace('/* PRODUCTION LIFECYCLE */', function(source, 'StorageBridge'))
                         .replace('/* PRODUCTION OWNER */', function(source, 'UpdateStorageOwner') + '\n' + function(source, 'StorageHoldTop'))
                         .replace('/* PRODUCTION CROP */', function(video, 'RenderBottom')))

    def test_real_mode_ownership(self):
        source = (ROOT/'3ds_port/src/3ds_bottom.c').read_text(encoding='utf-8')
        mode = function(source, 'CurrentMode')
        start = source.index('enum { MODE_OFF')
        modes = source[start:source.index(';', start)+1]
        names = sorted(set(re.findall(r'\b(Ctr\w+)\(\)', mode)))
        stubs = '\n'.join('static int '+n+'_value;\nstatic int '+n+'(void) { return '+n+'_value; }'
                          for n in names if n != 'CtrBottom_CurrentCallback')
        self.compile_run('''
#include <assert.h>
#include <stddef.h>
static int sEnabled=1,sInitDone=1,sStorageSession;
static struct { int active; } sSession;
static void *sPartyMenuCallback;
static void *CtrBottom_CurrentCallback(void) { return NULL; }
''' + modes + '\n' + stubs + '\n' + mode + '''
int main(void)
{
    CtrBottom_HasSave_value=1; CtrBottom_IsFieldCallback_value=1;
    assert(CurrentMode()==MODE_FIELD);
    CtrBottomStorage_PCMenu_value=1; assert(CurrentMode()==MODE_FIELD);
    CtrBottomStorage_PCMenu_value=0; sStorageSession=1;
    CtrBottom_IsFieldCallback_value=0;
    CtrBottomStorage_BoxOpen_value=1; assert(CurrentMode()==MODE_STORAGE_BOX);
    CtrBottomStorage_BoxOpen_value=0;
    CtrBottomSummary_IsActive_value=1; assert(CurrentMode()==MODE_SUMMARY);
    CtrBottomSummary_IsActive_value=0;
    CtrBottomBag_IsOpen_value=1; CtrBottomBag_FromParty_value=1;
    assert(CurrentMode()==MODE_BAG);
    CtrBottomBag_IsOpen_value=0;
    CtrBottomTMCase_IsOpen_value=1; assert(CurrentMode()==MODE_TMCASE);
    CtrBottomTMCase_IsOpen_value=0;
    CtrBottomPouch_IsOpen_value=1; assert(CurrentMode()==MODE_POUCH);
    CtrBottomPouch_IsOpen_value=0;
    assert(CurrentMode()==MODE_STORAGE_CHILD);
    CtrBottom_IsFieldCallback_value=1; assert(CurrentMode()==MODE_FIELD);
    sStorageSession=0; CtrBottom_IsFieldCallback_value=0;
    assert(CurrentMode()==MODE_OFF);
    CtrBottomParty_UsesTopPresentation_value=1;
    CtrBottomPartyChild_IsMovePanel_value=1;
    assert(CurrentMode()==MODE_PARTY_CHILD);
    CtrBottomPartyChild_IsMovePanel_value=0;
    CtrBottomSummary_IsMoveSelection_value=1;
    assert(CurrentMode()==MODE_PARTY_CHILD);
    CtrBottomSummary_IsMoveSelection_value=0;
    CtrBottomPartyChild_IsOpen_value=1;
    assert(CurrentMode()==MODE_PARTY_CHILD);
    CtrBottomPartyChild_IsOpen_value=0;
    CtrBottomParty_UsesTopPresentation_value=0;
    /* A field case must not be adopted without either lower session. */
    CtrBottomTMCase_IsOpen_value=1; assert(CurrentMode()==MODE_OFF);
    sSession.active=1; assert(CurrentMode()==MODE_TMCASE);
}
''')

    def test_snapshot_has_no_implicit_pokedex_selection(self):
        source = (ROOT/'3ds_port/src/3ds_bottom.c').read_text(encoding='utf-8')
        start = source.index('    memset(s, 0, sizeof(*s));', source.index('static void SnapshotState('))
        end = source.index('    if (mode == MODE_PARTY_CHILD', start)
        # Execute the production initialization used before every storage view.
        self.compile_run('''
#include <assert.h>
#include <string.h>
#include "3ds_poketouch.h"
typedef struct { unsigned char mode,pressed; int cursor,tapped; CtrPokeTouchView touch; } Snapshot;
static void initialize(Snapshot *s,int mode,int pressed)
{
''' + source[start:end] + '''
}
int main(void)
{
    Snapshot s;
    initialize(&s,11,0);
    assert(s.touch.selected==PT_NONE && s.touch.pressed==PT_NONE);
    for (int id=0;id<PT_COUNT;++id) assert(!s.touch.enabled[id]);
    /* The renderer's exact button-state rule must disable POKEDEX as well. */
    for (int id=0;id<PT_COUNT;++id)
        assert(!(s.touch.enabled[id] || s.touch.selected==id));
}
''')


if __name__ == '__main__':
    unittest.main()
