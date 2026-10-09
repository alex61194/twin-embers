#include <assert.h>
#include <stdbool.h>
#include <stdio.h>
#include "3ds_video.h"
#include "3ds_poketouch.h"
enum { MODE_STORAGE_PC=1,MODE_STORAGE_BOX,MODE_STORAGE_CHILD,MODE_SUMMARY,MODE_BAG,MODE_FIELD,BAG_BUSY=9 };
static int sStorageSession, fading, pcMenu, pcReady, boxPresented, namePresented, summaryReady, bagState;
static int CtrBottom_IsFading(void) { return fading; }
static int CtrBottomStorage_PCMenu(void) { return pcMenu; }
static int CtrBottomStorage_PCMenuReady(void) { return pcReady; }
static int CtrBottomStorage_BoxPresented(void) { return boxPresented; }
static int CtrBottomStorage_NamingPresented(void) { return namePresented; }
int CtrBottomSummary_Ready(void) { return summaryReady; }
static int get_state(void) { return bagState; }
static struct List { int (*state)(void); } list={get_state};
static struct List *ListFor(int mode) { return mode==MODE_BAG ? &list : NULL; }
/* PRODUCTION LIFECYCLE */
/* PRODUCTION OWNER */

/* Capture the actual GPU viewport sampling call; no renderer is executed. */
typedef struct { int width,height; float left,top,right,bottom; } Tex3DS_SubTexture;
typedef struct { void *tex; const Tex3DS_SubTexture *subtex; } C2D_Image;
static int sChrome,sSurface,sBottom;
static bool sBottomPcField;
static Tex3DS_SubTexture captured;
static void BlendForget(void) {}
static unsigned C2D_Color32(int r,int g,int b,int a) { return r+g+b+a; }
static void C2D_TargetClear(int target,unsigned color) { (void)target; (void)color; }
static void C2D_SceneBegin(int target) { (void)target; }
static void C2D_ViewReset(void) {}
static void Blend(int n,bool a,bool b) { (void)n; (void)a; (void)b; }
static void C2D_DrawImageAt(C2D_Image image,int x,int y,int z,void *tint,int sx,int sy)
{
    (void)z; (void)tint; assert(sx==1 && sy==1);
    if (image.tex==&sSurface) { assert(x==PT_VIEW_X && y==PT_VIEW_Y); captured=*image.subtex; }
}
static void C2D_Flush(void) {}
static void C3D_FrameSplit(int n) { assert(n==0); }
/* PRODUCTION CROP */
int main(void)
{
    pcMenu=pcReady=1;
    UpdateStorageOwner(MODE_FIELD);
    assert(!sStorageSession && !StorageBridge(MODE_FIELD) && !StorageHoldTop());
    pcReady=0; fading=1;
    assert(StorageHoldTop() && !sStorageSession); /* Entry fade: no lower PC viewport. */
    fading=0;
    assert(StorageHoldTop() && !sStorageSession); /* Fade ended, task has not opened box yet. */
    pcMenu=0; fading=0;
    UpdateStorageOwner(MODE_STORAGE_BOX);
    assert(sStorageSession && StorageHoldTop() && StorageBridge(MODE_STORAGE_BOX));
    boxPresented=1; assert(!StorageBridge(MODE_STORAGE_BOX));
    UpdateStorageOwner(MODE_SUMMARY); assert(sStorageSession);
    pcMenu=pcReady=1; UpdateStorageOwner(MODE_FIELD);
    assert(!sStorageSession && !StorageHoldTop()); /* Choices return to TOP. */
    pcMenu=pcReady=boxPresented=0;
    assert(!StorageBridge(MODE_FIELD)); sStorageSession=1;
    assert(StorageBridge(MODE_STORAGE_PC)); pcReady=1; assert(!StorageBridge(MODE_STORAGE_PC));
    assert(StorageBridge(MODE_STORAGE_BOX)); boxPresented=1; assert(!StorageBridge(MODE_STORAGE_BOX));
    assert(StorageBridge(MODE_STORAGE_CHILD)); namePresented=1; assert(!StorageBridge(MODE_STORAGE_CHILD));
    assert(StorageBridge(MODE_SUMMARY)); summaryReady=1; assert(!StorageBridge(MODE_SUMMARY));
    bagState=BAG_BUSY; assert(StorageBridge(MODE_BAG)); bagState=0; assert(!StorageBridge(MODE_BAG));
    fading=1; assert(StorageBridge(MODE_STORAGE_BOX)); fading=0;
    sBottomPcField=true; RenderBottom();
    assert(captured.width==240 && captured.height==160);
    assert(captured.left==80/512.0f && captured.right==320/512.0f);
    assert(captured.top==1-80/256.0f && captured.bottom==1-240/256.0f);
    sBottomPcField=false; RenderBottom();
    assert(captured.top==1-40/256.0f && captured.bottom==1-200/256.0f);
    puts("storage lifecycle: PC/box/summary/bag/naming setup and fade holds; native-field and centred UV crops PASS");
}
