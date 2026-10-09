# Native sound bandwidth — private candidate

Owner-approved starting code: `4ea5c63fe7f1f1a7d369ecdc6961ea024c542577`.
The owner reports that PC, mailbox and the prior fixes now work correctly,
and requests brighter sound. Work stays on `feature/storage-bottom-touch`.

## Change

The clean-profile candidate still used FireRed's 13,379-Hz default even though
NDSP already used polyphase interpolation and the PSG used band-limited steps.
Interpolation cannot restore harmonics above the source Nyquist limit.

`0103-audio-native-bandwidth.json` changes only the default frequency argument
of the real m4aSoundInit SoundMode call under PLATFORM_3DS, to the existing
SOUND_MODE_FREQ_31536 mode. The GBA branch retains SOUND_MODE_FREQ_13379.
Bit depth, master volume 12, five DirectSound channels, instrument/sample data,
stereo routing, gain, song tempo, anti-aliasing and queue depth are retained.
No equalizer, volume boost, derivative replacement mixer or new UI is added.
The original m4a assembly and the clean-profile 8-bit ring remain in use.

The existing mode table selects 528 samples per sound frame. Three slices
exactly fit the unchanged 1,584-sample plane; the old mode uses 224 samples
and seven slices. The transport already takes rate, slice size and period
from SoundInfo, so no fixed-rate resampling or queue-size change is needed.
It recalculates PSG rate, NDSP playback rate and sound catch-up from those
fields. Both rates correspond to approximately 59.7275 sound frames/s;
the existing small drift correction toward the same queue depth remains.

Native source Nyquist rises from about 6.69 kHz to 15.77 kHz. This restores
available high harmonics to the renderer; it does not invent detail missing
from original samples or claim a new sample bit depth.

## Automated verification

- The production mode call is compiled and executed in both native and GBA
  variants, checking the selected frequency and unchanged volume/channel bits.
- 30,000-frame cadence tests at each rate prove in-plane offsets, equal slice
  visits and the unchanged sound-frame timing. The native frame fits the
  existing 1,024-frame-sample transport limit and uses the full PCM plane.
- The real PSG renderer synthesizes a 4,096-Hz pulse with its existing BLEP
  anti-aliasing at both rates. A Hann-windowed projection measures its
  12,288-Hz third harmonic relative to the fundamental: -10.96 dB at the new
  rate. At the old rate that frequency cannot be represented; the projection
  folds to its 1,091-Hz alias, measuring only -73.43 dB of residual energy.
  Stereo equality and sample bounds pass.
  These values describe this synthetic signal, not a music recording.
- The test runs without game media, a ROM, a pack or an emulator. It is
  added to source CI and the ordinary tools regression suite.
- 51/51 local tools tests pass, including the previously accepted PC/mail
  regressions. Five existing host audio regressions also pass: mixer/routing,
  VCount/VBlank cadence, catch-up, PSG registers and BLEP alias suppression.
- The real m4a unit is compiled with ARM; the complete game is relinked and
  passes the 3DSX relocation and ARM11 m4a checks. The final private binary
  is regenerated with its clean source label and supplied outside Git.
- ABI `8f71cf3a`, Builder data, ROM, `.pak`, saves and all previous gameplay
  corrections are unchanged. No main/golden/earlier branch is modified.

## Acceptance boundary

No emulator was run, and no hardware listening test is claimed. The wider
synthetic bandwidth is measured; perceived brightness, hiss and balance
still need the owner's listening comparison on 3DS. Processing 528 instead
of 224 samples increases mixer work by about 2.36 times; buffers and their
time depth are unchanged, but actual underruns/FPS under load require hardware
checking, especially on Old 3DS. The existing worker and fallback remain.

Compare music, battle sounds and Pokémon cries using the same volume and
output device as the validated previous candidate. Check for harshness,
clicks, missing channels or new dropouts. Any later tuning should be a
separate reversible change. Source/structural audit success does not approve
binary publication; the private output includes its exact hash and CI result.
