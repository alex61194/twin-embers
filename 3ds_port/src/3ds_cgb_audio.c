/* Portable GBA PSG synthesizer. Channel model, frame sequencer rates and
 * sweep/envelope/length/LFSR rules follow ZallaxDev's cgb_audio.c
 * (pokeemerald-3Ds-dualscreen 1f3812d, MIT), reworked to integer math and
 * to FireRed's register shadow; see 3ds_cgb_audio.h. */
#include <string.h>

#include "3ds_cgb_audio.h"
#include "3ds_cgb_blep.h"

_Static_assert((int)CTR_CGB_BLEP_SPAN == (int)CTR_CGB_BLEP_TAPS, "kernel span");

static const uint8_t sNrx1[CTR_CGB_CHANNELS] = {
    CTR_CGB_REG_NR11, CTR_CGB_REG_NR21, CTR_CGB_REG_NR31, CTR_CGB_REG_NR41};
static const uint8_t sNrx2[CTR_CGB_CHANNELS] = {
    CTR_CGB_REG_NR12, CTR_CGB_REG_NR22, CTR_CGB_REG_NR32, CTR_CGB_REG_NR42};
static const uint8_t sNrx3[CTR_CGB_CHANNELS] = {
    CTR_CGB_REG_NR13, CTR_CGB_REG_NR23, CTR_CGB_REG_NR33, CTR_CGB_REG_NR43};
static const uint8_t sNrx4[CTR_CGB_CHANNELS] = {
    CTR_CGB_REG_NR14, CTR_CGB_REG_NR24, CTR_CGB_REG_NR34, CTR_CGB_REG_NR44};

/* Duty 12.5/25/50/75 %, step 0 first (MSB). */
static const uint8_t sDuty[4] = {0x01, 0x81, 0x87, 0x7E};

/* NR32: bit 7 forces 75 %; bits 5-6 select mute/100/50/25 %. In quarters. */
static unsigned WaveGain(const uint8_t *regs)
{
    static const uint8_t kGain[4] = {0, 4, 2, 1};
    uint8_t nr32 = regs[CTR_CGB_REG_NR32];

    if (nr32 & 0x80)
        return 3;
    return kGain[(nr32 >> 5) & 3];
}

static unsigned Freq(const uint8_t *regs, unsigned n)
{
    return regs[sNrx3[n]] | ((regs[sNrx4[n]] & 7u) << 8);
}

static int DacOn(const uint8_t *regs, unsigned n)
{
    if (n == 2)
        return (regs[CTR_CGB_REG_NR30] & 0x80) != 0;
    return (regs[sNrx2[n]] & 0xF8) != 0;
}

static void Stop(CtrCgb *cgb, unsigned n)
{
    CtrCgbChannel *ch = &cgb->ch[n];

    if (ch->enabled && ch->sounding)
        ++cgb->stats.stops[n];
    ch->enabled = 0;
    ch->sounding = 0;
}

static unsigned SweepCalc(const CtrCgb *cgb)
{
    unsigned delta = cgb->sweepFreq >> cgb->sweepShift;

    return cgb->sweepNeg ? cgb->sweepFreq - delta : cgb->sweepFreq + delta;
}

/* Phase step for a waveform whose full cycle runs at base/(2048-f) Hz. */
static uint32_t PhaseStep(uint32_t base, unsigned freq, uint32_t rate)
{
    return (uint32_t)(((uint64_t)base << 32) / ((uint64_t)(2048u - freq) * rate));
}

static void UpdateStep(CtrCgb *cgb, const uint8_t *regs, unsigned n)
{
    uint32_t rate = cgb->sampleRate;

    if (n < 2)
        cgb->ch[n].step = PhaseStep(131072u, Freq(regs, n), rate);
    else if (n == 2)
        cgb->ch[n].step = PhaseStep(65536u, Freq(regs, n), rate);
    else
    {
        /* 524288 / r / 2^(s+1) Hz, r = 0 counts as 0.5. */
        unsigned nr43 = regs[CTR_CGB_REG_NR43];
        unsigned r = nr43 & 7, s = nr43 >> 4;
        cgb->ch[3].step = r ? 262144u / (r << s) : 524288u >> s;
    }
    /* Band-limited mode turns a crossing's phase distance into a kernel
     * phase with one multiply: ARMv6K has no integer divider. */
    cgb->ch[n].blepScale = n < 3 && cgb->ch[n].step
        ? (float)CTR_CGB_BLEP_PHASES / (float)cgb->ch[n].step : 0.0f;
}

