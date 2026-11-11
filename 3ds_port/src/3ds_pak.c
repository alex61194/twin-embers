/* Adapted from ZallaxDev 6419a400; MIT: licenses/ZallaxDev-MIT.txt. */
/* twinembers.pak parsing; see include/3ds_pak.h. */

#include <stdlib.h>
#include <string.h>

#include "3ds_pak.h"

/* Immutable implementation: no lazy global table race at first read.
 * Constant 256-entry table (reflected 0xEDB88320, 1 KiB); identical results to the
 * bitwise form, one lookup per byte on the ARM11 (the 4-bit table took two). */
static const uint32_t sCrcTable[256] = {
    0x00000000u, 0x77073096u, 0xee0e612cu, 0x990951bau,
    0x076dc419u, 0x706af48fu, 0xe963a535u, 0x9e6495a3u,
    0x0edb8832u, 0x79dcb8a4u, 0xe0d5e91eu, 0x97d2d988u,
    0x09b64c2bu, 0x7eb17cbdu, 0xe7b82d07u, 0x90bf1d91u,
    0x1db71064u, 0x6ab020f2u, 0xf3b97148u, 0x84be41deu,
    0x1adad47du, 0x6ddde4ebu, 0xf4d4b551u, 0x83d385c7u,
    0x136c9856u, 0x646ba8c0u, 0xfd62f97au, 0x8a65c9ecu,
    0x14015c4fu, 0x63066cd9u, 0xfa0f3d63u, 0x8d080df5u,
    0x3b6e20c8u, 0x4c69105eu, 0xd56041e4u, 0xa2677172u,
    0x3c03e4d1u, 0x4b04d447u, 0xd20d85fdu, 0xa50ab56bu,
    0x35b5a8fau, 0x42b2986cu, 0xdbbbc9d6u, 0xacbcf940u,
    0x32d86ce3u, 0x45df5c75u, 0xdcd60dcfu, 0xabd13d59u,
    0x26d930acu, 0x51de003au, 0xc8d75180u, 0xbfd06116u,
    0x21b4f4b5u, 0x56b3c423u, 0xcfba9599u, 0xb8bda50fu,
    0x2802b89eu, 0x5f058808u, 0xc60cd9b2u, 0xb10be924u,
    0x2f6f7c87u, 0x58684c11u, 0xc1611dabu, 0xb6662d3du,
    0x76dc4190u, 0x01db7106u, 0x98d220bcu, 0xefd5102au,
    0x71b18589u, 0x06b6b51fu, 0x9fbfe4a5u, 0xe8b8d433u,
    0x7807c9a2u, 0x0f00f934u, 0x9609a88eu, 0xe10e9818u,
    0x7f6a0dbbu, 0x086d3d2du, 0x91646c97u, 0xe6635c01u,
    0x6b6b51f4u, 0x1c6c6162u, 0x856530d8u, 0xf262004eu,
    0x6c0695edu, 0x1b01a57bu, 0x8208f4c1u, 0xf50fc457u,
    0x65b0d9c6u, 0x12b7e950u, 0x8bbeb8eau, 0xfcb9887cu,
    0x62dd1ddfu, 0x15da2d49u, 0x8cd37cf3u, 0xfbd44c65u,
    0x4db26158u, 0x3ab551ceu, 0xa3bc0074u, 0xd4bb30e2u,
    0x4adfa541u, 0x3dd895d7u, 0xa4d1c46du, 0xd3d6f4fbu,
    0x4369e96au, 0x346ed9fcu, 0xad678846u, 0xda60b8d0u,
    0x44042d73u, 0x33031de5u, 0xaa0a4c5fu, 0xdd0d7cc9u,
    0x5005713cu, 0x270241aau, 0xbe0b1010u, 0xc90c2086u,
    0x5768b525u, 0x206f85b3u, 0xb966d409u, 0xce61e49fu,
    0x5edef90eu, 0x29d9c998u, 0xb0d09822u, 0xc7d7a8b4u,
    0x59b33d17u, 0x2eb40d81u, 0xb7bd5c3bu, 0xc0ba6cadu,
    0xedb88320u, 0x9abfb3b6u, 0x03b6e20cu, 0x74b1d29au,
    0xead54739u, 0x9dd277afu, 0x04db2615u, 0x73dc1683u,
    0xe3630b12u, 0x94643b84u, 0x0d6d6a3eu, 0x7a6a5aa8u,
    0xe40ecf0bu, 0x9309ff9du, 0x0a00ae27u, 0x7d079eb1u,
    0xf00f9344u, 0x8708a3d2u, 0x1e01f268u, 0x6906c2feu,
    0xf762575du, 0x806567cbu, 0x196c3671u, 0x6e6b06e7u,
    0xfed41b76u, 0x89d32be0u, 0x10da7a5au, 0x67dd4accu,
    0xf9b9df6fu, 0x8ebeeff9u, 0x17b7be43u, 0x60b08ed5u,
    0xd6d6a3e8u, 0xa1d1937eu, 0x38d8c2c4u, 0x4fdff252u,
    0xd1bb67f1u, 0xa6bc5767u, 0x3fb506ddu, 0x48b2364bu,
    0xd80d2bdau, 0xaf0a1b4cu, 0x36034af6u, 0x41047a60u,
    0xdf60efc3u, 0xa867df55u, 0x316e8eefu, 0x4669be79u,
    0xcb61b38cu, 0xbc66831au, 0x256fd2a0u, 0x5268e236u,
    0xcc0c7795u, 0xbb0b4703u, 0x220216b9u, 0x5505262fu,
    0xc5ba3bbeu, 0xb2bd0b28u, 0x2bb45a92u, 0x5cb36a04u,
    0xc2d7ffa7u, 0xb5d0cf31u, 0x2cd99e8bu, 0x5bdeae1du,
    0x9b64c2b0u, 0xec63f226u, 0x756aa39cu, 0x026d930au,
    0x9c0906a9u, 0xeb0e363fu, 0x72076785u, 0x05005713u,
    0x95bf4a82u, 0xe2b87a14u, 0x7bb12baeu, 0x0cb61b38u,
    0x92d28e9bu, 0xe5d5be0du, 0x7cdcefb7u, 0x0bdbdf21u,
    0x86d3d2d4u, 0xf1d4e242u, 0x68ddb3f8u, 0x1fda836eu,
    0x81be16cdu, 0xf6b9265bu, 0x6fb077e1u, 0x18b74777u,
    0x88085ae6u, 0xff0f6a70u, 0x66063bcau, 0x11010b5cu,
    0x8f659effu, 0xf862ae69u, 0x616bffd3u, 0x166ccf45u,
    0xa00ae278u, 0xd70dd2eeu, 0x4e048354u, 0x3903b3c2u,
    0xa7672661u, 0xd06016f7u, 0x4969474du, 0x3e6e77dbu,
    0xaed16a4au, 0xd9d65adcu, 0x40df0b66u, 0x37d83bf0u,
    0xa9bcae53u, 0xdebb9ec5u, 0x47b2cf7fu, 0x30b5ffe9u,
    0xbdbdf21cu, 0xcabac28au, 0x53b39330u, 0x24b4a3a6u,
    0xbad03605u, 0xcdd70693u, 0x54de5729u, 0x23d967bfu,
    0xb3667a2eu, 0xc4614ab8u, 0x5d681b02u, 0x2a6f2b94u,
    0xb40bbe37u, 0xc30c8ea1u, 0x5a05df1bu, 0x2d02ef8du,
};

uint32_t CtrPak_Crc32(uint32_t crc, const void *data, size_t size)
{
    const uint8_t *p = data;
    crc = ~crc;
    while (size--)
        crc = (crc >> 8) ^ sCrcTable[(crc ^ *p++) & 0xffu];
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
