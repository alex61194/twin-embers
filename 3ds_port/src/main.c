/* ARM11 diagnostic entry. FireRed's AgbMain is deliberately not linked yet.
 * Frame pacing and input follow Zallax's 3ds_port bootstrap, adapted here as
 * the first independent FireRed build gate. */
#include <3ds.h>
#include <stdio.h>
#include <sys/stat.h>
#include "gba_shadow.h"
#include "3ds_input.h"
#include "3ds_video.h"
#include "3ds_platform.h"

static FILE *OpenLog(void)
{
    if (mkdir("sdmc:/3ds", 0777) != 0) { /* Existing directory is fine. */ }
    if (mkdir("sdmc:/3ds/twinembers", 0777) != 0) { }
    return fopen("sdmc:/3ds/twinembers/port.log", "a");
}

static unsigned long sVBlankIrqs;
static void DiagnosticVBlank(void) { ++sVBlankIrqs; }

static void BuildVideoTest(void)
{
    /* Two indexed 4bpp tiles across a 64x32 BG0 tilemap. No game assets. */
    uint16_t *regs = (uint16_t *)gGbaShadow.regs;
    uint16_t *palette = (uint16_t *)gGbaShadow.pltt;
    uint16_t *map = (uint16_t *)(gGbaShadow.vram + 0x4000);
    regs[0x000 / 2] = 0x0100; /* BG0 on, mode 0. */
    regs[0x008 / 2] = 0x4800; /* Screenbase 8, size 64x32. */
    palette[0] = 0;
    palette[1] = 0x001f; /* Red. */
    palette[2] = 0x7c00; /* Blue. */
    for (unsigned i = 0; i < 32; ++i)
    {
        gGbaShadow.vram[32 + i] = 0x11;
        gGbaShadow.vram[64 + i] = 0x22;
    }
    for (unsigned y = 0; y < 32; ++y)
        for (unsigned x = 0; x < 64; ++x)
        {
            unsigned block = x / 32;
            unsigned offset = block * 1024 + y * 32 + x % 32;
            map[offset] = ((x / 4 + y / 4) & 1) ? 1 : 2;
        }
}

int main(void)
{
    GbaShadow_Reset();
    gGbaShadow.regs[0x200] = 1; /* Enable the emulated VBlank interrupt. */
    BuildVideoTest();
    CtrInput_Clear();
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    if (!CtrVideo_Init())
        CtrPlatform_Fatal("2D compositor init failed");
    CtrVideo_Bind((CtrVideoMemory){
        .vram = gGbaShadow.vram,
        .palette = (const uint16_t *)gGbaShadow.pltt,
        .oam = (const uint16_t *)gGbaShadow.oam,
        .regs = (const uint16_t *)gGbaShadow.regs,
    });
    FILE *log = OpenLog();
    if (log != NULL)
    {
        fputs("[BOOT] FireRed 3DS F1 start\n", log);
        fflush(log);
    }

    unsigned long frames = 0;
    unsigned long aPresses = 0;
    bool running = true;
    while (running && aptMainLoop())
    {
        CtrInput_Scan();
        const CtrInput *input = CtrInput_Get();
        GbaShadow_SetHeldKeys(input->held);
        if ((input->physicalDown & CTR_KEY_A) != 0)
            ++aPresses;
        if ((input->physicalDown & CTR_KEY_START) != 0)
            running = false;

        if (frames % 30 == 0 || input->physicalDown != 0)
        {
            printf("\x1b[1;1HFireRed 3DS | 2D compositor test\n");
            printf("\x1b[3;1HFRAME %-10lu\nVBlank IRQ %-8lu\nA presses %-8lu\n", frames, sVBlankIrqs, aPresses);
            printf("\x1b[7;1HA: count once per press\nSTART: clean exit\n");
            printf("\x1b[11;1HVirtual GBA RAM: %u bytes\n", (unsigned)sizeof(gGbaShadow));
            printf("Top: 400x240 tiled test pattern\n");
            printf("Game code: not linked yet\n");
            printf("Log: sdmc:/3ds/twinembers/port.log\n");
        }
        ++frames;
        CtrVideo_Present();
        GbaShadow_DispatchVBlank(DiagnosticVBlank);
    }

    if (log != NULL)
    {
        fprintf(log, "[BOOT] clean shutdown frames=%lu a=%lu\n", frames, aPresses);
        fclose(log);
    }
    CtrVideo_Shutdown();
    gfxExit();
    return 0;
}
