#ifndef CTR_EFFECT_OAM_H
#define CTR_EFFECT_OAM_H
#include <stdbool.h>
#include <stdint.h>

/* Screen-authored field actors retain signed coordinates before GBA OAM
 * truncates them. Entries are published with the matching OAM transfer. */
#define CTR_EFFECT_OAM_COUNT 128
typedef struct {
    int16_t x, y;
    uint16_t attr0, attr1;
    bool valid;
} CtrEffectOam;

static inline int CtrEffectOam_Near(int raw, int reference, unsigned period)
{
    unsigned mask = period - 1;
    int delta = (int)(((unsigned)(raw - reference) + period / 2) & mask)
              - (int)(period / 2);
    return reference + delta;
}

static inline bool CtrEffectOam_Resolve(const CtrEffectOam *entries, unsigned index,
                                      uint16_t attr0, uint16_t attr1, int *x, int *y)
{
    if (index >= CTR_EFFECT_OAM_COUNT || !entries[index].valid
        || entries[index].attr0 != attr0 || entries[index].attr1 != attr1)
        return false;
    *x = entries[index].x;
    *y = entries[index].y;
    return true;
}

void CtrVideo_ClearEffectOam(void);
void CtrVideo_MarkEffectOam(unsigned index, uint16_t attr0, uint16_t attr1, int x, int y);
void CtrVideo_CommitEffectOam(void);
#endif