static void Trigger(CtrCgb *cgb, uint8_t *regs, unsigned n)
{
    CtrCgbChannel *ch = &cgb->ch[n];
    unsigned max = n == 2 ? 256 : 64;
    uint8_t len = n == 2 ? regs[sNrx1[n]] : (uint8_t)(regs[sNrx1[n]] & 0x3F);
    int dac = DacOn(regs, n);
    int audible;

    /* FireRed writes NRx1 only at note start; its per-frame envelope
     * retriggers must not restart a running length counter. */
    if (!ch->enabled || !ch->sounding || len != ch->latchedLen)
    {
        ch->length = (uint16_t)(max - len);
        ch->latchedLen = len;
    }
    if (ch->length == 0)
        ch->length = (uint16_t)max;

    if (n == 2)
    {
        ch->phase = 0;
        audible = WaveGain(regs) != 0;
    }
    else
    {
        uint8_t nrx2 = regs[sNrx2[n]];
        ch->volume = nrx2 >> 4;
        ch->envDir = (nrx2 >> 3) & 1;
        ch->envPeriod = nrx2 & 7;
        ch->envTimer = ch->envPeriod ? ch->envPeriod : 8;
        audible = ch->volume != 0 || (ch->envDir && ch->envPeriod);
    }
    if (n == 3)
    {
        cgb->lfsr = 0x7FFF;
        cgb->noiseAcc = 0;
    }

    ch->enabled = (uint8_t)dac;
    if (n == 0 && dac)
    {
        uint8_t nr10 = regs[CTR_CGB_REG_NR10];
        cgb->sweepFreq = (uint16_t)Freq(regs, 0);
        cgb->sweepPeriod = (nr10 >> 4) & 7;
        cgb->sweepNeg = (nr10 >> 3) & 1;
        cgb->sweepShift = nr10 & 7;
        cgb->sweepTimer = cgb->sweepPeriod ? cgb->sweepPeriod : 8;
        cgb->sweepEnabled = cgb->sweepPeriod || cgb->sweepShift;
        if (cgb->sweepShift && SweepCalc(cgb) > 2047)
            ch->enabled = 0;
    }

    if (ch->enabled && dac && audible)
    {
        ++cgb->stats.starts[n];
        ch->sounding = 1;
    }
    else
    {
        if (ch->sounding)
            ++cgb->stats.stops[n];
        ch->sounding = 0;
    }
}

static void ClockLength(CtrCgb *cgb, const uint8_t *regs)
{
    for (unsigned n = 0; n < CTR_CGB_CHANNELS; ++n)
    {
        CtrCgbChannel *ch = &cgb->ch[n];
        if ((regs[sNrx4[n]] & 0x40) && ch->length && --ch->length == 0)
            Stop(cgb, n);
    }
}

static void ClockSweep(CtrCgb *cgb, uint8_t *regs)
{
    if (!cgb->ch[0].enabled || --cgb->sweepTimer != 0)
        return;
    cgb->sweepTimer = cgb->sweepPeriod ? cgb->sweepPeriod : 8;
    if (!cgb->sweepEnabled || !cgb->sweepPeriod)
        return;

    unsigned next = SweepCalc(cgb);
    if (next > 2047)
    {
        Stop(cgb, 0);
        return;
    }
    if (cgb->sweepShift)
    {
        cgb->sweepFreq = (uint16_t)next;
        regs[CTR_CGB_REG_NR13] = (uint8_t)next;
        regs[CTR_CGB_REG_NR14] = (uint8_t)((regs[CTR_CGB_REG_NR14] & 0xF8) | (next >> 8));
        UpdateStep(cgb, regs, 0);
        ++cgb->stats.sweepUpdates;
        if (SweepCalc(cgb) > 2047)
            Stop(cgb, 0);
    }
}

static void ClockEnvelope(CtrCgb *cgb)
{
    for (unsigned n = 0; n < CTR_CGB_CHANNELS; ++n)
    {
        CtrCgbChannel *ch = &cgb->ch[n];
        if (n == 2 || !ch->envPeriod || --ch->envTimer != 0)
            continue;
        ch->envTimer = ch->envPeriod;
        if (ch->envDir && ch->volume < 15)
            ++ch->volume;
        else if (!ch->envDir && ch->volume > 0)
            --ch->volume;
    }
}

