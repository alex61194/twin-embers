/* Separate ARM11 entry point for the real FireRed link target. */
#include <3ds.h>
#include <sys/stat.h>

#include "3ds_dma.h"
#include "3ds_flash.h"
#include "3ds_input.h"
#include "3ds_platform.h"
#include "3ds_video.h"
#include "gba_shadow.h"

extern void AgbMain(void);

int main(void)
{
    /* Boot stages are logged once to sdmc:/3ds/twinembers/port.log so a
     * failed AgbMain boot can be told apart from a platform init failure. */
    CtrLog_Write(CTR_LOG_BOOT, "native main reached");
    GbaShadow_Reset();
    CtrDma_Reset();
    CtrInput_Clear();
    CtrLog_Write(CTR_LOG_BOOT, "shadow memory initialized");
    gfxInitDefault();
    consoleInit(GFX_BOTTOM, NULL);
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/twinembers", 0777);
    if (!CtrFlash_InitMigrating("sdmc:/3ds/twinembers/twinembers.sav",
                                  "sdmc:/3ds/pokefirered/twinembers.sav",
                                  "sdmc:/3ds/pokefirered/firered.sav"))
        CtrPlatform_Fatal("Twin Embers save could not be opened or migrated. Original saves preserved; check SD space and permissions.");
    CtrLog_Write(CTR_LOG_FS, "flash initialized");
    if (!CtrVideo_Init())
        CtrPlatform_Fatal("FireRed compositor init failed");
    CtrVideo_Bind((CtrVideoMemory){
        .vram = gGbaShadow.vram,
        .palette = (const uint16_t *)gGbaShadow.pltt,
        .oam = (const uint16_t *)gGbaShadow.oam,
        .regs = (const uint16_t *)gGbaShadow.regs,
    });
    CtrLog_Write(CTR_LOG_VIDEO, "video initialized");
    CtrLog_Write(CTR_LOG_GAME, "entering AgbMain");
    AgbMain();
    CtrLog_Write(CTR_LOG_GAME, "AgbMain returned");
    CtrFlash_Close();
    CtrVideo_Shutdown();
    gfxExit();
    return 0;
}
