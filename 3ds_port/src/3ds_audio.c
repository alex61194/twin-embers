/* ARM11 audio frame owner. FireRed's m4a engine stays authoritative: each
 * SoundMain() frame yields one DirectSound slice (the proven pcmDmaCounter
 * cadence) plus CgbSound's PSG registers. This layer synthesizes the PSG,
 * mixes both into interleaved PCM16 and queues it on one NDSP channel.
 *
 * Queue: CTR_AUDIO_BUFFERS fixed linear-memory wave buffers, reused round
 * robin; no allocation per frame, no waiting. The game's frame pacing (3DS
 * VBlank) never matches the sound engine's rate/samples clock exactly, so playback
 * rate is trimmed by at most 1 % towards CTR_AUDIO_TARGET_DEPTH (Zallax
 * 3ds_audio.c). The trim has an integral term: hardware showed a pure
 * proportional trim settling one buffer short of target (queued=2, rate
 * 13345.6 Hz) with a periodic underrun. A full queue drops the frame; an
 * empty queue after playback started counts an underrun and is re-primed
 * with silence up to the target depth, so one late frame costs one gap
 * instead of a run of them. */
#include <3ds.h>
#include <string.h>

#include "3ds_audio.h"
#include "3ds_audio_cadence.h"
#include "3ds_audio_catchup.h"
#include "3ds_audio_mix.h"
#include "3ds_log.h"
#include "3ds_perf.h"
#include "gba_shadow.h"

enum {
    CTR_AUDIO_MAX_SAMPLES = 1024,
    CTR_AUDIO_BUFFERS = 8,
    CTR_AUDIO_TARGET_DEPTH = 4,   /* ~67 ms: hardware showed 30 ms frame spikes */
    CTR_AUDIO_STATS_PERIOD = 600, /* frames between NDSP stat lines (~10 s) */
    CTR_AUDIO_CHANNEL = 0,
    CTR_AUDIO_CATCHUP_MAX = 3,    /* extra sound frames per game frame */
};
#ifndef CTR_AUDIO_PSG_BLEP
#define CTR_AUDIO_PSG_BLEP 1
#endif
#ifndef CTR_AUDIO_POLYPHASE
#define CTR_AUDIO_POLYPHASE 1
#endif
#define CTR_AUDIO_TRIM     0.0025f
#define CTR_AUDIO_TRIM_MAX 0.0100f
/* Integral gain per submitted frame per buffer of error: a steady one-buffer
 * error moves the rate 0.06 %/s, far below audible pitch drift. */
#define CTR_AUDIO_TRIM_I   0.00001f
#define CTR_AUDIO_TRIM_I_MAX 0.0080f

static CtrCgb sCgb;
static uint32_t sCgbRate;
static int16_t sPsg[2 * CTR_AUDIO_MAX_SAMPLES];
static unsigned sPsgPeak;

static ndspWaveBuf sWave[CTR_AUDIO_BUFFERS];
static int16_t *sSamples;
static unsigned sNext;
static int sNdspUp;
static int sReady;
static int sStarted;
static uint32_t sBaseRate;
static float sRate;
static float sTrimI;
static uint32_t sPsgWindowPeak;
static CtrAudioStats sStats;
static unsigned sStatsFrames;
static CtrAudioCatchUp sCatchUp;
static unsigned sFrameSamples;

