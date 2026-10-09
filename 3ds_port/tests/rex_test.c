/* Host test of the REX loader core (src/3ds_rex_core.c).
 *
 * Builds engine/rex.bin images by hand and checks: direct and scattered unit
 * copies, pointer patching with a nonzero image delta, every structural
 * rejection, and that a rejected image writes nothing. */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_pak.h"
#include "3ds_rex.h"

static int failures;
#define CHECK(cond) do { if (!(cond)) { printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

#define ARENA_BASE 0x00800000u
#define ARENA_SIZE 0x400
static uint8_t arena[ARENA_SIZE];

typedef struct { const char *path; const uint8_t *data; uint32_t size; } Pack;
static Pack pack[4];
static int packCount;
static const char *failPath;

static bool Read(void *ctx, const char *path, void *buffer, uint32_t size)
{
    (void)ctx;
    for (int i = 0; i < packCount; i++)
        if (strcmp(pack[i].path, path) == 0 && pack[i].size == size && (!failPath || strcmp(failPath, path)))
        {
            memcpy(buffer, pack[i].data, size);
            return true;
        }
    return false;
}

static uint8_t *Resolve(void *ctx, uint32_t address, uint32_t size)
{
    (void)ctx;
    if (address < ARENA_BASE || (uint64_t)address - ARENA_BASE + size > ARENA_SIZE)
        return NULL;
    return arena + (address - ARENA_BASE);
}

typedef struct { uint32_t path, size, crc, live; } Region;
typedef struct { uint32_t region, source, destination, size, firstSite, siteCount; } Unit;
typedef struct { uint32_t address, target; } Site;

static uint8_t meta[1024];
static size_t metaSize;

static void Build(const Region *regions, int rc, const Unit *units, int uc, const Site *sites, int sc,
                  const char *strings, uint32_t stringBytes)
{
    uint8_t *body = meta + 32;
    uint32_t header[8];
    memcpy(body, regions, 16u * rc);
    memcpy(body + 16u * rc, units, 24u * uc);
    memcpy(body + 16u * rc + 24u * uc, sites, 8u * sc);
    memcpy(body + 16u * rc + 24u * uc + 8u * sc, strings, stringBytes);
    metaSize = 32 + 16u * rc + 24u * uc + 8u * sc + stringBytes;
    header[0] = CTR_REX_MAGIC; header[1] = CTR_REX_VERSION; header[2] = 0x100000;
    header[3] = rc; header[4] = uc; header[5] = sc; header[6] = stringBytes;
    header[7] = CtrPak_Crc32(0, body, metaSize - 32);
    memcpy(meta, header, 32);
}

static CtrRexStatus Apply(uint32_t delta, const char **failed)
{
    CtrRexEnv env = { Read, Resolve, NULL, delta };
    return CtrRex_Apply(meta, metaSize, &env, 0, failed);
}

static const uint8_t regionA[8] = { 1, 2, 3, 4, 0xaa, 0xbb, 0xcc, 0xdd };
static const uint8_t regionB[16] = { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25 };

static void Fixture(void)
{
    static const char strings[] = "gamedata/a.bin\0gamedata/b.bin";
    Region regions[2] = { { 0, 8, 0, 1 }, { 15, 16, 0, 1 } };
    Unit units[3] = {
        { 0, 0, ARENA_BASE + 0x10, 8, 0, 1 },      /* direct, one pointer at +4 */
        { 1, 4, ARENA_BASE + 0x40, 4, 1, 0 },      /* scattered: bytes 4..7 of B */
        { 1, 8, ARENA_BASE + 0x60, 8, 1, 2 },      /* scattered: bytes 8..15, pointer at +0, constant at +6 */
    };
    Site sites[3] = { { ARENA_BASE + 0x14, 0x00900123 }, { ARENA_BASE + 0x60, 0x00900200 },
                      { (ARENA_BASE + 0x66) | CTR_REX_SITE_16, 0x1234 } };
    memset(arena, 0xee, sizeof(arena));
    packCount = 2;
    pack[0] = (Pack){ "gamedata/a.bin", regionA, 8 };
    pack[1] = (Pack){ "gamedata/b.bin", regionB, 16 };
    failPath = NULL;
    Build(regions, 2, units, 3, sites, 3, strings, sizeof(strings));
}

