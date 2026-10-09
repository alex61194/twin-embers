/* Console glue for the REX loader: RomFS metadata in, pack bytes out. */
#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>

#include "3ds_data.h"
#include "3ds_log.h"
#include "3ds_rex.h"

#define REX_META_PATH CTR_DATA_ROMFS_DIR "engine/rex.bin"

static bool PackRead(void *ctx, const char *path, void *buffer, uint32_t size)
{
    (void)ctx;
    return CtrData_ReadInto(path, buffer, size);
}

static uint8_t *Identity(void *ctx, uint32_t address, uint32_t size)
{
    (void)ctx;
    (void)size;
    return (uint8_t *)(uintptr_t)address;
}

bool CtrRex_Init(uintptr_t referenceRuntime)
{
    FILE *file = fopen(REX_META_PATH, "rb");
    uint8_t *meta = NULL;
    long size = -1;
    const char *failed = NULL;
    CtrRexStatus status;
    uint32_t reference;
    CtrRexEnv env = { PackRead, Identity, NULL, 0 };
    u64 started = osGetTime();

    if (file != NULL && fseek(file, 0, SEEK_END) == 0 && (size = ftell(file)) > 32 && fseek(file, 0, SEEK_SET) == 0)
        meta = malloc((size_t)size);
    if (meta == NULL || fread(meta, 1, (size_t)size, file) != (size_t)size)
    {
        CtrLog_Write(CTR_LOG_ERROR, "rex: engine/rex.bin unreadable");
        if (file) fclose(file);
        free(meta);
        return false;
    }
    fclose(file);
    reference = (uint32_t)meta[8] | (uint32_t)meta[9] << 8 | (uint32_t)meta[10] << 16 | (uint32_t)meta[11] << 24;
    env.delta = (uint32_t)referenceRuntime - reference;
    status = CtrRex_Apply(meta, (size_t)size, &env, (uint32_t)referenceRuntime, &failed);
    free(meta);
    if (status != CTR_REX_OK)
    {
        CtrLog_Write(CTR_LOG_ERROR, "rex: load failed (%d) %s", (int)status, failed ? failed : "");
        return false;
    }
    CtrLog_Write(CTR_LOG_FS, "rex: game data loaded from the pack in %lu ms", (unsigned long)(osGetTime() - started));
    return true;
}
