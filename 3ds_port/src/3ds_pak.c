/* Adapted from ZallaxDev 6419a400; MIT: licenses/ZallaxDev-MIT.txt. */
/* twinembers.pak parsing; see include/3ds_pak.h. */

#include <stdlib.h>
#include <string.h>

#include "3ds_pak.h"

/* Immutable implementation: no lazy global table race at first read.
 * Constant 4-bit table (reflected 0xEDB88320); identical results to the
 * bitwise form, about 4x fewer operations per byte on the ARM11. */
static const uint32_t sCrcNibble[16] = {
    0x00000000u, 0x1db71064u, 0x3b6e20c8u, 0x26d930acu,
    0x76dc4190u, 0x6b6b51f4u, 0x4db26158u, 0x5005713cu,
    0xedb88320u, 0xf00f9344u, 0xd6d6a3e8u, 0xcb61b38cu,
    0x9b64c2b0u, 0x86d3d2d4u, 0xa00ae278u, 0xbdbdf21cu,
};

uint32_t CtrPak_Crc32(uint32_t crc, const void *data, size_t size)
{
    const uint8_t *p = data;
    crc = ~crc;
    while (size--)
    {
        crc ^= *p++;
        crc = (crc >> 4) ^ sCrcNibble[crc & 15u];
        crc = (crc >> 4) ^ sCrcNibble[crc & 15u];
    }
    return ~crc;
}

/* FNV-1a, 64 bit, over the path exactly as given ("maps/layouts.bin"). */
uint64_t CtrPak_PathId(const char *path)
{
    uint64_t hash = 0xcbf29ce484222325ull;

    for (const unsigned char *p = (const unsigned char *)path; *p; ++p)
    {
        hash ^= *p;
        hash *= 0x100000001b3ull;
    }
    return hash;
}

static uint32_t Le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

static uint64_t Le64(const uint8_t *p)
{
    return (uint64_t)Le32(p) | ((uint64_t)Le32(p + 4) << 32);
}

CtrPakStatus CtrPak_ParseHeader(const uint8_t raw[CTR_PAK_HEADER_BYTES], uint32_t engineAbi,
                                const uint8_t romSha1[20], CtrPakHeader *out)
{
    if (memcmp(raw, "FR3DPAK\0", 8) != 0)
        return CTR_PAK_BAD_MAGIC;
    if (CtrPak_Crc32(0, raw, 60) != Le32(raw + 60))
        return CTR_PAK_BAD_HEADER_CRC;
    out->schema = Le32(raw + 8);
    out->engineAbi = Le32(raw + 12);
    memcpy(out->romSha1, raw + 16, 20);
    out->entryCount = Le32(raw + 36);
    out->indexOffset = Le64(raw + 40);
    out->dataOffset = Le64(raw + 48);
    out->indexCrc = Le32(raw + 56);
    if (out->schema != CTR_PAK_SCHEMA_VERSION)
        return CTR_PAK_BAD_SCHEMA;
    if (engineAbi == 0 || out->engineAbi != engineAbi)
        return CTR_PAK_BAD_ABI;
    if (memcmp(out->romSha1, romSha1, 20) != 0)
        return CTR_PAK_BAD_ROM;
    if (out->entryCount == 0 || out->entryCount > 65536u)
        return CTR_PAK_BAD_COUNT;
    if (out->indexOffset != CTR_PAK_HEADER_BYTES
        || out->dataOffset != ((64ull + (uint64_t)out->entryCount * 40ull + 31ull) & ~31ull))
        return CTR_PAK_BAD_INDEX;
    return CTR_PAK_OK;
}

CtrPakStatus CtrPak_ParseIndex(const uint8_t *raw, const CtrPakHeader *header, CtrPakEntry *out)
{
    if (CtrPak_Crc32(0, raw, (size_t)header->entryCount * CTR_PAK_ENTRY_BYTES) != header->indexCrc)
        return CTR_PAK_BAD_INDEX_CRC;
    for (uint32_t i = 0; i < header->entryCount; ++i)
    {
        const uint8_t *e = raw + (size_t)i * CTR_PAK_ENTRY_BYTES;
        CtrPakEntry *entry = &out[i];

        entry->id = Le64(e);
        entry->type = Le32(e + 8);
        entry->flags = Le32(e + 12);
        entry->offset = Le64(e + 16);
        entry->storedSize = Le32(e + 24);
        entry->rawSize = Le32(e + 28);
        entry->crc32 = Le32(e + 32);
        entry->reserved = Le32(e + 36);
        if ((i != 0 && entry->id <= out[i - 1].id) || (entry->flags != 0 || entry->reserved != 0 || entry->type > 5 || (entry->offset & 31u))
         || entry->storedSize != entry->rawSize || entry->offset < header->dataOffset)
            return CTR_PAK_BAD_INDEX;
    }
    return CTR_PAK_OK;
}

const CtrPakEntry *CtrPak_Find(const CtrPakEntry *entries, uint32_t count, const char *path)
{
    uint64_t id = CtrPak_PathId(path);
    uint32_t lo = 0, hi = count;

    while (lo < hi)
    {
        uint32_t mid = lo + (hi - lo) / 2;
        if (entries[mid].id < id)
            lo = mid + 1;
        else
            hi = mid;
    }
    return lo < count && entries[lo].id == id ? &entries[lo] : NULL;
}

typedef struct
{
    uint64_t offset;
    uint32_t size;
} PakRange;

static int CompareRange(const void *a, const void *b)
{
    uint64_t x = ((const PakRange *)a)->offset, y = ((const PakRange *)b)->offset;
    return x < y ? -1 : x > y;
}

/* Schema 2: any index order, but the payloads tile the data area exactly. */
CtrPakStatus CtrPak_ValidateRanges(const CtrPakHeader *header, const CtrPakEntry *entries, uint64_t fileSize)
{
    uint64_t cursor = header->dataOffset;
    CtrPakStatus status = CTR_PAK_OK;
    PakRange *ranges;

    if (fileSize > 0x7fffffffull || cursor > fileSize || header->entryCount == 0) return CTR_PAK_BAD_INDEX;
    ranges = malloc((size_t)header->entryCount * sizeof(*ranges));
    if (ranges == NULL) return CTR_PAK_BAD_INDEX;
    for (uint32_t i = 0; i < header->entryCount; ++i)
    {
        ranges[i].offset = entries[i].offset;
        ranges[i].size = entries[i].rawSize;
    }
    qsort(ranges, header->entryCount, sizeof(*ranges), CompareRange);
    for (uint32_t i = 0; i < header->entryCount && status == CTR_PAK_OK; ++i)
    {
        const PakRange *r = &ranges[i];
        if (r->offset != cursor || r->size > fileSize - cursor) status = CTR_PAK_BAD_INDEX;
        cursor = (cursor + r->size + 31ull) & ~31ull;
        if (cursor > fileSize) status = CTR_PAK_BAD_INDEX;
    }
    free(ranges);
    return status == CTR_PAK_OK && cursor == fileSize ? CTR_PAK_OK : CTR_PAK_BAD_INDEX;
}