static uint32_t At(uint32_t offset)
{
    uint32_t v;
    memcpy(&v, arena + offset, 4);
    return v;
}

int main(void)
{
    const char *failed;

    Fixture();
    CHECK(Apply(0, &failed) == CTR_REX_OK);
    CHECK(memcmp(arena + 0x10, regionA, 4) == 0);
    CHECK(At(0x14) == 0x00900123);
    CHECK(memcmp(arena + 0x40, regionB + 4, 4) == 0);
    CHECK(At(0x60) == 0x00900200);
    CHECK(memcmp(arena + 0x64, regionB + 12, 2) == 0);
    CHECK(arena[0x66] == 0x34 && arena[0x67] == 0x12);
    CHECK(arena[0x20] == 0xee && arena[0x44] == 0xee);

    Fixture();
    CHECK(Apply(0x1000, &failed) == CTR_REX_OK);
    CHECK(At(0x14) == 0x00901123);
    CHECK(At(0x60) == 0x00901200);
    CHECK(arena[0x66] == 0x34 && arena[0x67] == 0x12); /* constants are not slid */

    /* The pack read is refused: the failing path is named. */
    Fixture();
    failPath = "gamedata/b.bin";
    CHECK(Apply(0, &failed) == CTR_REX_READ_FAILED);
    CHECK(failed && strcmp(failed, "gamedata/b.bin") == 0);

    /* Structural rejections never touch memory. */
    Fixture();
    meta[0] ^= 1;
    CHECK(Apply(0, &failed) == CTR_REX_BAD_HEADER);
    Fixture();
    meta[4] = 2;
    CHECK(Apply(0, &failed) == CTR_REX_BAD_HEADER);
    Fixture();
    metaSize -= 1;
    CHECK(Apply(0, &failed) == CTR_REX_BAD_TABLE);
    Fixture();
    meta[40] ^= 1; /* corrupt a body byte: CRC mismatch */
    CHECK(Apply(0, &failed) == CTR_REX_BAD_CRC);
    CHECK(At(0x10) == 0xeeeeeeee);
    Fixture();
    CHECK(CtrRex_Apply(meta, 16, &(CtrRexEnv){ Read, Resolve, NULL, 0 }, 0, &failed) == CTR_REX_BAD_HEADER);

    {   /* unit past the end of its region, site outside its unit, unmapped destination */
        static const char strings[] = "gamedata/a.bin";
        Region regions[1] = { { 0, 8, 0, 1 } };
        Unit bad[1] = { { 0, 4, ARENA_BASE, 8, 0, 0 } };
        Unit outside[1] = { { 0, 0, ARENA_BASE, 8, 0, 1 } };
        Unit unmapped[1] = { { 0, 0, ARENA_BASE + ARENA_SIZE, 8, 0, 0 } };
        Site site[1] = { { ARENA_BASE + 8, 1 } };

        Fixture();
        Build(regions, 1, bad, 1, NULL, 0, strings, sizeof(strings));
        CHECK(Apply(0, &failed) == CTR_REX_BAD_RANGE);
        Build(regions, 1, outside, 1, site, 1, strings, sizeof(strings));
        CHECK(Apply(0, &failed) == CTR_REX_BAD_RANGE);
        Build(regions, 1, unmapped, 1, NULL, 0, strings, sizeof(strings));
        CHECK(Apply(0, &failed) == CTR_REX_BAD_RANGE);
        CHECK(At(0) == 0xeeeeeeee);
    }

    if (failures)
        return 1;
    puts("PASS rex loader");
    return 0;
}