/* 512 Hz: length on even steps, sweep on 2 and 6, envelope on 7. */
static void Sequence(CtrCgb *cgb, uint8_t *regs)
{
    unsigned step = cgb->seqStep;

    if ((step & 1) == 0)
        ClockLength(cgb, regs);
    if (step == 2 || step == 6)
        ClockSweep(cgb, regs);
    if (step == 7)
        ClockEnvelope(cgb);
    cgb->seqStep = (uint8_t)((step + 1) & 7);
}

static void LoadWave(CtrCgb *cgb, const uint8_t *regs)
{
    const uint8_t *ram = regs + CTR_CGB_REG_WAVE_RAM;

    if (memcmp(cgb->waveRaw, ram, sizeof(cgb->waveRaw)) == 0)
        return;
    memcpy(cgb->waveRaw, ram, sizeof(cgb->waveRaw));
    for (unsigned i = 0; i < 16; ++i)
    {
        cgb->wave[2 * i] = (int8_t)(2 * (ram[i] >> 4) - 15);
        cgb->wave[2 * i + 1] = (int8_t)(2 * (ram[i] & 15) - 15);
    }
    ++cgb->stats.waveUpdates;
}

void CtrCgb_Init(CtrCgb *cgb, uint32_t sampleRate)
{
    memset(cgb, 0, sizeof(*cgb));
    cgb->sampleRate = sampleRate;
    cgb->lfsr = 0x7FFF;
    /* Power-on wave RAM is all zero nibbles. */
    for (unsigned i = 0; i < 32; ++i)
        cgb->wave[i] = -15;
}

/* Channel output in eighths of a half DAC step, before routing/master. */
static int ChannelSample(CtrCgb *cgb, const uint8_t *regs, unsigned n,
                         unsigned waveGain)
{
    CtrCgbChannel *ch = &cgb->ch[n];
    int out;

    if (n < 2)
    {
        unsigned duty = regs[sNrx1[n]] >> 6;
        unsigned bit = (sDuty[duty] >> (7 - (ch->phase >> 29))) & 1;
        out = (bit ? ch->volume : -(int)ch->volume) * 8;
        ch->phase += ch->step;
    }
    else if (n == 2)
    {
        out = cgb->wave[ch->phase >> 27] * 2 * (int)waveGain;
        ch->phase += ch->step;
    }
    else
    {
        /* Average every LFSR state inside this sample. */
        int sum = (cgb->lfsr & 1) ? -1 : 1;
        int count = 1;
        int width7 = (regs[CTR_CGB_REG_NR43] & 0x08) != 0;
        cgb->noiseAcc += ch->step;
        while (cgb->noiseAcc >= cgb->sampleRate)
        {
            unsigned bit = (cgb->lfsr ^ (cgb->lfsr >> 1)) & 1;
            cgb->noiseAcc -= cgb->sampleRate;
            cgb->lfsr = (uint16_t)((cgb->lfsr >> 1) | (bit << 14));
            if (width7)
                cgb->lfsr = (uint16_t)((cgb->lfsr & ~0x40u) | (bit << 6));
            sum += (cgb->lfsr & 1) ? -1 : 1;
            ++count;
        }
        out = sum * (int)ch->volume * 8 / count;
    }
    return out;
}

void CtrCgb_SetBandLimited(CtrCgb *cgb, int on)
{
    if (cgb == NULL)
        return;
    cgb->bandLimited = on != 0;
    memset(cgb->levelL, 0, sizeof(cgb->levelL));
    memset(cgb->levelR, 0, sizeof(cgb->levelR));
    memset(cgb->blepAcc, 0, sizeof(cgb->blepAcc));
    memset(cgb->blep, 0, sizeof(cgb->blep));
}

/* ── Band-limited rendering ─────────────────────────────────────────────
 *
 * The register model is the one above; only the output differs. Each
 * channel's routed level (before the PSG ratio shift) is tracked, and every
 * change, at the start of a sample or at the exact fraction where a duty or
 * wave step falls inside it, adds a band-limited step (3ds_cgb_blep.h) to a
 * delta buffer whose running sum is the output. A channel whose fundamental
 * is above Nyquist is rendered as its average: every harmonic would alias.
 * Noise keeps its per-sample average (already a box filter) and enters as
 * steps at sample starts. */

typedef struct {
    CtrCgb *cgb;
    uint8_t nr51;
    int masterL, masterR;
} Blep;

