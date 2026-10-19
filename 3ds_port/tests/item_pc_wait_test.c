/* The player's Item PC setup loop (src/item_pc.c ItemPc_RunSetup) with the port's yield. The yield text between
 * the markers is the production text of patch 0119, put in by tools/test_item_pc_wait.py.
 *
 * ItemPc_LoadGraphics waits for a queued graphics copy that only the DMA manager finishes, at a VBlank. On the GBA
 * the VBlank interrupt runs inside the loop; on the ARM11 it runs after the callback returns, so the loop must
 * return while a copy is queued instead of spinning (a frozen black top screen on Withdraw Item). */
#include <assert.h>
#include <stdio.h>

typedef signed short s16;
#define TRUE 1
#define FALSE 0

static int sDmaPending;      /* queued copies */
static int sVblankAfter;     /* GBA only: an interrupt clears them after this many setup calls */
static int sSetupCalls, sSetupDone, sLinkActive;

s16 WaitDma3Request(s16 index)
{
    assert(index == -1);
    return sDmaPending ? -1 : 0;
}

static int MenuHelpers_IsLinkActive(void)
{
    return sLinkActive;
}

/* The step that waits for the copy: it does not finish while one is queued. */
static int ItemPc_DoGfxSetup(void)
{
    sSetupCalls++;
#ifndef PLATFORM_3DS
    if (sVblankAfter && sSetupCalls >= sVblankAfter)
        sDmaPending = 0;                 /* the interrupt, inside the loop */
#endif
    if (sDmaPending)
        return FALSE;
    sSetupDone = 1;
    return TRUE;
}

/* PRODUCTION LOOP */

int main(void)
{
#ifdef PLATFORM_3DS
    /* A copy is queued: one call returns after a bounded number of steps, without finishing. */
    sDmaPending = 1;
    ItemPc_RunSetup();
    assert(sSetupCalls == 1 && !sSetupDone);
    ItemPc_RunSetup();                   /* the VBlank has not run: still waiting, still returning */
    assert(sSetupCalls == 2 && !sSetupDone);
    sDmaPending = 0;                     /* the native VBlank finished the copy */
    ItemPc_RunSetup();
    assert(sSetupDone);
    /* Nothing queued: the loop still runs steps back to back until the setup finishes. */
    sSetupCalls = sSetupDone = 0;
    ItemPc_RunSetup();
    assert(sSetupDone && sSetupCalls == 1);
#else
    /* The GBA loop is unchanged: it spins until the interrupt clears the copy. */
    sDmaPending = 1;
    sVblankAfter = 5;
    ItemPc_RunSetup();
    assert(sSetupDone && sSetupCalls == 5);
#endif
    /* A link in progress ends the loop either way. */
    sSetupCalls = sSetupDone = 0;
    sDmaPending = 1; sLinkActive = 1; sVblankAfter = 0;
    ItemPc_RunSetup();
    assert(sSetupCalls == 1 && !sSetupDone);

    puts("PASS item pc setup yield");
    return 0;
}