int CtrAudio_Init(void)
{
    const size_t bytes = (size_t)CTR_AUDIO_BUFFERS * CTR_AUDIO_MAX_SAMPLES * 2 * sizeof(int16_t);
    Result rc;

    if (sReady)
        return 1;
    rc = ndspInit();
    if (R_FAILED(rc))
    {
        CtrLog_Write(CTR_LOG_AUDIO, "NDSP unavailable (%08lx): no DSP firmware? audio off",
                     (unsigned long)rc);
        return 0;
    }
    sNdspUp = 1;
    sSamples = linearAlloc(bytes);
    if (sSamples == NULL)
    {
        CtrLog_Write(CTR_LOG_AUDIO, "NDSP: no linear memory for %u bytes, audio off",
                     (unsigned)bytes);
        CtrAudio_Shutdown();
        return 0;
    }
    memset(sSamples, 0, bytes);

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
    ndspChnReset(CTR_AUDIO_CHANNEL);
    /* The reported engine rate up to the DSP's 32728 Hz: polyphase instead
     * of linear interpolation, at no CPU cost. CTR_AUDIO_POLYPHASE=0 keeps
     * linear (A/B on hardware). */
    ndspChnSetInterp(CTR_AUDIO_CHANNEL, CTR_AUDIO_POLYPHASE ? NDSP_INTERP_POLYPHASE : NDSP_INTERP_LINEAR);
    ndspChnSetFormat(CTR_AUDIO_CHANNEL, NDSP_FORMAT_STEREO_PCM16);
    float mix[12];
    memset(mix, 0, sizeof(mix));
    mix[0] = 1.0f; /* front left */
    mix[1] = 1.0f; /* front right */
    ndspChnSetMix(CTR_AUDIO_CHANNEL, mix);

    memset(sWave, 0, sizeof(sWave));
    memset(&sStats, 0, sizeof(sStats));
    sNext = 0;
    sStarted = 0;
    sStatsFrames = 0;
    sBaseRate = 0; /* set from gSoundInfo.pcmFreq on the first frame */
    sTrimI = 0.0f;
    memset(&sCatchUp, 0, sizeof(sCatchUp));
    sFrameSamples = 0;
    sReady = 1;
    CtrLog_Write(CTR_LOG_AUDIO, "NDSP ready: stereo PCM16, %u x %u-sample buffers",
                 (unsigned)CTR_AUDIO_BUFFERS, (unsigned)CTR_AUDIO_MAX_SAMPLES);
    return 1;
}

static bool StopWorker(void);

void CtrAudio_Shutdown(void)
{
    /* The worker's stack is heap memory, unmapped at exit: stop it first. */
    if (!StopWorker())
        return; /* stuck: leave what it uses; the process is ending */
    sReady = 0;
    if (sNdspUp)
    {
        ndspChnWaveBufClear(CTR_AUDIO_CHANNEL);
        ndspExit();
        sNdspUp = 0;
    }
    if (sSamples != NULL)
    {
        linearFree(sSamples);
        sSamples = NULL;
    }
}

static unsigned QueuedBuffers(void)
{
    unsigned queued = 0;

    for (unsigned i = 0; i < CTR_AUDIO_BUFFERS; ++i)
        if (sWave[i].status == NDSP_WBUF_QUEUED || sWave[i].status == NDSP_WBUF_PLAYING)
            ++queued;
    return queued;
}

static void SetRate(float rate)
{
    if (rate != sRate)
    {
        sRate = rate;
        ndspChnSetRate(CTR_AUDIO_CHANNEL, rate);
    }
    sStats.rateMilliHz = (uint32_t)(rate * 1000.0f);
}

static void CorrectDrift(unsigned queued)
{
    float error = (float)queued - (float)CTR_AUDIO_TARGET_DEPTH;
    float trim;

    sTrimI += error * CTR_AUDIO_TRIM_I;
    if (sTrimI > CTR_AUDIO_TRIM_I_MAX)
        sTrimI = CTR_AUDIO_TRIM_I_MAX;
    else if (sTrimI < -CTR_AUDIO_TRIM_I_MAX)
        sTrimI = -CTR_AUDIO_TRIM_I_MAX;
    trim = error * CTR_AUDIO_TRIM + sTrimI;
    if (trim > CTR_AUDIO_TRIM_MAX)
        trim = CTR_AUDIO_TRIM_MAX;
    else if (trim < -CTR_AUDIO_TRIM_MAX)
        trim = -CTR_AUDIO_TRIM_MAX;
    SetRate((float)sBaseRate * (1.0f + trim));
}

/* Next round-robin buffer, or NULL while the DSP still owns it. */
static int16_t *ClaimBuffer(void)
{
    ndspWaveBuf *buf = &sWave[sNext];

    if (buf->status == NDSP_WBUF_QUEUED || buf->status == NDSP_WBUF_PLAYING)
        return NULL;
    return sSamples + (size_t)sNext * CTR_AUDIO_MAX_SAMPLES * 2;
}

