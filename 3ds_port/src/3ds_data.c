/* Adapted from ZallaxDev 6419a400; MIT: licenses/ZallaxDev-MIT.txt. */
/*
 * Game data backends: RomFS, loose files on the SD card, or one data pack.
 * See include/3ds_data.h for the contract and docs/ASSET_PIPELINE.md for the
 * pack format, which builder/firered3ds_builder/pak.py writes.
 */

#define _GNU_SOURCE
#include <3ds.h>
#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "3ds_data.h"
#include "3ds_pak.h"
#include "3ds_platform.h"
#include "3ds_perf.h"

/* 0 = choose at runtime; 1 = RomFS, 2 = loose, 3 = pack. */
#ifndef CTR_DATA_BACKEND
#define CTR_DATA_BACKEND 0
#endif

#define ENGINE_ABI_PATH "engine/abi.bin"
#define ENGINE_PROFILE_PATH "engine/profile.bin"
/* Written by tools/port_common/staging.py: a clean release image may only read the pack. */
static const uint8_t sCleanProfile[8] = { 'F', 'R', '3', 'D', 'C', 'L', 'N', 0 };

/* The one ROM the data pack can be built from: PokÃ©mon FireRed (USA, Europe). */
static const uint8_t sSupportedRomSha1[20] = {
    0x41, 0xcb, 0x23, 0xd8, 0xdc, 0xcc, 0x8e, 0xbd, 0x7c, 0x64,
    0x9c, 0xd8, 0xfb, 0xb5, 0x8e, 0xea, 0xce, 0x6e, 0x2f, 0xdc,
};

typedef struct
{
    uint64_t base;
    uint32_t size;
    uint32_t pos;
} PakStream;

static CtrDataBackend sBackend = CTR_DATA_NONE;
static bool sCleanRelease;
static uint32_t sEngineAbi;
static FILE *sPak;
static CtrPakEntry *sEntries;
static uint32_t sEntryCount;
static LightLock sPakLock;
static char sErrorTitle[96];
static char sErrorDetail[512];
#ifdef __3DS__
/* Hardware PERF v3: every lazy load cost 8-34 ms regardless of size (32 B
 * palettes included) because each fseek on the 64 KiB-buffered stdio pack
 * discards and refills the whole buffer from SD. Payload reads therefore use
 * one exact positional FS read; the stdio handle is kept for boot metadata. */
static Handle sPakHandle;
static bool sPakHandleOpen;
#endif
/* PERF v4: exact positional reads still paid a ~10 ms SD floor per asset.
 * The builder lays payloads out in lexical path order, so one 64 KiB
 * read-ahead window serves a whole burst (intro, battle interface/terrain).
 * Replaying the hardware load sequence: 340 logical reads -> 143 physical. */
#define PAK_WINDOW_BYTES (64u * 1024u)
static uint8_t *sPakWindow;
static uint64_t sPakWindowStart;
static uint32_t sPakWindowSize;
static uint64_t sPakSize;

/* Caller holds sPakLock. */
static bool PakReadRaw(uint64_t offset, void *buffer, uint32_t size)
{
    CTR_PERF_COUNT(PERF_PAK_PHYS_READS, 1);
#ifdef __3DS__
    if (sPakHandleOpen)
    {
        u32 got = 0;
        return R_SUCCEEDED(FSFILE_Read(sPakHandle, &got, offset, buffer, size)) && got == size;
    }
#endif
    return fseek(sPak, (long)offset, SEEK_SET) == 0 && fread(buffer, 1, size, sPak) == size;
}

