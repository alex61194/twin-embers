#ifndef FIRERED_3DS_AUDIO_CADENCE_H
#define FIRERED_3DS_AUDIO_CADENCE_H

#include <stdint.h>

/* FireRed PCM ring cadence, mirroring src/m4a_1.s so the ARM11 probe and the
 * host regression test share one definition.
 *
 * m4aSoundVSync (VCount tick): the u8 pcmDmaCounter is decremented; when the
 * signed result is not > 0 the counter reloads to pcmDmaPeriod. From the
 * m4aSoundVSyncOn() initial 0 the sequence reloads period then counts to 1.
 *
 * SoundMain (VBlank, after the tick): the fresh samplesPerVBlank slice starts at
 * offset 0 for counter 0/1, else (pcmDmaPeriod + 1 - counter) * count.
 * Native quality: 528 samples / period 3. Original GBA: 224 / period 7. */

static inline uint8_t CtrAudio_VSyncNext(uint8_t counter, uint8_t period)
{
    int8_t stepped = (int8_t)(uint8_t)(counter - 1u);
    if (stepped > 0)
        return (uint8_t)stepped;
    return period;
}

static inline unsigned CtrAudio_PcmSliceOffset(unsigned counter, unsigned period,
                                               unsigned samplesPerVBlank)
{
    if (counter > 1 && counter <= period)
        return (period + 1 - counter) * samplesPerVBlank;
    return 0;
}

#endif
