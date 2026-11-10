/* CtrSettings: the turbo, auto-run and REPEL AGAIN that survive a restart. */
#include <assert.h>
#include <stdio.h>

#include <string.h>

#include "3ds_settings.h"   /* built with CTR_SETTINGS_PATH "build/settings_test.cfg" */

int main(void)
{
    remove(CTR_SETTINGS_PATH);

    /* No file: the defaults. */
    assert(CtrSettings_Turbo() == 0 && CtrSettings_AutoRun() == 0 && CtrSettings_RepelAgain() == 1);

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
    assert(strncmp(text, "turbo=3\nautorun=1\nrepel=1\n", 26) == 0);
    for (size_t i = 26; text[i] != 0; ++i)
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
    assert(strncmp(text, "turbo=0\nautorun=0\nrepel=1\n", 26) == 0 && strchr(text + 26, 'u') == NULL);

    /* REPEL AGAIN off is kept, and read back as off. */
    CtrSettings_SetRepelAgain(0);
    assert(CtrSettings_RepelAgain() == 0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) == 63);
    fclose(file);
    assert(strstr(text, "repel=0\n") != NULL);

    /* The battle's ball button: the ball it last threw, kept after the others. */
    assert(CtrSettings_LastBall() == 0);
    CtrSettings_SetLastBall(3);
    assert(CtrSettings_LastBall() == 3);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) == 63);
    fclose(file);
    assert(strstr(text, "ball=3\n") != NULL);

    /* EXP SHARE: off until chosen, kept after the others, and a value that is not a level leaves it as it was. */
    assert(CtrSettings_ExpShare() == 0);
    CtrSettings_SetExpShare(2);
    assert(CtrSettings_ExpShare() == 2);
    CtrSettings_SetExpShare(5);
    assert(CtrSettings_ExpShare() == 2);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "expshare=2\n") != NULL && strstr(text, "ball=3\n") != NULL);

    /* REUSABLE TMS: off until chosen, any non-zero is on, written only when on. */
    assert(CtrSettings_TmReuse() == 0);
    CtrSettings_SetTmReuse(7);
    assert(CtrSettings_TmReuse() == 1);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "tmreuse=1\n") != NULL && strstr(text, "expshare=2\n") != NULL);
    CtrSettings_SetTmReuse(0);
    assert(CtrSettings_TmReuse() == 0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "tmreuse") == NULL);

    /* NUZLOCKE: off until chosen, written only when on. */
    assert(CtrSettings_Nuzlocke() == 0);
    CtrSettings_SetNuzlocke(1);
    assert(CtrSettings_Nuzlocke() == 1);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "nuzlocke=1\n") != NULL && strstr(text, "expshare=2\n") != NULL);
    CtrSettings_SetNuzlocke(0);
    assert(CtrSettings_Nuzlocke() == 0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "nuzlocke") == NULL);

    /* RANDOMLOCKE: off until chosen, written only when on. */
    assert(CtrSettings_Randomlocke() == 0);
    CtrSettings_SetRandomlocke(1);
    assert(CtrSettings_Randomlocke() == 1);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "randomlocke=1\n") != NULL && strstr(text, "expshare=2\n") != NULL);
    CtrSettings_SetRandomlocke(0);
    assert(CtrSettings_Randomlocke() == 0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "randomlocke") == NULL);

    /* The look for Red, Leaf and Blue: none until chosen, kept after the others, only a folder's name ([a-z0-9_], 15). */
    assert(CtrSettings_Look(CTR_LOOK_RED)[0] == 0 && CtrSettings_Look(CTR_LOOK_BLUE)[0] == 0);
    CtrSettings_SetLook(CTR_LOOK_RED, "ash_hgss");
    CtrSettings_SetLook(CTR_LOOK_BLUE, "gary_kanto");
    assert(strcmp(CtrSettings_Look(CTR_LOOK_RED), "ash_hgss") == 0 && strcmp(CtrSettings_Look(CTR_LOOK_BLUE), "gary_kanto") == 0);
    assert(CtrSettings_Look(CTR_LOOK_LEAF)[0] == 0 && CtrSettings_Look(3)[0] == 0);
    CtrSettings_SetLook(CTR_LOOK_LEAF, "Bad Name/..");
    assert(CtrSettings_Look(CTR_LOOK_LEAF)[0] == 0);                       /* the first character is not allowed: nothing */
    CtrSettings_SetLook(CTR_LOOK_LEAF, "abcdefghijklmnopqrstu");
    assert(strlen(CtrSettings_Look(CTR_LOOK_LEAF)) == 15);
    CtrSettings_SetLook(CTR_LOOK_LEAF, "");
    assert(CtrSettings_Look(CTR_LOOK_LEAF)[0] == 0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    {
        char big[256 + 1] = {0};

        assert(file != NULL && fread(big, 1, 256, file) == 256);
        fclose(file);
        assert(strstr(big, "look_red=ash_hgss\n") != NULL && strstr(big, "look_blue=gary_kanto\n") != NULL
               && strstr(big, "look_leaf") == NULL && strstr(big, "expshare=2\n") != NULL && strstr(big, "ball=3\n") != NULL);
    }

    remove(CTR_SETTINGS_PATH);
    puts("settings_test ok");
    return 0;
}
