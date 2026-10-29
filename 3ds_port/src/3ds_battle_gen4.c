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

/* A key's pack entry: graphics/gen4/battle/NNN.bin, or NNN_FF.bin for Unown's letters B to ? (form 1-27). */
static bool KeyPath(unsigned key, char *path, size_t size)
{
    unsigned dex = key & 511, form = key >> 9;

    if (dex < 1 || dex > 386 || form > 27 || (form && dex != 201))
        return false;
    if (form)
        snprintf(path, size, "graphics/gen4/battle/%03u_%02u.bin", dex, form);
    else
        snprintf(path, size, "graphics/gen4/battle/%03u.bin", dex);
    return true;
}

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

/* The species' pack entry, read once and kept while the same species is asked for again (a screen asks for its
 * picture and its palette one after the other). */
static uint8_t *sEntryData;
static uint32_t sEntrySize;
static unsigned sEntryDex;

static const uint8_t *Entry(unsigned nationalDex)
{
    char path[40];
    if (!KeyPath(nationalDex, path, sizeof(path)))
        return NULL;
    if (sEntryData && sEntryDex == nationalDex)
        return sEntryData;
    free(sEntryData);
    sEntryData = NULL;
    sEntryDex = 0;
    uint8_t *entry = CtrData_Load(path, &sEntrySize);
    if (!entry)
        return NULL;
    bool pair = sEntrySize >= 8 && (entry[4] & 1);
    if (sEntrySize < 4 || memcmp(entry, "G4B1", 4) || sEntrySize != HEADER + (pair ? 4u : 2u) * CTR_GEN4_PICTURE)
    {
        CtrLog_Write(CTR_LOG_GAME, "GEN4 %s: wrong size or kind", path);
        free(entry);
        return NULL;
    }
    sEntryData = entry;
    sEntryDex = nationalDex;
    return entry;
}

static const uint8_t *Picture(const uint8_t *entry, bool back, bool female)
{
    bool pair = entry[4] & 1;
    return entry + HEADER + ((female && pair) ? 2 : 0) * CTR_GEN4_PICTURE + (back ? CTR_GEN4_PICTURE : 0);
}

bool CtrGen4_Picture80(unsigned nationalDex, bool back, bool female, uint8_t out[CTR_GEN4_PICTURE])
{
    const uint8_t *entry = Entry(nationalDex);
    if (!entry)
        return false;
    memcpy(out, Picture(entry, back, female), CTR_GEN4_PICTURE);
    return true;
}