/* Reads exactly size bytes at an absolute pack offset; caller holds no lock. */
static bool PakReadAt(uint64_t offset, void *buffer, uint32_t size)
{
    bool ok = true;
    LightLock_Lock(&sPakLock);
    CTR_PERF_COUNT(PERF_PAK_READS, 1);
    /* Larger requests (and anything past a sector-aligned window) go direct. */
    if (sPakWindow == NULL || size > PAK_WINDOW_BYTES - 512u || offset + size > sPakSize)
        ok = PakReadRaw(offset, buffer, size);
    else
    {
        if (offset < sPakWindowStart || offset + size > sPakWindowStart + sPakWindowSize)
        {
            uint64_t start = offset & ~511ull;
            uint64_t left = sPakSize - start;
            uint32_t span = left < PAK_WINDOW_BYTES ? (uint32_t)left : PAK_WINDOW_BYTES;
            ok = PakReadRaw(start, sPakWindow, span);
            sPakWindowStart = start;
            sPakWindowSize = ok ? span : 0;
        }
        if (ok)
            memcpy(buffer, sPakWindow + (offset - sPakWindowStart), size);
    }
    LightLock_Unlock(&sPakLock);
    return ok;
}

static bool FileExists(const char *path)
{
    struct stat st;
    return stat(path, &st) == 0;
}

static void SetError(const char *title, const char *detail)
{
    snprintf(sErrorTitle, sizeof(sErrorTitle), "%s", title);
    snprintf(sErrorDetail, sizeof(sErrorDetail), "%s", detail);
    CtrLog_Write(CTR_LOG_ERROR, "data: %s", title);
}

static void ReadEngineAbi(void)
{
    FILE *file = fopen(CTR_DATA_ROMFS_DIR ENGINE_ABI_PATH, "rb");
    uint8_t raw[4] = {0};

    sEngineAbi = 0;
    if (file != NULL)
    {
        if (fread(raw, 1, sizeof(raw), file) == sizeof(raw))
            sEngineAbi = (uint32_t)raw[0] | ((uint32_t)raw[1] << 8) | ((uint32_t)raw[2] << 16)
                       | ((uint32_t)raw[3] << 24);
        fclose(file);
    }
}

static void ReadEngineProfile(void)
{
    FILE *file = fopen(CTR_DATA_ROMFS_DIR ENGINE_PROFILE_PATH, "rb");
    uint8_t raw[8] = {0};

    sCleanRelease = false;
    if (file != NULL)
    {
        sCleanRelease = fread(raw, 1, sizeof(raw), file) == sizeof(raw) && memcmp(raw, sCleanProfile, sizeof(raw)) == 0;
        fclose(file);
    }
}

static const CtrPakEntry *FindEntry(const char *path)
{
    return CtrPak_Find(sEntries, sEntryCount, path);
}

