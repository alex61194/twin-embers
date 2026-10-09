/* REX loader core: pure functions, no SDK, host-testable (tests/rex_test.c). */
#include <stdlib.h>
#include <string.h>

#include "3ds_pak.h"
#include "3ds_rex.h"

typedef struct
{
    uint32_t magic, version, reference, regionCount, unitCount, siteCount, stringBytes, bodyCrc;
} Header;
typedef struct { uint32_t path, size, crc, live; } Region;
typedef struct { uint32_t region, source, destination, size, firstSite, siteCount; } Unit;
typedef struct { uint32_t address, target; } Site;

static uint32_t Word(const uint8_t *at)
{
    uint32_t v;
    memcpy(&v, at, sizeof(v));
    return v;
}

static void LoadHeader(const uint8_t *p, Header *h)
{
    uint32_t *out = (uint32_t *)h;
    for (int i = 0; i < 8; i++)
        out[i] = Word(p + 4 * i);
}

CtrRexStatus CtrRex_Apply(const uint8_t *meta, size_t metaSize, const CtrRexEnv *env,
                          uint32_t referenceRuntime, const char **failed)
{
    Header h;
    const uint8_t *regions, *units, *sites, *strings;
    uint64_t need;
    uint32_t n;
    uint8_t *scratch = NULL;
    uint32_t scratchSize = 0;
    CtrRexStatus status = CTR_REX_OK;

    if (failed)
        *failed = NULL;
    if (metaSize < 32)
        return CTR_REX_BAD_HEADER;
    LoadHeader(meta, &h);
    if (h.magic != CTR_REX_MAGIC || h.version != CTR_REX_VERSION)
        return CTR_REX_BAD_HEADER;
    need = 32ull + 16ull * h.regionCount + 24ull * h.unitCount + 8ull * h.siteCount + h.stringBytes;
    if (need != metaSize)
        return CTR_REX_BAD_TABLE;
    if (CtrPak_Crc32(0, meta + 32, metaSize - 32) != h.bodyCrc)
        return CTR_REX_BAD_CRC;
    regions = meta + 32;
    units = regions + 16ull * h.regionCount;
    sites = units + 24ull * h.unitCount;
    strings = sites + 8ull * h.siteCount;

    /* Validate every table before writing anything. */
    for (n = 0; n < h.regionCount; n++)
    {
        Region r;
        r.path = Word(regions + 16 * n);
        if (r.path >= h.stringBytes || memchr(strings + r.path, 0, h.stringBytes - r.path) == NULL)
            return CTR_REX_BAD_TABLE;
    }
    for (n = 0; n < h.unitCount; n++)
    {
        Unit u;
        const uint8_t *p = units + 24 * n;
        u.region = Word(p), u.source = Word(p + 4), u.destination = Word(p + 8);
        u.size = Word(p + 12), u.firstSite = Word(p + 16), u.siteCount = Word(p + 20);
        if (u.region >= h.regionCount || (uint64_t)u.source + u.size > Word(regions + 16 * u.region + 4)
            || (uint64_t)u.firstSite + u.siteCount > h.siteCount || u.size == 0)
            return CTR_REX_BAD_RANGE;
        if (n && u.region < Word(units + 24 * (n - 1)))
            return CTR_REX_BAD_TABLE; /* units must be grouped by region */
        for (uint32_t s = 0; s < u.siteCount; s++)
        {
            uint32_t addr = Word(sites + 8 * (u.firstSite + s));
            uint32_t width = addr & CTR_REX_SITE_16 ? 2u : 4u;

            addr &= ~CTR_REX_SITE_16;
            if (u.size < width || addr < u.destination || addr - u.destination > u.size - width)
                return CTR_REX_BAD_RANGE;
        }
    }

    for (n = 0; n < h.unitCount;)
    {
        uint32_t region = Word(units + 24 * n);
        uint32_t end = n;
        const char *path = (const char *)strings + Word(regions + 16 * region);
        uint32_t regionSize = Word(regions + 16 * region + 4);
        bool direct;

        while (end < h.unitCount && Word(units + 24 * end) == region)
            end++;
        direct = end - n == 1 && Word(units + 24 * n + 4) == 0 && Word(units + 24 * n + 12) == regionSize;
        if (direct)
        {
            uint8_t *dst = env->resolve(env->ctx, Word(units + 24 * n + 8), regionSize);
            if (dst == NULL)
            {
                status = CTR_REX_BAD_RANGE;
                break;
            }
            if (!env->read(env->ctx, path, dst, regionSize))
            {
                status = CTR_REX_READ_FAILED;
                if (failed) *failed = path;
                break;
            }
        }
        else
        {
            if (regionSize > scratchSize)
            {
                free(scratch);
                scratch = malloc(regionSize);
                scratchSize = scratch ? regionSize : 0;
                if (scratch == NULL)
                {
                    status = CTR_REX_NO_MEMORY;
                    break;
                }
            }
            if (!env->read(env->ctx, path, scratch, regionSize))
            {
                status = CTR_REX_READ_FAILED;
                if (failed) *failed = path;
                break;
            }
            for (uint32_t u = n; u < end; u++)
            {
                const uint8_t *p = units + 24 * u;
                uint8_t *dst = env->resolve(env->ctx, Word(p + 8), Word(p + 12));
                if (dst == NULL)
                {
                    status = CTR_REX_BAD_RANGE;
                    break;
                }
                memcpy(dst, scratch + Word(p + 4), Word(p + 12));
            }
            if (status != CTR_REX_OK)
                break;
        }
        n = end;
    }
    free(scratch);
    if (status != CTR_REX_OK)
        return status;

    for (n = 0; n < h.unitCount; n++)
    {
        const uint8_t *p = units + 24 * n;
        uint32_t destination = Word(p + 8), count = Word(p + 20), first = Word(p + 16);
        uint8_t *base = count ? env->resolve(env->ctx, destination, Word(p + 12)) : NULL;

        if (count && base == NULL)
            return CTR_REX_BAD_RANGE;
        for (uint32_t s = 0; s < count; s++)
        {
            uint32_t addr = Word(sites + 8 * (first + s));
            uint32_t value = Word(sites + 8 * (first + s) + 4);

            if (addr & CTR_REX_SITE_16)
            {
                /* An absolute constant (a special's number), not an address. */
                uint16_t small = (uint16_t)value;
                memcpy(base + ((addr & ~CTR_REX_SITE_16) - destination), &small, sizeof(small));
            }
            else
            {
                value += env->delta;
                memcpy(base + (addr - destination), &value, sizeof(value));
            }
        }
    }
    (void)referenceRuntime;
    return CTR_REX_OK;
}
