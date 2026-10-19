#ifndef CTR_FORWARDER_H
#define CTR_FORWARDER_H

/*
 * The HOME Menu forwarder's handshake (3ds_port/forwarder). To start this game
 * the forwarder makes its own title Luma3DS's 3DSX title, so that Luma loads the
 * game in its place, and passes the previous 3DSX title as an argument. The game
 * puts that title back as soon as it starts: the forwarder then runs its own code
 * the next time, and the usual Homebrew Launcher entry keeps working.
 * Shared by the game and the forwarder; pure C, tested on the host.
 */
#include <stdint.h>
#include <string.h>

#define CTR_FORWARDER_RESTORE_ARG "twinembers-forwarder:hbldr-tid="
/* Luma3DS's shared configuration page (OS_SHAREDCFG_VADDR + 0x800), private
 * layout, stable since Luma v10: the selected 3DSX title is at 0x808. */
#define CTR_FORWARDER_LUMA_SELECTED_TID_OFFSET 0x808

/* An application title (high word 0x00040000), the only kind the forwarder passes. */
static inline int CtrForwarder_IsApplicationTid(uint64_t tid)
{
    return (uint32_t)(tid >> 32) == 0x00040000u && (uint32_t)tid != 0;
}

/* The title to restore from the game's arguments: exactly 16 hex digits after the
 * prefix, an application title. 0 when there is none or it is malformed. */
static inline uint64_t CtrForwarder_RestoreTid(int argc, char *const *argv)
{
    const size_t prefix = sizeof(CTR_FORWARDER_RESTORE_ARG) - 1;

    if (argv == NULL)
        return 0;
    for (int i = 1; i < argc; i++)
    {
        const char *arg = argv[i];
        uint64_t tid = 0;
        int digits = 0;

        if (arg == NULL || strncmp(arg, CTR_FORWARDER_RESTORE_ARG, prefix) != 0)
            continue;
        for (const char *p = arg + prefix; *p; p++, digits++)
        {
            int v = *p >= '0' && *p <= '9' ? *p - '0'
                  : *p >= 'A' && *p <= 'F' ? *p - 'A' + 10
                  : *p >= 'a' && *p <= 'f' ? *p - 'a' + 10 : -1;
            if (v < 0 || digits >= 16)
                return 0;
            tid = (tid << 4) | (uint64_t)v;
        }
        return digits == 16 && CtrForwarder_IsApplicationTid(tid) ? tid : 0;
    }
    return 0;
}

#endif