static void SubmitBuffer(int16_t *data, unsigned samples)
{
    uint64_t perfSubmit = CtrPerf_Begin();
    ndspWaveBuf *buf = &sWave[sNext];

    DSP_FlushDataCache(data, samples * 2 * sizeof(int16_t));
    memset(buf, 0, sizeof(*buf));
    buf->data_vaddr = data;
    buf->nsamples = samples;
    ndspChnWaveBufAdd(CTR_AUDIO_CHANNEL, buf);
    sNext = (sNext + 1) % CTR_AUDIO_BUFFERS;
    CtrPerf_End(PERF_NDSP_SUBMIT, perfSubmit);
}

static void LogStats(void)
{
    if (++sStatsFrames < CTR_AUDIO_STATS_PERIOD)
        return;
    sStatsFrames = 0;
    const CtrCgbStats *psg = &sCgb.stats;
    CtrLog_Write(CTR_LOG_AUDIO,
        "NDSP frames=%lu queued=%lu peakQueued=%lu underruns=%lu drops=%lu peak=%lu rate=%lu.%03lu psgPeak=%lu psgStarts=%lu,%lu,%lu,%lu",
        (unsigned long)sStats.frames, (unsigned long)sStats.queued,
        (unsigned long)sStats.maxQueued, (unsigned long)sStats.underruns,
        (unsigned long)sStats.drops, (unsigned long)sStats.peak,
        (unsigned long)(sStats.rateMilliHz / 1000), (unsigned long)(sStats.rateMilliHz % 1000),
        (unsigned long)sPsgWindowPeak,
        (unsigned long)psg->starts[0], (unsigned long)psg->starts[1],
        (unsigned long)psg->starts[2], (unsigned long)psg->starts[3]);
    sStats.maxQueued = 0;
    sStats.peak = 0;
    sPsgWindowPeak = 0;
}

/* Optional development 16-bit DirectSound ring. The clean-profile image
 * keeps this default and plays the original m4a engine's 8-bit ring. */
const int16_t *CtrM4a_Pcm16(void) __attribute__((weak));
const int16_t *CtrM4a_Pcm16(void)
{
    return NULL;
}

static void QueueFrame(const CtrAudioFrame *frame)
{
    unsigned samples = frame->samples;
    unsigned offset = CtrAudio_PcmSliceOffset(frame->counter, frame->period, samples);
    const uint8_t *regs = gGbaShadow.regs;

    if (offset + samples > frame->planeSize)
        return;
    if (sBaseRate != frame->rate)
    {
        sBaseRate = frame->rate;
        sRate = 0.0f;
        SetRate((float)sBaseRate);
    }

    unsigned queued = QueuedBuffers();
    if (queued == 0)
    {
        if (sStarted)
            ++sStats.underruns;
        /* (Re)prime: silence so the real frame lands at the target depth. */
        while (queued + 1 < CTR_AUDIO_TARGET_DEPTH)
        {
            int16_t *silence = ClaimBuffer();
            if (silence == NULL)
                break;
            memset(silence, 0, samples * 2 * sizeof(int16_t));
            SubmitBuffer(silence, samples);
            ++queued;
        }
    }

    int16_t *out = ClaimBuffer();
    if (out == NULL)
    {
        ++sStats.drops;
        sStats.queued = queued;
        CorrectDrift(queued);
        LogStats();
        return;
    }

    unsigned peak;
    if (regs[CTR_CGB_REG_NR52] & 0x80)
    {
        /* SOUNDCNT_X master enable gates PSG and DirectSound alike. */
        uint16_t soundcntH = (uint16_t)(regs[CTR_CGB_REG_SOUNDCNT_H]
                                        | (regs[CTR_CGB_REG_SOUNDCNT_H + 1] << 8));
        /* Optional 16-bit ring when a development mixer supplies it; the
         * original engine's 8-bit one in the clean image. Same layout. */
        const int16_t *pcm16 = CtrM4a_Pcm16();
        if (pcm16 != NULL)
            peak = CtrAudio_Mix16(pcm16 + offset, pcm16 + frame->planeSize + offset,
                                  soundcntH, sPsg, out, samples);
        else
            peak = CtrAudio_Mix(frame->pcm + offset, frame->pcm + frame->planeSize + offset,
                                soundcntH, sPsg, out, samples);
    }
    else
    {
        memset(out, 0, samples * 2 * sizeof(int16_t));
        peak = 0;
    }
    SubmitBuffer(out, samples);
    ++queued;
    sStarted = 1;

    ++sStats.frames;
    sStats.queued = queued;
    if (queued > sStats.maxQueued)
        sStats.maxQueued = queued;
    if (peak > sStats.peak)
        sStats.peak = peak;
    CorrectDrift(queued);
    LogStats();
}

