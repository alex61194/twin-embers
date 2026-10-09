"""Production lower-screen routing and Flash/cave geometry with synthetic state."""
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


def function(source, name):
    match = re.search(r'(?:static )?[\w *]+\b' + name + r'\([^;]+?\)\s*\{', source)
    if not match:
        raise AssertionError(name)
    end = source.index('{', match.start()) + 1
    depth = 1
    while depth:
        depth += (source[end] == '{') - (source[end] == '}')
        end += 1
    return source[match.start():end]


def additions(name, path):
    doc = json.loads((ROOT / 'patches/pokefirered' / name).read_text())
    row = next(r for r in doc['files'] if r['path'] == path)
    return ''.join(op['port'] for op in row['lines'] if 'port' in op)


class FieldPresentation(unittest.TestCase):
    def run_c(self, source):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            c = folder / 'test.c'
            c.write_text(source)
            exe = folder / ('test.exe' if os.name == 'nt' else 'test')
            subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-O2',
                            '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / '3ds_port/include'),
                            str(c), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_move_selection_and_item_animation_ownership(self):
        code = additions('0105-move-learning-bottom.json', 'src/pokemon_summary_screen.c')
        code += additions('0105-move-learning-bottom.json', 'src/pokemon_special_anim.c')
        code += additions('0105-move-learning-bottom.json', 'src/party_menu.c')
        code += additions('0107-cave-preview-presentation.json', 'src/fldeff_flash.c')
        self.run_c('''
#include <assert.h>
#include <stddef.h>
typedef void (*Callback)(void);
static void CB2_SetUpPSS(void) {}
static void CB2_RunPokemonSummaryScreen(void) {}
static void CB2_PSA(void) {}
static void CB2_ChangeMapMain(void) {}
static void Field(void) {}
static struct { Callback callback2; int inBattle; } gMain;
static int gPlayerParty[6], otherParty[6];
enum { PSS_MODE_NORMAL, PSS_MODE_SELECT_MOVE, PSS_MODE_FORGET_MOVE };
static struct Screen { struct { int *mons; } monList; int mode; } screen, *sMonSummaryScreen;
static int internal, *sPartyMenuInternal;
enum { Task_HandleRestoreWhichMoveInput=1, Task_DisplayLevelUpStatsPg2, Task_TryLearnNewMoves };
static int activeTask;
static int FuncIsActiveTask(int task) { return activeTask==task; }
static int CtrBottomParty_IsPartyCallback(void) { return gMain.callback2==Field; }
#define PLATFORM_3DS 1
''' + code + '''
int main(void) {
    assert(!CtrBottomSummary_IsMoveSelection());
    sMonSummaryScreen=&screen; screen.monList.mons=gPlayerParty;
    screen.mode=PSS_MODE_SELECT_MOVE; gMain.callback2=CB2_SetUpPSS;
    assert(CtrBottomSummary_IsMoveSelection());
    gMain.callback2=CB2_RunPokemonSummaryScreen;
    assert(CtrBottomSummary_IsMoveSelection());
    screen.mode=PSS_MODE_FORGET_MOVE; assert(CtrBottomSummary_IsMoveSelection());
    screen.mode=PSS_MODE_NORMAL; assert(!CtrBottomSummary_IsMoveSelection());
    screen.mode=PSS_MODE_SELECT_MOVE; screen.monList.mons=otherParty;
    assert(!CtrBottomSummary_IsMoveSelection());
    gMain.callback2=CB2_PSA; assert(CtrBottomPartyChild_IsOpen());
    gMain.inBattle=1; assert(!CtrBottomPartyChild_IsOpen());
    gMain.inBattle=0; gMain.callback2=Field;
    assert(!CtrBottomPartyChild_IsOpen() && !CtrField_CaveTransitionActive());
    sPartyMenuInternal=&internal;
    for (activeTask=1;activeTask<=3;++activeTask) assert(CtrBottomPartyChild_IsMovePanel());
    activeTask=0; assert(!CtrBottomPartyChild_IsMovePanel());
    gMain.callback2=CB2_ChangeMapMain; assert(CtrField_CaveTransitionActive());
    activeTask=1; assert(!CtrBottomPartyChild_IsMovePanel());
}
''')

    def test_child_session_survives_long_animation_and_releases_top(self):
        source = (ROOT / '3ds_port/src/3ds_bottom.c').read_text()
        begin = function(source, 'BeginSession')
        update = function(source, 'UpdateSession')
        start = source.index('enum { MODE_OFF')
        modes = source[start:source.index(';', start) + 1]
        names = set(re.findall(r'\b(Ctr\w+)\(\)', begin + update))
        names -= {'CtrBottom_CurrentCallback', 'CtrBottomParty_Callback'}
        stubs = '\n'.join('static int ' + n + '_value;\nint ' + n
                          + '(void) { return ' + n + '_value; }' for n in sorted(names))
        start = source.index('    memset(s, 0, sizeof(*s));', source.index('static void SnapshotState('))
        end = source.index('    if (mode == MODE_STORAGE_PC', start)
        snapshot = source[start:end]
        self.run_c('''
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "3ds_poketouch.h"
#define BRIDGE_MAX 150
#define CTR_SUMMARY_BATTLE_PARTY 2
#define CtrLog_Write(...) ((void)0)
static struct { int active,owner,battle,entered,frames,away; void *partyCallback; } sSession;
static int sScreen,sStorageSession,sPendingScreen=PT_NONE;
static void *sPartyMenuCallback;
static void *CtrBottom_CurrentCallback(void) { return NULL; }
static void *CtrBottomParty_Callback(void) { return NULL; }
static int CtrPokeTouch_LeavingFor(int root) { (void)root; return 0; }
static int NativeRoot(int root) { (void)root; return 0; }
''' + modes + '\n' + stubs + '\n' + begin + '''
static void BeginSessionPending(int owner) { BeginSession(owner); }
#define IS_CASE_MODE(m) ((m)==MODE_TMCASE || (m)==MODE_POUCH)
''' + update + '''
typedef struct { unsigned char mode,pressed; int cursor,tapped; CtrPokeTouchView touch; } Snapshot;
static void snapshot(Snapshot *s,int mode,int pressed) {
''' + snapshot + '''
}
int main(void) {
    assert(UpdateSession(MODE_PARTY_CHILD,0));
    assert(sSession.active && sSession.entered && sSession.owner==PT_POKEMON);
    for (int i=0;i<3000;++i) assert(UpdateSession(MODE_PARTY_CHILD,0));
    assert(sSession.away==0);
    Snapshot state; snapshot(&state,MODE_PARTY_CHILD,0);
    assert(state.touch.viewport && !state.touch.backVisible);
    for (int i=0;i<PT_COUNT;++i) assert(!state.touch.enabled[i]);
    CtrBottom_IsFieldCallback_value=1;
    CtrBottom_IsFading_value=1;
    assert(UpdateSession(MODE_FIELD,0) && sSession.active);
    CtrBottom_IsFading_value=0;
    assert(!UpdateSession(MODE_FIELD,0) && !sSession.active && sScreen==PT_NONE);
}
''')

    def test_flash_scanline_partition_cache_and_cleanup(self):
        video = (ROOT / '3ds_port/src/3ds_video.c').read_text()
        names = ('WindowEdge', 'BattleCurtain', 'IntroWindowCurtain', 'WindowCurtain',
                 'WindowSpan', 'WindowMaskOf', 'WindowRunAdd', 'FlashWindowSpan',
                 'WindowRuns', 'CtrVideo_SetFlashWindow')
        code = '\n'.join(function(video, name) for name in names)
        reset = additions('0106-flash-mask-restore.json', 'src/field_screen_effect.c')
        reset = '\n'.join(line for line in reset.splitlines() if 'SetGpuReg(' in line)
        self.run_c('''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#define CTR_GAME_WIDTH 400
#define CTR_GAME_HEIGHT 240
#define CTR_STAGE_X 80
#define CTR_STAGE_Y 40
#define VIEW_LEFT 0
#define VIEW_RIGHT (sNative ? 240 : 400)
#define VIEW_TOP 0
#define VIEW_BOTTOM (sNative ? 160 : 240)
static bool sIntro,sStage,sBattle,sNative,sFieldBarnDoorWipe,sBattleTransition;
static bool sFieldLayers=1,sWindowCacheValid,sFieldMoveShowMon;
static float sZoom=1;
static uint16_t regs[48],sFlashWindow[160];
static unsigned sFlashWindowCount;
static bool sObjWindowRow[240];
static uint8_t sObjWindow[240][400];
static unsigned Reg(unsigned offset) { return regs[offset/2]; }
static unsigned Min(unsigned a,unsigned b) { return a<b?a:b; }
static bool NativeIntroActive(void) { return false; }
static bool NativeIntroCinematic(void) { return false; }
static bool NativeIntroTitle(void) { return false; }
typedef struct { bool on[2],objOn; int x0[2],x1[2],y0[2],y1[2]; unsigned inside[2],obj,outside; } WindowSpans;
typedef struct { int x0,x1; unsigned mask; } WindowRun;
#define REG_OFFSET_WIN0H 0x40
#define REG_OFFSET_WIN0V 0x44
#define WIN_RANGE(a,b) (((a)<<8)|(b))
static void SetGpuReg(unsigned offset,unsigned value) { regs[offset/2]=value; }
''' + code + '''
int main(void) {
    WindowSpans spans={.on={true,false},.x0={40,0},.x1={400,0},.y1={240,0},.inside={63,0},.outside=1};
    uint16_t bounds[160]={0}; WindowRun runs[401];
    /* A dark cave's first row is closed, but its middle is open. */
    for (int y=60;y<100;++y) bounds[y]=WIN_RANGE(80,160);
    sWindowCacheValid=true; CtrVideo_SetFlashWindow(bounds,160);
    assert(!sWindowCacheValid);
    assert(WindowRuns(0,&spans,0,400,runs)==1 && runs[0].mask==1);
    assert(WindowRuns(120,&spans,0,400,runs)==3);
    assert(runs[1].mask==63 && runs[1].x0==133 && runs[1].x1==267);
    sWindowCacheValid=true; CtrVideo_SetFlashWindow(bounds,160);
    assert(sWindowCacheValid);
    bounds[80]=0x00ff; CtrVideo_SetFlashWindow(bounds,160);
    assert(!sWindowCacheValid);
    assert(WindowRuns(120,&spans,0,400,runs)==1 && runs[0].mask==63);
    /* A different scene must ignore a lingering Flash snapshot. */
    sFieldLayers=false;
    assert(WindowRuns(120,&spans,0,400,runs)==2 && runs[0].mask==1);
    sFieldLayers=true; sWindowCacheValid=true;
    sObjWindowRow[120]=true; sObjWindow[120][0]=1;
    /* Disabled OBJ windows cannot hide cave pixels. */
    assert(WindowRuns(120,&spans,0,400,runs)==1 && runs[0].mask==63);
    CtrVideo_SetFlashWindow(NULL,0); assert(!sWindowCacheValid && !sFlashWindowCount);
''' + reset + '''
    int first,last;
    WindowSpan(Reg(0x40),false,WindowCurtain(0),&first,&last);
    assert(first==0 && last==400);
    WindowSpan(Reg(0x44),true,false,&first,&last);
    assert(first==0 && last==240);
    sFieldMoveShowMon=true;
    WindowSpan(0x2878,true,false,&first,&last); assert(first==80 && last==160);
    WindowSpan(0x00f1,false,false,&first,&last); assert(first==0 && last==400);
    WindowSpan(0xf0f1,false,false,&first,&last); assert(first==last);
    WindowSpan(0xffff,false,false,&first,&last); assert(first==last);
    WindowSpan(0x00a1,true,false,&first,&last); assert(first==0 && last==240);
    sFieldMoveShowMon=false;
    /* Cave preview clips to the GBA stage; field geometry returns afterward. */
    sNative=true; sFieldLayers=false;
    WindowSpan(0x00ff,false,false,&first,&last); assert(last-first==240);
    sNative=false; sFieldLayers=true;
    WindowSpan(0x00ff,false,false,&first,&last); assert(last-first==400);
}
''')


if __name__ == '__main__':
    unittest.main()