bool CtrGen4_Picture64(unsigned nationalDex, bool back, bool female, uint8_t out[0x800])
{
    const uint8_t *entry = Entry(nationalDex);
    if (!entry)
        return false;
    const uint8_t *linear = Picture(entry, back, female);
    memset(out, 0, 0x800);
    /* Nearest pixel: 64 of the 80 rows and columns, every fifth one left out. */
    for (unsigned y = 0; y < 64; ++y)
        for (unsigned x = 0; x < 64; ++x)
        {
            unsigned sx = x * CTR_GEN4_SIZE / 64, sy = y * CTR_GEN4_SIZE / 64;
            uint8_t pair = linear[sy * (CTR_GEN4_SIZE / 2) + sx / 2];
            unsigned index = (sx & 1) ? pair >> 4 : pair & 15;
            out[((y / 8) * 8 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2] |= (uint8_t)(index << ((x & 1) * 4));
        }
    return true;
}

bool CtrGen4_Palette(unsigned nationalDex, bool shiny, uint16_t palette[16])
{
    const uint8_t *entry = Entry(nationalDex);
    if (!entry)
        return false;
    memcpy(palette, entry + (shiny ? 40 : 8), 32);
    return true;
}

bool CtrGen4_Load(unsigned battler, unsigned nationalDex, bool back, bool female, bool shiny,
                  const void *firePic, uint16_t palette[16])
{
    if (battler >= BATTLERS)
        return false;
    sBattlers[battler].active = false;
    char path[40];
    if (!KeyPath(nationalDex, path, sizeof(path)))
        return false;
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

bool CtrGen4_GbaPicture(unsigned battler, uint8_t out[0x800])
{
    if (battler >= BATTLERS || !sBattlers[battler].active)
        return false;
    const uint8_t *tiles = sBattlers[battler].tiles;
    memset(out, 0, 0x800);
    /* Each box pixel takes the picture pixel under its centre, as the sprite draws it (Match: the box centre on
     * the picture's pivot, a picture pixel SCALE box pixels). */
    for (int by = 0; by < 64; ++by)
    {
        float py = sBattlers[battler].pivotY + (by + 0.5f - 32) / CTR_GEN4_SCALE;
        if (py < 0 || py >= CTR_GEN4_SIZE)
            continue;
        for (int bx = 0; bx < 64; ++bx)
        {
            float px = CTR_GEN4_SIZE / 2.0f + (bx + 0.5f - 32) / CTR_GEN4_SCALE;
            if (px < 0 || px >= CTR_GEN4_SIZE)
                continue;
            unsigned x = (unsigned)px, y = (unsigned)py;
            uint8_t pair = tiles[((y / 8) * CTR_GEN4_TILES + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2];
            unsigned index = (x & 1) ? pair >> 4 : pair & 15;
            out[((by / 8) * 8 + bx / 8) * 32 + (by % 8) * 4 + (bx % 8) / 2] |= (uint8_t)(index << ((bx & 1) * 4));
        }
    }
    return true;
}

/* icons.bin (builder/firered3ds_builder/gen4_sprites.py): header, six palettes, an index an icon, the tiles. */
#define ICONS_HEADER 200
#define ICON_BYTES 1024
#define ICON_PALETTES 6
static uint8_t *sIcons;
static unsigned sIconCount, sIconPalettes, sIconTiles;
static bool sIconsTried;

static bool LoadIcons(void)
{
    if (sIconsTried)
        return sIcons != NULL;
    sIconsTried = true;
    uint32_t size = 0;
    uint8_t *entry = CtrData_Load("graphics/gen4/icons.bin", &size);
    if (!entry)
        return false;
    unsigned count = size >= 8 ? (unsigned)(entry[4] | entry[5] << 8) : 0;
    unsigned palettes = size >= 8 ? (unsigned)(entry[6] | entry[7] << 8) : 0;
    unsigned tiles = ICONS_HEADER + ((count + 3) & ~3u);
    if (size < 8 || memcmp(entry, "G4I1", 4) || !palettes || palettes > ICON_PALETTES
        || size != tiles + count * ICON_BYTES)
    {
        CtrLog_Write(CTR_LOG_GAME, "GEN4 icons.bin: wrong size or kind");
        free(entry);
        return false;
    }
    for (unsigned i = 0; i < count; ++i)
        if (entry[ICONS_HEADER + i] >= palettes)
        {
            CtrLog_Write(CTR_LOG_GAME, "GEN4 icons.bin: icon %u has no palette", i);
            free(entry);
            return false;
        }
    sIcons = entry;
    sIconCount = count;
    sIconPalettes = palettes;
    sIconTiles = tiles;
    return true;
}

const uint8_t *CtrGen4_IconTiles(int key)
{
    if (key < 0 || !LoadIcons() || (unsigned)key >= sIconCount)
        return NULL;
    return sIcons + sIconTiles + (unsigned)key * ICON_BYTES;
}

int CtrGen4_IconPalette(int key)
{
    if (key < 0 || !LoadIcons() || (unsigned)key >= sIconCount)
        return -1;
    return sIcons[ICONS_HEADER + key];
}

const uint16_t *CtrGen4_IconPaletteData(unsigned index)
{
    if (!LoadIcons() || index >= sIconPalettes)
        return NULL;
    return (const uint16_t *)(sIcons + 8 + index * 32);
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
