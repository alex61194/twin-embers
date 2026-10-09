/* FireRed's sound engine (m4a) on a worker core (CtrAudio_StartWorker).
 * Adapted from ZallaxDev's DualScreen 3ds_sound.c (MIT).
 *
 * The VCount handler calls m4aSoundVSync (advances the PCM ring) and the
 * VBlank handler m4aSoundMain (sequencer, mixer, CtrAudio_OnSoundMain). Both
 * reach this file through the linker (--wrap, Makefile): with a worker they
 * become one hand-over per frame, VSync then Main as on the GBA, and the game
 * thread goes on with its next frame while the worker mixes. The audio
 * catch-up in 3ds_game_bridge.c calls the same pair, so it is handed over too.
 *
 * On the GBA the engine runs inside an interrupt and the game's calls into it
 * are never interrupted by the game: m4a guards its players with an ident
 * flag that makes the interrupted side skip, not wait. On another core that
 * flag would drop a song start or a cry whenever the two met, so every m4a
 * entry point the game calls from outside m4a.c holds the worker's lock
 * instead, and is wrapped here. Calls inside m4a.c and m4a_1.s are not
 * wrapped (--wrap only redirects references between objects), and none of
 * the functions m4a_1.s or m4a_tables.c reference is in the list.
 *
 * Built with -fno-optimize-sibling-calls: m4aSoundVSync is Thumb (m4a_1.s),
 * and a tail call to it would be a B that needs an ARM->Thumb veneer, one
 * more than filter_3dsx_relocs.py expects. Every call here stays a BL/BLX.
 */
#include "global.h"
#include "gba/m4a_internal.h"
#include "3ds_audio.h"

static bool sThreaded, sTried;

void __real_m4aSoundVSync(void);
void __real_m4aSoundMain(void);

static void MixFrame(void)
{
    __real_m4aSoundVSync();
    __real_m4aSoundMain();
}

void __wrap_m4aSoundVSync(void)
{
    /* Threaded: it runs on the worker, right before the next m4aSoundMain.
     * A fast-forward frame that is not shown skips the pair: the engine
     * advances one sound frame per shown frame, at the music's own pace. */
    if (!sThreaded && !CtrAudio_SkipSound())
        __real_m4aSoundVSync();
}

void __wrap_m4aSoundMain(void)
{
    if (CtrAudio_SkipSound())
        return;
    if (!sTried)
    {
        sTried = true;
        sThreaded = CtrAudio_StartWorker(MixFrame);
    }
    if (sThreaded && !CtrAudio_Kick())
    {
        /* Back on the game thread for good. This frame's VSync was skipped
         * above, so it runs here, before the mix, as it would have. */
        sThreaded = false;
        __real_m4aSoundVSync();
    }
    if (!sThreaded)
        __real_m4aSoundMain();
}

#define LOCKED(ret, name, params, args)            \
    ret __real_##name params;                      \
    ret __wrap_##name params;                      \
    ret __wrap_##name params                       \
    {                                              \
        CtrAudio_LockSound();                      \
        ret result = __real_##name args;           \
        CtrAudio_UnlockSound();                    \
        return result;                             \
    }

#define LOCKED_VOID(name, params, args)            \
    void __real_##name params;                     \
    void __wrap_##name params;                     \
    void __wrap_##name params                      \
    {                                              \
        CtrAudio_LockSound();                      \
        __real_##name args;                        \
        CtrAudio_UnlockSound();                    \
    }

/* Exactly the m4a.c entry points FireRed calls from other units; keep in
 * sync with SOUND_WRAPPED in the Makefile. */
LOCKED_VOID(m4aSoundInit, (void), ())
LOCKED_VOID(m4aSoundVSyncOn, (void), ())
LOCKED_VOID(m4aSoundVSyncOff, (void), ())
LOCKED_VOID(m4aSongNumStart, (u16 n), (n))
LOCKED_VOID(m4aSongNumStop, (u16 n), (n))
LOCKED_VOID(m4aMPlayAllStop, (void), ())
LOCKED_VOID(m4aMPlayStop, (struct MusicPlayerInfo *info), (info))
LOCKED_VOID(m4aMPlayContinue, (struct MusicPlayerInfo *info), (info))
LOCKED_VOID(m4aMPlayFadeOut, (struct MusicPlayerInfo *info, u16 speed), (info, speed))
LOCKED_VOID(m4aMPlayFadeOutTemporarily, (struct MusicPlayerInfo *info, u16 speed), (info, speed))
LOCKED_VOID(m4aMPlayFadeIn, (struct MusicPlayerInfo *info, u16 speed), (info, speed))
LOCKED_VOID(m4aMPlayImmInit, (struct MusicPlayerInfo *info), (info))
LOCKED_VOID(m4aMPlayVolumeControl, (struct MusicPlayerInfo *info, u16 tracks, u16 volume), (info, tracks, volume))
LOCKED_VOID(m4aMPlayPanpotControl, (struct MusicPlayerInfo *info, u16 tracks, s8 pan), (info, tracks, pan))
LOCKED_VOID(SetPokemonCryVolume, (u8 value), (value))
LOCKED_VOID(SetPokemonCryPanpot, (s8 value), (value))
LOCKED_VOID(SetPokemonCryPitch, (s16 value), (value))
LOCKED_VOID(SetPokemonCryLength, (u16 value), (value))
LOCKED_VOID(SetPokemonCryRelease, (u8 value), (value))
LOCKED_VOID(SetPokemonCryProgress, (u32 value), (value))
LOCKED_VOID(SetPokemonCryChorus, (s8 value), (value))
LOCKED_VOID(SetPokemonCryStereo, (u32 value), (value))
LOCKED_VOID(SetPokemonCryPriority, (u8 value), (value))
LOCKED(struct MusicPlayerInfo *, SetPokemonCryTone, (struct ToneData *tone), (tone))
LOCKED(bool32, IsPokemonCryPlaying, (struct MusicPlayerInfo *info), (info))
