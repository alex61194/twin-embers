/* CtrSettings: the turbo, auto-run and REPEL AGAIN that survive a restart. */
#include <assert.h>
#include <stdio.h>

#include <string.h>

#include "3ds_settings.h"   /* built with CTR_SETTINGS_PATH "build/settings_test.cfg" */

int main(void)
{
    /* Settings are read once per process, so a file made here after the first read would not be read at all, and a file removed while the settings
     * hold it open is gone on Linux and macOS (not on Windows): the older build's files, CR LF and every restart are in settings_reload_test.c, one
     * process per session. This file starts from nothing, before the first read. */
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

    /* NUZLOCKE options: off until chosen, written only when on, read back. */
    char big[256];
    assert(CtrSettings_NuzDuplicates() == 0 && CtrSettings_NuzShiny() == 0 && CtrSettings_NuzLevelCap() == 0);
    CtrSettings_SetNuzDuplicates(1);
    CtrSettings_SetNuzShiny(1);
    CtrSettings_SetNuzLevelCap(1);
    assert(CtrSettings_NuzDuplicates() == 1 && CtrSettings_NuzShiny() == 1 && CtrSettings_NuzLevelCap() == 1);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(big, 0, sizeof(big));
    assert(file != NULL && fread(big, 1, sizeof(big) - 1, file) > 0);
    fclose(file);
    assert(strstr(big, "nuzdupes=1\n") != NULL && strstr(big, "nuzshiny=1\n") != NULL && strstr(big, "nuzcap=1\n") != NULL);
    CtrSettings_SetNuzDuplicates(0);
    CtrSettings_SetNuzShiny(0);
    CtrSettings_SetNuzLevelCap(0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(big, 0, sizeof(big));
    assert(file != NULL && fread(big, 1, sizeof(big) - 1, file) > 0);
    fclose(file);
    assert(strstr(big, "nuz") == NULL || strstr(big, "nuzlocke") != NULL);
    assert(strstr(big, "nuzdupes") == NULL && strstr(big, "nuzshiny") == NULL && strstr(big, "nuzcap") == NULL);

    /* WILD POKEMON (visiblewilds, wildsil, wildaggro): off until chosen; the keys are written while it is on, with the other two. */
    assert(CtrSettings_WildDensity() == 0 && CtrSettings_WildSilhouette() == 0 && CtrSettings_WildAggro() == 0);
    CtrSettings_SetWildDensity(2);
    CtrSettings_SetWildSilhouette(1);
    CtrSettings_SetWildAggro(3);
    assert(CtrSettings_WildDensity() == 2 && CtrSettings_WildSilhouette() == 1 && CtrSettings_WildAggro() == 3);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(big, 0, sizeof(big));
    assert(file != NULL && fread(big, 1, sizeof(big) - 1, file) > 0);
    fclose(file);
    assert(strstr(big, "visiblewilds=2\n") != NULL && strstr(big, "wildsil=1\n") != NULL && strstr(big, "wildaggro=3\n") != NULL);
    /* wildsil and wildaggro come before visiblewilds, so a cut file that kept visiblewilds kept them too */
    assert(strstr(big, "wildsil=") < strstr(big, "visiblewilds=") && strstr(big, "wildaggro=") < strstr(big, "visiblewilds="));
    /* values out of range are refused, not stored */
    CtrSettings_SetWildDensity(9);
    CtrSettings_SetWildAggro(-4);
    assert(CtrSettings_WildDensity() == 0 && CtrSettings_WildAggro() == 0);
    /* OFF with the display kept: the display key stays, the density key goes */
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(big, 0, sizeof(big));
    assert(file != NULL && fread(big, 1, sizeof(big) - 1, file) > 0);
    fclose(file);
    assert(strstr(big, "visiblewilds") == NULL && strstr(big, "wildsil=1\n") != NULL && strstr(big, "wildaggro") == NULL);
    CtrSettings_SetWildSilhouette(0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "visiblewilds") == NULL && strstr(text, "wildsil") == NULL && strstr(text, "wildaggro") == NULL);

    /* FREE HMS: off until chosen, written only when on. */
    assert(CtrSettings_FreeHm() == 0);
    CtrSettings_SetFreeHm(1);
    assert(CtrSettings_FreeHm() == 1);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "freehm=1\n") != NULL && strstr(text, "expshare=2\n") != NULL);
    CtrSettings_SetFreeHm(0);
    assert(CtrSettings_FreeHm() == 0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    memset(text, 0, sizeof(text));
    assert(file != NULL && fread(text, 1, sizeof(text) - 1, file) > 0);
    fclose(file);
    assert(strstr(text, "freehm") == NULL);

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
    assert(CtrSettings_Look(CTR_LOOK_LEAF)[0] == 0 && CtrSettings_Look(CTR_LOOK_SURF)[0] == 0 && CtrSettings_Look(4)[0] == 0);
    CtrSettings_SetLook(CTR_LOOK_SURF, "lapras");
    assert(strcmp(CtrSettings_Look(CTR_LOOK_SURF), "lapras") == 0);
    CtrSettings_SetLook(CTR_LOOK_SURF, "");
    assert(CtrSettings_Look(CTR_LOOK_SURF)[0] == 0);
    CtrSettings_SetLook(CTR_LOOK_LEAF, "Bad Name/..");
    assert(CtrSettings_Look(CTR_LOOK_LEAF)[0] == 0);                       /* the first character is not allowed: nothing */
    CtrSettings_SetLook(CTR_LOOK_LEAF, "abcdefghijklmnopqrstu");
    assert(strlen(CtrSettings_Look(CTR_LOOK_LEAF)) == 15);
    CtrSettings_SetLook(CTR_LOOK_LEAF, "");
    assert(CtrSettings_Look(CTR_LOOK_LEAF)[0] == 0);
    file = fopen(CTR_SETTINGS_PATH, "rb");
    {
        char big[512 + 1] = {0};

        assert(file != NULL && fread(big, 1, 512, file) == 512);
        fclose(file);
        assert(strstr(big, "look_red=ash_hgss\n") != NULL && strstr(big, "look_blue=gary_kanto\n") != NULL
               && strstr(big, "look_leaf") == NULL && strstr(big, "expshare=2\n") != NULL && strstr(big, "ball=3\n") != NULL);
    }

    /* A private room's code: 16 digits with at least six different, made from random bytes without bias, kept in the file, and nothing else is a code. */
    {
        char code[CTR_ROOM_CODE_DIGITS + 1];
        uint8_t run[20], few[20];
        char read[256] = {0};

        assert(CtrSettings_RoomCodeOk("4829137650218394") && CtrSettings_RoomCodeOk("0123456789012345") && CtrSettings_RoomCodeOk("1234567000000000"));
        assert(!CtrSettings_RoomCodeOk("0000000000000000") && !CtrSettings_RoomCodeOk("1212121212121212") && !CtrSettings_RoomCodeOk("1234512345123451"));
        assert(!CtrSettings_RoomCodeOk("482913765021839") && !CtrSettings_RoomCodeOk("48291376502183945") && !CtrSettings_RoomCodeOk("482913765021839x"));
        assert(!CtrSettings_RoomCodeOk("123456") && !CtrSettings_RoomCodeOk("") && !CtrSettings_RoomCodeOk(NULL));
        for (int i = 0; i < 20; ++i) {
            run[i] = (uint8_t)(i % 2 ? 250 + i % 5 : (37 * i + 11) % 250);      /* every other byte is one a digit cannot be made of: skipped */
            few[i] = (uint8_t)(i % 2);                                         /* 0101...: two different digits */
        }
        assert(!CtrSettings_MakeRoomCode(code, run, 20) && !CtrSettings_MakeRoomCode(code, few, 20));   /* ten digits from twenty bytes; and a plain one */
        {
            uint8_t more[40];
            char want[CTR_ROOM_CODE_DIGITS + 1];
            int d = 0;

            for (int i = 0; i < 40; ++i)
                more[i] = (uint8_t)(i % 3 == 2 ? 250 + i % 5 : (37 * i + 11) % 250);      /* every third byte is one a digit cannot be made of */
            for (int i = 0; i < 40 && d < CTR_ROOM_CODE_DIGITS; ++i)
                if (more[i] < 250)
                    want[d++] = (char)('0' + more[i] % 10);
            want[d] = 0;
            code[0] = 'x';
            assert(CtrSettings_MakeRoomCode(code, more, 40) && !strcmp(code, want) && CtrSettings_RoomCodeOk(code));
        }
        CtrSettings_SetRoomCode("4829137650218394");
        CtrSettings_SetPrivateRoom(1);
        assert(!strcmp(CtrSettings_RoomCode(), "4829137650218394") && CtrSettings_PrivateRoom());
        file = fopen(CTR_SETTINGS_PATH, "rb");
        assert(file != NULL);
        assert(fread(read, 1, sizeof(read) - 1, file) > 0);
        fclose(file);
        assert(strstr(read, "roomcode=4829137650218394\n") != NULL);
        CtrSettings_SetRoomCode("123456");                                      /* not a code: none is kept */
        assert(!CtrSettings_RoomCode()[0] && !CtrSettings_PrivateRoom());
        CtrSettings_SetRoomCode("4829137650218394");
        CtrSettings_SetRoomCode("0000000000000000");
        assert(!CtrSettings_RoomCode()[0]);
    }

    /* The worst case (every key written, the longest values) must fit the fixed block with room to spare: Format stops
     * silently at the block's end, which would drop the last keys. The block is 512 bytes; keep the text under 320. */
    CtrSettings_SetTurbo(4);
    CtrSettings_SetAutoRun(1);
    CtrSettings_SetEmbers(2);
    assert(CtrSettings_Style() == 1);               /* Poke Ball is the default: no style= line is written for it */
    CtrSettings_SetStyle(0);                        /* the choice of Ascuas is the one that is kept (style=0), so it is in the worst case */
    CtrSettings_SetRoomCode("4829137650218394");
    CtrSettings_SetPrivateRoom(1);
    CtrSettings_SetRepelAgain(1);
    CtrSettings_SetCompanion(2);
    CtrSettings_SetExpShare(2);
    CtrSettings_SetTmReuse(1);
    CtrSettings_SetNuzlocke(1);
    CtrSettings_SetRandomlocke(1);
    CtrSettings_SetNuzDuplicates(1);
    CtrSettings_SetNuzShiny(1);
    CtrSettings_SetNuzLevelCap(1);
    CtrSettings_SetNuzSet(1);
    CtrSettings_SetRandomTarget(2);
    CtrSettings_SetRandomNoLegendary(1);
    CtrSettings_SetFreeHm(1);
    CtrSettings_SetWildDensity(3);
    CtrSettings_SetWildSilhouette(1);
    CtrSettings_SetWildAggro(4);
    CtrSettings_SetLook(CTR_LOOK_RED, "abcdefghijklmno");
    CtrSettings_SetLook(CTR_LOOK_LEAF, "abcdefghijklmno");
    CtrSettings_SetLook(CTR_LOOK_BLUE, "abcdefghijklmno");
    CtrSettings_SetLook(CTR_LOOK_SURF, "abcdefghijklmno");
    CtrSettings_SetLastBall(65535);
    {
        static char worst[4096];
        size_t n;

        file = fopen(CTR_SETTINGS_PATH, "rb");
        assert(file != NULL);
        n = fread(worst, 1, sizeof(worst) - 1, file);
        fclose(file);
        while (n > 0 && worst[n - 1] == '\n' && (n < 2 || worst[n - 2] == '\n'))
            --n;                                                    /* the blank padding */
        printf("settings worst case: %u bytes of text\n", (unsigned)n);
        assert(n < 400);   /* the file holds 512 (a private room's code and flag, 37 bytes, are in this worst case); the wild keys are before the looks and the ball */
        assert(strstr(worst, "nuzset=1\n") && strstr(worst, "randtarget=2\n") && strstr(worst, "randnolegend=1\n"));
        assert(strstr(worst, "visiblewilds=3\n") && strstr(worst, "wildsil=1\n") && strstr(worst, "wildaggro=4\n") && strstr(worst, "look_blue=abcdefghijklmno\n") && strstr(worst, "look_surf=abcdefghijklmno\n") && strstr(worst, "ball=65535\n"));
    }

    remove(CTR_SETTINGS_PATH);
    puts("settings_test ok");
    return 0;
}
