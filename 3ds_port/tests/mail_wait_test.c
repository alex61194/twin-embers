#include <assert.h>
#include <stdint.h>
#include <stdio.h>
typedef uint8_t u8;
enum { FALSE=0, TRUE=1 };
static struct { u8 state; } gMain;
static int dmaBusy, linkBusy, calls, linkActive;
static void (*nextCallback)(void);
static void CB2_RunShowMailCB(void) {}
static void SetMainCallback2(void (*cb)(void)) { nextCallback=cb; }
static int MenuHelpers_IsLinkActive(void) { return linkActive; }
static int DoInitMailView(void)
{
    /* The real init state 9 waits for a pending background copy, and state
     * 15 waits on link input. Neither advances until a later frame. */
    assert(++calls<64); /* The old callback hits this instead of returning. */
    if ((gMain.state==9 && dmaBusy) || (gMain.state==15 && linkBusy)) return FALSE;
    if (gMain.state==18) return TRUE;
    ++gMain.state;
    return FALSE;
}
/* PRODUCTION CALLBACK */
int main(void)
{
#ifdef PLATFORM_3DS
    dmaBusy=linkBusy=1;
    CB2_InitMailView(); assert(gMain.state==9 && calls==10 && !nextCallback);
    calls=0;
    for (int frame=0;frame<8;++frame) {
        CB2_InitMailView(); assert(gMain.state==9 && !nextCallback);
    }
    assert(calls==8); /* Exactly one blocked step per frame, never a busy loop. */
    dmaBusy=0; /* Native VBlank completed the queued copy. */
    calls=0; CB2_InitMailView(); assert(gMain.state==15 && !nextCallback);
    calls=0;
    for (int frame=0;frame<8;++frame) {
        CB2_InitMailView(); assert(gMain.state==15 && !nextCallback);
    }
    assert(calls==8);
    linkBusy=0; calls=0; CB2_InitMailView();
    assert(gMain.state==18 && nextCallback==CB2_RunShowMailCB);
    puts("PASS mail native: blocked DMA/link return to VBlank, resume and install reader once ready");
#else
    /* The original GBA fast loop and its link-active one-step exit remain. */
    CB2_InitMailView(); assert(gMain.state==18 && calls==19 && nextCallback==CB2_RunShowMailCB);
    gMain.state=0; calls=0; nextCallback=0; linkActive=1;
    CB2_InitMailView(); assert(gMain.state==1 && calls==1 && !nextCallback);
    puts("PASS mail GBA: original fast init and link-active exit retained");
#endif
}
