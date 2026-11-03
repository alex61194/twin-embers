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

/* A trainer's picture (CtrGen4_LoadTrainer): matched by the tiles and palette of FireRed's picture. */
typedef struct {
    bool active;
    unsigned pic, tileNum, paletteNum;
    Battler shown;
} Trainer;

static Trainer sTrainers[BATTLERS];
#define TRAINER_HEADER 40

/* The player's backs (CtrGen4_BackPalette): each read once, its frames as tiles. */
#define BACKS 2
#define BACK_FRAMES_MAX 8
typedef struct {
    bool tried, ok, placed;
    unsigned frames;
    uint16_t palette[16];
    float pivotY;
    int bottom;                     /* the first frame's lowest row, -1 if empty */
    uint8_t (*tiles)[CTR_GEN4_BACK_PICTURE];
} Back;

static Back sBacks[BACKS];
static int FireBottom(const uint8_t *pic);

/* A key's pack entry: graphics/gen4/battle/NNN.bin, or NNN_FF.bin for Unown's letters B to ? (form 1-27). */
static bool KeyPath(unsigned key, char *path, size_t size)
{
    unsigned dex = key & 511, form = key >> 9;

    if (dex < 1 || dex > 386 || (form && !(dex == 201 && form <= 27) && !(dex == 351 && form <= 3)))
        return false;
    if (form)
        snprintf(path, size, "graphics/gen4/battle/%03u_%02u.bin", dex, form);
    else
        snprintf(path, size, "graphics/gen4/battle/%03u.bin", dex);
    return true;
}

/* Spinda's spots (spinda.bin, gen4_sprites.py): read once and kept. */
#define SPINDA_DEX 327
static uint8_t *sSpinda;
static uint32_t sSpindaSize, sSpindaPersonality;
static bool sSpindaTried;

void CtrGen4_SpindaPersonality(uint32_t personality)
{
    sSpindaPersonality = personality;
}

/* A picture as shown: Spinda's front one with this personality's spots (HeartGold's RawChardata_PlaceSpindaSpots
 * on its first frame: each spot moved by one byte of the personality, a spot pixel of colour 1-3 made 6-8), any
 * other the entry's own. */
