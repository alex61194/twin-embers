/* HeartGold's battle sprites in place of FireRed's (see include/3ds_battle_gen4.h). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_battle_gen4.h"
#include "3ds_data.h"
#include "3ds_log.h"
#include "3ds_video.h"

#define BATTLERS 4
#define HEADER 72

typedef struct {
    bool active;
    float pivotY;
    uint8_t tiles[CTR_GEN4_PICTURE];
} Battler;

static Battler sBattlers[BATTLERS];

/* The lowest row of a 64x64 FireRed picture (8x8 tiles, eight to a row) with a pixel in it, or -1. */
static int FireBottom(const uint8_t *pic)
{
    for (int y = 63; y >= 0; --y)
        for (int tx = 0; tx < 8; ++tx)
        {
            const uint8_t *row = pic + ((y / 8) * 8 + tx) * 32 + (y % 8) * 4;
            if (row[0] | row[1] | row[2] | row[3])
                return y;
        }
    return -1;
}

/* The same for an 80x80 picture in rows of 40 bytes. */
static int Gen4Bottom(const uint8_t *linear)
{
    for (int y = CTR_GEN4_SIZE - 1; y >= 0; --y)
        for (int x = 0; x < CTR_GEN4_SIZE / 2; ++x)
            if (linear[y * (CTR_GEN4_SIZE / 2) + x])
                return y;
    return -1;
}

static bool Match(unsigned attr2, CtrGen4Obj *out)
{
    for (unsigned b = 0; b < BATTLERS; ++b)
    {
        if (!sBattlers[b].active)
            continue;
        int oam = CtrGen4_SpriteOam(b);
        if (oam < 0 || (attr2 & 1023) != ((unsigned)oam & 1023) || (attr2 >> 12) != ((unsigned)oam >> 12))
            continue;
        out->slot = b;
        out->tiles = sBattlers[b].tiles;
        out->pivotX = CTR_GEN4_SIZE / 2.0f;
        out->pivotY = sBattlers[b].pivotY;
        out->scale = CTR_GEN4_SCALE;
        return true;
    }
    return false;
}

bool CtrGen4_Load(unsigned battler, unsigned nationalDex, bool back, bool female, bool shiny,
                  const void *firePic, uint16_t palette[16])
{
    if (battler >= BATTLERS)
        return false;
    sBattlers[battler].active = false;
    if (nationalDex < 1 || nationalDex > 386)
        return false;
    char path[40];
    snprintf(path, sizeof(path), "graphics/gen4/battle/%03u.bin", nationalDex);
    uint32_t size = 0;
    uint8_t *entry = CtrData_Load(path, &size);
    if (!entry)
        return false;
    bool pair = size >= 8 && (entry[4] & 1);
    if (size < 4 || memcmp(entry, "G4B1", 4) || size != HEADER + (pair ? 4u : 2u) * CTR_GEN4_PICTURE)
    {
        CtrLog_Write(CTR_LOG_GAME, "GEN4 %s: wrong size or kind", path);
        free(entry);
        return false;
    }
    const uint8_t *linear = entry + HEADER + ((female && pair) ? 2 : 0) * CTR_GEN4_PICTURE
                          + (back ? CTR_GEN4_PICTURE : 0);
    memcpy(palette, entry + (shiny ? 40 : 8), 32);
    /* Rows of 40 bytes to the GBA's 8x8 tiles, ten to a row. */
    uint8_t *tiles = sBattlers[battler].tiles;
    for (unsigned ty = 0; ty < CTR_GEN4_TILES; ++ty)
        for (unsigned tx = 0; tx < CTR_GEN4_TILES; ++tx)
            for (unsigned r = 0; r < 8; ++r)
                memcpy(tiles + ((ty * CTR_GEN4_TILES + tx) * 8 + r) * 4,
                       linear + (ty * 8 + r) * (CTR_GEN4_SIZE / 2) + tx * 4, 4);
    /* Standing where FireRed's picture stood, centred across: the picture's lowest row ends on the box row where
     * FireRed's ends. The box's centre (row 32) shows picture row pivotY; a picture row is SCALE box rows. */
    int fire = FireBottom(firePic), gen4 = Gen4Bottom(linear);
    sBattlers[battler].pivotY = fire >= 0 && gen4 >= 0 ? (gen4 + 1) - (fire + 1 - 32) / CTR_GEN4_SCALE
                                                       : CTR_GEN4_SIZE / 2.0f;
    sBattlers[battler].active = true;
    free(entry);
    CtrVideo_SetObjOverride(Match);
    return true;
}

void CtrGen4_Drop(unsigned battler)
{
    if (battler < BATTLERS)
        sBattlers[battler].active = false;
}

void CtrGen4_DropAll(void)
{
    for (unsigned b = 0; b < BATTLERS; ++b)
        sBattlers[b].active = false;
}

void CtrGen4_EraseFrom(unsigned battler, unsigned boxRow)
{
    if (battler >= BATTLERS || !sBattlers[battler].active)
        return;
    float row = sBattlers[battler].pivotY + ((float)boxRow - 32) / CTR_GEN4_SCALE;
    int from = row < 0 ? 0 : (int)(row + 0.5f);
    uint8_t *tiles = sBattlers[battler].tiles;
    for (int y = from; y < CTR_GEN4_SIZE; ++y)
        for (unsigned tx = 0; tx < CTR_GEN4_TILES; ++tx)
            memset(tiles + (((unsigned)y / 8 * CTR_GEN4_TILES + tx) * 8 + (unsigned)y % 8) * 4, 0, 4);
}
