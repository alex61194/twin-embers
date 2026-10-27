/* CtrSettings: the turbo and auto-run that survive a restart. */
#include <assert.h>
#include <stdio.h>

#include <string.h>

#include "3ds_settings.h"   /* built with CTR_SETTINGS_PATH "build/settings_test.cfg" */

int main(void)
{
    remove(CTR_SETTINGS_PATH);

    /* No file: the defaults. */
    assert(CtrSettings_Turbo() == 0 && CtrSettings_AutoRun() == 0);

    CtrSettings_SetTurbo(3);
    CtrSettings_SetAutoRun(1);
    assert(CtrSettings_Turbo() == 3 && CtrSettings_AutoRun() == 1);

    /* The file holds both, so a later session reads them back. */
    FILE *file = fopen(CTR_SETTINGS_PATH, "rb");
    char text[64] = {0};
    assert(file != NULL);
    assert(fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    /* Rewritten in place: the text, then blank lines up to a fixed size. */
    assert(strncmp(text, "turbo=3\nautorun=1\n", 18) == 0);
    for (size_t i = 18; text[i] != 0; ++i)
        assert(text[i] == '\n');

    /* A value outside 2..4 is off. */
    CtrSettings_SetTurbo(9);
    assert(CtrSettings_Turbo() == 0);

    /* A shorter text over a longer one leaves nothing of the old behind. */
    CtrSettings_SetAutoRun(0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) == 63);
    fclose(file);
    assert(strncmp(text, "turbo=0\nautorun=0\n", 18) == 0 && strchr(text + 18, 'u') == NULL);

    remove(CTR_SETTINGS_PATH);
    puts("settings_test ok");
    return 0;
}
