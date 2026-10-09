/* The 3DS has no GBA serial or GameCube multiboot hardware. Preserve the
 * intro timing while reporting that these optional transfers are absent. */
#include <stdlib.h>
#include <string.h>

#include "global.h"
#include "libgcnmultiboot.h"
#include "gba/multiboot.h"
#include "3ds_flash.h"

const char RomHeaderGameCode[4] = { 'B', 'P', 'R', 'E' };
const char RomHeaderSoftwareVersion = 0;

void GameCubeMultiBoot_Init(struct GcmbStruct *state)
{
    memset(state, 0, sizeof(*state));
}

void GameCubeMultiBoot_Main(struct GcmbStruct *state)
{
    (void)state;
}

void GameCubeMultiBoot_HandleSerialInterrupt(struct GcmbStruct *state)
{
    (void)state;
}

void GameCubeMultiBoot_ExecuteProgram(struct GcmbStruct *state)
{
    (void)state;
}

void GameCubeMultiBoot_Quit(void)
{
}

int MultiBoot(struct MultiBootParam *param)
{
    (void)param;
    return 1;
}

/* These GBA serial payloads are never entered on 3DS. Their symbols remain
 * because FireRed's optional transfer screens refer to them. */
const u8 gMultiBootProgram_PokemonColosseum_Start[1] = {0};
const u8 gMultiBootProgram_BerryGlitchFix_Start[0x3bf4] = {0};
const u8 gMultiBootProgram_BerryGlitchFix_End[1] = {0};

void SoftReset(u32 flags)
{
    (void)flags;
    CtrFlash_Close();
    exit(0);
}