static bool OpenPak(void)
{
    uint8_t header[CTR_PAK_HEADER_BYTES];
    uint8_t *index = NULL;
    CtrPakHeader info;
    CtrPakStatus status;
    char detail[512];

    sPak = fopen(CTR_DATA_PAK_PATH, "rb");
    if (sPak == NULL)
    {
        SetError("twinembers data pack missing.",
                 "Use twinembers Builder with your own\n"
                 "Pokemon FireRed ROM to create it, then\n"
                 "install it as:\n\n"
                 "  /3ds/twinembers/twinembers.pak");
        return false;
    }
    setvbuf(sPak, NULL, _IOFBF, 64 * 1024);
    status = fread(header, 1, sizeof(header), sPak) == sizeof(header)
           ? CtrPak_ParseHeader(header, sEngineAbi, sSupportedRomSha1, &info) : CTR_PAK_BAD_MAGIC;
    switch (status)
    {
    case CTR_PAK_OK:
        break;
    case CTR_PAK_BAD_MAGIC:
        SetError("This is not a game data pack.",
                 "/3ds/twinembers/twinembers.pak is not a\n"
                 "data pack. Run the builder again.");
        goto fail;
    case CTR_PAK_BAD_SCHEMA:
    case CTR_PAK_BAD_ABI:
        snprintf(detail, sizeof(detail),
                 "It was generated for a different\n"
                 "release. Run the builder that came\n"
                 "with this release again.\n\n"
                 "Engine ABI: %08lX (schema %u)\n"
                 "Pack ABI:   %08lX (schema %lu)",
                 (unsigned long)sEngineAbi, CTR_PAK_SCHEMA_VERSION,
                 (unsigned long)info.engineAbi, (unsigned long)info.schema);
        SetError("The data pack does not match this release.", detail);
        goto fail;
    case CTR_PAK_BAD_ROM:
        SetError("The data pack comes from another ROM.",
                 "Only Pokemon FireRed (USA, Europe) is\n"
                 "supported. Run the builder with that ROM.");
        goto fail;
    default:
        SetError("The data pack is damaged.",
                 "It does not pass the integrity check.\n"
                 "Run the builder again.");
        goto fail;
    }
    index = malloc((size_t)info.entryCount * CTR_PAK_ENTRY_BYTES);
    sEntries = malloc((size_t)info.entryCount * sizeof(*sEntries));
    if (index == NULL || sEntries == NULL
     || fseek(sPak, (long)info.indexOffset, SEEK_SET) != 0
     || fread(index, CTR_PAK_ENTRY_BYTES, info.entryCount, sPak) != info.entryCount)
    {
        SetError("The data pack could not be read.", "The SD card could not deliver its index.");
        goto fail;
    }
    if (CtrPak_ParseIndex(index, &info, sEntries) != CTR_PAK_OK)
    {
        SetError("The data pack is damaged.",
                 "Its index does not pass the integrity\n"
                 "check. Run the builder again.");
        goto fail;
    }
    /* Boot validates metadata only. Payload CRCs are checked on access. */
    if (fseek(sPak, 0, SEEK_END) != 0 || ftell(sPak) < 0
        || CtrPak_ValidateRanges(&info, sEntries, (uint64_t)ftell(sPak)) != CTR_PAK_OK)
    {
        SetError("The data pack is damaged.", "Its payload ranges are invalid.");
        goto fail;
    }
    free(index);
    sPakSize = (uint64_t)ftell(sPak);
    sPakWindow = malloc(PAK_WINDOW_BYTES); /* NULL just means direct reads */
    sPakWindowStart = 0;
    sPakWindowSize = 0;
    sEntryCount = info.entryCount;
    LightLock_Init(&sPakLock);
#ifdef __3DS__
    {
        const char *rawPath = CTR_DATA_PAK_PATH;
        if (strncmp(rawPath, "sdmc:", 5) == 0)
            rawPath += 5;
        sPakHandleOpen = R_SUCCEEDED(FSUSER_OpenFileDirectly(&sPakHandle, ARCHIVE_SDMC,
            fsMakePath(PATH_EMPTY, ""), fsMakePath(PATH_ASCII, rawPath), FS_OPEN_READ, 0));
        if (!sPakHandleOpen)
            CtrLog_Write(CTR_LOG_FS, "data: raw pack handle unavailable, using stdio");
    }
#endif
    CtrLog_Write(CTR_LOG_FS, "data: pack %lu entries, ABI %08lx",
                 (unsigned long)sEntryCount, (unsigned long)info.engineAbi);
    return true;

fail:
    free(index);
    free(sEntries);
    sEntries = NULL;
    fclose(sPak);
    sPak = NULL;
    return false;
}

bool CtrData_Init(void)
{
    int choice = CTR_DATA_BACKEND;

    CtrData_Shutdown();
    sErrorTitle[0] = sErrorDetail[0] = 0;
    ReadEngineAbi();
    ReadEngineProfile();
    if (sEngineAbi == 0)
    {
        SetError("Engine ABI is missing.", "Rebuild this private development image.");
        return false;
    }
    mkdir("sdmc:/3ds", 0777);
    mkdir("sdmc:/3ds/twinembers", 0777);
    /* A clean release has no embedded data and no development back door: it
     * reads the player's pack or stops with a message. */
    if (sCleanRelease)
        choice = 3;
    else if (choice == 0)
    {
        if (FileExists(CTR_DATA_LOOSE_MARKER))
            choice = 2;
        else if (FileExists(CTR_DATA_EMBEDDED_MARKER))
            choice = 1;
        else
            choice = 3;
    }
    switch (choice)
    {
    case 1:
        if (!FileExists(CTR_DATA_EMBEDDED_MARKER))
        {
            SetError("This build has no embedded game data.", "Install a data pack or loose data files.");
            return false;
        }
        sBackend = CTR_DATA_ROMFS;
        break;
    case 2:
        sBackend = CTR_DATA_LOOSE;
        break;
    case 3:
        if (!OpenPak())
            return false;
        sBackend = CTR_DATA_PAK;
        break;
    default:
        SetError("Invalid data backend.", "Rebuild the engine with backend 0, 1, 2 or 3.");
        return false;
    }
    CtrLog_Write(CTR_LOG_FS, "data: %s backend, engine ABI %08lx",
                 CtrData_BackendName(), (unsigned long)sEngineAbi);
    return true;
}

