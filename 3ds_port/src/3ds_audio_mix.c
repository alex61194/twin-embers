#include <stddef.h>

#include "3ds_audio_mix.h"

static int32_t Clamp16(int32_t v)
{
    if (v > 32767)
        return 32767;
    if (v < -32768)
        return -32768;
    return v;
}

/* Shared body: A and B samples on the 16-bit scale (8-bit * 256). */
static unsigned Mix(const int8_t *a8, const int8_t *b8, const int16_t *a16, const int16_t *b16,
                    uint16_t soundcntH, const int16_t *psg, int16_t *out, unsigned frames);

unsigned CtrAudio_Mix(const int8_t *fifoA, const int8_t *fifoB, uint16_t soundcntH,
                      const int16_t *psg, int16_t *out, unsigned frames)
{
    return Mix(fifoA, fifoB, NULL, NULL, soundcntH, psg, out, frames);
}

unsigned CtrAudio_Mix16(const int16_t *fifoA, const int16_t *fifoB, uint16_t soundcntH,
                        const int16_t *psg, int16_t *out, unsigned frames)
{
    return Mix(NULL, NULL, fifoA, fifoB, soundcntH, psg, out, frames);
}

static unsigned Mix(const int8_t *a8, const int8_t *b8, const int16_t *a16, const int16_t *b16,
                    uint16_t soundcntH, const int16_t *psg, int16_t *out, unsigned frames)
{
    int haveA = a8 != NULL || a16 != NULL, haveB = b8 != NULL || b16 != NULL;
    int32_t gainA = (soundcntH & CTR_MIX_A_FULL) ? 128 : 64;
    int32_t gainB = (soundcntH & CTR_MIX_B_FULL) ? 128 : 64;
    int32_t aL = haveA && (soundcntH & CTR_MIX_A_LEFT) ? gainA : 0;
    int32_t aR = haveA && (soundcntH & CTR_MIX_A_RIGHT) ? gainA : 0;
    int32_t bL = haveB && (soundcntH & CTR_MIX_B_LEFT) ? gainB : 0;
    int32_t bR = haveB && (soundcntH & CTR_MIX_B_RIGHT) ? gainB : 0;
    unsigned peak = 0;

    if (out == NULL)
        return 0;
    for (unsigned i = 0; i < frames; ++i)
    {
        int32_t left, right;
        if (a16 != NULL || b16 != NULL)
        {
            int32_t a = a16 ? a16[i] : 0;
            int32_t b = b16 ? b16[i] : 0;
            left = (a * aL + b * bL) >> 8;
            right = (a * aR + b * bR) >> 8;
        }
        else
        {
            int32_t a = a8 ? a8[i] : 0;
            int32_t b = b8 ? b8[i] : 0;
            left = a * aL + b * bL;
            right = a * aR + b * bR;
        }

        if (psg != NULL)
        {
            left += psg[2 * i];
            right += psg[2 * i + 1];
        }
        left = Clamp16(left);
        right = Clamp16(right);
        out[2 * i] = (int16_t)left;
        out[2 * i + 1] = (int16_t)right;

        unsigned absL = (unsigned)(left < 0 ? -left : left);
        unsigned absR = (unsigned)(right < 0 ? -right : right);
        if (absL > peak)
            peak = absL;
        if (absR > peak)
            peak = absR;
    }
    return peak;
}
