/* Native error UI only; game/renderer/logging checkpoint remains unchanged. */
#include <3ds.h>
#include "3ds_platform.h"

void CtrPlatform_ShowDataError(const char *title, const char *detail)
{
    CtrLog_Write(CTR_LOG_ERROR, "data error: %s: %s", title, detail);
    CtrPlatform_ShowError("twinembers data error", title, detail);
}
