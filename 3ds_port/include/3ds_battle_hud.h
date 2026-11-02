#ifndef CTR_BATTLE_HUD_H
#define CTR_BATTLE_HUD_H

/*
 * The battle's healthboxes drawn by the port (3ds_battle_hud.c), 1:1 on the 400x240 screen, in place of FireRed's.
 *
 * In a single battle (not the Safari Zone's), battle_interface.c (recipe 0207) tells this module what each box
 * shows, at the moments it would print it into its own sprite tiles: the name and gender, the level, the HP numbers,
 * how many pixels of the HP and EXP bars are filled, the status and whether the foe's species is caught. The
 * compositor (3ds_video.c) still walks FireRed's OAM: the healthbox's main sprite is where the box is drawn (its
 * position is the GBA one, slide-ins and bobbing included), and that sprite, its other half and the HP bar sprite
 * are not drawn. Doubles and the Safari Zone keep FireRed's boxes.
 */

#include <stdbool.h>
#include <stdint.h>

#define CTR_HUD_BATTLERS 2               /* singles: the player's battler 0 and the foe's 1 */
#define CTR_HUD_NAME_LEN 12              /* FireRed's 10-character nickname and its EOS, in the game's charset */
#define CTR_HUD_HP_PIXELS 48             /* FireRed's HP bar */
#define CTR_HUD_EXP_PIXELS 64            /* FireRed's EXP bar */
/* Room for one box and its drop shadow; the texture holds one per battler, one above the other. */
#define CTR_HUD_IMAGE_W 168
#define CTR_HUD_IMAGE_H 60

enum { CTR_HUD_MALE = 0, CTR_HUD_FEMALE, CTR_HUD_NO_GENDER };
enum { CTR_HUD_OK = 0, CTR_HUD_PSN, CTR_HUD_PAR, CTR_HUD_SLP, CTR_HUD_FRZ, CTR_HUD_BRN };

/* Game side (battle_interface.c). A healthbox was created: `native` (a single battle, not the Safari Zone's) when the
 * port draws it, its main sprite's OAM tile and its HP bar sprite's, and the game's unfaded palette buffer (the box's
 * fades follow the healthbox palette's). */
void CtrHud_Box(unsigned battler, bool native, bool player, unsigned boxTile, unsigned barTile,
                const uint16_t *unfadedPalette);
void CtrHud_Name(unsigned battler, const uint8_t *name, unsigned gender);
void CtrHud_Level(unsigned battler, unsigned level);
void CtrHud_Hp(unsigned battler, int value, bool max);
void CtrHud_HpBar(unsigned battler, unsigned pixels);
void CtrHud_ExpBar(unsigned battler, unsigned pixels);
void CtrHud_Status(unsigned battler, unsigned status);
void CtrHud_Caught(unsigned battler, bool caught);

/* Compositor side. What an OAM tile number is: CTR_HUD_NONE, or the battler's box (its main sprite, where the box
 * is drawn) or another of its sprites (left out). */
enum { CTR_HUD_NONE = 0, CTR_HUD_MAIN, CTR_HUD_PART };
typedef struct CtrHudImage {
    uint32_t version;                    /* changes whenever the pixels do */
    uint16_t width, height;
    int16_t dx, dy;                      /* the box's top left from the main sprite's, in screen pixels */
    const uint32_t *pixels;              /* width x height, rows CTR_HUD_IMAGE_W apart, 0xRRGGBBAA */
    const uint16_t *unfaded;             /* the game's unfaded palette buffer, or NULL */
} CtrHudImage;
unsigned CtrHud_Match(unsigned tile, unsigned *battler);
/* The battler's box, drawn again when what it shows changed. NULL without one. */
const CtrHudImage *CtrHud_Image(unsigned battler);

/* For the host test: the box drawn with this state, as CtrHud_Image does. */
typedef struct CtrHudState {
    bool native, player, caught;
    uint8_t gender, level, status, hpPixels, expPixels;
    uint16_t hp, maxHp, boxTile, barTile;
    uint8_t name[CTR_HUD_NAME_LEN];
} CtrHudState;
void CtrHud_Render(const CtrHudState *state, uint32_t *pixels, unsigned *width, unsigned *height);

#endif
