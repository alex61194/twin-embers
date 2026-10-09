/* Native error UI only; game/renderer/logging checkpoint remains unchanged. */
#include <3ds.h>
#include <stdio.h>
#include <stdlib.h>
#include "3ds_platform.h"
#include "3ds_data_error_text.h"

static void PrintRow(void *ctx, const char *text, size_t length)
{
    (void)ctx;
    printf("%.*s\n", (int)length, text);
}

void CtrPlatform_ShowDataError(const char *title, const char *detail)
{
    CtrLog_Write(CTR_LOG_ERROR, "data error: %s: %s", title, detail);
    printf("\x1b[2J");
    CtrError_WrapText("twinembers data error", PrintRow, NULL);
    CtrError_WrapText(title, PrintRow, NULL);
    CtrError_WrapText(detail, PrintRow, NULL);
    CtrError_WrapText("Press A to exit.", PrintRow, NULL);
    while (aptMainLoop())
    {
        hidScanInput();
        if (hidKeysDown() & KEY_A) break;
        gspWaitForVBlank();
    }
    exit(1);
}
