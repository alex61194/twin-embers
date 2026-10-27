#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "3ds_storage_touch.h"
#include "3ds_storage_bridge.h"
#include "3ds_poketouch.h"

typedef uint8_t u8;
typedef void (*TaskFunc)(u8);
enum { NUM_TASKS=16, OPTIONS_COUNT=5, STATE_HANDLE_INPUT=2, STATE_ERROR_MSG=3, MOVE_MODE_NORMAL=0 };
#define tState data[0]
#define tSelectedOption data[1]
struct Task { TaskFunc func; u8 isActive; int16_t data[16]; };
static struct Task gTasks[NUM_TASKS];
static struct { void (*callback2)(void); } gMain;
static struct { u8 active; } gPaletteFade;
static struct Storage {
    u8 taskId, state, menuItemsCount, inBoxMovingMode;
    struct { u8 tilemapLeft, tilemapTop, width; } menuWindow;
} storage, *gStorage;
static int sCursorArea, sCursorPosition, sInMultiMoveMode, menuCursor;
static int Menu_GetCursorPos(void) { return menuCursor; }
static void CB2_PokeStorage(void) {}
#define TASK(name) static void name(u8 id) { (void)id; }
TASK(Task_PCMainMenu)
TASK(Task_PokeStorageMain)
TASK(Task_OnSelectedMon)
TASK(Task_HandleBoxOptions)
TASK(Task_HandleWallpapers)
TASK(Task_DepositMenu)
TASK(Task_JumpBox)
TASK(Task_ReleaseMon)
TASK(Task_CloseBoxWhileHoldingItem)
TASK(Task_OnCloseBoxPressed)
TASK(Task_OnBPressed)
TASK(Task_InitPokeStorage)
TASK(Task_ShowPokeStorage)
TASK(Task_ReshowPokeStorage)
/* PRODUCTION BRIDGE */

enum { MODE_STORAGE_PC=1, MODE_STORAGE_BOX, MODE_STORAGE_CHILD };
enum { K_A=1, K_B=2, K_START=8, K_RIGHT=16, K_LEFT=32, K_UP=64, K_DOWN=128 };
typedef struct { unsigned held; int touchActive, touchDown, touchX, touchY; } CtrInput;
static CtrInput input;
static const CtrInput *CtrInput_Get(void) { return &input; }
/* The row the controller asks the game's menu cursor to take, and the box place it taps (-1: none). */
static int askedRow = -1, tappedArea = -1, tappedPos = -1;
static void CtrMenu_Ask(int row) { askedRow = row; }
static void CtrStorage_Tap(int area, int position) { tappedArea = area; tappedPos = position; }
static int CtrStorage_TapPending(void) { return tappedArea >= 0; }
static int markTap = -1, markMenu;
static void CtrMarkings_Tap(int position) { markTap = position; }
int CtrBottomStorage_MarkMenu(void) { return markMenu; }

/* PRODUCTION TOUCH TRANSFORM */
static struct { int mode; CtrPokeTouchView touch; } sShown;   /* touch.box.on 0: FireRed's own picture */
static int boxHitOverride = -1;
int CtrPokeTouch_BoxHit(const CtrPokeTouchView *v, int x, int y) { (void)v; (void)x; (void)y; return boxHitOverride; }
static int sHaveShown;
static uint16_t sRouteKey;
/* PRODUCTION STRUCTS */
/* PRODUCTION CONTROLLER */

/* The box's next input takes the tap: the plan holds nothing more. */
static int took(int area, int position)
{
    int ok = tappedArea == area && tappedPos == position && !sRouteKey;
    tappedArea = -1;
    RunStoragePlan(MODE_STORAGE_BOX);
    return ok && !sStorePlan.kind;
}

