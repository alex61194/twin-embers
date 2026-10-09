/* Band-limited PSG output (CtrCgb_SetBandLimited): same register model and
 * settled levels as the point-sampled path, a fixed delay, no drift, and far
 * less aliasing, measured on the spectrum. */
#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_cgb_audio.h"
#include "3ds_cgb_blep.h"

enum { RATE = 13379, FRAME = 224, N = 4096 };

static uint8_t sRegs[0x400];
static CtrCgb sCgb;

static void Reset(int bandLimited)
{
    memset(sRegs, 0, sizeof(sRegs));
    sRegs[CTR_CGB_REG_NR52] = 0x8F;
    sRegs[CTR_CGB_REG_NR50] = 0x77;
    sRegs[CTR_CGB_REG_NR51] = 0xFF;
    sRegs[CTR_CGB_REG_SOUNDCNT_H] = 0x0E;
    CtrCgb_Init(&sCgb, RATE);
    CtrCgb_SetBandLimited(&sCgb, bandLimited);
}

static void StartPulse(unsigned ch, uint8_t duty, uint8_t env, unsigned freq)
{
    sRegs[ch == 0 ? CTR_CGB_REG_NR11 : CTR_CGB_REG_NR21] = (uint8_t)(duty << 6);
    sRegs[ch == 0 ? CTR_CGB_REG_NR12 : CTR_CGB_REG_NR22] = env;
    sRegs[ch == 0 ? CTR_CGB_REG_NR13 : CTR_CGB_REG_NR23] = (uint8_t)freq;
    sRegs[ch == 0 ? CTR_CGB_REG_NR14 : CTR_CGB_REG_NR24] = (uint8_t)(0x80 | (freq >> 8));
}

static void StartWave(const uint8_t ram[16], unsigned freq)
{
    memcpy(sRegs + CTR_CGB_REG_WAVE_RAM, ram, 16);
    sRegs[CTR_CGB_REG_NR30] = 0x80;
    sRegs[CTR_CGB_REG_NR32] = 0x20;
    sRegs[CTR_CGB_REG_NR33] = (uint8_t)freq;
    sRegs[CTR_CGB_REG_NR34] = (uint8_t)(0x80 | (freq >> 8));
}

/* Left channel of `count` samples, rendered in FireRed-sized frames. */
static void Capture(int16_t *left, unsigned count)
{
    int16_t frame[2 * FRAME];
    for (unsigned done = 0; done < count;)
    {
        assert(CtrCgb_Render(&sCgb, sRegs, frame, FRAME) == FRAME);
        for (unsigned i = 0; i < FRAME && done < count; ++i)
            left[done++] = frame[2 * i];
    }
}

/* Hann-windowed spectrum: power within +-4 bins of the harmonics k*f0 below
 * Nyquist, against everything else but DC. Returns alias/signal in dB. */
static double AliasDb(const int16_t *x, double f0)
{
    static double re[N / 2], im[N / 2];
    double window[N], total = 0, signal = 0;

    for (unsigned n = 0; n < N; ++n)
        window[n] = x[n] * (0.5 - 0.5 * cos(2 * M_PI * n / (N - 1)));
    for (unsigned k = 0; k < N / 2; ++k)
    {
        double sr = 0, si = 0;
        for (unsigned n = 0; n < N; ++n)
        {
            double a = 2 * M_PI * (double)k * n / N;
            sr += window[n] * cos(a);
            si -= window[n] * sin(a);
        }
        re[k] = sr;
        im[k] = si;
    }
    for (unsigned k = 5; k < N / 2; ++k)
    {
        double p = re[k] * re[k] + im[k] * im[k];
        double freq = (double)k * RATE / N;
        double harmonic = f0 * floor(freq / f0 + 0.5);
        int near = harmonic > 0 && fabs(freq - harmonic) <= 4.0 * RATE / N;
        total += p;
        if (near)
            signal += p;
    }
    return 10 * log10((total - signal) / signal);
}

