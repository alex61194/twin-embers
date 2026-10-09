#ifndef FIRERED_3DS_AUDIO_MIX_H
#define FIRERED_3DS_AUDIO_MIX_H

#include <stdint.h>

/* SOUNDCNT_H DirectSound bits (FireRed SoundInit: A right, B left, both
 * full volume, PSG 100 %). */
enum {
    CTR_MIX_A_FULL = 0x0004,
    CTR_MIX_B_FULL = 0x0008,
    CTR_MIX_A_RIGHT = 0x0100,
    CTR_MIX_A_LEFT = 0x0200,
    CTR_MIX_B_RIGHT = 0x1000,
    CTR_MIX_B_LEFT = 0x2000,
};

/* Mixes one m4a frame into interleaved PCM16 stereo (L, R).
 * fifoA/fifoB: signed 8-bit DirectSound slices (either may be NULL).
 * psg: interleaved PCM16-scale PSG from CtrCgb_Render (may be NULL).
 * DirectSound uses the GBA DAC scale shared with the PSG: one 8-bit step is
 * 2 DAC steps at 100 % (128 PCM16 units), 1 step at 50 % (64 units). The
 * int32 sum is clamped to [-32768, 32767]. Returns the peak |sample|. */
unsigned CtrAudio_Mix(const int8_t *fifoA, const int8_t *fifoB, uint16_t soundcntH,
                      const int16_t *psg, int16_t *out, unsigned frames);

/* The same mix from the 16-bit DirectSound ring (3ds_m4a_mixer.c), whose
 * samples are the 8-bit ones scaled by 256 with the precision the GBA drops:
 * a 16-bit sample v contributes (v * gain) >> 8, exactly v8 * gain when v
 * carries no fraction (v = 256 * v8). */
unsigned CtrAudio_Mix16(const int16_t *fifoA, const int16_t *fifoB, uint16_t soundcntH,
                        const int16_t *psg, int16_t *out, unsigned frames);

#endif