void CtrAudio_OnSoundMain(const CtrAudioFrame *frame)
{
    unsigned samples = frame->samples;

    if (frame->rate < 1024 || samples == 0 || samples > CTR_AUDIO_MAX_SAMPLES)
        return;
    uint64_t perfBridge = CtrPerf_Begin();
    sFrameSamples = samples;
    /* SampleFreqSet() may change the rate; restart the PSG clock with it. */
    if (sCgbRate != frame->rate)
    {
        CtrCgb_Init(&sCgb, frame->rate);
        /* Band-limited PSG: no aliasing from the square/wave/noise steps
         * (3ds_cgb_audio.h). CTR_AUDIO_PSG_BLEP=0 keeps point sampling. */
        CtrCgb_SetBandLimited(&sCgb, CTR_AUDIO_PSG_BLEP);
        sCgbRate = frame->rate;
    }
    CtrCgb_Render(&sCgb, gGbaShadow.regs, sPsg, samples);

    unsigned peak = 0;
    for (unsigned i = 0; i < 2 * samples; ++i)
    {
        unsigned v = sPsg[i] < 0 ? (unsigned)-sPsg[i] : (unsigned)sPsg[i];
        if (v > peak)
            peak = v;
    }
    sPsgPeak = peak;
    if (peak > sPsgWindowPeak)
        sPsgWindowPeak = peak;

    if (sReady)
        QueueFrame(frame);
    CtrPerf_End(PERF_AUDIO_BRIDGE, perfBridge);
    if (gCtrPerf.enabled)
    {
        gCtrPerf.audioQueued = sStats.queued;
        gCtrPerf.audioRateMilliHz = sStats.rateMilliHz;
        gCtrPerf.audioUnderruns = sStats.underruns;
        gCtrPerf.audioDrops = sStats.drops;
    }
}

unsigned CtrAudio_CatchUpFrames(void)
{
    uint64_t frameTicks;

    if (!sReady || !sStarted || sBaseRate == 0 || sFrameSamples == 0)
        return 0;
    /* One sound frame of real time, from the rate/slice size m4a reported. */
    frameTicks = (uint64_t)sFrameSamples * SYSCLOCK_ARM11 / sBaseRate;
    return CtrAudio_CatchUpPlan(&sCatchUp, CtrPerf_PlatformClock(), frameTicks,
                                QueuedBuffers(), CTR_AUDIO_TARGET_DEPTH,
                                CTR_AUDIO_CATCHUP_MAX);
}

void CtrAudio_ResetCatchUp(void)
{
    /* The next CtrAudio_CatchUpFrames only takes a new baseline. */
    memset(&sCatchUp, 0, sizeof(sCatchUp));
}

const CtrCgbStats *CtrAudio_CgbStats(void)
{
    return &sCgb.stats;
}

unsigned CtrAudio_PsgPeak(void)
{
    return sPsgPeak;
}

const CtrAudioStats *CtrAudio_Stats(void)
{
    return &sStats;
}

/* ── Sound engine worker (see 3ds_audio.h) ───────────────────────────────── */

#ifndef CTR_AUDIO_WORKER
#define CTR_AUDIO_WORKER 1
#endif
/* Old 3DS: the share of the system core lent to the application. */
#define CTR_AUDIO_SYSCORE_PERCENT 50
/* On the system core the mix only gets that share of the time. If the game
 * thread waits for it longer than this on average, it mixes inline again:
 * never slower than without the worker. */
#define CTR_AUDIO_WAIT_LIMIT_MS 1.0f
#define CTR_AUDIO_WAIT_WINDOW 120

static Thread sWorker;
static LightEvent sKick, sIdle;
static RecursiveLock sSoundLock;
static void (*sMix)(void);
static volatile bool sWorkerQuit;
static bool sWorkerInline;
static int sWorkerCore;
static unsigned sKicks;
static uint64_t sWaited;

