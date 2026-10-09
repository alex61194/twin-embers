/* Checkpoint 1: the ARM11 frame must tick m4aSoundVSync (VCount, line 150)
 * before SoundMain (VBlank, line 160) so pcmDmaCounter cycles the seven
 * 224-sample slices instead of sticking at 0. This pins the exact counter
 * state machine and slice formula from src/m4a_1.s plus the dispatch order. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "gba_shadow.h"
#include "3ds_audio_cadence.h"

enum {
    PERIOD = 7,          /* PCM_DMA_BUF_SIZE / 224 for SOUND_MODE_FREQ_13379 */
    SAMPLES = 224,
    BUF_SIZE = 1584,     /* PCM_DMA_BUF_SIZE */
    IE_VBLANK = 1,
    IE_VCOUNT = 4,
};

static int sVCountCalls;
static int sVBlankCalls;
static uint8_t sCounter;
static unsigned sOffsets[32];
static unsigned sCounters[32];
static unsigned sFrames;

static void MockVCount(void)
{
    assert(gGbaShadow.regs[0x006] == 150);
    assert((gGbaShadow.regs[0x004] & 4) != 0);
    assert((gGbaShadow.regs[0x202] & 4) != 0);
    sCounter = CtrAudio_VSyncNext(sCounter, PERIOD);
    ++sVCountCalls;
}

static void MockVBlank(void)
{
    assert(gGbaShadow.regs[0x006] == 160);
    sCounters[sFrames] = sCounter;
    sOffsets[sFrames] = CtrAudio_PcmSliceOffset(sCounter, PERIOD, SAMPLES);
    ++sFrames;
    ++sVBlankCalls;
}

static void CheckHelpers(void)
{
    /* m4aSoundVSync byte sequence from the m4aSoundVSyncOn() initial 0. */
    assert(CtrAudio_VSyncNext(0, PERIOD) == 7);
    assert(CtrAudio_VSyncNext(7, PERIOD) == 6);
    assert(CtrAudio_VSyncNext(2, PERIOD) == 1);
    assert(CtrAudio_VSyncNext(1, PERIOD) == 7);

    /* SoundMain slice: 0/1 -> 0, else (period + 1 - counter) * count. */
    assert(CtrAudio_PcmSliceOffset(0, PERIOD, SAMPLES) == 0);
    assert(CtrAudio_PcmSliceOffset(1, PERIOD, SAMPLES) == 0);
    assert(CtrAudio_PcmSliceOffset(7, PERIOD, SAMPLES) == 224);
    assert(CtrAudio_PcmSliceOffset(6, PERIOD, SAMPLES) == 448);
    assert(CtrAudio_PcmSliceOffset(2, PERIOD, SAMPLES) == 1344);

    /* Every reachable slice stays inside the PCM plane. */
    for (unsigned c = 0; c <= PERIOD; ++c)
        assert(CtrAudio_PcmSliceOffset(c, PERIOD, SAMPLES) + SAMPLES <= BUF_SIZE);
}

static void CheckDispatchGating(void)
{
    GbaShadow_Reset(); /* IME=1, IE=0 out of reset. */
    gGbaShadow.regs[0x200] = IE_VCOUNT;
    gGbaShadow.regs[0x208] = 0;
    GbaShadow_DispatchVCount(MockVCount);
    assert(sVCountCalls == 0); /* IME=0: masked, like hardware. */
    gGbaShadow.regs[0x200] = 0;
    gGbaShadow.regs[0x208] = 1;
    GbaShadow_DispatchVCount(MockVCount);
    assert(sVCountCalls == 0); /* IE=0: masked. */
    gGbaShadow.regs[0x200] = IE_VCOUNT;
    GbaShadow_DispatchVCount(MockVCount);
    assert(sVCountCalls == 1);
    assert(gGbaShadow.regs[0x006] == 150);
    assert((gGbaShadow.regs[0x004] & 4) == 0);
    assert((gGbaShadow.regs[0x202] & 4) == 0);
    GbaShadow_DispatchVBlank(MockVBlank);
    assert(sVBlankCalls == 0); /* VBlank still masked without its IE bit. */
    gGbaShadow.regs[0x200] = (uint8_t)(IE_VBLANK | IE_VCOUNT);
    GbaShadow_DispatchVBlank(MockVBlank);
    assert(sVBlankCalls == 1);
    assert(gGbaShadow.regs[0x006] == 0); /* EndVBlank parks VCOUNT at 0. */
}

int main(void)
{
    CheckHelpers();
    CheckDispatchGating();

    /* Full frames in GBA scanline order: VCount tick, then VBlank mix. */
    GbaShadow_Reset();
    gGbaShadow.regs[0x200] = (uint8_t)(IE_VBLANK | IE_VCOUNT);
    gGbaShadow.regs[0x208] = 1;
    sVCountCalls = 0;
    sVBlankCalls = 0;
    sCounter = 0; /* m4aSoundVSyncOn() leaves pcmDmaCounter at 0. */
    sFrames = 0;
    for (unsigned f = 0; f < 16; ++f)
    {
        GbaShadow_DispatchVCount(MockVCount);
        GbaShadow_DispatchVBlank(MockVBlank);
    }
    assert(sVCountCalls == 16 && sVBlankCalls == 16);

    /* Counter must advance 7..1 and wrap; stuck-at-0 is the old bug. */
    static const unsigned kCounters[16] = {7, 6, 5, 4, 3, 2, 1, 7,
                                           6, 5, 4, 3, 2, 1, 7, 6};
    static const unsigned kOffsets[16] = {224, 448, 672, 896, 1120, 1344, 0, 224,
                                          448, 672, 896, 1120, 1344, 0, 224, 448};
    for (unsigned f = 0; f < 16; ++f)
    {
        assert(sCounters[f] == kCounters[f]);
        assert(sOffsets[f] == kOffsets[f]);
    }

    /* Each period visits all seven slices exactly once: none lost, none
     * repeated (sorted multiset check over frames 1..7). */
    unsigned seen = 0;
    for (unsigned f = 0; f < 7; ++f)
        seen |= 1u << (sOffsets[f] / SAMPLES);
    assert(seen == 0x7Fu);

    (void)memset(sOffsets, 0, sizeof(sOffsets));
    return 0;
}
