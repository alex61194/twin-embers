/* Original libctru adapter, alex61194 with AI assistance; MIT. */
#include <3ds.h>
#include "3ds_rng.h"

uint16_t CtrRng_GetSeed(void)
{
    static uint32_t sample;
    uint64_t random = 0;
    /* Balance each successful service open, including generation failure.
     * Service permission/unavailability is normal in some homebrew loaders. */
    if (R_SUCCEEDED(psInit()))
    {
        if (R_FAILED(PS_GenerateRandomBytes(&random, sizeof(random))))
            random = 0; /* Do not consume a partly written failure buffer. */
        psExit();
    }
    /* The clocks also vary in emulators and when ps:ps is unavailable. The
     * process-local counter supplements them; it is not stored in a save.
     * No HID polling, frame hook, persistent service or heap allocation. */
    return CtrRng_Fold(random, svcGetSystemTick(), osGetTime(), ++sample);
}
