/* Runs the production data loader with only the SDK lock stubbed out. */
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "3ds_data.h"
#include "3ds_log.h"

void CtrLog_Write(CtrLogCategory category, const char *format, ...)
{ (void)category; (void)format; }

int main(int argc, char **argv)
{
    assert(argc == 2);
    /* Backend chosen at run time (built without CTR_DATA_BACKEND). */
    if (!strcmp(argv[1], "clean-missing-pack"))
    {
        /* The image is clean: markers for embedded/loose data are not honoured and
         * there is no pack, so startup must stop with the readable message. */
        assert(!CtrData_Init());
        assert(CtrData_GetBackend() == CTR_DATA_NONE);
        assert(!strcmp(CtrData_ErrorTitle(), "twinembers data pack missing."));
        assert(strstr(CtrData_ErrorDetail(), "Use twinembers Builder") != NULL);
        assert(strstr(CtrData_ErrorDetail(), "FireRed3DS") == NULL);
        assert(strstr(CtrData_ErrorDetail(), "/3ds/twinembers/twinembers.pak") != NULL
            || strstr(CtrData_ErrorDetail(), "twinembers.pak") != NULL);
        return 0;
    }
    if (!strcmp(argv[1], "clean-reads-pack"))
    {
        assert(CtrData_Init() && CtrData_GetBackend() == CTR_DATA_PAK);
        uint32_t size;
        assert(CtrData_Size("graphics/good.bin", &size) && size == 4096);
        assert(!CtrData_Size("only-in-romfs.bin", &size));
        CtrData_Shutdown();
        return 0;
    }
    if (!strcmp(argv[1], "development-uses-embedded"))
    {
        assert(CtrData_Init() && CtrData_GetBackend() == CTR_DATA_ROMFS);
        CtrData_Shutdown();
        return 0;
    }
    if (!strcmp(argv[1], "metadata-bad"))
    {
        assert(!CtrData_Init());
        CtrData_Shutdown();
        return 0;
    }
    /* Corruption in an untouched payload must not trigger a boot scan. */
    assert(CtrData_Init());
    uint32_t size;
    assert(CtrData_Size("graphics/bad.bin", &size) && size == 8192);
    unsigned char *good = CtrData_Load("graphics/good.bin", &size);
    assert(good && size == 4096 && good[0] == 17 && good[size] == 0);
    free(good);
    assert(!CtrData_Load("graphics/bad.bin", NULL));
    assert(!CtrData_Open("graphics/bad.bin"));
    unsigned char buffer[8192];
    assert(!CtrData_ReadInto("graphics/bad.bin", buffer, sizeof(buffer)));
    assert(!CtrData_ReadInto("graphics/good.bin", buffer, 4095));
    assert(CtrData_ReadInto("graphics/good.bin", buffer, 4096));
    FILE *stream = CtrData_Open("graphics/good.bin");
    assert(stream && fseek(stream, 31, SEEK_SET) == 0 && fgetc(stream) == 17);
    fclose(stream);
    assert(strlen(CtrData_ErrorDetail()) > 0);
    CtrData_Shutdown();
    return 0;
}
