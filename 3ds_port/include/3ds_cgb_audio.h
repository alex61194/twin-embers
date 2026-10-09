#ifndef FIRERED_3DS_CGB_AUDIO_H
#define FIRERED_3DS_CGB_AUDIO_H

#include <stdint.h>

/* Portable GBA PSG (CGB channels 1-4), adapted from ZallaxDev's
 * cgb_audio.c (pokeemerald-3Ds-dualscreen 1f3812d, MIT) to FireRed's
 * ARM11 register shadow. FireRed's CgbSound() stays the sequencer: it writes
 * NR10..NR52 and wave RAM into the shadow once per frame, then
 * CtrCgb_Render() interprets those writes and synthesizes the frame.
 *
 * Events are recovered from the shadow instead of hooks in CgbSound:
 * - NRx4 bit 7 (write-only on hardware) is a trigger; Render consumes and
 *   clears it, so every CgbSound trigger is seen exactly once.
 * - NRx2 / NR30 DAC bits, NR51 routing, NR50 master, SOUNDCNT_H PSG ratio,
 *   duty (NRx1), frequency and NR32 wave volume are read live.
 * - Wave RAM changes are detected by comparison with the last snapshot.
 * Sweep writes its new frequency back into NR13/NR14 like hardware.
 *
 * Output is interleaved stereo int16 on PCM16 scale: GBA DAC full range
 * (+-512 steps) maps to +-32768, so four full PSG channels at master 7 and
 * 100% ratio peak at +-15360, the GBA's real PSG headroom. */

enum {
    CTR_CGB_CHANNELS = 4,
    /* regs[] offsets relative to the GBA I/O base (0x04000000). */
    CTR_CGB_REG_NR10 = 0x60,
    CTR_CGB_REG_NR11 = 0x62,
    CTR_CGB_REG_NR12 = 0x63,
    CTR_CGB_REG_NR13 = 0x64,
    CTR_CGB_REG_NR14 = 0x65,
    CTR_CGB_REG_NR21 = 0x68,
    CTR_CGB_REG_NR22 = 0x69,
    CTR_CGB_REG_NR23 = 0x6C,
    CTR_CGB_REG_NR24 = 0x6D,
    CTR_CGB_REG_NR30 = 0x70,
    CTR_CGB_REG_NR31 = 0x72,
    CTR_CGB_REG_NR32 = 0x73,
    CTR_CGB_REG_NR33 = 0x74,
    CTR_CGB_REG_NR34 = 0x75,
    CTR_CGB_REG_NR41 = 0x78,
    CTR_CGB_REG_NR42 = 0x79,
    CTR_CGB_REG_NR43 = 0x7C,
    CTR_CGB_REG_NR44 = 0x7D,
    CTR_CGB_REG_NR50 = 0x80,
    CTR_CGB_REG_NR51 = 0x81,
    CTR_CGB_REG_SOUNDCNT_H = 0x82,
    CTR_CGB_REG_NR52 = 0x84,
    CTR_CGB_REG_WAVE_RAM = 0x90,
    CTR_CGB_REG_SPAN = 0xA0, /* regs[] must hold at least this many bytes */
};

typedef struct {
    uint32_t starts[CTR_CGB_CHANNELS];  /* audible trigger */
    uint32_t stops[CTR_CGB_CHANNELS];   /* silent trigger, DAC off, length, sweep */
    uint32_t waveUpdates;
    uint32_t sweepUpdates;
} CtrCgbStats;

typedef struct {
    uint8_t enabled;     /* hardware channel-on status (NR52 bit) */
    uint8_t sounding;    /* last trigger was audible and still enabled */
    uint8_t volume;
    uint8_t envDir;
    uint8_t envPeriod;
    uint8_t envTimer;
    uint8_t latchedLen;  /* NRx1 length bits at the last reload */
    uint16_t length;     /* hardware length counter */
    uint32_t phase;      /* pulse: 8 steps, wave: 32 samples per 2^32 */
    uint32_t step;       /* phase step per sample; CH4: LFSR clocks/s */
    float blepScale;     /* kernel phases per unit of phase (band-limited) */
} CtrCgbChannel;

/* Band-limited output: the largest Render call it handles in one pass
 * (longer calls are split), and the kernel span (3ds_cgb_blep.h). */
enum {
    CTR_CGB_BLEP_CHUNK = 1024,
    CTR_CGB_BLEP_SPAN = 18,
};

typedef struct {
    uint32_t sampleRate;
    uint32_t seqAcc;       /* 512 Hz frame sequencer accumulator */
    uint8_t seqStep;
    CtrCgbChannel ch[CTR_CGB_CHANNELS];
    /* CH1 sweep */
    uint16_t sweepFreq;
    uint8_t sweepPeriod;
    uint8_t sweepTimer;
    uint8_t sweepShift;
    uint8_t sweepNeg;
    uint8_t sweepEnabled;
    /* CH3 wave */
    uint8_t waveRaw[16];
    int8_t wave[32];       /* 2*s-15, first nibble high */
    /* CH4 noise */
    uint16_t lfsr;
    uint32_t noiseAcc;
    CtrCgbStats stats;
    /* Band-limited mode (CtrCgb_SetBandLimited). Each level change is
     * placed at its exact fractional time as a band-limited step instead of
     * appearing at the next sample, so square, wave and noise harmonics
     * above the sample rate's Nyquist frequency no longer fold back as
     * aliases. levelL/R hold each channel's routed level before the PSG
     * ratio shift; blep[] holds pending kernel deltas (L, R interleaved),
     * blepAcc the running sums, in units of 2^CTR_CGB_BLEP_BITS. */
    uint8_t bandLimited;
    int32_t levelL[CTR_CGB_CHANNELS], levelR[CTR_CGB_CHANNELS];
    int32_t blepAcc[2];
    int32_t blep[2 * (CTR_CGB_BLEP_CHUNK + CTR_CGB_BLEP_SPAN)];
} CtrCgb;

void CtrCgb_Init(CtrCgb *cgb, uint32_t sampleRate);

/* Off (the default after Init): every channel is point-sampled, exactly as
 * the GBA register model computes it, with no delay. On: the same register
 * model, rendered band-limited; output is delayed by CTR_CGB_BLEP_DELAY
 * samples (0.6 ms at 13379 Hz) and settles on exactly the same levels. */
void CtrCgb_SetBandLimited(CtrCgb *cgb, int on);

/* Synthesizes `frames` stereo frames into out[0 .. 2*frames). `regs` is the
 * GBA I/O register image; trigger bits and sweep frequency are written back.
 * Returns the number of frames written (0 on invalid arguments). */
unsigned CtrCgb_Render(CtrCgb *cgb, uint8_t *regs, int16_t *out, unsigned frames);

#endif