void CtrData_Shutdown(void)
{
#ifdef __3DS__
    if (sPakHandleOpen)
        FSFILE_Close(sPakHandle);
    sPakHandleOpen = false;
#endif
    if (sPak != NULL)
        fclose(sPak);
    sPak = NULL;
    free(sEntries);
    sEntries = NULL;
    sEntryCount = 0;
    free(sPakWindow);
    sPakWindow = NULL;
    sPakWindowSize = 0;
    sBackend = CTR_DATA_NONE;
}

CtrDataBackend CtrData_GetBackend(void) { return sBackend; }
const char *CtrData_ErrorTitle(void) { return sErrorTitle; }
const char *CtrData_ErrorDetail(void) { return sErrorDetail; }
uint32_t CtrData_EngineAbi(void) { return sEngineAbi; }

const char *CtrData_BackendName(void)
{
    switch (sBackend)
    {
    case CTR_DATA_ROMFS: return "romfs";
    case CTR_DATA_LOOSE: return "loose";
    case CTR_DATA_PAK: return "pak";
    default: return "none";
    }
}

static ssize_t PakRead(void *cookie, char *buffer, size_t size)
{
    PakStream *stream = cookie;
    size_t left = stream->size - stream->pos;


    if (size > left)
        size = left;
    if (size == 0)
        return 0;
    if (!PakReadAt(stream->base + stream->pos, buffer, (uint32_t)size))
        return -1;
    stream->pos += (uint32_t)size;
    return (ssize_t)size;
}

static int PakSeek(void *cookie, off_t *offset, int whence)
{
    PakStream *stream = cookie;
    int64_t target;

    switch (whence)
    {
    case SEEK_SET: target = *offset; break;
    case SEEK_CUR: target = (int64_t)stream->pos + *offset; break;
    case SEEK_END: target = (int64_t)stream->size + *offset; break;
    default: errno = EINVAL; return -1;
    }
    if (target < 0 || target > stream->size)
    {
        errno = EINVAL;
        return -1;
    }
    stream->pos = (uint32_t)target;
    *offset = target;
    return 0;
}

static int PakClose(void *cookie)
{
    free(cookie);
    return 0;
}

static bool PathValid(const char *path)
{
    return path != NULL && *path && *path != '/' && strstr(path, "..") == NULL && strchr(path, ':') == NULL
        && strchr(path, '\\') == NULL && strlen(path) < 400
        && strstr(path, "//") == NULL && path[strlen(path) - 1] != '/';
}

static FILE *OpenUnchecked(const char *path)
{
    char full[512];

    if (!PathValid(path))
        return NULL;
    switch (sBackend)
    {
    case CTR_DATA_ROMFS:
        snprintf(full, sizeof(full), CTR_DATA_ROMFS_DIR "%s", path);
        return fopen(full, "rb");
    case CTR_DATA_LOOSE:
        snprintf(full, sizeof(full), CTR_DATA_LOOSE_DIR "%s", path);
        return fopen(full, "rb");
    case CTR_DATA_PAK:
    {
        const CtrPakEntry *entry = FindEntry(path);
        PakStream *stream;
        cookie_io_functions_t io = { PakRead, NULL, PakSeek, PakClose };
        FILE *file;

        if (entry == NULL || (stream = malloc(sizeof(*stream))) == NULL)
            return NULL;
        stream->base = entry->offset;
        stream->size = entry->rawSize;
        stream->pos = 0;
        file = fopencookie(stream, "rb", io);
        if (file == NULL)
            free(stream);
        return file;
    }
    default:
        return NULL;
    }
}

/* Legacy seekable streams are verified before exposing any bytes. Bulk
 * loaders use OpenUnchecked internally and validate their single read. */
