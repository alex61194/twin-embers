/* The forwarder handshake (3ds_forwarder.h): the game restores Luma3DS's 3DSX title only from a
 * well-formed argument the forwarder wrote, never from anything else on its command line. */
#include <stdio.h>

#include "3ds_forwarder.h"

static int failures;
#define CHECK(c, ...) do { if (!(c)) { printf("FAIL: " __VA_ARGS__); printf("\n"); failures++; } } while (0)

static uint64_t Parse(const char *arg)
{
    char *argv[] = {"sdmc:/3ds/twinembers/twinembers.3dsx", (char *)arg, NULL};
    return CtrForwarder_RestoreTid(arg ? 2 : 1, argv);
}

int main(void)
{
    char written[96];
    char *forwarder[] = {"sdmc:/3ds/twinembers/twinembers.3dsx", written};

    /* What the forwarder writes (forwarder/source/main.c), read back. */
    snprintf(written, sizeof(written), CTR_FORWARDER_RESTORE_ARG "%016llX", 0x000400000D921E00ULL);
    CHECK(CtrForwarder_RestoreTid(2, forwarder) == 0x000400000D921E00ULL, "the forwarder's own argument");
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG "000400000d921e00") == 0x000400000D921E00ULL, "lower case");

    /* Started from the Homebrew Launcher: no argument, nothing restored. */
    CHECK(Parse(NULL) == 0, "no argument");
    CHECK(CtrForwarder_RestoreTid(0, NULL) == 0, "no argv");
    CHECK(Parse("--debug") == 0, "an unrelated argument");

    /* Malformed or not an application title: refused. */
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG "") == 0, "empty");
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG "000400000D921E0") == 0, "15 digits");
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG "000400000D921E000") == 0, "17 digits");
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG "000400000D921E0G") == 0, "not hex");
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG " 00400000D921E00") == 0, "leading space");
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG "-00400000D921E00") == 0, "sign");
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG "0004000000000000") == 0, "zero title");
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG "0004013000002202") == 0, "a system module");
    CHECK(Parse(CTR_FORWARDER_RESTORE_ARG "FFFFFFFFFFFFFFFF") == 0, "all ones");
    CHECK(Parse("emerald3ds-forwarder:hbldr-tid=000400000D921E00") == 0, "another game's forwarder");
    CHECK(CtrForwarder_IsApplicationTid(0x0004000001086500ULL), "the forwarder's own title");

    if (failures) { printf("%d failures\n", failures); return 1; }
    puts("PASS forwarder handshake");
    return 0;
}
