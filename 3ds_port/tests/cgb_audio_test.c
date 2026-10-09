/* Functional checks for the portable PSG synthesizer. Register images are
 * built exactly as FireRed's CgbSound()/MPlayExtender() write them. */
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_cgb_audio.h"

enum { RATE = 13379, FRAME = 224, GUARD = 64 };

static uint8_t sRegs[0x400];
static CtrCgb sCgb;
static int16_t sOut[2 * FRAME];

/* MPlayExtender + SoundInit: master on, NR50 0x77, PSG ratio 100 %, all
 * channels routed both ways unless a test changes NR51. */
static void Reset(void)
{
    memset(sRegs, 0, sizeof(sRegs));
    sRegs[CTR_CGB_REG_NR52] = 0x8F;
    sRegs[CTR_CGB_REG_NR50] = 0x77;
    sRegs[CTR_CGB_REG_NR51] = 0xFF;
    sRegs[CTR_CGB_REG_SOUNDCNT_H] = 0x0E; /* SOUND_ALL_MIX_FULL */
    CtrCgb_Init(&sCgb, RATE);
}

static void Render(void)
{
    assert(CtrCgb_Render(&sCgb, sRegs, sOut, FRAME) == FRAME);
}

static int Peak(int side)
{
    int peak = 0;
    for (unsigned i = 0; i < FRAME; ++i)
    {
        int v = abs(sOut[2 * i + side]);
        if (v > peak)
            peak = v;
    }
    return peak;
}

/* CH1/CH2 note start: duty in NRx1, env in NRx2, freq, trigger. */
static void StartPulse(unsigned ch, uint8_t duty, uint8_t env, unsigned freq)
{
    sRegs[ch == 0 ? CTR_CGB_REG_NR11 : CTR_CGB_REG_NR21] = (uint8_t)(duty << 6);
    sRegs[ch == 0 ? CTR_CGB_REG_NR12 : CTR_CGB_REG_NR22] = env;
    sRegs[ch == 0 ? CTR_CGB_REG_NR13 : CTR_CGB_REG_NR23] = (uint8_t)freq;
    sRegs[ch == 0 ? CTR_CGB_REG_NR14 : CTR_CGB_REG_NR24] = (uint8_t)(0x80 | (freq >> 8));
}

static void TestSilence(void)
{
    Reset();
    Render();
    for (unsigned i = 0; i < 2 * FRAME; ++i)
        assert(sOut[i] == 0);

    /* CgbOscOff(1): NR12=8 (volume 0, DAC on) + trigger -> silent. */
    StartPulse(0, 2, 0x08, 1750);
    Render();
    assert(Peak(0) == 0 && Peak(1) == 0);
    assert(sCgb.stats.starts[0] == 0);

    /* CH3 DAC off (NR30=0, CgbOscOff(3)) -> silent even with a trigger. */
    sRegs[CTR_CGB_REG_NR32] = 0x20;
    sRegs[CTR_CGB_REG_NR34] = 0x87;
    Render();
    assert(Peak(0) == 0 && Peak(1) == 0);

    /* Master off (NR52 bit 7) silences an audible pulse. */
    StartPulse(1, 2, 0xF0, 1750);
    sRegs[CTR_CGB_REG_NR52] = 0x0F;
    Render();
    assert(Peak(0) == 0 && Peak(1) == 0);
}

static void TestPulse(void)
{
    Reset();
    /* 131072/(2048-1750) = 440 Hz, volume 15, 50 % duty, no envelope. */
    StartPulse(0, 2, 0xF0, 1750);
    Render();
    assert((sRegs[CTR_CGB_REG_NR14] & 0x80) == 0); /* trigger consumed */
    assert(sCgb.stats.starts[0] == 1);
    /* 15 half-steps * 8 (master 7) * 32 = 3840 = one full channel. */
    assert(Peak(0) == 3840 && Peak(1) == 3840);

    /* Both polarities appear and 440 Hz toggles ~2*440*224/13379 times. */
    int pos = 0, neg = 0, edges = 0;
    for (unsigned i = 0; i < FRAME; ++i)
    {
        pos += sOut[2 * i] > 0;
        neg += sOut[2 * i] < 0;
        if (i && (sOut[2 * i] > 0) != (sOut[2 * i - 2] > 0))
            ++edges;
    }
    assert(pos > 90 && neg > 90);
    assert(edges >= 13 && edges <= 16);

    /* Master volume 3 halves the level; PSG ratio 50 % halves again. */
    sRegs[CTR_CGB_REG_NR50] = 0x33;
    Render();
    assert(Peak(0) == 1920);
    sRegs[CTR_CGB_REG_SOUNDCNT_H] = 0x0D;
    Render();
    assert(Peak(0) == 960);
}

