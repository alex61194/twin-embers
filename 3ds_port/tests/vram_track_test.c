/* CtrVramTrack: a block is stamped with the token of the frame its change is
 * first seen in; bytes checked at a token stay valid until a later stamp. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "3ds_vram_track.h"

enum { BLOCKS = 0x18000 / CTR_VRAM_TRACK_BLOCK };
static _Alignas(4) uint8_t vram[0x18000];
static uint32_t copy[0x18000 / 4];
static uint32_t stamps[BLOCKS];

int main(void)
{
    /* Frame 1: VRAM equals the zeroed copy, nothing is stamped. */
    assert(CtrVramTrack_Update(vram, copy, stamps, BLOCKS, 1) == 0);
    assert(!CtrVramTrack_MayDiffer(stamps, 0x4000, 0));

    /* A tile verified at token 1 holds while its block does not change. */
    uint32_t checked = 1;
    vram[0x0400] = 7; /* other block */
    assert(CtrVramTrack_Update(vram, copy, stamps, BLOCKS, 2) == 1);
    assert(stamps[1] == 2);
    assert(!CtrVramTrack_MayDiffer(stamps, 0x0000, checked)); /* block 0 untouched */
    assert(CtrVramTrack_MayDiffer(stamps, 0x0400, checked));
    assert(CtrVramTrack_MayDiffer(stamps, 0x07E0, checked)); /* same block, other tile */
    assert(!CtrVramTrack_MayDiffer(stamps, 0x0400, 2));      /* re-verified at 2 */

    /* A change in the last word of a block and of VRAM is seen. */
    vram[0x07FF] = 1;
    vram[0x17FFF] = 1;
    assert(CtrVramTrack_Update(vram, copy, stamps, BLOCKS, 3) == 2);
    assert(stamps[1] == 3 && stamps[BLOCKS - 1] == 3);
    /* The copy follows: an unchanged frame stamps nothing. */
    assert(CtrVramTrack_Update(vram, copy, stamps, BLOCKS, 4) == 0);
    assert(memcmp(copy, vram, sizeof(vram)) == 0);

    /* Changed and changed back between two frames: nothing to see, and the
     * cached bytes are right again. */
    vram[0x2000] = 9;
    vram[0x2000] = 0;
    assert(CtrVramTrack_Update(vram, copy, stamps, BLOCKS, 5) == 0);

    /* Every word position of a block is compared. */
    for (unsigned i = 0; i < CTR_VRAM_TRACK_BLOCK; i += 4)
    {
        vram[0x8000 + i] ^= 0x80;
        assert(CtrVramTrack_Update(vram, copy, stamps, BLOCKS, 100 + i) == 1);
        assert(stamps[0x8000 / CTR_VRAM_TRACK_BLOCK] == 100 + i);
    }
    /* Lazy: a block is compared at most once per token, only when asked, and a change found later is stamped with
     * the later token. */
    {
        static uint32_t lazyCopy[0x18000 / 4], lazyStamps[BLOCKS], synced[BLOCKS];
        memcpy(lazyCopy, vram, sizeof(vram));
        vram[0x0c00] ^= 1;   /* block 3 */
        assert(CtrVramTrack_SyncBlock(vram, lazyCopy, lazyStamps, synced, 2, 500) == 0);
        assert(CtrVramTrack_SyncBlock(vram, lazyCopy, lazyStamps, synced, 3, 500) == 500);
        vram[0x0c00] ^= 1;   /* changed back, but block 3 was already compared at 500 */
        assert(CtrVramTrack_SyncBlock(vram, lazyCopy, lazyStamps, synced, 3, 500) == 500);
        assert(memcmp(lazyCopy + 0x0c00 / 4, vram + 0x0c00, CTR_VRAM_TRACK_BLOCK) != 0);
        /* Unread for frames 501-509: frame 510 sees the change back, stamped 510. */
        assert(CtrVramTrack_SyncBlock(vram, lazyCopy, lazyStamps, synced, 3, 510) == 510);
        assert(memcmp(lazyCopy, vram, sizeof(vram)) == 0);
        assert(CtrVramTrack_SyncBlock(vram, lazyCopy, lazyStamps, synced, 3, 511) == 510);
    }
    puts("PASS vram track: per-kilobyte stamps, first/last word, copy refresh, revert, all word positions");
    return 0;
}
