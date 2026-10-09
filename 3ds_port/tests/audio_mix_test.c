/* DirectSound (8-bit planar) + PSG -> PCM16 interleaved, FireRed routing. */
#include <assert.h>
#include <stdint.h>
#include <string.h>

#include "3ds_audio_mix.h"

enum { FRAMES = 224, GUARD = 16 };

/* SoundInit: B left, A right, A/B full, PSG full (SOUND_ALL_MIX_FULL). */
static const uint16_t kFireRed = CTR_MIX_B_LEFT | CTR_MIX_A_RIGHT | 0x000E;

static int8_t sA[FRAMES], sB[FRAMES];
static int16_t sPsg[2 * FRAMES];
static int16_t sBuf[2 * FRAMES + 2 * GUARD];
static int16_t *const sOut = sBuf + GUARD;

static unsigned Mix(const int8_t *a, const int8_t *b, uint16_t cnt, const int16_t *psg)
{
    return CtrAudio_Mix(a, b, cnt, psg, sOut, FRAMES);
}

int main(void)
{
    for (unsigned i = 0; i < 2 * FRAMES + 2 * GUARD; ++i)
        sBuf[i] = 0x7A7A;

    /* Silence + silence. */
    assert(Mix(sA, sB, kFireRed, sPsg) == 0);
    for (unsigned i = 0; i < 2 * FRAMES; ++i)
        assert(sOut[i] == 0);
    assert(Mix(NULL, NULL, kFireRed, NULL) == 0);

    /* DirectSound only: FIFO A (plane 0) is right, FIFO B (plane 1) left. */
    for (unsigned i = 0; i < FRAMES; ++i)
    {
        sA[i] = (int8_t)(i & 1 ? 100 : -100);
        sB[i] = 25;
    }
    assert(Mix(sA, sB, kFireRed, NULL) == 12800);
    for (unsigned i = 0; i < FRAMES; ++i)
    {
        assert(sOut[2 * i] == 25 * 128);                       /* left = B */
        assert(sOut[2 * i + 1] == (i & 1 ? 12800 : -12800));  /* right = A */
    }
    /* Half-volume FIFO A (bit 2 clear) halves the right channel only. */
    Mix(sA, sB, kFireRed & ~CTR_MIX_A_FULL, NULL);
    assert(sOut[1] == -6400 && sOut[0] == 3200);
    /* Swapped routing swaps sides; both-sides routing sums. */
    Mix(sA, sB, CTR_MIX_A_LEFT | CTR_MIX_B_RIGHT | 0x0C, NULL);
    assert(sOut[0] == -12800 && sOut[1] == 3200);
    Mix(sA, sB, CTR_MIX_A_LEFT | CTR_MIX_A_RIGHT | 0x0C, NULL);
    assert(sOut[0] == -12800 && sOut[1] == -12800);

    /* PSG only keeps its stereo image. */
    memset(sA, 0, sizeof(sA));
    memset(sB, 0, sizeof(sB));
    for (unsigned i = 0; i < FRAMES; ++i)
    {
        sPsg[2 * i] = 3840;
        sPsg[2 * i + 1] = -1920;
    }
    assert(Mix(sA, sB, kFireRed, sPsg) == 3840);
    assert(sOut[0] == 3840 && sOut[1] == -1920);
    assert(Mix(NULL, NULL, kFireRed, sPsg) == 3840);

    /* Both sources add. */
    sA[0] = 10;
    sB[0] = -10;
    Mix(sA, sB, kFireRed, sPsg);
    assert(sOut[0] == 3840 - 1280 && sOut[1] == -1920 + 1280);

    /* FireRed's worst case (one full FIFO + four full PSG channels) fits
     * without clipping: 127*128 + 15360 = 31616. */
    sA[0] = 127;
    sB[0] = -128;
    sPsg[0] = -15360;
    sPsg[1] = 15360;
    Mix(sA, sB, kFireRed, sPsg);
    assert(sOut[0] == -128 * 128 - 15360 && sOut[1] == 127 * 128 + 15360);

    /* Saturation when a routing stacks both FIFOs and the PSG. */
    uint16_t both = CTR_MIX_A_LEFT | CTR_MIX_A_RIGHT | CTR_MIX_B_LEFT | CTR_MIX_B_RIGHT | 0x0E;
    sA[0] = 127;
    sB[0] = 127;
    sPsg[0] = 15360;
    sPsg[1] = 15360;
    assert(Mix(sA, sB, both, sPsg) == 32767);
    assert(sOut[0] == 32767 && sOut[1] == 32767);
    sA[0] = -128;
    sB[0] = -128;
    sPsg[0] = -15360;
    sPsg[1] = -15360;
    assert(Mix(sA, sB, both, sPsg) == 32768);
    assert(sOut[0] == -32768 && sOut[1] == -32768);

    /* No writes outside [out, out + 2*frames). */
    for (unsigned i = 0; i < GUARD; ++i)
    {
        assert(sBuf[i] == 0x7A7A);
        assert(sBuf[GUARD + 2 * FRAMES + i] == 0x7A7A);
    }
    assert(CtrAudio_Mix(sA, sB, kFireRed, sPsg, NULL, FRAMES) == 0);

    /* 16-bit ring: v = 256 * v8 mixes exactly like the 8-bit ring, for every
     * routing and A/B volume combination. */
    static int16_t sA16[FRAMES], sB16[FRAMES], sOut8[2 * FRAMES];
    unsigned seed = 7;
    for (unsigned i = 0; i < FRAMES; ++i)
    {
        seed = seed * 1103515245u + 12345u;
        sA[i] = (int8_t)(seed >> 16);
        sB[i] = (int8_t)(seed >> 24);
        sA16[i] = (int16_t)(sA[i] * 256);
        sB16[i] = (int16_t)(sB[i] * 256);
        sPsg[2 * i] = (int16_t)((seed >> 8) & 0x3FF) - 512;
        sPsg[2 * i + 1] = (int16_t)((seed >> 4) & 0x3FF) - 512;
    }
    for (unsigned cnt = 0; cnt < 0x10000; cnt += 0x0004)
    {
        uint16_t h = (uint16_t)(cnt & (CTR_MIX_A_FULL | CTR_MIX_B_FULL | CTR_MIX_A_LEFT | CTR_MIX_A_RIGHT
                                       | CTR_MIX_B_LEFT | CTR_MIX_B_RIGHT));
        unsigned p8 = CtrAudio_Mix(sA, sB, h, sPsg, sOut8, FRAMES);
        unsigned p16 = CtrAudio_Mix16(sA16, sB16, h, sPsg, sOut, FRAMES);
        assert(p8 == p16 && memcmp(sOut8, sOut, sizeof(sOut8)) == 0);
    }
    /* The fraction the GBA drops is kept: +1/256 of an 8-bit step per 1. */
    memset(sPsg, 0, sizeof(sPsg));
    for (unsigned i = 0; i < FRAMES; ++i)
    {
        sA16[i] = (int16_t)(256 * 10 + 128); /* 10.5 steps */
        sB16[i] = 0;
    }
    CtrAudio_Mix16(sA16, sB16, kFireRed, sPsg, sOut, FRAMES);
    assert(sOut[1] == 10 * 128 + 64 && sOut[0] == 0); /* A right, full */
    /* Saturation, and a missing plane reads as silence. */
    for (unsigned i = 0; i < FRAMES; ++i)
        sA16[i] = sB16[i] = 32767;
    CtrAudio_Mix16(sA16, sB16, CTR_MIX_A_LEFT | CTR_MIX_B_LEFT | CTR_MIX_A_FULL | CTR_MIX_B_FULL,
                   NULL, sOut, FRAMES);
    assert(sOut[0] == 32767 && sOut[1] == 0);
    CtrAudio_Mix16(NULL, sB16, kFireRed, NULL, sOut, FRAMES);
    assert(sOut[1] == 0 && sOut[0] == (32767 * 128) >> 8);
    for (unsigned i = 0; i < GUARD; ++i)
    {
        assert(sBuf[i] == 0x7A7A);
        assert(sBuf[GUARD + 2 * FRAMES + i] == 0x7A7A);
    }
    return 0;
}