static int CountHigh(uint8_t duty)
{
    Reset();
    StartPulse(1, duty, 0xF0, 1920); /* 1024 Hz: many cycles per frame */
    Render();
    int high = 0;
    for (unsigned i = 0; i < FRAME; ++i)
        high += sOut[2 * i] > 0;
    return high;
}

static void TestDuty(void)
{
    int d0 = CountHigh(0), d1 = CountHigh(1), d2 = CountHigh(2), d3 = CountHigh(3);
    /* 12.5 %, 25 %, 50 %, 75 % of 224 samples, within aliasing slack. */
    assert(abs(d0 - 28) <= 8);
    assert(abs(d1 - 56) <= 8);
    assert(abs(d2 - 112) <= 8);
    assert(abs(d3 - 168) <= 8);
    assert(d0 < d1 && d1 < d2 && d2 < d3);
}

static void TestRouting(void)
{
    Reset();
    sRegs[CTR_CGB_REG_NR51] = 0x10; /* CH1 left only */
    StartPulse(0, 2, 0xF0, 1750);
    Render();
    assert(Peak(0) == 3840 && Peak(1) == 0);

    sRegs[CTR_CGB_REG_NR51] = 0x02; /* CH2 right only; CH1 now unrouted */
    StartPulse(1, 2, 0xA0, 1750);
    Render();
    assert(Peak(0) == 0 && Peak(1) == 10 * 8 * 32);

    /* Per-side master volume from NR50 (left 1, right 7). */
    sRegs[CTR_CGB_REG_NR51] = 0x11;
    sRegs[CTR_CGB_REG_NR50] = 0x07;
    Render();
    assert(Peak(0) == 15 * 1 * 32 && Peak(1) == 15 * 8 * 32);
}

static void TestWave(void)
{
    Reset();
    /* Ramp 0..15,15..0 wave: first byte 0x01 is nibbles 0 then 1. */
    static const uint8_t ramp[16] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xAB, 0xCD, 0xEF,
                                     0xFE, 0xDC, 0xBA, 0x98, 0x76, 0x54, 0x32, 0x10};
    memcpy(sRegs + CTR_CGB_REG_WAVE_RAM, ramp, 16);
    /* FireRed CH3 start: NR30 0x80, NR32 gCgb3Vol (0x20 full), trigger.
     * 65536/(2048-2045) Hz is far too fast; use 65536/(2048-1536)=128 Hz. */
    sRegs[CTR_CGB_REG_NR30] = 0x80;
    sRegs[CTR_CGB_REG_NR32] = 0x20;
    sRegs[CTR_CGB_REG_NR33] = 0x00;
    sRegs[CTR_CGB_REG_NR34] = 0x80 | 6;
    Render();
    assert(sCgb.stats.waveUpdates == 1 && sCgb.stats.starts[2] == 1);
    /* First sample reads nibble 0 -> (2*0-15) half steps. */
    assert(sOut[0] == -15 * 8 * 32);
    int peakPos = 0;
    for (unsigned i = 0; i < FRAME; ++i)
        if (sOut[2 * i] > peakPos)
            peakPos = sOut[2 * i];
    assert(peakPos == 15 * 8 * 32); /* nibble 15 reached */

    /* Different wave RAM -> different output; a flat wave of 8s is +1. */
    memset(sRegs + CTR_CGB_REG_WAVE_RAM, 0x88, 16);
    Render();
    assert(sCgb.stats.waveUpdates == 2);
    for (unsigned i = 0; i < FRAME; ++i)
        assert(sOut[2 * i] == 1 * 8 * 32);

    /* NR32 volume codes: 50 % (0x40), 25 % (0x60), 75 % (0x80), mute. */
    memset(sRegs + CTR_CGB_REG_WAVE_RAM, 0xFF, 16);
    sRegs[CTR_CGB_REG_NR32] = 0x40;
    Render();
    assert(sOut[0] == 15 * 8 * 32 / 2);
    sRegs[CTR_CGB_REG_NR32] = 0x60;
    Render();
    assert(sOut[0] == 15 * 8 * 32 / 4);
    sRegs[CTR_CGB_REG_NR32] = 0x80;
    Render();
    assert(sOut[0] == 15 * 8 * 32 * 3 / 4);
    sRegs[CTR_CGB_REG_NR32] = 0x00;
    Render();
    assert(Peak(0) == 0);
}