static void Render2(int16_t *out)
{
    assert(CtrCgb_Render(&sCgb, sRegs, out, FRAME) == FRAME);
}

static double MeasurePulse(int bandLimited, unsigned freq, uint8_t duty)
{
    static int16_t x[N + 512];
    Reset(bandLimited);
    StartPulse(0, duty, 0xF0, freq);
    Capture(x, 512); /* past the attack and the kernel delay */
    Capture(x, N);
    return AliasDb(x, 131072.0 / (2048 - freq));
}

static double MeasureWave(int bandLimited, unsigned freq)
{
    static const uint8_t ramp[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    static int16_t x[N + 512];
    Reset(bandLimited);
    StartWave(ramp, freq);
    Capture(x, 512);
    Capture(x, N);
    return AliasDb(x, 65536.0 / (2048 - freq));
}

static void TestAliasing(void)
{
    /* High notes, where point sampling folds the most: C7 (~2080 Hz) on
     * CH1 at 50 % and 12.5 % duty, E6 (~1310 Hz), and a ~1170 Hz CH3 ramp. */
    static const struct { unsigned freq; uint8_t duty; } pulses[] = {
        {1985, 2}, {1985, 0}, {1948, 2}, {1900, 1},
    };
    for (unsigned t = 0; t < sizeof(pulses) / sizeof(pulses[0]); ++t)
    {
        double point = MeasurePulse(0, pulses[t].freq, pulses[t].duty);
        double blep = MeasurePulse(1, pulses[t].freq, pulses[t].duty);
        printf("pulse f=%u duty=%u: alias %.1f dB point, %.1f dB band-limited\n",
               pulses[t].freq, pulses[t].duty, point, blep);
        assert(blep <= -35.0 && blep <= point - 20.0);
    }
    double point = MeasureWave(0, 1992), blep = MeasureWave(1, 1992);
    printf("wave f=1992: alias %.1f dB point, %.1f dB band-limited\n", point, blep);
    assert(blep <= -35.0 && blep <= point - 20.0);
}

/* Two synthesizers fed the same register writes: point and band-limited. */
static uint8_t sRegsB[0x400];
static CtrCgb sCgbB;

static void RenderBoth(int16_t *point, int16_t *blep, unsigned frames)
{
    memcpy(sRegsB, sRegs, sizeof(sRegs));
    assert(CtrCgb_Render(&sCgb, sRegs, point, frames) == frames);
    assert(CtrCgb_Render(&sCgbB, sRegsB, blep, frames) == frames);
    assert(memcmp(sRegs, sRegsB, sizeof(sRegs)) == 0); /* same register model */
}

/* Wherever the point-sampled output holds still for the whole kernel span,
 * the band-limited output DELAY samples later is exactly the same value. */
static void TestSettledLevels(void)
{
    enum { FRAMES = 40 };
    static int16_t point[2 * FRAME * FRAMES], blep[2 * FRAME * FRAMES];
    static const struct { uint8_t nr50, nr51, ratio, duty; unsigned freq; } cases[] = {
        {0x77, 0xFF, 0x0E, 2, 0},      /* 64 Hz, full */
        {0x33, 0x12, 0x0D, 1, 512},    /* 85 Hz, master 3, PSG 50 %, split routing */
        {0x70, 0x21, 0x0C, 3, 1024},   /* 128 Hz, master 7/0, PSG 25 % */
    };
    unsigned compared = 0;
    for (unsigned c = 0; c < sizeof(cases) / sizeof(cases[0]); ++c)
    {
        Reset(0);
        CtrCgb_Init(&sCgbB, RATE);
        CtrCgb_SetBandLimited(&sCgbB, 1);
        sRegs[CTR_CGB_REG_NR50] = cases[c].nr50;
        sRegs[CTR_CGB_REG_NR51] = cases[c].nr51;
        sRegs[CTR_CGB_REG_SOUNDCNT_H] = cases[c].ratio;
        StartPulse(0, cases[c].duty, 0xF3, cases[c].freq);   /* decaying envelope */
        StartPulse(1, 2, 0xA0, cases[c].freq + 100);
        for (unsigned f = 0; f < FRAMES; ++f)
            RenderBoth(point + 2 * FRAME * f, blep + 2 * FRAME * f, FRAME);
        for (unsigned m = CTR_CGB_BLEP_TAPS; m + CTR_CGB_BLEP_TAPS < FRAME * FRAMES; ++m)
        {
            unsigned src = m - CTR_CGB_BLEP_DELAY;
            int still = 1;
            for (unsigned side = 0; side < 2; ++side)
                for (unsigned k = src - CTR_CGB_BLEP_DELAY - 1; k <= src + CTR_CGB_BLEP_DELAY + 1; ++k)
                    still &= point[2 * k + side] == point[2 * src + side];
            if (!still)
                continue;
            assert(blep[2 * m] == point[2 * src] && blep[2 * m + 1] == point[2 * src + 1]);
            ++compared;
        }
    }
    assert(compared > 10000);
}

/* A flat wave has no steps: after the kernel it is exactly the level, and
 * before it the output is the kernel's small pre-ring of the trigger only. */
static void TestDelay(void)
{
    static const uint8_t flat[16] = {0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88,
                                     0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88};
    int16_t out[2 * FRAME];
    Reset(1);
    StartWave(flat, 1536);
    assert(CtrCgb_Render(&sCgb, sRegs, out, FRAME) == FRAME);
    for (unsigned m = 0; m < CTR_CGB_BLEP_DELAY - 2; ++m)
        assert(abs(out[2 * m]) < 8);
    for (unsigned m = CTR_CGB_BLEP_TAPS; m < FRAME; ++m)
        assert(out[2 * m] == 1 * 8 * 32 && out[2 * m + 1] == 1 * 8 * 32);
}

/* Fundamentals above Nyquist render as their average, not as aliases. */
static void TestAboveNyquist(void)
{
    int16_t out[2 * FRAME];
    Reset(1);
    StartPulse(0, 2, 0xF0, 2047); /* 131 kHz, 50 %: average 0 */
    Render2(out);
    for (unsigned m = CTR_CGB_BLEP_TAPS; m < FRAME; ++m)
        assert(out[2 * m] == 0);
    Reset(1);
    StartPulse(0, 0, 0xF0, 2047); /* 12.5 %: 15 * (2*1 - 8) * 8 * 4 */
    Render2(out);
    for (unsigned m = CTR_CGB_BLEP_TAPS; m < FRAME; ++m)
        assert(out[2 * m] == 15 * -6 * 8 * 4);
}

/* One long call and FireRed-sized calls give the same samples, so the kernel
 * tails carried between calls and the chunking of long calls are exact. */
static void TestChunking(void)
{
    enum { TOTAL = 3 * CTR_CGB_CHANNELS * FRAME * 2 };
    static int16_t whole[2 * TOTAL], parts[2 * TOTAL];
    static const uint8_t ramp[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    for (unsigned pass = 0; pass < 2; ++pass)
    {
        Reset(1);
        StartPulse(0, 1, 0xF1, 1900);
        StartPulse(1, 3, 0xF0, 1700);
        StartWave(ramp, 1800);
        sRegs[CTR_CGB_REG_NR42] = 0xF2;
        sRegs[CTR_CGB_REG_NR43] = 0x21;
        sRegs[CTR_CGB_REG_NR44] = 0x80;
        if (pass == 0)
            assert(CtrCgb_Render(&sCgb, sRegs, whole, TOTAL) == TOTAL);
        else
            for (unsigned done = 0; done < TOTAL; done += FRAME)
                assert(CtrCgb_Render(&sCgb, sRegs, parts + 2 * done, FRAME) == FRAME);
    }
    assert((unsigned)TOTAL > (unsigned)CTR_CGB_BLEP_CHUNK);
    assert(memcmp(whole, parts, sizeof(whole)) == 0);
}

/* Integer kernel phases sum exactly to 2^BITS, so thousands of random notes,
 * envelopes, sweeps, routing and master changes leave no residue: once every
 * channel stops, the output returns to exactly zero. */
static void TestNoDrift(void)
{
    int16_t out[2 * FRAME];
    unsigned seed = 12345;
    Reset(1);
    for (unsigned f = 0; f < 3000; ++f)
    {
        seed = seed * 1103515245u + 12345u;
        unsigned r = seed >> 8;
        switch (r % 6)
        {
        case 0: StartPulse(0, r >> 3 & 3, (uint8_t)(0x80 | (r >> 5 & 0x7F)), r >> 12 & 2047); break;
        case 1: StartPulse(1, r >> 3 & 3, (uint8_t)(r >> 5), r >> 13 & 2047); break;
        case 2:
            sRegs[CTR_CGB_REG_NR30] = 0x80;
            sRegs[CTR_CGB_REG_NR32] = (uint8_t)(r & 0xE0);
            sRegs[CTR_CGB_REG_WAVE_RAM + (r >> 8 & 15)] = (uint8_t)(r >> 12);
            sRegs[CTR_CGB_REG_NR33] = (uint8_t)(r >> 3);
            sRegs[CTR_CGB_REG_NR34] = (uint8_t)(0x80 | (r >> 20 & 7));
            break;
        case 3:
            sRegs[CTR_CGB_REG_NR42] = (uint8_t)r;
            sRegs[CTR_CGB_REG_NR43] = (uint8_t)(r >> 8);
            sRegs[CTR_CGB_REG_NR44] = 0x80;
            break;
        case 4:
            sRegs[CTR_CGB_REG_NR50] = (uint8_t)r;
            sRegs[CTR_CGB_REG_NR51] = (uint8_t)(r >> 8);
            break;
        default: sRegs[CTR_CGB_REG_NR10] = (uint8_t)(r & 0x7F); break;
        }
        assert(CtrCgb_Render(&sCgb, sRegs, out, FRAME) == FRAME);
    }
    sRegs[CTR_CGB_REG_NR52] = 0x00; /* master off stops every channel */
    sRegs[CTR_CGB_REG_SOUNDCNT_H] = 0x0E;
    assert(CtrCgb_Render(&sCgb, sRegs, out, FRAME) == FRAME);
    assert(CtrCgb_Render(&sCgb, sRegs, out, FRAME) == FRAME);
    for (unsigned i = 0; i < 2 * FRAME; ++i)
        assert(out[i] == 0);
    assert(sCgb.blepAcc[0] == 0 && sCgb.blepAcc[1] == 0);
}

static void TestKernel(void)
{
    for (unsigned p = 0; p < CTR_CGB_BLEP_PHASES; ++p)
    {
        int sum = 0;
        for (unsigned j = 0; j < CTR_CGB_BLEP_TAPS; ++j)
            sum += kCtrCgbBlep[p][j];
        assert(sum == 1 << CTR_CGB_BLEP_BITS);
    }
    /* Off again: the point-sampled path, no delay. */
    int16_t out[2 * FRAME];
    Reset(1);
    CtrCgb_SetBandLimited(&sCgb, 0);
    StartPulse(0, 2, 0xF0, 1750);
    assert(CtrCgb_Render(&sCgb, sRegs, out, FRAME) == FRAME);
    assert(out[0] == 3840 || out[0] == -3840);
}

int main(void)
{
    TestKernel();
    TestDelay();
    TestAboveNyquist();
    TestSettledLevels();
    TestChunking();
    TestNoDrift();
    TestAliasing();
    puts("PASS PSG band-limited: exact kernel, delay, above-Nyquist averages, settled levels equal point "
         "sampling, chunking, no drift, aliasing down >= 20 dB");
    return 0;
}
