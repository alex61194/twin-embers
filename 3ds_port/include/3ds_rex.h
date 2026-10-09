#ifndef CTR_REX_H
#define CTR_REX_H

/*
 * ROM-extracted data (REX): FireRed's read-only game tables.
 *
 * The executable reserves zero-filled storage for every table (tools/rex_objects.py)
 * and carries only romfs:/engine/rex.bin, a map of where each pack region goes and
 * which words are pointers (tools/rex_link.py). The pack holds the bytes. Before
 * AgbMain() the loader copies every live unit out of its region and writes the
 * final address into each pointer word. SDK-free.
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define CTR_REX_MAGIC 0x58523346u /* "F3RX" little endian */
#define CTR_REX_VERSION 1u
/* Set in a site address when the word is a 16-bit constant, not a 32-bit pointer. */
#define CTR_REX_SITE_16 0x80000000u

typedef enum
{
    CTR_REX_OK,
    CTR_REX_BAD_HEADER,
    CTR_REX_BAD_CRC,
    CTR_REX_BAD_TABLE,
    CTR_REX_READ_FAILED,
    CTR_REX_BAD_RANGE,
    CTR_REX_NO_MEMORY,
} CtrRexStatus;

/* Environment of the loader; the console build binds these to the pack and to
 * plain memory, the host test to a fake pack and an arena. */
typedef struct
{
    /* Exact-size, CRC-checked payload read. */
    bool (*read)(void *ctx, const char *path, void *buffer, uint32_t size);
    /* Memory behind a link-time address range, or NULL when it is not mapped. */
    uint8_t *(*resolve)(void *ctx, uint32_t address, uint32_t size);
    void *ctx;
    /* Added to every value written (runtime image base minus link-time base). */
    uint32_t delta;
} CtrRexEnv;

/* Applies engine/rex.bin. On failure *failed names the offending pack path or
 * NULL. Never leaves a partially validated table in use: all structure is
 * checked before the first byte is written. */
CtrRexStatus CtrRex_Apply(const uint8_t *meta, size_t metaSize, const CtrRexEnv *env,
                          uint32_t referenceRuntime, const char **failed);

/* Console glue: reads romfs:/engine/rex.bin and applies it with the pack. */
bool CtrRex_Init(uintptr_t referenceRuntime);

#endif