static void StartNoise(uint8_t nr43, uint8_t env)
{
    sRegs[CTR_CGB_REG_NR42] = env;
    sRegs[CTR_CGB_REG_NR43] = nr43;
    sRegs[CTR_CGB_REG_NR44] = 0x80;
}

static void TestNoise(void)
{
    Reset();
    StartNoise(0x41, 0xF0); /* r=1 s=4: 16384 LFSR clocks/s */
    assert(sCgb.lfsr == 0x7FFF);
    Render();
    assert(sCgb.stats.starts[3] == 1);
    uint16_t after = sCgb.lfsr;
    assert(after != 0x7FFF);
    /* The output is not constant: both signs appear. */
    int pos = 0, neg = 0;
    for (unsigned i = 0; i < FRAME; ++i)
    {
        pos += sOut[2 * i] > 0;
        neg += sOut[2 * i] < 0;
    }
    assert(pos > 0 && neg > 0);

    /* Reference LFSR: 15-bit, 16384*224/13379 = 274 clocks this frame. */
    uint16_t ref = 0x7FFF;
    for (unsigned i = 0; i < 274; ++i)
    {
        unsigned bit = (ref ^ (ref >> 1)) & 1;
        ref = (uint16_t)((ref >> 1) | (bit << 14));
    }
    assert(after == ref);

    /* Width 7 (NR43 bit 3) repeats every 127 clocks. */
    Reset();
    StartNoise(0x08 | 0x41, 0xF0);
    Render();
    uint16_t w7 = sCgb.lfsr;
    ref = 0x7FFF;
    for (unsigned i = 0; i < 274; ++i)
    {
        unsigned bit = (ref ^ (ref >> 1)) & 1;
        ref = (uint16_t)((ref >> 1) | (bit << 14));
        ref = (uint16_t)((ref & ~0x40u) | (bit << 6));
    }
    assert(w7 == ref);
}

static void TestTrigger(void)
{
    Reset();
    /* Envelope 0xF1: volume 15, decrease every 1/64 s (3.5 steps/frame). */
    StartPulse(0, 2, 0xF1, 1750);
    Render();
    Render();
    assert(sCgb.ch[0].volume < 15);
    /* Retrigger (FireRed's per-step NRx4|0x80) reloads the volume. */
    sRegs[CTR_CGB_REG_NR14] |= 0x80;
    Render();
    assert(sCgb.ch[0].volume >= 11 && sCgb.stats.starts[0] == 2);

    /* Noise retrigger reseeds the LFSR. */
    StartNoise(0x31, 0xF0);
    Render();
    assert(sCgb.lfsr != 0x7FFF);
    sRegs[CTR_CGB_REG_NR44] = 0x80;
    Render();
    uint16_t again = sCgb.lfsr;
    Reset();
    StartNoise(0x31, 0xF0);
    Render();
    assert(again == sCgb.lfsr);

    /* Wave retrigger restarts at sample 0. */
    Reset();
    sRegs[CTR_CGB_REG_WAVE_RAM] = 0x0F;
    sRegs[CTR_CGB_REG_NR30] = 0x80;
    sRegs[CTR_CGB_REG_NR32] = 0x20;
    sRegs[CTR_CGB_REG_NR34] = 0x80 | 6;
    Render();
    uint32_t phase = sCgb.ch[2].phase;
    assert(phase != 0);
    Render();
    assert(sCgb.ch[2].phase != phase);
    sRegs[CTR_CGB_REG_NR34] = 0x80 | 6;
    Render();
    assert(sCgb.ch[2].phase == phase);

    /* Length: NRx4 bit 6 with NR21 length 63 -> 1/256 s, then off. */
    Reset();
    sRegs[CTR_CGB_REG_NR21] = 0x80 | 63;
    sRegs[CTR_CGB_REG_NR22] = 0xF0;
    sRegs[CTR_CGB_REG_NR23] = 0x00;
    sRegs[CTR_CGB_REG_NR24] = 0xC0 | 7;
    Render();
    assert(!sCgb.ch[1].enabled && sCgb.stats.stops[1] == 1);
    Render();
    assert(Peak(0) == 0);

    /* Sweep up: shift 1 period 1 overflows 1500 within a frame; the swept
     * frequency is written back to NR13/NR14 first. */
    Reset();
    sRegs[CTR_CGB_REG_NR10] = 0x11;
    StartPulse(0, 2, 0xF0, 1000);
    Render();
    assert(sCgb.stats.sweepUpdates >= 1);
    assert((sRegs[CTR_CGB_REG_NR13] | ((sRegs[CTR_CGB_REG_NR14] & 7) << 8)) == 1500);
    assert(!sCgb.ch[0].enabled && sCgb.stats.stops[0] == 1);

    /* Sweep down never overflows and keeps lowering the frequency. */
    Reset();
    sRegs[CTR_CGB_REG_NR10] = 0x19;
    StartPulse(0, 2, 0xF0, 1024);
    Render();
    unsigned f = sRegs[CTR_CGB_REG_NR13] | ((sRegs[CTR_CGB_REG_NR14] & 7) << 8);
    assert(f < 1024 && sCgb.ch[0].enabled);
}

