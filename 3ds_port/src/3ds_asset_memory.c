/* FireRed data-read boundary. Existing memory/decompression algorithms and
 * array addresses are preserved; only missing payload bytes are populated. */
#include <string.h>
#include "3ds_assets.h"
#include "3ds_platform.h"

void CtrAssets_RequireRange(const void *source, size_t bytes)
{
    if (!CtrAssets_EnsureRange(source, bytes))
        CtrPlatform_ShowDataError("FireRed graphics could not be loaded.",
            "A graphics payload is missing, damaged, or does not match this engine. Rebuild the data pack.");
}

void *CtrAssets_Memcpy(void *dest, const void *source, size_t bytes)
{
    CtrAssets_RequireRange(source, bytes);
    return memcpy(dest, source, bytes);
}

void *CtrAssets_Memmove(void *dest, const void *source, size_t bytes)
{
    CtrAssets_RequireRange(source, bytes);
    return memmove(dest, source, bytes);
}
