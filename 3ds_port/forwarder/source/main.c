// HOME Menu forwarder for Twin Embers.
//
// Adapted from the forwarder of ZallaxDev's pokeemerald-3Ds-dualscreen
// (3ds_port/forwarder, MIT; see licenses/ZallaxDev-MIT.txt and NOTICE.md).
//
// Installed once as a CIA, it starts sdmc:/3ds/twinembers/twinembers.3dsx, so
// updating the game only ever replaces the 3DSX. It asks Luma3DS's hb:ldr to
// load that file, makes its own title the one Luma loads 3DSX files through
// (no Homebrew Launcher title has to be installed) and restarts itself, so Luma
// starts the game in its place. It passes Luma's previous 3DSX title to the game
// (3ds_forwarder.h), which puts it back as soon as it starts: the next start of
// this title runs this code again, and the usual Homebrew Launcher entry keeps
// working.

#include <3ds.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "3ds_forwarder.h"

#define TARGET_SD_PATH "/3ds/twinembers/twinembers.3dsx"
#define TARGET_ARGV0   "sdmc:" TARGET_SD_PATH
#define FORWARDER_TID  0x0004000001086500ULL   // forwarder-rsf.yml UniqueId 0x10865

// Luma3DS shared config page (private layout, stable since v10).
typedef struct {
    u64 hbldr3dsxTid;
    u64 selectedHbldr3dsxTid;
    bool useHbldr;
} LumaSharedConfig;
#define LUMA_SHARED_CONFIG ((volatile LumaSharedConfig *)(OS_SHAREDCFG_VADDR + 0x800))
_Static_assert(0x800 + __builtin_offsetof(LumaSharedConfig, selectedHbldr3dsxTid)
               == CTR_FORWARDER_LUMA_SELECTED_TID_OFFSET, "the game restores the same field");

// hb:ldr reads whole fixed-size buffers, whatever size is declared.
static char sTarget[0x400] __attribute__((aligned(4)));
static u32 sArgv[0x400 / 4];

static Result HbldrSetTarget(Handle h, const char *path)
{
    u32 *cmd = getThreadCommandBuffer();
    strncpy(sTarget, path, sizeof(sTarget) - 1);
    cmd[0] = IPC_MakeHeader(2, 0, 2);
    cmd[1] = IPC_Desc_StaticBuffer(strlen(sTarget) + 1, 0);
    cmd[2] = (u32)sTarget;
    Result res = svcSendSyncRequest(h);
    return R_FAILED(res) ? res : (Result)cmd[1];
}

// argv[0] is the game's path (its RomFS), argv[1] the title to restore.
static Result HbldrSetArgv(Handle h, const char *argv0, u64 restoreTid)
{
    u32 *cmd = getThreadCommandBuffer();
    char *p = (char *)&sArgv[1];
    sArgv[0] = 2;
    p += sprintf(p, "%s", argv0) + 1;
    sprintf(p, CTR_FORWARDER_RESTORE_ARG "%016llX", (unsigned long long)restoreTid);
    cmd[0] = IPC_MakeHeader(3, 0, 2);
    cmd[1] = IPC_Desc_StaticBuffer(sizeof(sArgv), 1);
    cmd[2] = (u32)sArgv;
    Result res = svcSendSyncRequest(h);
    return R_FAILED(res) ? res : (Result)cmd[1];
}

static bool IsLuma(void)
{
    s64 version;
    return R_SUCCEEDED(svcGetSystemInfo(&version, 0x10000, 0));
}

// Prepares hb:ldr and Luma's 3DSX title; on success main() returns and
// libctru's chainloader restarts this title, which Luma loads as the game.
static Result Forward(const char **why, u64 *chainTid)
{
    struct stat st;
    if (stat(TARGET_ARGV0, &st) != 0)
    {
        *why = "The game is not on the SD card.\n\n"
               "Copy twinembers.3dsx to the SD card\n"
               "so that this file exists:\n\n  " TARGET_ARGV0 "\n\n"
               "Make twinembers.pak, next to it,\n"
               "with the Twin Embers Builder.";
        return -1;
    }
    if (!IsLuma() || !LUMA_SHARED_CONFIG->useHbldr)
    {
        *why = "This shortcut needs Luma3DS.\n\n"
               "Start the game from the Homebrew Launcher.";
        return -1;
    }

    u64 selfTid;
    if (R_FAILED(APT_GetProgramID(&selfTid)))
        selfTid = FORWARDER_TID;
    u64 previousTid = LUMA_SHARED_CONFIG->selectedHbldr3dsxTid;
    // A previous run whose game did not restore it (an older 3DSX): never hand
    // the game our own title, or the Homebrew Launcher entry stays ours.
    if (previousTid == selfTid || !CtrForwarder_IsApplicationTid(previousTid))
        previousTid = LUMA_SHARED_CONFIG->hbldr3dsxTid;

    Handle hbldr;
    // A global named port, not an srv service (srv would wait for it forever).
    Result res = svcConnectToPort(&hbldr, "hb:ldr");
    if (R_SUCCEEDED(res))
    {
        res = HbldrSetTarget(hbldr, TARGET_SD_PATH);
        if (R_SUCCEEDED(res))
            res = HbldrSetArgv(hbldr, TARGET_ARGV0, previousTid);
        svcCloseHandle(hbldr);
    }
    if (R_FAILED(res))
    {
        *why = "Luma3DS refused to load the game (hb:ldr).\n\n"
               "Update Luma3DS, or start the game from the\n"
               "Homebrew Launcher.";
        return res;
    }

    // Luma's PM makes the selected title current when this process exits;
    // the restart is then loaded as the 3DSX.
    LUMA_SHARED_CONFIG->selectedHbldr3dsxTid = selfTid;
    *chainTid = selfTid;
    return 0;
}

int main(void)
{
    const char *why = NULL;
    u64 chainTid = 0;
    Result res = Forward(&why, &chainTid);

    if (R_SUCCEEDED(res))
    {
        aptSetChainloader(chainTid, MEDIATYPE_SD);
        return 0;
    }

    gfxInitDefault();
    consoleInit(GFX_TOP, NULL);
    printf("\n Twin Embers\n\n");
    printf(" %s\n", why);
    if (res != -1)
        printf("\n Error 0x%08lX\n", (unsigned long)res);
    printf("\n\n Press START to exit.\n");
    while (aptMainLoop())
    {
        hidScanInput();
        if (hidKeysDown() & KEY_START)
            break;
        gfxFlushBuffers();
        gfxSwapBuffers();
        gspWaitForVBlank();
    }
    gfxExit();
    return 0;
}
