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
/* The pictures are shown 1:1 on the screen: the battle scene is zoomed 1.5 (CTR_BATTLE_ZOOM), so a picture pixel is
 * 1/1.5 of a GBA pixel and an 80x80 picture takes 80x80 screen pixels (53.3 GBA pixels: it fits FireRed's 64x64 box). */
#define CTR_GEN4_SCALE (1.0f / 1.5f)
/* The player's back comes 1.25 times as large as its 80x80 art, enlarged by the Builder as pixel art (100x100 on a
 * 104x104 picture, 13x13 tiles), and is shown 1:1 like the rest. */
#define CTR_GEN4_BACK_SIZE 104
#define CTR_GEN4_BACK_TILES (CTR_GEN4_BACK_SIZE / 8)
#define CTR_GEN4_BACK_PICTURE (CTR_GEN4_BACK_SIZE * CTR_GEN4_BACK_SIZE / 2)
/* Which pictures: a species' national dex (1-386) and its form: Unown's letter 0-27 as FireRed numbers them (A to Z,
 * !, ?), Castform's weather 0-3 (normal, sun, rain, ice). The functions below that take a "nationalDex" take this
 * key. */
#define CTR_GEN4_KEY(dex, form) ((dex) | ((form) << 9))

/* Game side (battle_gfx_sfx_util.c). */
/* After FireRed's picture of `battler` is in `firePic` (64x64, frame 0): load HeartGold's picture of
 * that species. False (FireRed's picture stays) without its pack entry. `palette` gets the 16 colours
 * to load in the battler's OBJ palette slot. */
bool CtrGen4_Load(unsigned battler, unsigned nationalDex, bool back, bool female, bool shiny,
                  const void *firePic, uint16_t palette[16]);
/* The battler shows FireRed's picture again (a substitute, a ghost, a species left out). */
void CtrGen4_Drop(unsigned battler);
void CtrGen4_DropAll(void);
/* For a move animation that copies the battler onto a background (CopyBattlerSpriteToBg): HeartGold's picture as
 * FireRed's 64x64 4bpp one (8x8 tiles, eight to a row), at the battle's scale and where the sprite shows it. False,
 * `out` untouched, when the battler shows FireRed's. */
bool CtrGen4_GbaPicture(unsigned battler, uint8_t out[0x800]);
/* FireRed erases the picture from this row of its 64x64 box down (a foe fainting): so is HeartGold's, from the
 * picture row drawn there. */
void CtrGen4_EraseFrom(unsigned battler, unsigned boxRow);

/* Outside battle (FireRed's picture loaders, recipe 0195): a species' HeartGold picture, front or back, as FireRed's
 * 64x64 4bpp one (8x8 tiles, eight to a row), the whole 80x80 picture shrunk into it; its normal or shiny palette;
 * and the 80x80 picture itself in rows of 40 bytes (the lower screen's Summary draws it whole). False without the
 * species' pack entry. */
bool CtrGen4_Picture64(unsigned nationalDex, bool back, bool female, uint8_t out[0x800]);
bool CtrGen4_Palette(unsigned nationalDex, bool shiny, uint16_t palette[16]);
bool CtrGen4_Picture80(unsigned nationalDex, bool back, bool female, uint8_t out[CTR_GEN4_PICTURE]);
/* The personality whose spots a Spinda front picture asked for next gets (graphics/gen4/spinda.bin, HeartGold's
 * way of placing them); the game sets it with each Spinda's key. */
void CtrGen4_SpindaPersonality(uint32_t personality);

/* HeartGold's menu icons (graphics/gen4/icons.bin, read once and kept): key 0 the egg, 1-386 the national dex,
 * 387-413 Unown's letters B to ?. NULL / -1 without the entry or for another key. All or none: the Builder writes
 * the entry only with every icon, and its palettes then stand for FireRed's icon palettes 0-5. */
const uint8_t *CtrGen4_IconTiles(int key);           /* 1024 bytes: two 32x32 frames, as FireRed's */
int CtrGen4_IconPalette(int key);                    /* which of the palettes below */
const uint16_t *CtrGen4_IconPaletteData(unsigned index);

/* Trainers (battle_gfx_sfx_util.c's DecompressTrainerFrontPic, recipe 0206): after FireRed's front picture
 * `trainerPic` (TRAINER_PIC_*) of the trainer standing for `battler` is in `firePic` (64x64, frame 0) and its
 * sprite sheet starts at OBJ tile `tileNum` with OBJ palette `paletteNum`: load HeartGold's picture from
 * graphics/gen4/trainer/NNN.bin. False (FireRed's picture stays) without that entry. `palette` gets its 16
 * colours, to load in that palette. A trainer's sprite is matched by its tiles and palette, not its battler,
 * so it keeps its picture while the battler's Pokemon comes out. */
bool CtrGen4_LoadTrainer(unsigned battler, unsigned trainerPic, const void *firePic, unsigned tileNum,
                         unsigned paletteNum, uint16_t palette[16]);
/* FireRed frees the picture's tiles and palette (FreeTrainerFrontPicPaletteAndTile). */
void CtrGen4_DropTrainer(unsigned trainerPic);

/* The player's back (Red's or Leaf's, TRAINER_BACK_PIC_*): graphics/gen4/back/NNN.bin, fan art in HeartGold's
 * style with FireRed's five frames, 104x104 pictures (CTR_GEN4_BACK_SIZE), read once and kept. `palette` gets its 16 colours for the game to load where
 * it loads FireRed's back palette. False without the entry. */
bool CtrGen4_BackPalette(unsigned backPic, uint16_t palette[16]);
/* Implemented by the game's battle code: whether `battler`'s sprite is a trainer back the pack may replace, and
 * then its OAM tile and palette (tile | palette << 12), its back picture, the frame its animation shows and
 * FireRed's first frame (64x64, for where the picture stands); -1 otherwise. */
int CtrGen4_BackSprite(unsigned battler, unsigned *backPic, unsigned *frame, const void **firePic);

/* Pictures the compositor can draw at once: the four battlers' Pokemon (slots 0-3), a trainer for each battler
 * (slots 4-7), then a back for each (slots 8-11). */
#define CTR_GEN4_SLOTS 12
#define CTR_GEN4_TRAINER_SLOT(battler) (4 + (battler))
#define CTR_GEN4_BACK_SLOT(battler) (8 + (battler))

/* Compositor side. For an OAM entry in battle, the HeartGold picture it stands for. */
typedef struct CtrGen4Obj {
    unsigned slot;                  /* 0-11: which picture (CTR_GEN4_SLOTS) */
    const uint8_t *tiles;           /* side x side 4bpp tiles, row by row */
    unsigned side;                  /* tiles to a row and a column: 10 (CTR_GEN4_TILES), 13 for a back */
    float pivotX, pivotY;           /* the picture's point drawn on the 64x64 box's centre */
    float scale;                    /* GBA pixels per picture pixel */
} CtrGen4Obj;

/* Implemented by the game's battle code: the battler sprite's OAM tile number and palette
 * (tile | palette << 12), or -1 without one. */
int CtrGen4_SpriteOam(unsigned battler);

#endif
