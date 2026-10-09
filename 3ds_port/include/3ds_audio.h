#ifndef FIRERED_3DS_AUDIO_H
#define FIRERED_3DS_AUDIO_H

#include <stdbool.h>
#include <stdint.h>

#include "3ds_cgb_audio.h"

/* One FireRed sound frame, captured right after SoundMain() has run
 * CgbSound() and mixed the fresh DirectSound slice. Plain fields keep this
 * layer free of the game's SoundInfo layout. */
typedef struct {
    const int8_t *pcm;       /* gSoundInfo.pcmBuffer: FIFO A plane, FIFO B plane */
    unsigned planeSize;      /* PCM_DMA_BUF_SIZE */
    unsigned counter;        /* pcmDmaCounter */
    unsigned period;         /* pcmDmaPeriod */
    unsigned samples;        /* pcmSamplesPerVBlank */
    uint32_t rate;           /* pcmFreq */
} CtrAudioFrame;

/* Called from m4aSoundMain() after SoundMain() on PLATFORM_3DS only:
 * synthesizes the PSG, mixes it with the fresh DirectSound slice and, when
 * NDSP is up, queues the PCM16 frame. Never blocks. */
void CtrAudio_OnSoundMain(const CtrAudioFrame *frame);
/* GBA runs m4aSoundVSync/m4aSoundMain from interrupts, so a lagging main
 * loop never slows music. The ARM11 dispatches them once per game frame.
 * Called once per frame after that dispatch: returns how many additional
 * sound frames (0..3) are owed to real time, only while the NDSP queue is
 * below its target depth so catching up can never overfill it. */
unsigned CtrAudio_CatchUpFrames(void);
/* After HOME/sleep: the time the game was suspended is not owed sound. */
void CtrAudio_ResetCatchUp(void);

/* The game's sound engine (m4a) on another core.
 *
 * On the GBA, m4aSoundVSync and m4aSoundMain run from interrupts. Here the
 * pair can run on a worker thread instead: on a New 3DS on core 2, which the
 * system leaves idle; on an Old 3DS on the system core (core 1) for a share
 * of its time. CtrAudio_Kick waits for the previous frame's mix and hands the
 * worker the next one, so the engine still advances exactly one sound frame
 * per call, while the game thread goes on with its next frame. The game's
 * calls into m4a hold CtrAudio_LockSound (3ds_sound.c) so they never meet a
 * mix halfway. StartWorker returns false (and the caller mixes inline) when
 * no core can be had or CTR_AUDIO_WORKER is 0; Kick returns false once the
 * system core proves too slow, and the caller mixes inline from then on. */
bool CtrAudio_StartWorker(void (*mix)(void));
bool CtrAudio_Kick(void);
/* Fast-forward: the game frames that are not shown skip the sound engine
 * (3ds_sound.c), so music and effects keep their pace. Game thread only. */
void CtrAudio_SetSkipSound(bool skip);
bool CtrAudio_SkipSound(void);
/* Waits for the mix in flight; idle without a worker. Recursive. */
void CtrAudio_LockSound(void);
void CtrAudio_UnlockSound(void);

/* NDSP output, owned by the ARM11 entry point. Init returns 0 (after one
 * log line) when NDSP/DSP firmware is unavailable; the game then runs
 * silently. Shutdown is idempotent and safe after a partial init. */
int CtrAudio_Init(void);
void CtrAudio_Shutdown(void);

typedef struct {
    uint32_t frames;     /* m4a frames queued to NDSP */
    uint32_t queued;     /* wave buffers pending after the last submit */
    uint32_t maxQueued;  /* deepest queue seen in the current stats window */
    uint32_t underruns;  /* queue found empty after playback had started */
    uint32_t drops;      /* frames discarded because every buffer was busy */
    uint32_t peak;       /* loudest PCM16 sample in the current window */
    uint32_t rateMilliHz;/* NDSP playback rate after queue-depth trim */
} CtrAudioStats;

const CtrAudioStats *CtrAudio_Stats(void);

const CtrCgbStats *CtrAudio_CgbStats(void);
/* Peak absolute PSG sample of the last synthesized frame (PCM16 scale). */
unsigned CtrAudio_PsgPeak(void);

#endif