static void task(TaskFunc fn, int state)
{
    memset(gTasks,0,sizeof(gTasks));
    gTasks[0].isActive=1; gTasks[0].func=fn;
    storage.taskId=0; storage.state=state;
}
static void tap(int mode,int x,int y)
{
    input=(CtrInput){.touchActive=1,.touchDown=1,.touchX=x+PT_VIEW_X,.touchY=y+PT_VIEW_Y};
    ProcessStorageTouch(mode,&input); RunStoragePlan(mode);
    assert(sRouteKey==0);
    input.touchDown=0;
    for (int i=0;i<90;++i) { ProcessStorageTouch(mode,&input); RunStoragePlan(mode); assert(!sRouteKey); }
    input.touchActive=0;
    ProcessStorageTouch(mode,&input); RunStoragePlan(mode);
}
static void no_repeat(int mode)
{
    sRouteKey=0;
    for (int i=0;i<90;++i) { ProcessStorageTouch(mode,&input); RunStoragePlan(mode); assert(!sRouteKey); }
}
int main(void)
{
    assert(!CtrBottomStorage_BoxOpen());
    gStorage=&storage; gMain.callback2=CB2_PokeStorage;
    task(Task_PokeStorageMain,0);
    assert(CtrBottomStorage_BoxReady());
    storage.taskId=NUM_TASKS; assert(!CtrBottomStorage_BoxReady()); assert(!CtrBottomStorage_YesNo());
    storage.taskId=0; gTasks[0].isActive=0; assert(!CtrBottomStorage_BoxReady());
    gTasks[0].isActive=1; gPaletteFade.active=1; assert(!CtrBottomStorage_BoxReady());
    gPaletteFade.active=0;
    task(Task_InitPokeStorage,0); assert(!CtrBottomStorage_BoxPresented());
    task(Task_ShowPokeStorage,0); assert(!CtrBottomStorage_BoxPresented());
    task(Task_ReshowPokeStorage,0); assert(!CtrBottomStorage_BoxPresented());
    task(Task_PokeStorageMain,0); assert(CtrBottomStorage_BoxPresented());
    sCursorArea=ST_AREA_BOX;
    for (int slot=0;slot<30;++slot) {
        sCursorPosition=slot;
        tap(MODE_STORAGE_BOX,100+24*(slot%6),44+24*(slot/6));
        assert(took(ST_AREA_BOX,slot)); no_repeat(MODE_STORAGE_BOX);
    }
    /* Any other cell, the title, its arrows and the buttons: one tap each, straight there, no D-pad steps. */
    sCursorPosition=0;
    tap(MODE_STORAGE_BOX,100+24*5,44+24*4); assert(took(ST_AREA_BOX,29)); no_repeat(MODE_STORAGE_BOX);
    tap(MODE_STORAGE_BOX,162,12); assert(took(ST_AREA_TITLE,0)); no_repeat(MODE_STORAGE_BOX);
    tap(MODE_STORAGE_BOX,92,28); assert(took(ST_AREA_TITLE,1)); no_repeat(MODE_STORAGE_BOX);
    tap(MODE_STORAGE_BOX,228,28); assert(took(ST_AREA_TITLE,2)); no_repeat(MODE_STORAGE_BOX);
    tap(MODE_STORAGE_BOX,120,14); assert(took(ST_AREA_BUTTONS,0)); no_repeat(MODE_STORAGE_BOX);
    tap(MODE_STORAGE_BOX,208,14); assert(took(ST_AREA_BUTTONS,1)); no_repeat(MODE_STORAGE_BOX);
    sCursorPosition=0;
    /* A touch dragged out cannot rearm after returning, even at touch-up. */
    input=(CtrInput){.touchActive=1,.touchDown=1,.touchX=100+PT_VIEW_X,.touchY=44+PT_VIEW_Y};
    ProcessStorageTouch(MODE_STORAGE_BOX,&input); input.touchDown=0; input.touchX=-1;
    ProcessStorageTouch(MODE_STORAGE_BOX,&input); input.touchX=100+PT_VIEW_X;
    ProcessStorageTouch(MODE_STORAGE_BOX,&input); input.touchActive=0;
    ProcessStorageTouch(MODE_STORAGE_BOX,&input); RunStoragePlan(MODE_STORAGE_BOX); assert(!sRouteKey && tappedArea<0);
    /* A popup replacing another at the same coordinate cancels the finger. */
    storage.menuWindow.tilemapLeft=20; storage.menuWindow.tilemapTop=2; storage.menuWindow.width=9;
    storage.menuItemsCount=4; menuCursor=0; task(Task_HandleWallpapers,2);
    input=(CtrInput){.touchActive=1,.touchDown=1,.touchX=170+PT_VIEW_X,.touchY=25+PT_VIEW_Y};
    ProcessStorageTouch(MODE_STORAGE_BOX,&input); storage.state=4;
    input.touchDown=0; input.touchActive=0;
    ProcessStorageTouch(MODE_STORAGE_BOX,&input); RunStoragePlan(MODE_STORAGE_BOX); assert(!sRouteKey);
    task(Task_OnSelectedMon,2); storage.menuItemsCount=7;
    storage.menuWindow.tilemapTop=1;
    for (int row=0;row<7;++row) {
        menuCursor=row;
        tap(MODE_STORAGE_BOX,170,8+16*row+4); assert(sRouteKey==K_A); no_repeat(MODE_STORAGE_BOX);
    }
    /* Every yes/no controller uses the original live menu and geometry. */
    TaskFunc yesno[]={Task_ReleaseMon,Task_CloseBoxWhileHoldingItem,Task_OnCloseBoxPressed,Task_OnBPressed};
    for (unsigned i=0;i<4;++i) {
        task(yesno[i],i<2?1:2); menuCursor=1;
        assert(CtrBottomStorage_YesNo() && CtrBottomStorage_ContextCount()==2);
        tap(MODE_STORAGE_BOX,200,112); assert(sRouteKey==K_A); no_repeat(MODE_STORAGE_BOX);
    }
    /* An animation holds the tap back until the box takes input again; nothing is pressed meanwhile. */
    task(Task_PokeStorageMain,0); sCursorPosition=0;
    input=(CtrInput){.touchActive=1,.touchDown=1,.touchX=124+PT_VIEW_X,.touchY=44+PT_VIEW_Y};
    ProcessStorageTouch(MODE_STORAGE_BOX,&input); input.touchDown=0; input.touchActive=0;
    ProcessStorageTouch(MODE_STORAGE_BOX,&input); storage.state=1;
    for (int i=0;i<10;++i) { RunStoragePlan(MODE_STORAGE_BOX); assert(!sRouteKey && tappedArea<0); }
    storage.state=0; RunStoragePlan(MODE_STORAGE_BOX);
    assert(took(ST_AREA_BOX,1)); no_repeat(MODE_STORAGE_BOX);
    /* A held button drops a tap the box has not taken yet. */
    tap(MODE_STORAGE_BOX,148,44); assert(tappedArea==ST_AREA_BOX && tappedPos==2);
    input.held=K_B; RunStoragePlan(MODE_STORAGE_BOX); assert(!sStorePlan.kind && !sRouteKey && tappedArea<0); input.held=0;
    /* A different input controller drops it too. */
    tap(MODE_STORAGE_BOX,172,44); sRouteKey=0;
    task(Task_OnSelectedMon,2); RunStoragePlan(MODE_STORAGE_BOX);
    assert(!sStorePlan.kind && !sRouteKey && tappedArea<0);
    /* The marking card: a tapped mark, OK or CANCEL goes to the card at once, nothing pressed. */
    {
        static const int cx[6]={35,73,111,149,55,129}, cy[6]={94,94,94,94,132,132};
        task(Task_PokeStorageMain,1); markMenu=1;
        sHaveShown=1; sShown.mode=MODE_STORAGE_BOX; sShown.touch.box.on=1; sShown.touch.box.state=PT_BOX_STATE_MARKS;
        for (int i=0;i<6;++i) {
            markTap=-1; boxHitOverride=ST_HIT_MENU_BASE+i;
            tap(MODE_STORAGE_BOX,cx[i],cy[i]); assert(markTap==i && !sRouteKey && !sStorePlan.kind);
        }
        boxHitOverride=-1; sHaveShown=0; sShown.touch.box.on=0; markMenu=0;
    }
    task(Task_JumpBox,1);
    tap(MODE_STORAGE_BOX,124,88); assert(sRouteKey==K_LEFT); no_repeat(MODE_STORAGE_BOX);
    tap(MODE_STORAGE_BOX,196,88); assert(sRouteKey==K_RIGHT); no_repeat(MODE_STORAGE_BOX);
    tap(MODE_STORAGE_BOX,160,96); assert(sRouteKey==K_A); no_repeat(MODE_STORAGE_BOX);
    task(Task_PokeStorageMain,0); sCursorArea=ST_AREA_PARTY;
    for (int slot=0;slot<7;++slot) {
        sCursorPosition=slot;
        int x=slot==0?104:152, y=slot==0?64:slot==6?132:16+24*(slot-1);
        tap(MODE_STORAGE_BOX,x,y); assert(took(ST_AREA_PARTY,slot)); no_repeat(MODE_STORAGE_BOX);
    }
    task(Task_PCMainMenu,STATE_HANDLE_INPUT); gTasks[0].tState=STATE_HANDLE_INPUT; gTasks[0].tSelectedOption=0;
    for (int option=0;option<5;++option) {
        gTasks[0].tSelectedOption=option;
        tap(MODE_STORAGE_PC,20,12+16*option); assert(sRouteKey==K_A); no_repeat(MODE_STORAGE_PC);
    }
    /* Clearing a root error with one arrow keeps navigation in that root. */
    gTasks[0].tState=STATE_ERROR_MSG; gTasks[0].tSelectedOption=0;
    /* The menu's error message does not take the asked row: the cursor stays and an arrow follows. */
    tap(MODE_STORAGE_PC,20,12+16*2); assert(!sRouteKey && askedRow==2);
    RunStoragePlan(MODE_STORAGE_PC); assert(!sRouteKey);
    RunStoragePlan(MODE_STORAGE_PC); assert(sRouteKey==K_DOWN); sRouteKey=0;
    gTasks[0].tState=STATE_HANDLE_INPUT; gTasks[0].tSelectedOption=2;   /* the pending ask took the row */
    RunStoragePlan(MODE_STORAGE_PC); assert(!sRouteKey);
    RunStoragePlan(MODE_STORAGE_PC); assert(sRouteKey==K_A); no_repeat(MODE_STORAGE_PC);
    puts("storage controller: bridge guards, touch hold/drag, popup identity, direct box taps, physical cancellation and single release PASS");
}
