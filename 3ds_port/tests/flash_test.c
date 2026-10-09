#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "global.h"
#include "gba/flash_internal.h"
#include "3ds_flash.h"

int main(void)
{
    const char *path = "build/flash-test.sav";
    u8 sector[4096];
    u8 readback[4096];
    memset(sector, 0x42, sizeof(sector));
    remove(path);
    assert(CtrFlash_Init(path));
    assert(IdentifyFlash() == 0);
    ReadFlash(0, 0, readback, sizeof(readback));
    assert(readback[0] == 0xff && readback[4095] == 0xff);
    assert(ProgramFlashSectorAndVerify(31, sector) == 0);
    assert(ProgramFlashByte(31, 15, 0x17) == 0);
    assert(CtrFlash_Flush());
    CtrFlash_Close();
    assert(CtrFlash_Init(path));
    ReadFlash(31, 0, readback, sizeof(readback));
    assert(readback[0] == 0x42 && readback[15] == 0x17);
    assert(EraseFlashSector(31) == 0);
    assert(CtrFlash_Flush());
    CtrFlash_Close();
    FILE *file = fopen(path, "rb");
    assert(file);
    assert(fseek(file, 0, SEEK_END) == 0);
    assert(ftell(file) == 131072);
    fclose(file);
    remove(path);

    /* Save renamed to twinembers.sav: an old firered.sav is copied once and left in place. */
    const char *legacy = "build/flash-test-legacy.sav";
    const char *renamed = "build/flash-test-twinembers.sav";
    remove(legacy);
    remove(renamed);
    assert(CtrFlash_Init(legacy));
    assert(ProgramFlashSectorAndVerify(3, sector) == 0);
    assert(CtrFlash_Flush());
    CtrFlash_Close();
    assert(CtrFlash_InitMigrating(renamed, legacy, NULL));
    ReadFlash(3, 0, readback, sizeof(readback));
    assert(readback[0] == 0x42 && readback[4095] == 0x42);
    assert(ProgramFlashByte(3, 0, 0x01) == 0);   /* progress after migration lands in the new file only */
    assert(CtrFlash_Flush());
    CtrFlash_Close();
    assert(CtrFlash_InitMigrating(renamed, legacy, NULL));   /* new file exists: the old one is not copied again */
    ReadFlash(3, 0, readback, sizeof(readback));
    assert(readback[0] != 0x42);                          /* the new file kept its own progress */
    CtrFlash_Close();
    assert(CtrFlash_Init(legacy));
    ReadFlash(3, 0, readback, sizeof(readback));
    assert(readback[0] == 0x42);                       /* legacy file untouched */
    CtrFlash_Close();
    remove(legacy);
    remove(renamed);
    /* Neither exists: a fresh save is created under the new name. */
    assert(CtrFlash_InitMigrating(renamed, legacy, NULL));
    CtrFlash_Close();
    file = fopen(renamed, "rb");
    assert(file);
    fclose(file);
    file = fopen(legacy, "rb");
    assert(!file);
    remove(renamed);
    /* Fallback is used only when the preferred source is absent. */
    assert(CtrFlash_Init(legacy));
    assert(ProgramFlashSectorAndVerify(3, sector) == 0);
    CtrFlash_Close();
    assert(CtrFlash_InitMigrating(renamed, "build/absent.sav", legacy));
    ReadFlash(3, 0, readback, sizeof(readback));
    assert(readback[0] == 0x42);
    CtrFlash_Close();
    remove(renamed);

    /* Destination creation failure must not silently start a new game. */
    assert(!CtrFlash_InitMigrating("build/absent-dir/save.sav", legacy, NULL));
    /* Invalid preferred source must not fall through to a valid fallback. */
    file = fopen(renamed, "wb");
    assert(file);
    assert(fputs("truncated", file) >= 0);
    assert(fclose(file) == 0);
    assert(!CtrFlash_InitMigrating(path, renamed, legacy));
    assert(fopen(path, "rb") == NULL);
    /* Invalid existing destination must stay untouched. */
    assert(!CtrFlash_InitMigrating(renamed, legacy, NULL));
    file = fopen(renamed, "rb");
    assert(file && fseek(file, 0, SEEK_END) == 0 && ftell(file) == 9);
    fclose(file);
    remove(legacy);
    remove(renamed);
    return 0;
}