static const uint8_t *Shown(const uint8_t *linear, unsigned key, bool back)
{
    static uint8_t spotted[CTR_GEN4_PICTURE];

    if (back || key != SPINDA_DEX)
        return linear;
    if (!sSpindaTried)
    {
        sSpindaTried = true;
        sSpinda = CtrData_Load("graphics/gen4/spinda.bin", &sSpindaSize);
        if (sSpinda && (sSpindaSize < 4 || memcmp(sSpinda, "G4P1", 4)))
        {
            CtrLog_Write(CTR_LOG_GAME, "GEN4 spinda.bin: wrong kind");
            free(sSpinda);
            sSpinda = NULL;
        }
    }
    if (!sSpinda)
        return linear;
    memcpy(spotted, linear, sizeof(spotted));
    uint32_t pid = sSpindaPersonality, at = 4;
    for (int spot = 0; spot < 4; ++spot, pid >>= 8)
    {
        if (at >= sSpindaSize)
            break;
        unsigned count = sSpinda[at++];
        for (unsigned j = 0; j < count && at + 1 < sSpindaSize; ++j, at += 2)
        {
            int x = sSpinda[at] + (int)(pid & 0x0F) - 8, y = sSpinda[at + 1] + (int)((pid & 0xF0) >> 4) - 8;
            if (x < 0 || x >= CTR_GEN4_SIZE || y < 0 || y >= CTR_GEN4_SIZE)
                continue;
            uint8_t *byte = &spotted[y * (CTR_GEN4_SIZE / 2) + x / 2];
            unsigned shift = (x & 1) * 4, index = (*byte >> shift) & 15;
            if (index >= 1 && index <= 3)
                *byte = (uint8_t)(*byte + (5 << shift));
        }
    }
    return spotted;
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
        out->side = CTR_GEN4_TILES;
        out->pivotX = CTR_GEN4_SIZE / 2.0f;
        out->pivotY = sBattlers[b].pivotY;
        out->scale = CTR_GEN4_SCALE;
        return true;
    }
    for (unsigned b = 0; b < BATTLERS; ++b)
    {
        unsigned pic = 0, frame = 0;
        const void *fire = NULL;
        int oam = CtrGen4_BackSprite(b, &pic, &frame, &fire);
        if (oam < 0 || pic >= BACKS || (attr2 & 1023) != ((unsigned)oam & 1023) || (attr2 >> 12) != ((unsigned)oam >> 12))
            continue;
        Back *back = &sBacks[pic];
        if (!back->ok || frame >= back->frames)
            continue;
        if (!back->placed && fire)
        {
            /* Where the first frame stands: its lowest row on FireRed's, as for the Pokemon. */
            int bottom = FireBottom(fire);
            back->pivotY = bottom >= 0 && back->bottom >= 0 ? (back->bottom + 1) - (bottom + 1 - 32) / CTR_GEN4_SCALE
                                                           : CTR_GEN4_BACK_SIZE / 2.0f;
            back->placed = true;
        }
        out->slot = CTR_GEN4_BACK_SLOT(b);
        out->tiles = back->tiles[frame];
        out->side = CTR_GEN4_BACK_TILES;
        out->pivotX = CTR_GEN4_BACK_SIZE / 2.0f;
        out->pivotY = back->placed ? back->pivotY : CTR_GEN4_BACK_SIZE / 2.0f;
        out->scale = CTR_GEN4_SCALE;
        return true;
    }
    for (unsigned b = 0; b < BATTLERS; ++b)
    {
        const Trainer *trainer = &sTrainers[b];
        if (!trainer->active || (attr2 & 1023) != trainer->tileNum || (attr2 >> 12) != trainer->paletteNum)
            continue;
        out->slot = CTR_GEN4_TRAINER_SLOT(b);
        out->tiles = trainer->shown.tiles;
        out->side = CTR_GEN4_TILES;
        out->pivotX = CTR_GEN4_SIZE / 2.0f;
        out->pivotY = trainer->shown.pivotY;
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
    memcpy(out, Shown(Picture(entry, back, female), nationalDex, back), CTR_GEN4_PICTURE);
    return true;
}

