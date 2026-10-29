#ifndef CTR_BATTLE_GEN4_H
#define CTR_BATTLE_GEN4_H

/*
 * HeartGold's battle sprites in place of FireRed's 64x64 pictures (3ds_battle_gen4.c).
 *
 * The Builder's optional download puts one graphics/gen4/battle/NNN.bin per species in the pack
 * (builder/firered3ds_builder/gen4_sprites.py has the layout). FireRed still loads, animates and
 * tints its own battler sprite; battle_gfx_sfx_util.c (recipe 0192) tells this module which
 * species each battler shows, and the compositor draws the 80x80 HeartGold picture where it
 * would draw that sprite, with the sprite's position, affine matrix, palette and blending. The
 * picture's palette replaces FireRed's in the battler's OBJ palette slot, so the game's fades
 * and tints apply to it as they did to FireRed's.
 */

#include <stdbool.h>
#include <stdint.h>

#define CTR_GEN4_SIZE 80
#define CTR_GEN4_TILES (CTR_GEN4_SIZE / 8)
#define CTR_GEN4_PICTURE (CTR_GEN4_SIZE * CTR_GEN4_SIZE / 2)

/* Game side (battle_gfx_sfx_util.c). */
/* After FireRed's picture of `battler` is in `firePic` (64x64, frame 0): load HeartGold's picture of
 * that species. False (FireRed's picture stays) without its pack entry. `palette` gets the 16 colours
 * to load in the battler's OBJ palette slot. */
bool CtrGen4_Load(unsigned battler, unsigned nationalDex, bool back, bool female, bool shiny,
                  const void *firePic, uint16_t palette[16]);
/* The battler shows FireRed's picture again (a substitute, a ghost, a species left out). */
void CtrGen4_Drop(unsigned battler);
void CtrGen4_DropAll(void);
/* FireRed erases the picture from this row of its 64x64 box down (a foe fainting): so is HeartGold's. */
void CtrGen4_EraseFrom(unsigned battler, unsigned boxRow);

/* Compositor side. For an OAM entry in battle, the HeartGold picture it stands for. */
typedef struct CtrGen4Obj {
    unsigned slot;                  /* 0-3: which picture */
    const uint8_t *tiles;           /* 10x10 4bpp tiles, row by row */
    int offsetX, offsetY;           /* the 80x80 picture's corner in the 64x64 box */
} CtrGen4Obj;

/* Implemented by the game's battle code: the battler sprite's OAM tile number and palette
 * (tile | palette << 12), or -1 without one. */
int CtrGen4_SpriteOam(unsigned battler);

#endif