static void Emit(const Blep *b, unsigned n, unsigned i, unsigned kernelPhase, int level)
{
    CtrCgb *cgb = b->cgb;
    int32_t l = (b->nr51 & (0x10u << n)) ? level * b->masterL * 4 : 0;
    int32_t r = (b->nr51 & (0x01u << n)) ? level * b->masterR * 4 : 0;
    int32_t dl = l - cgb->levelL[n], dr = r - cgb->levelR[n];

    if (dl == 0 && dr == 0)
        return;
    cgb->levelL[n] = l;
    cgb->levelR[n] = r;
    const int16_t *k = kCtrCgbBlep[kernelPhase];
    int32_t *dst = cgb->blep + 2 * i;
    if (dl == dr)
    {
        /* Centred and equal on both sides (the common NR51/NR50 setup). */
        for (unsigned j = 0; j < CTR_CGB_BLEP_TAPS; ++j)
        {
            int32_t v = dl * k[j];
            dst[2 * j] += v;
            dst[2 * j + 1] += v;
        }
        return;
    }
    for (unsigned j = 0; j < CTR_CGB_BLEP_TAPS; ++j)
    {
        dst[2 * j] += dl * k[j];
        dst[2 * j + 1] += dr * k[j];
    }
}

static unsigned KernelPhase(const CtrCgbChannel *ch, uint32_t consumed)
{
    unsigned p = (unsigned)((float)consumed * ch->blepScale);
    return p < CTR_CGB_BLEP_PHASES ? p : CTR_CGB_BLEP_PHASES - 1;
}

static int PulseLevel(const CtrCgbChannel *ch, uint8_t pattern)
{
    unsigned bit = (pattern >> (7 - (ch->phase >> 29))) & 1;
    return (bit ? ch->volume : -(int)ch->volume) * 8;
}

static void BlepPulse(const Blep *b, uint8_t *regs, unsigned n, unsigned i)
{
    CtrCgbChannel *ch = &b->cgb->ch[n];
    uint8_t pattern = sDuty[regs[sNrx1[n]] >> 6];

    if (ch->step > 0x80000000u)
    {
        /* Above Nyquist: the duty cycle's average, (high - low) of 8. */
        Emit(b, n, i, 0, (int)ch->volume * (2 * __builtin_popcount(pattern) - 8));
        ch->phase += ch->step;
        return;
    }
    Emit(b, n, i, 0, PulseLevel(ch, pattern));
    /* Duty steps are 2^29 of phase apart: at most four in one sample. */
    uint32_t remaining = ch->step, consumed = 0;
    for (;;)
    {
        uint32_t dist = 0x20000000u - (ch->phase & 0x1FFFFFFFu);
        if (dist > remaining)
        {
            ch->phase += remaining;
            return;
        }
        ch->phase += dist;
        remaining -= dist;
        consumed += dist;
        Emit(b, n, i, KernelPhase(ch, consumed), PulseLevel(ch, pattern));
    }
}

static void BlepWave(const Blep *b, unsigned i, unsigned waveGain)
{
    CtrCgb *cgb = b->cgb;
    CtrCgbChannel *ch = &cgb->ch[2];
    int gain = 2 * (int)waveGain;

    if (ch->step > 0x80000000u)
    {
        int sum = 0;
        for (unsigned k = 0; k < 32; ++k)
            sum += cgb->wave[k];
        Emit(b, 2, i, 0, sum * gain / 32);
        ch->phase += ch->step;
        return;
    }
    Emit(b, 2, i, 0, cgb->wave[ch->phase >> 27] * gain);
    /* Wave steps are 2^27 apart: at most sixteen in one sample. */
    uint32_t remaining = ch->step, consumed = 0;
    for (;;)
    {
        uint32_t dist = 0x08000000u - (ch->phase & 0x07FFFFFFu);
        if (dist > remaining)
        {
            ch->phase += remaining;
            return;
        }
        ch->phase += dist;
        remaining -= dist;
        consumed += dist;
        Emit(b, 2, i, KernelPhase(ch, consumed), cgb->wave[ch->phase >> 27] * gain);
    }
}

static int32_t Clamp16(int32_t v)
{
    return v > 32767 ? 32767 : v < -32768 ? -32768 : v;
}

