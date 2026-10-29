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
static uint8_t sEntry[72 + 4 * CTR_GEN4_PICTURE];
static uint32_t sEntrySize;
static char sAsked[64];

/* icons.bin: 3 icons, 2 palettes. */
static uint8_t sIconsEntry[200 + 4 + 3 * 1024];

void *CtrData_Load(const char *path, uint32_t *outSize)
{
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
    return obj->tiles[((y / 8) * CTR_GEN4_TILES + x / 8) * 32 + (y % 8) * 4 + (x % 8) / 2];
}

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
     * row 61, so the box centre (32) shows picture row 71 - 29 * 1.4 = 30.4. */
    MakeEntry(false, 70);
    assert(CtrGen4_Load(1, 6, false, false, false, fire, palette));
    assert(palette[0] == 0x10 && palette[15] == 0x1f);
    sOam[1] = 0x200 | (1 << 12);
    assert(sMatch);
    assert(!sMatch(0x200 | (2 << 12), &obj));       /* another palette */
    assert(!sMatch(0x240 | (1 << 12), &obj));       /* another tile */
    assert(sMatch(0x200 | (1 << 12), &obj));
    assert(obj.slot == 1 && obj.pivotX == 40.0f && Near(obj.pivotY, 30.4f) && Near(obj.scale, 1.0f / 1.4f));
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

    /* Fainting: FireRed erases its box from row 48 down, picture row 30.4 + 16 * 1.4 = 52.8, so from row 53. */
    CtrGen4_EraseFrom(1, 48);
    assert(sMatch(0x200 | (1 << 12), &obj));
    assert(PixelByte(&obj, 40, 52) == 0x11 && PixelByte(&obj, 40, 53) == 0 && PixelByte(&obj, 0, 70) == 0);

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

    puts("battle_gen4_test: ok");
    return 0;
}
