#ifndef CTR_LOOKS_H
#define CTR_LOOKS_H

#include <stdint.h>

/* The alternative player looks the pack has (the Builder's --alt-looks): the list is the pack's graphics/gen4/look/index.bin
 * (b"G4L1", then a line "folder<TAB>name<TAB>boy or girl<TAB>credit" for each), read the first time it is asked for once the
 * pack is open. The OPTIONS' look rows (3ds_poketouch_options.c) list them, keep the choice for Red, Leaf and the rival Blue in
 * the settings (CtrSettings_Look) and show a preview of the look under them; the pictures themselves are 3ds_battle_gen4.c's. */
#define CTR_LOOKS_MAX 24
#define CTR_LOOK_ID_LEN 16          /* a folder's name and its end (CtrSettings_Look keeps 15 characters) */
#define CTR_LOOK_NAME_LEN 12        /* the name the options show, at most 11 characters and its end */
#define CTR_LOOK_CREDIT_LEN 128     /* the artists named under the preview */
#define CTR_LOOK_WALK_BYTES 512     /* the walking picture: 32x32 at 4 bits, as 4x4 tiles of 32 bytes, row by row of tiles */

/* How many looks the pack has: none without the Builder's alternative looks (or before the pack is open). */
int CtrLooks_Count(void);
/* The look's folder name, the name the options show and its credit (index 0 to CtrLooks_Count() - 1), NULL out of range. */
const char *CtrLooks_Id(int index);
const char *CtrLooks_Name(int index);
const char *CtrLooks_Credit(int index);
/* The look's walking picture facing down (the pack's walk.bin, its first frame) and its 16 colours (15-bit, 0 clear), for the
 * preview. 0 when the pack does not have it. */
int CtrLooks_Walk(int index, uint8_t tiles[CTR_LOOK_WALK_BYTES], uint16_t palette[16]);
/* The index of the look chosen for the slot (CTR_LOOK_*), or -1 for none (or one the pack does not have). */
int CtrLooks_Chosen(int slot);
/* Choose the look of `index` for the slot, or none with -1: kept in the settings, and the pictures already read are dropped. */
void CtrLooks_Choose(int slot, int index);

#endif
