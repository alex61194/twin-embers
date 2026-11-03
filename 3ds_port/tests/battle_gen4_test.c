/* HeartGold battle sprites (src/3ds_battle_gen4.c) on synthetic pack entries: no sprite, no pack. */
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_battle_gen4.h"
#include "3ds_data.h"
#include "3ds_log.h"
#include "3ds_video.h"

/* The pack: one entry, built by each test. */
static uint8_t sEntry[40 + 5 * CTR_GEN4_BACK_PICTURE];
static uint32_t sEntrySize;
static char sAsked[64];

/* icons.bin: 3 icons, 2 palettes. */
static uint8_t sIconsEntry[200 + 4 + 3 * 1024];

/* The game's object events and the compositor's palettes, as the NPC tests need them. */
static unsigned sNpcGfx, sNpcFrame, sNpcTile = 0xFFFF;
static const uint16_t sNpcUnfaded[512];
static uint16_t sNpcPalette[12][16];
static unsigned sNpcPaletteSets;

int CtrNpc_Sprite(unsigned tileNum, unsigned paletteNum, unsigned *graphicsId, unsigned *frame, const uint16_t **unfaded)
{
    if (tileNum != sNpcTile || paletteNum != 3)
        return 0;
    *graphicsId = sNpcGfx;
    *frame = sNpcFrame;
    *unfaded = sNpcUnfaded;
    return 1;
}

void CtrVideo_SetNpcPalette(unsigned slot, const uint16_t *colours)
{
    memcpy(sNpcPalette[slot], colours, 32);
    ++sNpcPaletteSets;
}

void *CtrData_Load(const char *path, uint32_t *outSize)
{
    if (strcmp(path, "graphics/gen4/spinda.bin") == 0)
    {
        /* One spot of two pixels, (10, 10) and (11, 10); the other three empty. */
        static const uint8_t spinda[] = {'G', '4', 'P', '1', 2, 10, 10, 11, 10, 0, 0, 0};
        void *copy = malloc(sizeof(spinda));
        memcpy(copy, spinda, sizeof(spinda));
        *outSize = sizeof(spinda);
        return copy;
    }
    if (strcmp(path, "graphics/gen4/icons.bin") == 0)
    {
        void *copy = malloc(sizeof(sIconsEntry));
        memcpy(copy, sIconsEntry, sizeof(sIconsEntry));
        *outSize = sizeof(sIconsEntry);
        return copy;
    }
    snprintf(sAsked, sizeof(sAsked), "%s", path);
    if (!sEntrySize)
        return NULL;
    void *copy = malloc(sEntrySize + 1);
    memcpy(copy, sEntry, sEntrySize);
    *outSize = sEntrySize;
    return copy;
}

void CtrLog_Write(CtrLogCategory category, const char *format, ...)
{
    (void)category;
    (void)format;
}

static bool (*sMatch)(unsigned attr2, struct CtrGen4Obj *out);

void CtrVideo_SetObjOverride(bool (*match)(unsigned attr2, struct CtrGen4Obj *out))
{
    sMatch = match;
}

/* The game's battler sprites: tile | palette << 12, or -1. */
static int sOam[4] = {-1, -1, -1, -1};

int CtrGen4_SpriteOam(unsigned battler)
{
    return sOam[battler];
}

/* The player's back sprite: tile | palette << 12 or -1, its picture, its frame and FireRed's frame. */
static int sBackOam[4] = {-1, -1, -1, -1};
static unsigned sBackPic, sBackFrame;
static const void *sBackFire;

int CtrGen4_BackSprite(unsigned battler, unsigned *backPic, unsigned *frame, const void **firePic)
{
    *backPic = sBackPic;
    *frame = sBackFrame;
    *firePic = sBackFire;
    return sBackOam[battler];
}

