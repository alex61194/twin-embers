/* FireRed-compatible 128 KiB flash image stored on SD. No GBA bus accesses. */
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

#include "global.h"
#include "gba/flash_internal.h"
#include "3ds_flash.h"
#ifdef __arm__
#include <sys/stat.h>
#include "3ds_log.h"
#define HW_SAVE_TRACE(...) CtrLog_Write(CTR_LOG_FS, __VA_ARGS__)
#else
#define HW_SAVE_TRACE(...) ((void)0)
#endif

#define FLASH_SECTOR_SIZE 4096u
#define FLASH_SECTOR_COUNT 32u

static u8 sImage[FLASH_ROM_SIZE_1M];
static FILE *sFile;
static u32 sDirtySectors;

static const struct FlashType sFlashType = {
    FLASH_ROM_SIZE_1M,
    { FLASH_SECTOR_SIZE, 12, FLASH_SECTOR_COUNT, 0 },
    { 0, 0 },
    { { 0xc2, 0x09 } },
};

u16 gFlashNumRemainingBytes;
u8 gFlashTimeoutFlag;
u8 (*PollFlashStatus)(u8 *);
u16 (*WaitForFlashWrite)(u8, u8 *, u8);
const u16 *gFlashMaxTime;
const struct FlashType *gFlash = &sFlashType;

static u16 WriteByte(u16 sector, u32 offset, u8 value)
{
    if (sector >= FLASH_SECTOR_COUNT || offset >= FLASH_SECTOR_SIZE)
        return 0x80ff;
    sImage[sector * FLASH_SECTOR_SIZE + offset] = value;
    sDirtySectors |= 1u << sector;
    return 0;
}

static u16 EraseSector(u16 sector)
{
    if (sector >= FLASH_SECTOR_COUNT)
        return 0x80ff;
    memset(sImage + sector * FLASH_SECTOR_SIZE, 0xff, FLASH_SECTOR_SIZE);
    sDirtySectors |= 1u << sector;
    return 0;
}

static u16 EraseChip(void)
{
    memset(sImage, 0xff, sizeof(sImage));
    sDirtySectors = UINT32_MAX;
    return 0;
}

static u16 WriteSector(u16 sector, void *src)
{
    if (sector >= FLASH_SECTOR_COUNT || !src)
        return 0x80ff;
    memcpy(sImage + sector * FLASH_SECTOR_SIZE, src, FLASH_SECTOR_SIZE);
    sDirtySectors |= 1u << sector;
    return 0;
}

u16 (*ProgramFlashByte)(u16, u32, u8) = WriteByte;
u16 (*ProgramFlashSector)(u16, void *) = WriteSector;
u16 (*EraseFlashChip)(void) = EraseChip;
u16 (*EraseFlashSector)(u16) = EraseSector;

/* O_EXCL is essential: neither migration nor a fresh save may truncate a
 * destination that exists (including one created between the existence check
 * and this open). Only files created by this invocation may be removed. */
static FILE *CreateExclusive(const char *path)
{
    int fd = open(path, O_RDWR | O_CREAT | O_EXCL, 0666);
    if (fd < 0)
        return NULL;
    FILE *file = fdopen(fd, "wb+");
    if (!file)
    {
        int error = errno;
        close(fd);
        remove(path);
        errno = error;
    }
    return file;
}

bool CtrFlash_Init(const char *path)
{
    memset(sImage, 0xff, sizeof(sImage));
    sDirtySectors = 0;
    sFile = fopen(path, "rb+");
    if (sFile)
    {
        /* A truncated, oversized or unreadable save is an error, never a
         * partially blank image that the game could treat as a new game. */
        bool ok = fread(sImage, 1, sizeof(sImage), sFile) == sizeof(sImage);
        int extra = fgetc(sFile);
        ok = ok && extra == EOF && !ferror(sFile);
        if (!ok || fseek(sFile, 0, SEEK_SET) != 0)
        {
            fclose(sFile);
            sFile = NULL;
            HW_SAVE_TRACE("HW SAVE ERROR invalid or unreadable save %s", path);
            return false;
        }
        return true;
    }
    if (errno != ENOENT)
        return false;
    sFile = CreateExclusive(path);
    if (!sFile)
        return false;
    sDirtySectors = UINT32_MAX;
    if (!CtrFlash_Flush())
    {
        fclose(sFile);
        sFile = NULL;
        remove(path);
        return false;
    }
    return true;
}

