#include "global.h"
#include "sprite.h"
#include "3ds_intro_native.h"

/* Snapshot only; callbacks, animation cursors and RNG remain in FireRed. */
extern u8 gSpriteOrder[];
unsigned CtrIntro_ReadActors(CtrIntroActor *actors, unsigned capacity)
{
    unsigned count = 0;
    for (int i = MAX_SPRITES - 1; i >= 0 && count < capacity; --i)
    {
        const struct Sprite *s = &gSprites[gSpriteOrder[i]];
        if (!s->inUse || s->invisible || !s->template) continue;
        CtrIntroActor *a = &actors[count++];
        a->x = s->x + s->x2; a->y = s->y + s->y2;
        a->baseX = s->x; a->baseY = s->y;
        a->tag = s->template->tileTag;
        a->tileOffset = s->oam.tileNum - s->sheetTileStart;
        a->priority = s->oam.priority; a->palette = s->oam.paletteNum;
        a->anim = s->animNum; a->mode = s->oam.objMode;
        a->subpriority = s->subpriority;
        a->flipX = s->hFlip; a->flipY = s->vFlip;
        a->matrixA = a->matrixD = 256;
        if (s->oam.affineMode & ST_OAM_AFFINE_ON_MASK)
        {
            a->matrixA = gOamMatrices[s->oam.matrixNum].a;
            a->matrixD = gOamMatrices[s->oam.matrixNum].d;
        }
    }
    return count;
}