/* An entry whose pictures are filled with `fill` (each picture its own) down to row `bottom`. */
static void MakeEntry(bool female, unsigned bottom)
{
    memset(sEntry, 0, sizeof(sEntry));
    memcpy(sEntry, "G4B1", 4);
    sEntry[4] = female;
    for (unsigned i = 0; i < 16; ++i)
    {
        sEntry[8 + i * 2] = (uint8_t)(0x10 + i);    /* normal */
        sEntry[40 + i * 2] = (uint8_t)(0x40 + i);   /* shiny */
    }
    unsigned pictures = female ? 4 : 2;
    for (unsigned p = 0; p < pictures; ++p)
        for (unsigned y = 0; y <= bottom; ++y)
            memset(sEntry + 72 + p * CTR_GEN4_PICTURE + y * 40, (int)(0x11 * (p + 1)), 40);
    sEntrySize = 72 + pictures * CTR_GEN4_PICTURE;
}

/* A FireRed 64x64 picture whose lowest pixel is on `bottom`. */
static void FirePic(uint8_t pic[0x800], unsigned bottom)
{
    memset(pic, 0, 0x800);
    pic[((bottom / 8) * 8 + 3) * 32 + (bottom % 8) * 4 + 1] = 0x10;
}

static bool Near(float a, float b)
{
    return a - b < 0.001f && b - a < 0.001f;
}