FILE *CtrData_Open(const char *path)
{
    FILE *file = OpenUnchecked(path);
    if (file != NULL && sBackend == CTR_DATA_PAK)
    {
        const CtrPakEntry *entry = FindEntry(path);
        uint8_t block[4096];
        uint32_t left = entry->rawSize, crc = 0;
        while (left)
        {
            size_t amount = left < sizeof(block) ? left : sizeof(block);
            if (fread(block, 1, amount, file) != amount) goto damaged;
            crc = CtrPak_Crc32(crc, block, amount);
            left -= amount;
        }
        if (crc != entry->crc32 || fseek(file, 0, SEEK_SET) != 0) goto damaged;
    }
    return file;
damaged:
    fclose(file);
    SetError("The data pack is damaged.", "A payload fails its CRC/integrity check. Rebuild the pack.");
    return NULL;
}

bool CtrData_Size(const char *path, uint32_t *outSize)
{
    if (sBackend == CTR_DATA_PAK)
    {
        const CtrPakEntry *entry = PathValid(path) ? FindEntry(path) : NULL;
        if (entry != NULL && outSize != NULL)
            *outSize = entry->rawSize;
        return entry != NULL;
    }
    FILE *file = OpenUnchecked(path);
    long size;

    if (file == NULL)
        return false;
    size = fseek(file, 0, SEEK_END) == 0 ? ftell(file) : -1;
    fclose(file);
    if (size < 0)
        return false;
    if (outSize != NULL)
        *outSize = (uint32_t)size;
    return true;
}

bool CtrData_Exists(const char *path)
{
    return CtrData_Size(path, NULL);
}

bool CtrData_ReadIntoCrc(const char *path, void *buffer, uint32_t size, uint32_t *outCrc)
{
    uint64_t perfRead = CtrPerf_Begin();
    uint32_t actual;
    FILE *file;
    bool ok;
    if (sBackend == CTR_DATA_PAK)
    {
        const CtrPakEntry *entry = buffer != NULL && PathValid(path) ? FindEntry(path) : NULL;
        if (entry == NULL || entry->rawSize != size)
        {
            CtrPerf_End(PERF_DATA_READ, perfRead);
            return false;
        }
        ok = PakReadAt(entry->offset, buffer, size);
    }
    else
    {
        if (buffer == NULL || !CtrData_Size(path, &actual) || actual != size
            || (file = OpenUnchecked(path)) == NULL)
        {
            CtrPerf_End(PERF_DATA_READ, perfRead);
            return false;
        }
        ok = fread(buffer, 1, size, file) == size;
        fclose(file);
    }
    CtrPerf_End(PERF_DATA_READ, perfRead);
    if (ok) CTR_PERF_COUNT(PERF_DATA_READ_BYTES, size);
    if (ok && (sBackend == CTR_DATA_PAK || outCrc != NULL))
    {
        uint64_t perfCrc = CtrPerf_Begin();
        uint32_t crc = CtrPak_Crc32(0, buffer, size);
        if (outCrc != NULL) *outCrc = crc;
        if (sBackend == CTR_DATA_PAK) ok = crc == FindEntry(path)->crc32;
        CtrPerf_End(PERF_DATA_CRC, perfCrc);
    }
    if (!ok)
    {
        SetError("The game data could not be loaded.", "A payload is truncated or fails its CRC. Rebuild the pack.");
        CtrLog_Write(CTR_LOG_ERROR, "data: invalid payload %s", path);
    }
    return ok;
}

bool CtrData_ReadInto(const char *path, void *buffer, uint32_t size)
{
    return CtrData_ReadIntoCrc(path, buffer, size, NULL);
}

void *CtrData_Load(const char *path, uint32_t *outSize)
{
    uint32_t size;
    uint8_t *buffer;
    if (!CtrData_Size(path, &size) || size == UINT32_MAX)
        return NULL;
    buffer = malloc((size_t)size + 1);
    if (buffer == NULL || !CtrData_ReadInto(path, buffer, size))
    {
        free(buffer);
        CtrLog_Write(CTR_LOG_ERROR, "data: short read %s", path);
        return NULL;
    }
    buffer[size] = 0;
    if (outSize != NULL)
        *outSize = size;
    return buffer;
}