static void Worker(void *arg)
{
    (void)arg;
    for (;;)
    {
        LightEvent_Wait(&sKick);
        if (sWorkerQuit)
            break;
        RecursiveLock_Lock(&sSoundLock);
        sMix();
        RecursiveLock_Unlock(&sSoundLock);
        LightEvent_Signal(&sIdle);
    }
}

bool CtrAudio_StartWorker(void (*mix)(void))
{
    s32 priority = 0x30;
    int core = 2;

    if (sWorker != NULL)
        return !sWorkerInline;
    if (!CTR_AUDIO_WORKER || mix == NULL)
    {
        CtrLog_Write(CTR_LOG_AUDIO, "sound engine mixes on the game thread (worker off)");
        return false;
    }
    sMix = mix;
    sWorkerQuit = false;
    sWorkerInline = false;
    sKicks = 0;
    sWaited = 0;
    RecursiveLock_Init(&sSoundLock);
    LightEvent_Init(&sKick, RESET_ONESHOT);
    LightEvent_Init(&sIdle, RESET_STICKY);
    LightEvent_Signal(&sIdle);
    svcGetThreadPriority(&priority, CUR_THREAD_HANDLE);
    /* Above the game thread: the game only ever waits on the mix it asked for. */
    priority = priority > 0x18 ? priority - 1 : priority;
    /* Core 2 exists on a New 3DS only; an Old 3DS refuses it. */
    sWorker = threadCreate(Worker, NULL, 32 * 1024, priority, core, false);
    if (sWorker == NULL && R_SUCCEEDED(APT_SetAppCpuTimeLimit(CTR_AUDIO_SYSCORE_PERCENT)))
    {
        core = 1;
        sWorker = threadCreate(Worker, NULL, 32 * 1024, priority, core, false);
    }
    if (sWorker == NULL)
    {
        CtrLog_Write(CTR_LOG_AUDIO, "sound engine mixes on the game thread (no core to spare)");
        return false;
    }
    sWorkerCore = core;
    CtrLog_Write(CTR_LOG_AUDIO, "sound engine mixes on core %d", core);
    return true;
}

bool CtrAudio_Kick(void)
{
    uint64_t start;

    if (sWorker == NULL || sWorkerInline)
        return false;
    start = svcGetSystemTick();
    LightEvent_Wait(&sIdle);
    sWaited += svcGetSystemTick() - start;
    if (++sKicks == CTR_AUDIO_WAIT_WINDOW)
    {
        float waitMs = (float)(sWaited * 1000.0 / SYSCLOCK_ARM11 / sKicks);

        sKicks = 0;
        sWaited = 0;
        if (sWorkerCore == 1 && waitMs > CTR_AUDIO_WAIT_LIMIT_MS)
        {
            /* The worker is idle and stays so: it is never kicked again. */
            sWorkerInline = true;
            CtrLog_Write(CTR_LOG_AUDIO, "sound engine back on the game thread: it waited "
                         "%.2f ms a frame for the system core", (double)waitMs);
            return false;
        }
    }
    LightEvent_Clear(&sIdle);
    LightEvent_Signal(&sKick);
    return true;
}

void CtrAudio_LockSound(void)
{
    if (sWorker != NULL)
        RecursiveLock_Lock(&sSoundLock);
}

void CtrAudio_UnlockSound(void)
{
    if (sWorker != NULL)
        RecursiveLock_Unlock(&sSoundLock);
}

static bool sSkipSound;

void CtrAudio_SetSkipSound(bool skip) { sSkipSound = skip; }
bool CtrAudio_SkipSound(void) { return sSkipSound; }

static bool StopWorker(void)
{
    if (sWorker == NULL)
        return true;
    /* Bounded waits: exit must never hang the console on a stuck worker. */
    LightEvent_WaitTimeout(&sIdle, 1000000000LL);
    sWorkerQuit = true;
    LightEvent_Signal(&sKick);
    if (R_FAILED(threadJoin(sWorker, 2000000000ULL)))
        return false;
    threadFree(sWorker);
    sWorker = NULL;
    return true;
}