/* frames <= CTR_CGB_BLEP_CHUNK. */
static void RenderBandLimited(CtrCgb *cgb, uint8_t *regs, int16_t *out, unsigned frames,
                              const Blep *b, int shift, unsigned waveGain)
{
    for (unsigned i = 0; i < frames; ++i)
    {
        cgb->seqAcc += 512;
        if (cgb->seqAcc >= cgb->sampleRate)
        {
            cgb->seqAcc -= cgb->sampleRate;
            Sequence(cgb, regs);
        }
        for (unsigned n = 0; n < CTR_CGB_CHANNELS; ++n)
        {
            if (!cgb->ch[n].enabled)
                Emit(b, n, i, 0, 0);
            else if (n < 2)
                BlepPulse(b, regs, n, i);
            else if (n == 2)
                BlepWave(b, i, waveGain);
            else
                Emit(b, n, i, 0, ChannelSample(cgb, regs, n, waveGain));
        }
    }
    for (unsigned i = 0; i < frames; ++i)
    {
        cgb->blepAcc[0] += cgb->blep[2 * i];
        cgb->blepAcc[1] += cgb->blep[2 * i + 1];
        out[2 * i] = (int16_t)Clamp16(cgb->blepAcc[0] >> (CTR_CGB_BLEP_BITS + shift));
        out[2 * i + 1] = (int16_t)Clamp16(cgb->blepAcc[1] >> (CTR_CGB_BLEP_BITS + shift));
    }
    /* Carry the kernel tails that reach past this call; clear behind them. */
    memmove(cgb->blep, cgb->blep + 2 * frames, sizeof(int32_t) * 2 * CTR_CGB_BLEP_TAPS);
    memset(cgb->blep + 2 * CTR_CGB_BLEP_TAPS, 0, sizeof(int32_t) * 2 * frames);
}

unsigned CtrCgb_Render(CtrCgb *cgb, uint8_t *regs, int16_t *out, unsigned frames)
{
    if (cgb == NULL || regs == NULL || out == NULL || cgb->sampleRate < 1024)
        return 0;

    LoadWave(cgb, regs);
    for (unsigned n = 0; n < CTR_CGB_CHANNELS; ++n)
    {
        if (regs[sNrx4[n]] & 0x80)
        {
            regs[sNrx4[n]] &= 0x7F;
            Trigger(cgb, regs, n);
        }
        if (!DacOn(regs, n))
            Stop(cgb, n);
        UpdateStep(cgb, regs, n);
    }

    int power = (regs[CTR_CGB_REG_NR52] & 0x80) != 0;
    if (!power)
        for (unsigned n = 0; n < CTR_CGB_CHANNELS; ++n)
            Stop(cgb, n);

    uint8_t nr50 = regs[CTR_CGB_REG_NR50];
    uint8_t nr51 = regs[CTR_CGB_REG_NR51];
    int masterL = ((nr50 >> 4) & 7) + 1;
    int masterR = (nr50 & 7) + 1;
    unsigned ratio = regs[CTR_CGB_REG_SOUNDCNT_H] & 3;
    /* SOUNDCNT_H bits 0-1: 25 %, 50 %, 100 %, (3 prohibited -> 25 %).
     * Shift instead of divide: ARMv6K has no hardware divider. */
    int shift = ratio == 2 ? 0 : ratio == 1 ? 1 : 2;
    unsigned waveGain = WaveGain(regs);

    if (cgb->bandLimited)
    {
        Blep b = {cgb, nr51, masterL, masterR};
        for (unsigned done = 0; done < frames;)
        {
            unsigned n = frames - done < CTR_CGB_BLEP_CHUNK ? frames - done : CTR_CGB_BLEP_CHUNK;
            RenderBandLimited(cgb, regs, out + 2 * done, n, &b, shift, waveGain);
            done += n;
        }
        return frames;
    }

    for (unsigned i = 0; i < frames; ++i)
    {
        int left = 0, right = 0;

        cgb->seqAcc += 512;
        if (cgb->seqAcc >= cgb->sampleRate)
        {
            cgb->seqAcc -= cgb->sampleRate;
            Sequence(cgb, regs);
        }
        for (unsigned n = 0; n < CTR_CGB_CHANNELS; ++n)
        {
            if (!cgb->ch[n].enabled)
                continue;
            int s = ChannelSample(cgb, regs, n, waveGain);
            if (nr51 & (0x10u << n))
                left += s;
            if (nr51 & (0x01u << n))
                right += s;
        }
        /* eighths of a half step -> PCM16 (x32 per half step). */
        out[2 * i] = (int16_t)((left * masterL * 4) >> shift);
        out[2 * i + 1] = (int16_t)((right * masterR * 4) >> shift);
    }
    return frames;
}
