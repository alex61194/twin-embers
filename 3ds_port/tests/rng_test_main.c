/* Asset-free libctru smoke test; alex61194 with AI assistance, MIT. */
#include <3ds.h>
#include <stdio.h>
#include "3ds_rng.h"

int main(void)
{
    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    FILE *log = fopen("sdmc:/rng-test.txt", "a");
    uint64_t probe = 0;
    Result service = psInit();
    if (R_SUCCEEDED(service))
    {
        service = PS_GenerateRandomBytes(&probe, sizeof(probe));
        psExit();
    }
    if (log) fprintf(log, "ps_random=%s\n", R_SUCCEEDED(service) ? "available" : "fallback");
    bool passed = CtrRng_Fold(0, 0, 0, 0) == 0;
    /* Fixed expected results are also checked by the independent host model. */
    passed = passed && CtrRng_Fold(UINT64_C(0x0123456789abcdef),
                  UINT64_C(0xfedcba9876543210), 123456789, 1) == 16498;
    printf("Twin Embers entropy diagnostic\nControlled vectors: %s\n", passed ? "PASS" : "FAIL");
    if (log) fprintf(log, "vectors=%s\n", passed ? "PASS" : "FAIL");
    for (unsigned i = 0; i < 32; ++i)
    {
        uint16_t seed = CtrRng_GetSeed();
        printf("%02u: %05u\n", i, seed);
        if (log) fprintf(log, "sample=%u seed=%u\n", i, seed);
    }
    if (log) { fputs("complete\n", log); fclose(log); }
    /* Auto-exit for isolated emulator runs; START exits earlier on hardware. */
    for (unsigned frame = 0; frame < 120 && aptMainLoop(); ++frame)
    {
        hidScanInput();
        if (hidKeysDown() & KEY_START) break;
        gfxFlushBuffers(); gfxSwapBuffers(); gspWaitForVBlank();
    }
    gfxExit();
    return passed ? 0 : 1;
}