bool CtrGen4_Picture64(unsigned nationalDex, bool back, bool female, uint8_t out[0x800])
{
    const uint8_t *entry = Entry(nationalDex);
    if (!entry)
        return false;
    const uint8_t *linear = Shown(Picture(entry, back, female), nationalDex, back);
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

/* An 80x80 picture in rows of 40 bytes into `shown`: the GBA's 8x8 tiles, ten to a row, and the picture row on the
 * 64x64 box's centre so that it stands where FireRed's picture `firePic` stood. */
/* Rows of side * 4 bytes to the GBA's 8x8 tiles, `side` to a row. */
static void Tiles(uint8_t *tiles, const uint8_t *linear, unsigned side)
{
    for (unsigned ty = 0; ty < side; ++ty)
        for (unsigned tx = 0; tx < side; ++tx)
            for (unsigned r = 0; r < 8; ++r)
                memcpy(tiles + ((ty * side + tx) * 8 + r) * 4, linear + (ty * 8 + r) * side * 4 + tx * 4, 4);
}

static void Place(Battler *shown, const uint8_t *linear, const void *firePic)
{
    Tiles(shown->tiles, linear, CTR_GEN4_TILES);
    /* Standing where FireRed's picture stood, centred across: the picture's lowest row ends on the box row where
     * FireRed's ends. The box's centre (row 32) shows picture row pivotY; a picture row is SCALE box rows. */
    int fire = FireBottom(firePic), gen4 = Gen4Bottom(linear);
    shown->pivotY = fire >= 0 && gen4 >= 0 ? (gen4 + 1) - (fire + 1 - 32) / CTR_GEN4_SCALE : CTR_GEN4_SIZE / 2.0f;
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
    const uint8_t *linear = Shown(entry + HEADER + ((female && pair) ? 2 : 0) * CTR_GEN4_PICTURE
                                  + (back ? CTR_GEN4_PICTURE : 0), nationalDex, back);
    memcpy(palette, entry + (shiny ? 40 : 8), 32);
    Place(&sBattlers[battler], linear, firePic);
    sBattlers[battler].active = true;
    free(entry);
    CtrVideo_SetObjOverride(Match);
    return true;
}

bool CtrGen4_LoadTrainer(unsigned battler, unsigned trainerPic, const void *firePic, unsigned tileNum,
                         unsigned paletteNum, uint16_t palette[16])
{
    if (battler >= BATTLERS)
        return false;
    Trainer *trainer = &sTrainers[battler];
    trainer->active = false;
    if (trainerPic > 999 || tileNum > 1023 || paletteNum > 15)
        return false;
    char path[40];
    snprintf(path, sizeof(path), "graphics/gen4/trainer/%03u.bin", trainerPic);
    uint32_t size = 0;
    uint8_t *entry = CtrData_Load(path, &size);
    if (!entry)
        return false;
    if (size != TRAINER_HEADER + CTR_GEN4_PICTURE || memcmp(entry, "G4T1", 4))
    {
        CtrLog_Write(CTR_LOG_GAME, "GEN4 %s: wrong size or kind", path);
        free(entry);
        return false;
    }
    memcpy(palette, entry + 8, 32);
    Place(&trainer->shown, entry + TRAINER_HEADER, firePic);
    trainer->pic = trainerPic;
    trainer->tileNum = tileNum;
    trainer->paletteNum = paletteNum;
    trainer->active = true;
    free(entry);
    CtrVideo_SetObjOverride(Match);
    return true;
}

bool CtrGen4_BackPalette(unsigned backPic, uint16_t palette[16])
{
    if (backPic >= BACKS)
        return false;
    Back *back = &sBacks[backPic];
    if (!back->tried)
    {
        back->tried = true;
        char path[40];
        snprintf(path, sizeof(path), "graphics/gen4/back/%03u.bin", backPic);
        uint32_t size = 0;
        uint8_t *entry = CtrData_Load(path, &size);
        if (!entry)
            return false;
        unsigned frames = size >= 8 ? entry[4] : 0;
        if (memcmp(entry, "G4K2", 4) || !frames || frames > BACK_FRAMES_MAX || entry[5] != CTR_GEN4_BACK_SIZE
            || size != TRAINER_HEADER + frames * CTR_GEN4_BACK_PICTURE)
        {
            CtrLog_Write(CTR_LOG_GAME, "GEN4 %s: wrong size or kind", path);
            free(entry);
            return false;
        }
        back->tiles = malloc(frames * sizeof(*back->tiles));
        if (!back->tiles)
        {
            free(entry);
            return false;
        }
        memcpy(back->palette, entry + 8, 32);
        for (unsigned f = 0; f < frames; ++f)
            Tiles(back->tiles[f], entry + TRAINER_HEADER + f * CTR_GEN4_BACK_PICTURE, CTR_GEN4_BACK_TILES);
        back->bottom = -1;
        for (unsigned i = 0; i < CTR_GEN4_BACK_PICTURE; ++i)
            if (entry[TRAINER_HEADER + i])
                back->bottom = (int)(i / (CTR_GEN4_BACK_SIZE / 2));
        back->frames = frames;
        back->ok = true;
        free(entry);
    }
    if (!back->ok)
        return false;
    memcpy(palette, back->palette, 32);
    CtrVideo_SetObjOverride(Match);
    return true;
}

void CtrGen4_DropTrainer(unsigned trainerPic)
{
    for (unsigned b = 0; b < BATTLERS; ++b)
        if (sTrainers[b].pic == trainerPic)
            sTrainers[b].active = false;
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
    {
        sBattlers[b].active = false;
        sTrainers[b].active = false;
    }
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

/* The battle backgrounds (CtrBattleBg_Load): the terrain's entry read once and kept while the same terrain is asked
 * for again (a battle loads its tiles, map and palette one after the other). */
#define BG_TERRAINS 20
#define BG_HEADER 40
#define BG_CHAR_BYTES 16384
#define BG_MAP_BYTES 4096
#define BG_TILES_MAX 512

static struct {
    bool tried;
    unsigned terrain, tiles;
    uint8_t *entry;
} sBg;

static const uint8_t *BgEntry(unsigned terrain)
{
    if (terrain >= BG_TERRAINS)
        return NULL;
    if (sBg.tried && sBg.terrain == terrain)
        return sBg.entry;
    free(sBg.entry);
    sBg.entry = NULL;
    sBg.tried = true;
    sBg.terrain = terrain;
    char path[40];
    snprintf(path, sizeof(path), "graphics/gen4/bg/%03u.bin", terrain);
    uint32_t size = 0;
    uint8_t *entry = CtrData_Load(path, &size);
    if (!entry)
        return NULL;
    unsigned tiles = size >= BG_HEADER ? entry[4] | entry[5] << 8 : 0;
    if (size < BG_HEADER || memcmp(entry, "G4G1", 4) || !tiles || tiles > BG_TILES_MAX
        || size != BG_HEADER + tiles * 32 + BG_MAP_BYTES)
    {
        CtrLog_Write(CTR_LOG_GAME, "GEN4 %s: wrong size or kind", path);
        free(entry);
        return NULL;
    }
    sBg.tiles = tiles;
    sBg.entry = entry;
    return entry;
}

int CtrBattleBg_Load(unsigned terrain, unsigned part, void *dest)
{
    const uint8_t *entry = BgEntry(terrain);
    if (!entry)
        return 0;
    switch (part)
    {
    case 0:
        memset(dest, 0, BG_CHAR_BYTES);
        memcpy(dest, entry + BG_HEADER, sBg.tiles * 32);
        return 1;
    case 1:
        memcpy(dest, entry + BG_HEADER + sBg.tiles * 32, BG_MAP_BYTES);
        return 1;
    case 2:
        for (unsigned bank = 0; bank < 3; ++bank)
            memcpy((uint8_t *)dest + bank * 32, entry + 8, 32);
        return 1;
    case 3:
        return 1;
    }
    return 0;
}

/* The field's people (CtrNpc_Match): each person's entry read once and kept while it is on screen, in one of as many
 * slots as the compositor has pictures; the least recently shown goes when a new one comes. A person has nine frames,
 * the player as many as FireRed's pic table (up to 20). */
#define NPC_FRAMES_MAX 32
#define NPC_HEADER 40
#define NPC_FRAME_BYTES 512

typedef struct {
    bool used;
    unsigned gfx;
    uint32_t stamp;
    uint8_t *entry;
} Npc;

static Npc sNpcs[CTR_GEN4_SLOTS];
static uint32_t sNpcTick;
static uint32_t sNpcFailed[8];      /* graphics ids whose entry is not there or not right: asked once */

static Npc *NpcLoad(unsigned gfx)
{
    unsigned i, slot = 0;
    char path[40];
    uint32_t size = 0;
    uint8_t *entry;

    for (i = 0; i < CTR_GEN4_SLOTS; ++i)
        if (sNpcs[i].used && sNpcs[i].gfx == gfx)
            return &sNpcs[i];
    if (gfx > 255 || (sNpcFailed[gfx / 32] >> (gfx % 32) & 1))
        return NULL;
    sNpcFailed[gfx / 32] |= 1u << (gfx % 32);
    snprintf(path, sizeof(path), "graphics/gen4/npc/%03u.bin", gfx);
    entry = CtrData_Load(path, &size);
    if (!entry)
        return NULL;
    if (size < NPC_HEADER || memcmp(entry, "G4N1", 4) || !entry[4] || entry[4] > NPC_FRAMES_MAX || entry[5] != 32
        || size != NPC_HEADER + entry[4] * (uint32_t)NPC_FRAME_BYTES)
    {
        CtrLog_Write(CTR_LOG_GAME, "GEN4 %s: wrong size or kind", path);
        free(entry);
        return NULL;
    }
    for (i = 0; i < CTR_GEN4_SLOTS; ++i)
        if (!sNpcs[i].used)
        {
            slot = i;
            break;
        }
        else if (sNpcs[i].stamp < sNpcs[slot].stamp)
            slot = i;
    free(sNpcs[slot].entry);
    sNpcs[slot] = (Npc){true, gfx, ++sNpcTick, entry};
    sNpcFailed[gfx / 32] &= ~(1u << (gfx % 32));
    {
        uint16_t palette[16];

        for (i = 0; i < 16; ++i)
            palette[i] = (uint16_t)(entry[8 + i * 2] | entry[9 + i * 2] << 8);
        CtrVideo_SetNpcPalette(slot, palette);
    }
    return &sNpcs[slot];
}

bool CtrNpc_Match(unsigned attr2, unsigned width, unsigned height, CtrGen4Obj *out)
{
    unsigned gfx = 0, frame = 0;
    const uint16_t *unfaded = NULL;
    Npc *npc;

    if (!CtrNpc_Sprite(attr2 & 1023, attr2 >> 12, &gfx, &frame, &unfaded))
        return false;
    npc = NpcLoad(gfx);
    if (!npc)
        return false;
    npc->stamp = ++sNpcTick;
    if (frame >= npc->entry[4])
        frame = 0;                  /* a person's tenth image (a bow) is not drawn */
    (void)width;
    out->slot = (unsigned)(npc - sNpcs);
    out->tiles = npc->entry + NPC_HEADER + frame * NPC_FRAME_BYTES;
    out->side = 4;
    out->pivotX = 16.0f;
    out->pivotY = 32.0f - (float)height / 2.0f;
    out->scale = 1.0f;
    out->unfaded = unfaded;
    return true;
}

/* The player's whole pictures (CtrPlayer_Swap): FireRed's data for each, as the game gives it (CtrPlayer_Asset), with
 * the pack's entry that takes its place; read once, the first time the BIOS asks with the pack open. */
#define SWAPS (CTR_PLAYER_PICTURES * 2 * 2)

static struct {
    const void *src;
    uint8_t *data;
    uint32_t size;
} sSwaps[SWAPS];
static unsigned sSwapCount;
static bool sSwapsRead;

static void SwapsRead(void)
{
    static const char *const kNames[CTR_PLAYER_PICTURES] = {"front", "intro", "icon"};
    static const char *const kGender[2] = {"boy", "girl"};
    static const char *const kKind[CTR_PLAYER_PICTURES][2] = {{"4bpp", "gbapal"}, {"8bpp", "gbapal"},
                                                               {"4bpp", "gbapal"}};

    sSwapsRead = true;
    for (unsigned which = 0; which < CTR_PLAYER_PICTURES; ++which)
        for (unsigned female = 0; female < 2; ++female)
            for (unsigned palette = 0; palette < 2; ++palette)
            {
                const void *src = CtrPlayer_Asset(which, female, palette);
                char path[48];
                uint32_t size = 0;
                uint8_t *data;

                if (!src)
                    continue;
                snprintf(path, sizeof(path), "graphics/gen4/player/%s_%s.%s", kNames[which], kGender[female],
                         kKind[which][palette]);
                data = CtrData_Load(path, &size);
                if (!data)
                    continue;
                sSwaps[sSwapCount].src = src;
                sSwaps[sSwapCount].data = data;
                sSwaps[sSwapCount].size = size;
                ++sSwapCount;
            }
}

bool CtrPlayer_Swap(const void *src, void *dest, uint32_t size)
{
    if (!sSwapsRead)
    {
        if (CtrData_GetBackend() == CTR_DATA_NONE)
            return false;           /* before the pack is open (the BIOS copies from the start) */
        SwapsRead();
    }
    for (unsigned i = 0; i < sSwapCount; ++i)
        if (sSwaps[i].src == src)
        {
            if (sSwaps[i].size != size)
            {
                CtrLog_Write(CTR_LOG_GAME, "GEN4 player picture %u: %lu bytes, FireRed's has %lu", i,
                             (unsigned long)sSwaps[i].size, (unsigned long)size);
                return false;
            }
            memcpy(dest, sSwaps[i].data, size);
            return true;
        }
    return false;
}
