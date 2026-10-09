"""Signed field-effect OAM, actor ownership and invocation-strip placement."""
import os
from pathlib import Path
import subprocess
import tempfile
import unittest
from test_field_presentation import additions, function

ROOT = Path(__file__).resolve().parents[1]


class FlyPresentation(unittest.TestCase):
    def run_c(self, code):
        with tempfile.TemporaryDirectory() as tmp:
            folder = Path(tmp)
            c = folder / 'test.c'
            c.write_text(code)
            exe = folder / ('test.exe' if os.name == 'nt' else 'test')
            subprocess.run([os.environ.get('CC', 'gcc'), '-std=c11', '-O2',
                            '-Wall', '-Wextra', '-Werror', '-I' + str(ROOT / '3ds_port/include'),
                            str(c), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True)

    def test_exact_oam_is_published_with_the_matching_frame(self):
        video = (ROOT / '3ds_port/src/3ds_video.c').read_text()
        code = '\n'.join(function(video, name) for name in
                         ('CtrVideo_ClearEffectOam', 'CtrVideo_MarkEffectOam', 'CtrVideo_CommitEffectOam'))
        self.run_c('''
#include <assert.h>
#include <string.h>
#include "3ds_effect_oam.h"
static CtrEffectOam sEffectOamPending[128],sEffectOamShown[128];
''' + code + '''
int main(void) {
    int x,y;
    /* Negative flight Y must stay above TOP, not wrap into its bottom. */
    assert(CtrEffectOam_Near(208,-48,256)==-48);
    assert(CtrEffectOam_Near(464,-48,512)==-48);
    assert(CtrEffectOam_Near(448,448,512)==448);
    /* A subsprite is unwrapped around its parent's signed origin. */
    assert(CtrEffectOam_Near(8,-8,512)==8);
    CtrVideo_ClearEffectOam();
    CtrVideo_MarkEffectOam(5,208,464,-48+80,-48+40);
    assert(!CtrEffectOam_Resolve(sEffectOamShown,5,208,464,&x,&y));
    CtrVideo_CommitEffectOam();
    assert(CtrEffectOam_Resolve(sEffectOamShown,5,208,464,&x,&y) && x==32 && y==-8);
    CtrVideo_ClearEffectOam();
    CtrVideo_MarkEffectOam(5,80,368,448,120);
    /* Building a new OAM buffer cannot move the currently shown actor. */
    assert(CtrEffectOam_Resolve(sEffectOamShown,5,208,464,&x,&y) && y==-8);
    CtrVideo_CommitEffectOam();
    assert(!CtrEffectOam_Resolve(sEffectOamShown,5,208,464,&x,&y));
    assert(CtrEffectOam_Resolve(sEffectOamShown,5,80,368,&x,&y) && x==448);
    assert(!CtrEffectOam_Resolve(sEffectOamShown,128,80,368,&x,&y));
    CtrVideo_ClearEffectOam(); CtrVideo_CommitEffectOam();
    assert(!CtrEffectOam_Resolve(sEffectOamShown,5,80,368,&x,&y));
}
''')

    def test_only_invocation_and_flight_actors_are_translated(self):
        code = additions('0108-fly-authored-geometry.json', 'src/field_effect.c')
        code = function(code, 'CtrFieldEffect_ShowMonActive') + '\n' + function(code, 'CtrFieldEffect_AuthoredSprite')
        self.run_c('''
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#define TRUE 1
#define MAX_SPRITES 64
typedef unsigned char u8;
struct Sprite { void (*callback)(struct Sprite *); bool coordOffsetEnabled; };
static struct Sprite gSprites[64];
static struct { u8 spriteId; } gPlayerAvatar;
static void Task_ShowMon_Outdoors(u8 n) { (void)n; }
static void Task_ShowMon_Indoors(u8 n) { (void)n; }
static void Task_FlyOut(u8 n) { (void)n; }
static void Task_FlyIn(u8 n) { (void)n; }
static struct { void (*func)(u8); short data[16]; } gTasks[16];
static u8 FindTaskIdByFunc(void (*func)(u8)) {
    for (u8 i=0;i<16;++i) if (gTasks[i].func==func) return i;
    return 0xff;
}
static int FuncIsActiveTask(void (*func)(u8)) { return FindTaskIdByFunc(func)!=0xff; }
static void SpriteCB_FlyBirdLeaveBall(struct Sprite *s) { (void)s; }
static void SpriteCB_FlyBirdSwoopDown(struct Sprite *s) { (void)s; }
static void SpriteCB_FlyBirdReturnToBall(struct Sprite *s) { (void)s; }
static void SpriteCB_FlyBirdWithPlayer(struct Sprite *s) { (void)s; }
''' + code + '''
int main(void) {
    assert(!CtrFieldEffect_ShowMonActive());
    gPlayerAvatar.spriteId=0; gSprites[0].coordOffsetEnabled=true;
    for (int i=0;i<64;++i) assert(!CtrFieldEffect_AuthoredSprite(&gSprites[i]));
    gTasks[0].func=Task_ShowMon_Outdoors; gTasks[0].data[15]=3;
    assert(CtrFieldEffect_ShowMonActive() && CtrFieldEffect_AuthoredSprite(&gSprites[3]));
    assert(!CtrFieldEffect_AuthoredSprite(&gSprites[4]));
    gTasks[0].func=Task_ShowMon_Indoors;
    assert(CtrFieldEffect_AuthoredSprite(&gSprites[3]));
    gTasks[0].data[15]=-1; assert(!CtrFieldEffect_AuthoredSprite(&gSprites[3]));
    gTasks[0].func=Task_FlyOut;
    gSprites[1].callback=SpriteCB_FlyBirdLeaveBall; assert(CtrFieldEffect_AuthoredSprite(&gSprites[1]));
    gSprites[1].callback=SpriteCB_FlyBirdSwoopDown; assert(CtrFieldEffect_AuthoredSprite(&gSprites[1]));
    gSprites[1].callback=SpriteCB_FlyBirdWithPlayer; assert(CtrFieldEffect_AuthoredSprite(&gSprites[1]));
    gSprites[1].callback=SpriteCB_FlyBirdReturnToBall; assert(CtrFieldEffect_AuthoredSprite(&gSprites[1]));
    assert(!CtrFieldEffect_AuthoredSprite(&gSprites[0]));
    gSprites[0].coordOffsetEnabled=false; assert(CtrFieldEffect_AuthoredSprite(&gSprites[0]));
    gTasks[0].func=Task_FlyIn; assert(CtrFieldEffect_AuthoredSprite(&gSprites[0]));
    gSprites[0].coordOffsetEnabled=true; assert(!CtrFieldEffect_AuthoredSprite(&gSprites[0]));
    memset(gTasks,0,sizeof(gTasks)); assert(!CtrFieldEffect_ShowMonActive());
}
''')

    def test_renderer_uses_exact_effect_coordinates_and_keeps_world_decoding(self):
        video = (ROOT / '3ds_port/src/3ds_video.c').read_text()
        draw = function(video, 'DrawObjects')
        start = draw.index('        int x = attr1 & 511, y = attr0 & 255;')
        end = draw.index('        if (x >= sClipX1', start)
        self.run_c('''
#include <assert.h>
#include "3ds_effect_oam.h"
#include "3ds_obj_window.h"
#define CTR_VOXEL_ENABLED 0
#define VIEW_RIGHT 400
#define VIEW_BOTTOM 240
static bool sFieldLayers=true,sIntro,sStage,sCentred,sBattle;
static CtrEffectOam sEffectOamShown[128];
static int px,py;
static void decode(int i,unsigned attr0,unsigned attr1,unsigned boxW,unsigned boxH) {
''' + draw[start:end] + '''
    px=x; py=y;
}
int main(void) {
    sEffectOamShown[3]=(CtrEffectOam){32,-8,208,464,true};
    decode(3,208,464,32,32); assert(px==32 && py==-8);
    /* The same encoded Y on an untagged NPC keeps the native world reading. */
    decode(4,208,464,32,32); assert(px==-48 && py==208);
    sEffectOamShown[3]=(CtrEffectOam){448,120,80,368,true};
    decode(3,80,368,64,64); assert(px==448 && py==120);
    sFieldLayers=false; sIntro=true;
    decode(3,80,368,64,64); assert(px==368 && py==80);
}
''')

    def test_invocation_strip_and_mon_share_native_center(self):
        video = (ROOT / '3ds_port/src/3ds_video.c').read_text()
        code = '\n'.join(function(video, name) for name in ('DrawFieldUiRect', 'DrawFieldUi'))
        self.run_c('''
#include <assert.h>
#include <stdbool.h>
#define CTR_GAME_WIDTH 400
#define CTR_GAME_HEIGHT 240
#define CTR_STAGE_X 80
#define CTR_STAGE_Y 40
#define MAP_NAME_POPUP_MAP_Y 224
#define MAP_NAME_POPUP_HEIGHT 32
static int sClipX0,sClipX1=400,sClipY0,sClipY1=240,sViewX,sViewY;
static bool sFieldMoveShowMon,sQuestLogLayout,sMapNamePopup,sBattleTransition;
static int calls,vx,vy,right;
static unsigned Reg(unsigned n) { (void)n; return 0; }
static void ViewBase(void) {}
static void DrawTextBg(unsigned bg) { assert(bg==0); ++calls; vx=sViewX; vy=sViewY; right=sClipX1; }
''' + code + '''
int main(void) {
    sFieldMoveShowMon=true; DrawFieldUi(0);
    assert(calls==1 && vx==0 && vy==40 && right==400);
    assert(80+vy==120 && (40+vy)==80 && (120+vy)==160);
    assert(sViewX==0 && sViewY==0 && sClipX1==400 && sClipY1==240);
    sFieldMoveShowMon=false; calls=0; DrawFieldUi(0);
    assert(calls==1 && vx==80 && vy==80 && right==240);
}
''')

    def test_actual_trajectory_expressions_reach_beyond_native_edges(self):
        code = additions('0108-fly-authored-geometry.json', 'src/field_effect.c')
        expressions = [line for line in code.splitlines() if 'Cos(sprite->data[2], DISPLAY_WIDTH' in line]
        self.assertEqual(len(expressions), 2)
        condition = next(line.strip()[4:-1] for line in code.splitlines() if 'if (sprite->x < -0x40 -' in line)
        self.run_c('''
#include <assert.h>
static int nativeWidth=400;
#define DISPLAY_WIDTH nativeWidth
static int Cos(int angle,int radius) { return angle==0?radius:-radius; }
struct Sprite { int x,x2,data[8]; } actor,*sprite=&actor;
static void swoop(void) {
''' + expressions[0] + '''
}
static void travel(void) {
''' + expressions[1] + '''
}
static int offscreen(void) { return ''' + condition + '''; }
int main(void) {
    actor.data[2]=0; swoop(); assert(200+actor.x2==420);
    travel(); assert(200+actor.x2==460);
    actor.data[2]=128; travel(); assert(200+actor.x2==-60);
    actor.x=-80; assert(!offscreen());
    actor.x=-145; assert(offscreen());
    nativeWidth=240; actor.data[2]=0; swoop(); assert(actor.x2==140);
    travel(); assert(actor.x2==180);
    actor.x=-65; assert(offscreen());
}
''')

    def test_real_background_walker_does_not_cut_strip_at_256(self):
        video = (ROOT / '3ds_port/src/3ds_video.c').read_text()
        self.run_c('''
#include <assert.h>
#include <stdbool.h>
#define CTR_GAME_WIDTH 400
#define CTR_GAME_HEIGHT 240
static bool sFieldMoveShowMon,sStage,sBattle,sCentred;
static int sClipX0,sClipX1=400,sClipY0,sClipY1=240,sSpanShiftX;
static int rightSeen;
static unsigned Reg(unsigned n) { (void)n; return 0; }
static unsigned Min(unsigned a,unsigned b) { return a<b?a:b; }
static void ViewBase(void) {}
static int LayerDrawable(unsigned bg) { (void)bg; return 0; }
static void DrawStageBg(unsigned bg) { (void)bg; }
static void DrawBattleText(unsigned bg) { (void)bg; }
static void DrawBattleBg(unsigned bg) { (void)bg; }
#define DrawLayerRect(...) ((void)0)
static void DrawTextSpan(unsigned bg,int left,int right,int top,int bottom) {
    (void)bg; (void)left; (void)top; (void)bottom; rightSeen=right;
}
''' + function(video, 'DrawTextBg') + '''
int main(void) {
    sFieldMoveShowMon=true; DrawTextBg(0); assert(rightSeen==400);
    DrawTextBg(1); assert(rightSeen==256);
    sClipX0=256; DrawTextBg(0); assert(rightSeen==400);
    sClipX0=0; sFieldMoveShowMon=false; DrawTextBg(0); assert(rightSeen==256);
}
''')

    def test_boarding_and_arrival_are_synchronized_at_the_center(self):
        code = additions('0110-fly-phase-handoffs.json', 'src/field_effect.c')
        selection = next(line for line in code.splitlines()
                         if 'sprite->callback = SpriteCB_FlyBirdWithPlayer;' in line)
        self.run_c('''
#include <assert.h>
#include "3ds_fly_phase.h"
struct Sprite { unsigned angle; void (*callback)(struct Sprite *); };
/* Different fixture callbacks expose an unwanted phase-speed change. */
static void SpriteCB_FlyBirdWithPlayer(struct Sprite *sprite) { sprite->angle+=2; }
static void SpriteCB_FlyBirdSwoopDown(struct Sprite *sprite) { sprite->angle+=4; }
static void affine_completed(struct Sprite *sprite) {
''' + selection + '''
}
int main(void) {
    unsigned angle=0;
    for (int i=0;i<40;++i) angle=CtrFly_NextSwoopAngle(angle,true);
    assert(angle==64); /* The pickup waits underneath the jumping player. */
    assert(CtrFly_NextSwoopAngle(angle,false)==68);
    struct Sprite bird={0,SpriteCB_FlyBirdWithPlayer};
    for (int frame=0;frame<33;++frame) {
        if (frame==16) affine_completed(&bird);
        bird.callback(&bird);
        if (frame<32) assert(!CtrFly_CanDismount(bird.angle));
    }
    assert(bird.angle==66 && CtrFly_CanDismount(bird.angle));
    /* The actor's last presented angle was 64, exactly the screen centre. */
    bird.callback=SpriteCB_FlyBirdSwoopDown;
    bird.callback(&bird); assert(bird.angle==70);
    assert(CtrFly_NextSwoopAngle(124,false)==128);
}
''')


if __name__ == '__main__':
    unittest.main()