static void TestBounds(void)
{
    static int16_t buf[2 * FRAME + 2 * GUARD];
    Reset();
    StartPulse(0, 2, 0xF0, 1750);
    StartNoise(0x00, 0xF0); /* fastest noise, 39 clocks per sample */
    sRegs[CTR_CGB_REG_NR30] = 0x80;
    sRegs[CTR_CGB_REG_NR32] = 0x80;
    sRegs[CTR_CGB_REG_NR34] = 0x87; /* 2047: fastest wave */
    for (unsigned i = 0; i < sizeof(buf) / sizeof(buf[0]); ++i)
        buf[i] = 0x5A5A;
    for (unsigned frame = 0; frame < 120; ++frame)
        assert(CtrCgb_Render(&sCgb, sRegs, buf + GUARD, FRAME) == FRAME);
    for (unsigned i = 0; i < GUARD; ++i)
    {
        assert(buf[i] == 0x5A5A);
        assert(buf[GUARD + 2 * FRAME + i] == 0x5A5A);
    }
    /* Only the documented registers are touched. */
    uint8_t copy[sizeof(sRegs)];
    memcpy(copy, sRegs, sizeof(copy));
    CtrCgb_Render(&sCgb, sRegs, buf + GUARD, FRAME);
    for (unsigned i = 0; i < sizeof(copy); ++i)
        if (i < CTR_CGB_REG_NR10 || i >= CTR_CGB_REG_SPAN)
            assert(copy[i] == sRegs[i]);
    /* Invalid arguments write nothing. */
    assert(CtrCgb_Render(&sCgb, sRegs, NULL, FRAME) == 0);
    assert(CtrCgb_Render(NULL, sRegs, buf, FRAME) == 0);
}

static void TestFrameCount(void)
{
    Reset();
    StartPulse(1, 2, 0xF0, 1750);
    /* Exactly 224 frames per tick, and 512 Hz sequencer steps accrue at
     * the FireRed rate: 60 ticks -> 60*224*512/13379 = 514 steps. */
    unsigned before = sCgb.seqStep;
    for (unsigned i = 0; i < 60; ++i)
        Render();
    assert((unsigned)((before + 514) & 7) == sCgb.seqStep);
    int16_t tail[2 * FRAME + 2];
    tail[2 * FRAME] = 0x1234;
    tail[2 * FRAME + 1] = 0x4321;
    assert(CtrCgb_Render(&sCgb, sRegs, tail, FRAME) == FRAME);
    assert(tail[2 * FRAME] == 0x1234 && tail[2 * FRAME + 1] == 0x4321);
}

int main(void)
{
    TestSilence();
    TestPulse();
    TestDuty();
    TestRouting();
    TestWave();
    TestNoise();
    TestTrigger();
    TestBounds();
    TestFrameCount();
    return 0;
}
