/* Exercise the real boot initializer and save loader with valid storage data. */
#include <assert.h>
#include <stdio.h>
#include "main.h"
#include "quest_log.h"
#include "pokemon_storage_system.h"
#include "save.h"
#include "3ds_log.h"

struct Main gMain;
u8 gQuestLogPlaybackState;
struct SaveBlock1 gSaveBlock1;
struct SaveBlock2 gSaveBlock2;
struct PokemonStorage gPokemonStorage;
struct SaveBlock1 *gSaveBlock1Ptr;
struct SaveBlock2 *gSaveBlock2Ptr;
struct PokemonStorage *gPokemonStoragePtr;
bool32 gFlashMemoryPresent = TRUE;
u8 gDecompressionBuffer[0x4000];
static unsigned sSerializedLoads;

void CB2_InitCopyrightScreenAfterBootup(void) {}
void SetMainCallback2(MainCallback cb) { gMain.callback2 = cb; }
void LoadSerializedGame(void) { ++sSerializedLoads; }
bool8 MigrateLegacy3dsSave(void) { return FALSE; }
void CtrLog_Write(CtrLogCategory category, const char *format, ...) {}

/* Included verbatim by the runner from src/main.c, not a test copy. */
#include "save_storage_boot_init.inc"
/* Host pointers enlarge SaveBlock1's quest-log fields. Flash capacity is
 * checked by the ARM11 build; here exercise the loader's actual sector sizes. */
#undef STATIC_ASSERT
#define STATIC_ASSERT(expr, id) _Static_assert(sizeof(void *) > 4 || (expr), #id)
#include "../../src/save.c"

static struct SaveSector sFlash[SECTORS_COUNT];

void ReadFlash(u16 sector, u32 offset, void *dest, u32 size)
{
    assert(sector < SECTORS_COUNT && offset + size <= SECTOR_SIZE);
    memcpy(dest, (u8 *)&sFlash[sector] + offset, size);
}

int main(void)
{
    unsigned i;
    InitMainCallbacks();
    /* Catch the old null base before the loader writes to offset 0x5D00. */
    assert(gPokemonStoragePtr == &gPokemonStorage);
    assert(gSaveBlock1Ptr == &gSaveBlock1);
    assert(gSaveBlock2Ptr == &gSaveBlock2);

    memset(sFlash, 0, sizeof(sFlash));
    for (i = 0; i < NUM_SECTORS_PER_SLOT; ++i)
    {
        /* Save-slot rotation can put logical sector 11 first, as in the dump. */
        unsigned id = (i + 11) % NUM_SECTORS_PER_SLOT;
        memset(sFlash[i].data, id + 1, sSaveSlotLayout[id].size);
        sFlash[i].id = id;
        sFlash[i].signature = SECTOR_SIGNATURE;
        sFlash[i].checksum = CalculateChecksum(sFlash[i].data, sSaveSlotLayout[id].size);
    }
    assert(LoadGameSave(SAVE_NORMAL) == SAVE_STATUS_OK);
    assert(gSaveFileStatus == SAVE_STATUS_OK && sSerializedLoads == 1);
    assert(sSaveSlotLayout[11].offset == 0x5D00);
    assert(gRamSaveSectorLocations[11].data == (u8 *)&gPokemonStorage + 0x5D00);
    for (i = 0; i < NUM_SECTORS_PER_SLOT; ++i)
    {
        unsigned id = sFlash[i].id;
        assert(memcmp(gRamSaveSectorLocations[id].data, sFlash[i].data,
                      sSaveSlotLayout[id].size) == 0);
    }
    puts("PASS boot save load: all 14 sectors restored, including storage chunk 6");
    return 0;
}