bool CtrFlash_InitMigrating(const char *path, const char *legacyPath,
                            const char *fallbackPath)
{
    FILE *current = fopen(path, "rb");
    if (current)
    {
        if (fclose(current) != 0)
            return false;
        return CtrFlash_Init(path);
    }
    if (errno != ENOENT)
        return false;

    const char *sources[] = { legacyPath, fallbackPath };
    for (unsigned i = 0; i < sizeof(sources) / sizeof(sources[0]); ++i)
    {
        if (!sources[i])
            continue;
        FILE *old = fopen(sources[i], "rb");
        if (!old)
        {
            if (errno != ENOENT)
                return false;
            continue;
        }
        /* Validate the complete source before creating the destination. */
        unsigned char *image = sImage;
        bool ok = fread(image, 1, sizeof(sImage), old) == sizeof(sImage);
        int extra = fgetc(old);
        ok = ok && extra == EOF && !ferror(old);
        if (fclose(old) != 0)
            ok = false;
        if (!ok)
        {
            HW_SAVE_TRACE("HW SAVE ERROR invalid or unreadable legacy save %s", sources[i]);
            return false;
        }
        FILE *copy = CreateExclusive(path);
        if (!copy)
        {
            HW_SAVE_TRACE("HW SAVE ERROR cannot create migration destination %s errno=%d", path, errno);
            return false;
        }
        ok = fwrite(image, 1, sizeof(sImage), copy) == sizeof(sImage);
        if (fflush(copy) != 0)
            ok = false;
        if (fclose(copy) != 0)
            ok = false;
        if (!ok)
        {
            /* No fallthrough to fresh-save creation after any copy failure. */
            remove(path);
            HW_SAVE_TRACE("HW SAVE ERROR copying %s -> %s", sources[i], path);
            return false;
        }
        HW_SAVE_TRACE("HW SAVE migrated %s -> %s", sources[i], path);
        return CtrFlash_Init(path);
    }
    return CtrFlash_Init(path);
}

/* A sector is clean only once fflush has confirmed every byte written: fwrite
 * only fills the stdio buffer. A failed seek, write or flush leaves all of this
 * pass's sectors pending and clears the stream error, so a retry rewrites them
 * (each at its own fixed offset) and never reports a save it did not write. */
bool CtrFlash_Flush(void)
{
    if (!sFile)
        return false;
    u32 pending = sDirtySectors;
    if (pending == 0)
        return true;
    for (u32 sector = 0; sector < FLASH_SECTOR_COUNT; ++sector)
    {
        if (!(pending & (1u << sector)))
            continue;
        if (fseek(sFile, sector * FLASH_SECTOR_SIZE, SEEK_SET) != 0
            || fwrite(sImage + sector * FLASH_SECTOR_SIZE, 1,
                      FLASH_SECTOR_SIZE, sFile) != FLASH_SECTOR_SIZE)
        {
            clearerr(sFile);
            HW_SAVE_TRACE("HW SAVE ERROR writing sector %u", (unsigned)sector);
            return false;
        }
    }
    if (fflush(sFile) != 0)
    {
        clearerr(sFile);
        HW_SAVE_TRACE("HW SAVE ERROR flushing the save");
        return false;
    }
    sDirtySectors &= ~pending;
    return true;
}

bool CtrFlash_Close(void)
{
    if (!sFile)
        return true;
    /* One retry: the failed attempt cleared the stream error. */
    bool ok = CtrFlash_Flush() || CtrFlash_Flush();
    if (fclose(sFile) != 0)
        ok = false;
    sFile = NULL;
    if (!ok)
        HW_SAVE_TRACE("HW SAVE ERROR closing the save");
    return ok;
}

u16 IdentifyFlash(void)
{
    gFlash = &sFlashType;
    return 0;
}

u16 SetFlashTimerIntr(u8 timerNum, void (**intrFunc)(void))
{
    (void)timerNum;
    (void)intrFunc;
    return 0;
}

void ReadFlash(u16 sector, u32 offset, void *dest, u32 size)
{
    if (!dest || sector >= FLASH_SECTOR_COUNT || offset > FLASH_SECTOR_SIZE
        || size > FLASH_SECTOR_SIZE - offset)
        return;
    memcpy(dest, sImage + sector * FLASH_SECTOR_SIZE + offset, size);
}

u32 ProgramFlashSectorAndVerify(u16 sector, u8 *src)
{
    if (WriteSector(sector, src))
        return 0x80ff;
    return memcmp(sImage + sector * FLASH_SECTOR_SIZE, src,
                  FLASH_SECTOR_SIZE) == 0 ? 0 : 0x80ff;
}

u32 ProgramFlashSectorAndVerifyNBytes(u16 sector, void *src, u32 size)
{
    if (size > FLASH_SECTOR_SIZE || WriteSector(sector, src))
        return 0x80ff;
    return memcmp(sImage + sector * FLASH_SECTOR_SIZE, src, size) == 0
        ? 0 : 0x80ff;
}
