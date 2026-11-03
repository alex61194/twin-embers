/* The port's healthboxes (src/3ds_battle_hud.c) on the host: which OAM tiles are a box's, the boxes' sizes and
 * anchors, their bars and redraws. The font is synthetic: every glyph a 4x7 block of ink, 5 pixels wide. */
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "3ds_battle_hud.h"
#include "3ds_video.h"

static uint8_t sFont[512 * 64], sSmall[512 * 32];
const uint8_t *CtrTwinFontData(void) { return sFont; }
unsigned CtrTwinFontWidth(unsigned code) { return code == 0 ? 3 : 5; }
const uint8_t *CtrTwinSmallFontData(void) { return sSmall; }
unsigned CtrTwinSmallFontWidth(unsigned code) { return code == 0 ? 3 : 5; }

static unsigned (*sMatch)(unsigned tile, unsigned *battler);
static const CtrHudImage *(*sImage)(unsigned battler);
void CtrVideo_SetBattleHud(unsigned (*match)(unsigned tile, unsigned *battler),
                           const CtrHudImage *(*image)(unsigned battler))
{
    sMatch = match;
    sImage = image;
}

/* Ink (value 1) in columns 0-3, rows 3-9: two-bit pixels, the high byte holding the left four. */
static void MakeFonts(void)
{
    for (unsigned code = 1; code < 512; ++code)
        for (unsigned y = 3; y <= 9; ++y)
        {
            sFont[code * 64 + y * 2 + 1] = 0x55;
            sSmall[code * 32 + y * 2 + 1] = 0x55;
        }
}

static uint32_t Pixel(const CtrHudImage *image, unsigned x, unsigned y)
{
    return image->pixels[y * CTR_HUD_IMAGE_W + x];
}

static unsigned Rgb(uint32_t v) { return v >> 8; }

static const uint8_t NAME[] = {0xC6, 0xBF, 0xBB, 0xFF};     /* "LEA" */
static const uint16_t UNFADED[512];