static unsigned FireNibble(const uint8_t *pic, unsigned x, unsigned y)
{
    uint8_t pair = pic[((y / 8) * 8 + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2];
    return (x & 1) ? pair >> 4 : pair & 15;
}

static uint8_t PixelByte(const CtrGen4Obj *obj, unsigned x, unsigned y)
{
    return obj->tiles[((y / 8) * obj->side + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2];
}

#define BG_CHAR 16384

int main(void)
{
    uint8_t fire[0x800];
    uint16_t palette[16];
    CtrGen4Obj obj;

    /* No pack entry: FireRed's picture stays, nothing is matched. */
    sEntrySize = 0;
    FirePic(fire, 60);
    assert(!CtrGen4_Load(0, 25, false, false, false, fire, palette));
    assert(strcmp(sAsked, "graphics/gen4/battle/025.bin") == 0);
    assert(!CtrGen4_Load(0, 0, false, false, false, fire, palette));
    assert(!CtrGen4_Load(0, 387, false, false, false, fire, palette));

    /* A wrong entry is refused. */
    MakeEntry(false, 70);
    sEntry[0] = 'X';
    assert(!CtrGen4_Load(0, 1, false, false, false, fire, palette));
    MakeEntry(false, 70);
    sEntrySize -= 1;
    assert(!CtrGen4_Load(0, 1, false, false, false, fire, palette));

    /* The front picture, normal palette, standing on FireRed's lowest row: its row 71 (just past 70) ends on box
     * row 61, so the box centre (32) shows picture row 71 - 29 * 1.5 = 27.5. */
    MakeEntry(false, 70);
    assert(CtrGen4_Load(1, 6, false, false, false, fire, palette));
    assert(palette[0] == 0x10 && palette[15] == 0x1f);
    sOam[1] = 0x200 | (1 << 12);
    assert(sMatch);
    assert(!sMatch(0x200 | (2 << 12), &obj));       /* another palette */
    assert(!sMatch(0x240 | (1 << 12), &obj));       /* another tile */
    assert(sMatch(0x200 | (1 << 12), &obj));
    assert(obj.slot == 1 && obj.pivotX == 40.0f && Near(obj.pivotY, 27.5f) && Near(obj.scale, 1.0f / 1.5f));
    assert(PixelByte(&obj, 0, 0) == 0x11 && PixelByte(&obj, 79, 70) == 0x11 && PixelByte(&obj, 79, 71) == 0);

    /* The picture on a background, as FireRed's 64x64 one: the box centre is coloured, the corner is not, and its
     * lowest row is FireRed's (60), as the sprite stands. */
    {
        uint8_t bg[0x800];
        memset(bg, 0xEE, sizeof(bg));
        assert(CtrGen4_GbaPicture(1, bg));
        assert(FireNibble(bg, 32, 32) == 1 && FireNibble(bg, 0, 0) == 0);
        assert(FireNibble(bg, 32, 60) == 1 && FireNibble(bg, 32, 61) == 0);
        assert(!CtrGen4_GbaPicture(3, bg));
    }

    /* The back picture and the shiny palette. */
    assert(CtrGen4_Load(1, 6, true, false, true, fire, palette));
    assert(palette[0] == 0x40 && palette[15] == 0x4f);
    assert(sMatch(0x200 | (1 << 12), &obj) && PixelByte(&obj, 5, 5) == 0x22);

    /* The female pair when there is one, the male one when there is not. */
    MakeEntry(true, 70);
    assert(CtrGen4_Load(1, 3, false, true, false, fire, palette));
    assert(sMatch(0x200 | (1 << 12), &obj) && PixelByte(&obj, 5, 5) == 0x33);
    assert(CtrGen4_Load(1, 3, true, true, false, fire, palette));
    assert(sMatch(0x200 | (1 << 12), &obj) && PixelByte(&obj, 5, 5) == 0x44);
    MakeEntry(false, 70);
    assert(CtrGen4_Load(1, 3, false, true, false, fire, palette));
    assert(sMatch(0x200 | (1 << 12), &obj) && PixelByte(&obj, 5, 5) == 0x11);

    /* Fainting: FireRed erases its box from row 48 down, picture row 27.5 + 16 * 1.5 = 51.5, so from row 52. */
    CtrGen4_EraseFrom(1, 48);
    assert(sMatch(0x200 | (1 << 12), &obj));
    assert(PixelByte(&obj, 40, 51) == 0x11 && PixelByte(&obj, 40, 52) == 0 && PixelByte(&obj, 0, 70) == 0);

    /* Without its sprite, or dropped, a battler matches nothing. */
    sOam[1] = -1;
    assert(!sMatch(0x200 | (1 << 12), &obj));
    sOam[1] = 0x200 | (1 << 12);
    CtrGen4_Drop(1);
    assert(!sMatch(0x200 | (1 << 12), &obj));
    assert(CtrGen4_Load(1, 3, false, false, false, fire, palette));
    CtrGen4_DropAll();
    assert(!sMatch(0x200 | (1 << 12), &obj));

    /* A failed load drops what the battler showed before. */
    assert(CtrGen4_Load(1, 3, false, false, false, fire, palette));
    sEntrySize = 0;
    assert(!CtrGen4_Load(1, 4, false, false, false, fire, palette));
    assert(!sMatch(0x200 | (1 << 12), &obj));

    /* A FireRed picture with no pixel: centred instead. */
    MakeEntry(false, 70);
    memset(fire, 0, sizeof(fire));
    assert(CtrGen4_Load(2, 3, false, false, false, fire, palette));
    sOam[2] = 0x300 | (2 << 12);
    assert(sMatch(0x300 | (2 << 12), &obj) && obj.pivotY == 40.0f);

    /* Outside battle: the 80x80 picture shrunk into FireRed's 64x64 (row 70 is its row 56), its palettes, and the
     * whole picture; nothing without the entry or outside the dex. */
    {
        uint8_t pic[0x800], big[CTR_GEN4_PICTURE];
        uint16_t pal[16];
        MakeEntry(true, 70);
        assert(CtrGen4_Picture64(10, false, false, pic));
        assert(FireNibble(pic, 0, 0) == 1 && FireNibble(pic, 63, 56) == 1 && FireNibble(pic, 63, 57) == 0);
        assert(CtrGen4_Picture64(10, true, true, pic) && FireNibble(pic, 5, 5) == 4);
        assert(CtrGen4_Palette(10, false, pal) && pal[3] == 0x13);
        assert(CtrGen4_Palette(10, true, pal) && pal[3] == 0x43);
        assert(CtrGen4_Picture80(10, false, true, big) && big[0] == 0x33 && big[40 * 71] == 0);
        assert(!CtrGen4_Picture64(0, false, false, pic) && !CtrGen4_Picture80(387, false, false, big));
        sEntrySize = 0;
        assert(!CtrGen4_Palette(11, false, pal));
    }

    /* Unown's letters have their own entries; other forms do not exist. */
    MakeEntry(false, 70);
    assert(CtrGen4_Load(0, CTR_GEN4_KEY(201, 2), false, false, false, fire, palette));
    assert(strcmp(sAsked, "graphics/gen4/battle/201_02.bin") == 0);
    assert(CtrGen4_Load(0, CTR_GEN4_KEY(201, 0), false, false, false, fire, palette));
    assert(strcmp(sAsked, "graphics/gen4/battle/201.bin") == 0);
    assert(!CtrGen4_Load(0, CTR_GEN4_KEY(201, 28), false, false, false, fire, palette));
    assert(!CtrGen4_Load(0, CTR_GEN4_KEY(25, 1), false, false, false, fire, palette));
    CtrGen4_Drop(0);

    /* Castform's weather forms have their own entries. */
    MakeEntry(false, 70);
    assert(CtrGen4_Load(0, CTR_GEN4_KEY(351, 3), false, false, false, fire, palette));
    assert(strcmp(sAsked, "graphics/gen4/battle/351_03.bin") == 0);
    assert(!CtrGen4_Load(0, CTR_GEN4_KEY(351, 4), false, false, false, fire, palette));
    CtrGen4_Drop(0);

    /* Spinda's spots: personality 0x88 leaves the spot where the table puts it, colour 1 becomes 6; colour 0 stays,
     * the back picture has none, another species neither. */
    {
        uint8_t big[CTR_GEN4_PICTURE];
        MakeEntry(false, 70);
        CtrGen4_SpindaPersonality(0x88);
        assert(CtrGen4_Picture80(327, false, false, big));
        assert(big[10 * 40 + 5] == 0x66 && big[10 * 40 + 6] == 0x11 && big[11 * 40 + 5] == 0x11);
        CtrGen4_SpindaPersonality(0x99);     /* one right and one down: (11, 11) and (12, 11) */
        assert(CtrGen4_Picture80(327, false, false, big));
        assert(big[11 * 40 + 5] == 0x61 && big[11 * 40 + 6] == 0x16 && big[10 * 40 + 5] == 0x11);
        assert(CtrGen4_Picture80(327, true, false, big) && big[11 * 40 + 5] == 0x22);
        assert(CtrGen4_Picture80(328, false, false, big) && big[11 * 40 + 5] == 0x11);
    }

    /* Trainers: graphics/gen4/trainer/NNN.bin, matched by the picture's own tiles and palette, whatever the battler's
     * Pokemon shows; freeing FireRed's picture drops it. */
    CtrGen4_DropAll();
    sEntrySize = 0;
    assert(!CtrGen4_LoadTrainer(1, 116, fire, 0x300, 5, palette));
    assert(strcmp(sAsked, "graphics/gen4/trainer/116.bin") == 0);
    memset(sEntry, 0, sizeof(sEntry));
    memcpy(sEntry, "G4T1", 4);
    for (unsigned i = 0; i < 16; ++i)
        sEntry[8 + i * 2] = (uint8_t)(0x20 + i);
    for (unsigned y = 0; y <= 70; ++y)
        memset(sEntry + 40 + y * 40, 0x33, 40);
    sEntrySize = 40 + CTR_GEN4_PICTURE;
    sEntry[0] = 'X';
    assert(!CtrGen4_LoadTrainer(1, 116, fire, 0x300, 5, palette));
    sEntry[0] = 'G';
    sEntrySize -= 1;
    assert(!CtrGen4_LoadTrainer(1, 116, fire, 0x300, 5, palette));
    sEntrySize += 1;
    assert(!CtrGen4_LoadTrainer(4, 116, fire, 0x300, 5, palette));
    assert(!CtrGen4_LoadTrainer(1, 116, fire, 0x300, 16, palette));
    FirePic(fire, 60);
    assert(CtrGen4_LoadTrainer(1, 116, fire, 0x300, 5, palette));
    assert(palette[0] == 0x20 && palette[15] == 0x2f);
    sOam[1] = -1;                                   /* the trainer's sprite is no Pokemon's */
    assert(sMatch(0x300 | (5 << 12), &obj));
    assert(obj.slot == CTR_GEN4_TRAINER_SLOT(1) && obj.slot < CTR_GEN4_SLOTS && obj.side == CTR_GEN4_TILES);
    assert(Near(obj.pivotY, 71 - 29 / CTR_GEN4_SCALE) && Near(obj.pivotX, 40));
    assert(PixelByte(&obj, 0, 0) == 0x33 && PixelByte(&obj, 0, 71) == 0);
    assert(!sMatch(0x300 | (4 << 12), &obj) && !sMatch(0x301 | (5 << 12), &obj));
    /* Its Pokemon coming out of the ball takes the battler's slot; the trainer stays matched by its own tiles. */
    MakeEntry(false, 70);
    assert(CtrGen4_Load(1, 6, false, false, false, fire, palette));
    sOam[1] = 0x200 | (1 << 12);
    assert(sMatch(0x200 | (1 << 12), &obj) && obj.slot == 1);
    assert(sMatch(0x300 | (5 << 12), &obj) && obj.slot == CTR_GEN4_TRAINER_SLOT(1));
    CtrGen4_DropTrainer(115);                       /* another picture: kept */
    assert(sMatch(0x300 | (5 << 12), &obj));
    CtrGen4_DropTrainer(116);
    assert(!sMatch(0x300 | (5 << 12), &obj) && sMatch(0x200 | (1 << 12), &obj));
    CtrGen4_DropAll();
    sOam[1] = -1;


    /* The player's back: graphics/gen4/back/NNN.bin, read once; each frame shown on the back sprite's tiles. */
    sEntrySize = 0;
    assert(!CtrGen4_BackPalette(1, palette));      /* Leaf's missing: never asked again */
    assert(strcmp(sAsked, "graphics/gen4/back/001.bin") == 0);
    assert(!CtrGen4_BackPalette(2, palette));
    memset(sEntry, 0, sizeof(sEntry));
    memcpy(sEntry, "G4K2", 4);
    sEntry[4] = 5;
    sEntry[5] = CTR_GEN4_BACK_SIZE;
    for (unsigned i = 0; i < 16; ++i)
        sEntry[8 + i * 2] = (uint8_t)(0x40 + i);
    for (unsigned f = 0; f < 5; ++f)
        for (unsigned y = 0; y <= 95; ++y)
            memset(sEntry + 40 + f * CTR_GEN4_BACK_PICTURE + y * (CTR_GEN4_BACK_SIZE / 2), 0x11 * (f + 1),
                   CTR_GEN4_BACK_SIZE / 2);
    sEntrySize = 40 + 5 * CTR_GEN4_BACK_PICTURE;
    assert(!CtrGen4_BackPalette(1, palette));
    assert(CtrGen4_BackPalette(0, palette) && palette[0] == 0x40 && palette[15] == 0x4f);
    sEntry[0] = 'X';                                /* already read */
    assert(CtrGen4_BackPalette(0, palette));
    sEntry[0] = 'G';
    FirePic(fire, 63);
    sBackPic = 0;
    sBackFrame = 0;
    sBackFire = fire;
    sBackOam[0] = 0x100 | (8 << 12);
    assert(sMatch(0x100 | (8 << 12), &obj));
    assert(obj.slot == CTR_GEN4_BACK_SLOT(0) && obj.slot < CTR_GEN4_SLOTS);
    assert(obj.side == CTR_GEN4_BACK_TILES && Near(obj.scale, CTR_GEN4_SCALE));
    assert(Near(obj.pivotX, 52) && Near(obj.pivotY, 96 - 32 / CTR_GEN4_SCALE));
    assert(PixelByte(&obj, 0, 0) == 0x11 && PixelByte(&obj, 0, 96) == 0);
    sBackFrame = 3;
    assert(sMatch(0x100 | (8 << 12), &obj) && PixelByte(&obj, 103, 95) == 0x44);
    sBackFrame = 5;                                 /* no such frame: FireRed's */
    assert(!sMatch(0x100 | (8 << 12), &obj));
    sBackFrame = 0;
    assert(!sMatch(0x100 | (7 << 12), &obj) && !sMatch(0x101 | (8 << 12), &obj));
    sBackPic = 1;                                   /* Leaf's never read: FireRed's */
    assert(!sMatch(0x100 | (8 << 12), &obj));
    sBackPic = 0;
    sBackOam[0] = -1;                               /* gone */
    assert(!sMatch(0x100 | (8 << 12), &obj));

    /* Icons: read once, each with its palette; out of range or a bad index, none. */
    memcpy(sIconsEntry, "G4I1", 4);
    sIconsEntry[4] = 3;
    sIconsEntry[6] = 2;
    sIconsEntry[8] = 0x34;                 /* palette 0, colour 0 */
    sIconsEntry[8 + 32 + 2] = 0x56;        /* palette 1, colour 1 */
    sIconsEntry[200] = 0;
    sIconsEntry[201] = 1;
    sIconsEntry[202] = 1;
    for (unsigned i = 0; i < 3; ++i)
        memset(sIconsEntry + 204 + i * 1024, (int)(0x10 * (i + 1)), 1024);
    assert(CtrGen4_IconTiles(0) && CtrGen4_IconTiles(0)[0] == 0x10 && CtrGen4_IconTiles(2)[1023] == 0x30);
    assert(!CtrGen4_IconTiles(3) && !CtrGen4_IconTiles(-1));
    assert(CtrGen4_IconPalette(1) == 1 && CtrGen4_IconPalette(3) == -1);
    assert(CtrGen4_IconPaletteData(0)[0] == 0x34 && CtrGen4_IconPaletteData(1)[1] == 0x56);
    assert(!CtrGen4_IconPaletteData(2));
    sIconsEntry[0] = 'X';                  /* already read: still there */
    assert(CtrGen4_IconTiles(1));

    /* The battle backgrounds: a terrain's entry (3 tiles, palette, map) goes to VRAM as it is, the rest of the char
     * block cleared, and one palette fills the three banks BG3 reads. */
    {
        static uint8_t tiles[BG_CHAR], map[4096], pal[48 * 2];
        uint16_t banks[48];

        memset(sEntry, 0, sizeof(sEntry));
        memcpy(sEntry, "G4G1", 4);
        sEntry[4] = 3;
        for (unsigned i = 0; i < 16; ++i)
        {
            sEntry[8 + i * 2] = (uint8_t)(i + 1);
            sEntry[9 + i * 2] = 0x40;
        }
        for (unsigned i = 0; i < 3 * 32; ++i)
            sEntry[40 + i] = (uint8_t)(0xA0 + i / 32);
        for (unsigned i = 0; i < 4096; ++i)
            sEntry[40 + 96 + i] = (uint8_t)(i * 7);
        sEntrySize = 40 + 96 + 4096;
        memset(tiles, 0xEE, sizeof(tiles));
        assert(CtrBattleBg_Load(3, 3, NULL));
        assert(strcmp(sAsked, "graphics/gen4/bg/003.bin") == 0);
        assert(CtrBattleBg_Load(3, 0, tiles));
        assert(tiles[0] == 0xA0 && tiles[31] == 0xA0 && tiles[32] == 0xA1 && tiles[95] == 0xA2);
        assert(tiles[96] == 0 && tiles[BG_CHAR - 1] == 0);
        assert(CtrBattleBg_Load(3, 1, map));
        assert(map[0] == 0 && map[1] == 7 && map[4095] == (uint8_t)(4095 * 7));
        assert(CtrBattleBg_Load(3, 2, pal));
        memcpy(banks, pal, sizeof(banks));
        for (unsigned i = 0; i < 16; ++i)
            assert(banks[i] == (0x4000 | (i + 1)) && banks[16 + i] == banks[i] && banks[32 + i] == banks[i]);
        assert(!CtrBattleBg_Load(3, 4, tiles));
        assert(!CtrBattleBg_Load(20, 0, tiles) && !CtrBattleBg_Load(1000, 3, NULL));

        /* Another terrain, a wrong kind and a wrong size: FireRed's own data stays. */
        sEntry[0] = 'X';
        assert(!CtrBattleBg_Load(4, 0, tiles));
        sEntry[0] = 'G';
        sEntry[4] = 4;
        assert(!CtrBattleBg_Load(5, 0, tiles));
        sEntry[4] = 3;
        sEntrySize = 0;
        assert(!CtrBattleBg_Load(6, 3, NULL));
    }

    /* The field's people: a person's entry (nine 32x32 frames and a palette) is read once, the frame its sprite shows
     * is the picture, drawn on its own slot and palette; no entry, a wrong one or no sprite leaves FireRed's. */
    {
        CtrGen4Obj person;

        memset(sEntry, 0, sizeof(sEntry));
        memcpy(sEntry, "G4N1", 4);
        sEntry[4] = 9;
        sEntry[5] = 32;
        for (unsigned i = 0; i < 16; ++i)
        {
            sEntry[8 + i * 2] = (uint8_t)(0x10 + i);
            sEntry[9 + i * 2] = 0x20;
        }
        for (unsigned f = 0; f < 9; ++f)
            memset(sEntry + 40 + f * 512, (int)(0x30 + f), 512);
        sEntrySize = 40 + 9 * 512;
        sNpcTile = 100;
        sNpcGfx = 21;
        sNpcFrame = 4;
        assert(CtrNpc_Match(100 | 3 << 12, 16, 32, &person));
        assert(strcmp(sAsked, "graphics/gen4/npc/021.bin") == 0);
        assert(person.side == 4 && person.tiles[0] == 0x34 && person.tiles[511] == 0x34);
        assert(person.pivotX == 16.0f && person.pivotY == 16.0f && person.scale == 1.0f);
        assert(person.unfaded == sNpcUnfaded);
        assert(sNpcPaletteSets == 1 && sNpcPalette[person.slot][1] == 0x2011);
        /* Read once: the same person again, another frame, the same slot, the palette not set again. */
        sNpcFrame = 0;
        {
            CtrGen4Obj again;

            assert(CtrNpc_Match(100 | 3 << 12, 16, 16, &again));
            assert(again.slot == person.slot && again.tiles[0] == 0x30 && again.pivotY == 24.0f);
            assert(sNpcPaletteSets == 1);
        }
        /* A person's tenth image is drawn as the first. */
        sNpcFrame = 9;
        assert(CtrNpc_Match(100 | 3 << 12, 16, 32, &person) && person.tiles[0] == 0x30);
        /* Another person takes another slot. */
        sNpcGfx = 22;
        {
            CtrGen4Obj other;

            assert(CtrNpc_Match(100 | 3 << 12, 16, 32, &other) && other.slot != person.slot && sNpcPaletteSets == 2);
        }
        /* Not a person's tiles or palette: nothing. */
        assert(!CtrNpc_Match(101 | 3 << 12, 16, 32, &person));
        assert(!CtrNpc_Match(100 | 4 << 12, 16, 32, &person));
        /* Entry missing, wrong or too large an id: FireRed's own, and the read is not repeated. */
        sEntrySize = 0;
        sNpcGfx = 30;
        assert(!CtrNpc_Match(100 | 3 << 12, 16, 32, &person));
        sEntrySize = 40 + 9 * 512;
        assert(!CtrNpc_Match(100 | 3 << 12, 16, 32, &person));
        sNpcGfx = 31;
        sEntry[4] = 8;
        assert(!CtrNpc_Match(100 | 3 << 12, 16, 32, &person));
        sNpcGfx = 300;
        assert(!CtrNpc_Match(100 | 3 << 12, 16, 32, &person));
        /* More people than slots: the least recently shown goes, and comes back when it is shown again. */
        sEntry[4] = 9;
        for (unsigned g = 40; g < 40 + 12; ++g)
        {
            sNpcGfx = g;
            assert(CtrNpc_Match(100 | 3 << 12, 16, 32, &person));
        }
        sNpcGfx = 21;
        assert(CtrNpc_Match(100 | 3 << 12, 16, 32, &person));
        /* The player's sheets have as many frames as FireRed's pic table: walking and running, 20; past the last, the
         * first. No frames at all is not an entry. */
        sEntry[4] = 20;
        for (unsigned f = 0; f < 20; ++f)
            memset(sEntry + 40 + f * 512, (int)(0x30 + f), 512);
        sEntrySize = 40 + 20 * 512;
        sNpcGfx = 0;
        sNpcFrame = 15;
        assert(CtrNpc_Match(100 | 3 << 12, 16, 32, &person) && person.tiles[0] == 0x3F);
        assert(strcmp(sAsked, "graphics/gen4/npc/000.bin") == 0);
        sNpcFrame = 20;
        assert(CtrNpc_Match(100 | 3 << 12, 16, 32, &person) && person.tiles[0] == 0x30);
        sEntry[4] = 0;
        sEntrySize = 40;
        sNpcGfx = 7;
        assert(!CtrNpc_Match(100 | 3 << 12, 16, 32, &person));
    }

    puts("battle_gen4_test: ok");
    return 0;
}