int main(void)
{
    unsigned battler = 9;

    MakeFonts();

    /* A single battle: the player's 64x64 pair, its HP bar sprite, and a foe's 64x32 pair. */
    CtrHud_Box(0, CTR_HUD_PLAYER, 0, false, 0x100, 0x300, UNFADED);
    CtrHud_Box(1, CTR_HUD_FOE, 1, false, 0x200, 0x310, UNFADED);
    assert(sMatch && sImage);
    assert(sMatch(0x100, &battler) == CTR_HUD_MAIN && battler == 0);
    assert(sMatch(0x140, &battler) == CTR_HUD_PART && battler == 0);      /* its other half */
    assert(sMatch(0x17F, &battler) == CTR_HUD_PART && sMatch(0x180, &battler) == CTR_HUD_NONE);
    assert(sMatch(0x304, &battler) == CTR_HUD_PART && sMatch(0x308, &battler) == CTR_HUD_PART);
    assert(sMatch(0x309, &battler) == CTR_HUD_NONE);
    assert(sMatch(0x200, &battler) == CTR_HUD_MAIN && battler == 1);
    assert(sMatch(0x23F, &battler) == CTR_HUD_PART && sMatch(0x240, &battler) == CTR_HUD_NONE);
    assert(!sImage(2) && !sImage(3));

    /* The player's box: 136x49 and its shadow; at rest it is where the design puts it (260, 141). */
    CtrHud_Name(0, NAME, CTR_HUD_MALE, false);
    CtrHud_Level(0, 12);
    CtrHud_Hp(0, 27, false);
    CtrHud_Hp(0, 34, true);
    CtrHud_HpBar(0, 48);
    CtrHud_ExpBar(0, 32);
    const CtrHudImage *player = sImage(0);
    assert(player && player->width == 137 && player->height == 51 && player->unfaded == UNFADED);
    float sx = 126 * CTR_BATTLE_ZOOM + 200 - 120 * CTR_BATTLE_ZOOM + player->dx;
    float sy = 56 * CTR_BATTLE_ZOOM + 192 - 112 * CTR_BATTLE_ZOOM + player->dy;
    assert(fabsf(sx - 260) < 0.01f && fabsf(sy - 141) < 0.01f);
    /* Outside the plate it is clear; the drop shadow is translucent; the plate opaque. */
    assert(Pixel(player, 0, 0) == 0);
    assert((Pixel(player, 136, 43) & 255) == 107);
    assert((Pixel(player, 80, 10) & 255) == 255);

    /* A full HP bar is green to its end; a fifth is red. The track starts 16 past the capsule's left (x 16). */
    unsigned version = player->version;
    assert(Rgb(Pixel(player, 16 + 16 + 4, 27 + 5)) == 0x48D478);   /* the fill's third row */
    CtrHud_HpBar(0, 9);
    player = sImage(0);
    assert(player->version != version);
    assert(Rgb(Pixel(player, 16 + 16 + 2, 27 + 5)) == 0xEC4434);
    assert(Rgb(Pixel(player, 16 + 16 + 60, 27 + 5)) == 0x464C60);  /* the empty track */
    /* Nothing changed, nothing redrawn. */
    version = player->version;
    CtrHud_HpBar(0, 9);
    CtrHud_Level(0, 12);
    assert(sImage(0)->version == version);

    /* The foe's box: 152x36 at the design's (10, 22). */
    CtrHud_Name(1, NAME, CTR_HUD_FEMALE, true);
    const CtrHudImage *foe = sImage(1);
    assert(foe->width == 153 && foe->height == 38);
    sx = 12 * CTR_BATTLE_ZOOM + 200 - 120 * CTR_BATTLE_ZOOM + foe->dx;
    sy = 14 * CTR_BATTLE_ZOOM + 192 - 112 * CTR_BATTLE_ZOOM + foe->dy;
    assert(fabsf(sx - 10) < 0.01f && fabsf(sy - 22) < 0.01f);
    /* Its Poke Ball: grey until the species is caught. */
    assert(Rgb(Pixel(foe, 5 + 4, 12 + 2)) == 0xAAAEBA);
    CtrHud_Caught(1, true);
    assert(Rgb(Pixel(sImage(1), 5 + 4, 12 + 2)) == 0xE8382C);

    /* A double battle: four boxes, the player's low ones (no EXP bar), 64x32 pairs. */
    for (unsigned b = 0; b < 4; ++b)
        CtrHud_Box(b, b & 1 ? CTR_HUD_FOE : CTR_HUD_PLAYER_DOUBLE, b, true, 0x100 + b * 0x40, 0x300 + b * 0x10, UNFADED);
    assert(sMatch(0x100, &battler) == CTR_HUD_MAIN && battler == 0);
    assert(sMatch(0x13F, &battler) == CTR_HUD_PART && battler == 0);
    assert(sMatch(0x140, &battler) == CTR_HUD_MAIN && battler == 1);
    assert(sMatch(0x1C0, &battler) == CTR_HUD_MAIN && battler == 3);
    const CtrHudImage *low = sImage(2);
    assert(low && low->width == 137 && low->height == 38);
    sx = 139 * CTR_BATTLE_ZOOM + 200 - 120 * CTR_BATTLE_ZOOM + low->dx;
    sy = 84 * CTR_BATTLE_ZOOM + 192 - 112 * CTR_BATTLE_ZOOM + low->dy;
    assert(fabsf(sx - 260) < 0.01f && fabsf(sy - 150) < 0.01f);
    sx = 0 * CTR_BATTLE_ZOOM + 200 - 120 * CTR_BATTLE_ZOOM + sImage(3)->dx;
    assert(fabsf(sx - 6) < 0.01f);

    /* The Safari Zone after it: the right-hand battlers go, the player's box is the Safari Zone's, with no HP bar
     * sprite. */
    CtrHud_Box(0, CTR_HUD_SAFARI, 0, false, 0x100, 0xFFFF, UNFADED);
    CtrHud_Box(1, CTR_HUD_FOE, 1, false, 0x200, 0x310, UNFADED);
    assert(!sImage(2) && !sImage(3));
    assert(sMatch(0x1C0, &battler) == CTR_HUD_NONE);
    assert(sMatch(0x17F, &battler) == CTR_HUD_PART && battler == 0);
    CtrHud_SafariBalls(0, 30);
    assert(sImage(0)->width == 137 && sImage(0)->height == 38);

    /* A box the port leaves to FireRed matches nothing and has no picture. */
    CtrHud_Box(0, CTR_HUD_OFF, 0, false, 0x100, 0x300, UNFADED);
    assert(sMatch(0x100, &battler) == CTR_HUD_NONE && !sImage(0));

    puts("battle_hud_test: ok");
    return 0;
}
