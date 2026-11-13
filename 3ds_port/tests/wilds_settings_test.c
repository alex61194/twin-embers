/* settings.cfg and the wild Pokemon keys (visiblewilds = the density, wildsil, wildaggro): reading an old file, a new one, an
 * unknown or damaged one, and a file cut at every byte. 3ds_settings.c reads the file once per process, so each case runs this
 * program again with its number (and a cut length for the cuts). Built with CTR_SETTINGS_PATH "build/wilds_settings_test.cfg". */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_settings.h"

#define BLOCK 512   /* the file's fixed size (SETTINGS_FILE_BYTES in 3ds_settings.c) */

static const char kFull[] =
    "turbo=3\nautorun=1\nrepel=1\nembers=2\ncompanion=2\nexpshare=2\ntmreuse=1\nnuzlocke=1\nrandomlocke=1\nnuzdupes=1\nnuzshiny=1\nnuzcap=1\nfreehm=1\n"
    "wildsil=1\nwildaggro=4\nvisiblewilds=1\nlook_red=abcdefghijklmno\nlook_leaf=abcdefghijklmno\nlook_blue=abcdefghijklmno\nball=65535\n";

static void Put(const char *text, size_t length, int pad)
{
    FILE *f = fopen(CTR_SETTINGS_PATH, "wb");
    char block[BLOCK];

    assert(f != NULL);
    if (pad)
    {
        memset(block, '\n', sizeof(block));
        memcpy(block, text, length < sizeof(block) ? length : sizeof(block));
        fwrite(block, 1, sizeof(block), f);
    }
    else
        fwrite(text, 1, length, f);
    fclose(f);
}

#define PutS(text, pad) Put(text, strlen(text), pad)

static void Expect(int density, int sil, int aggro)
{
    if (CtrSettings_WildDensity() != density || CtrSettings_WildSilhouette() != sil || CtrSettings_WildAggro() != aggro)
    {
        printf("FAIL: density %d sil %d aggro %d, wanted %d %d %d\n", CtrSettings_WildDensity(), CtrSettings_WildSilhouette(),
               CtrSettings_WildAggro(), density, sil, aggro);
        exit(1);
    }
}

static int Case(int n, int cut)
{
    char junk[2200];

    switch (n)
    {
    case 0:     /* the first version's file: visiblewilds=1 was "on", with neither new key: NORMAL, and nothing else lost */
        PutS("turbo=3\nautorun=1\nrepel=0\nvisiblewilds=1\nball=7\n", 1);
        Expect(2, 0, 0);
        assert(CtrSettings_Turbo() == 3 && CtrSettings_AutoRun() == 1 && CtrSettings_RepelAgain() == 0 && CtrSettings_LastBall() == 7);
        break;
    case 1:     /* the new file with FEW (a 1 that has wildsil and wildaggro) stays FEW */
        PutS("wildsil=0\nwildaggro=0\nvisiblewilds=1\n", 1);
        Expect(1, 0, 0);
        break;
    case 2:
        PutS("wildsil=1\nwildaggro=4\nvisiblewilds=3\n", 1);
        Expect(3, 1, 4);
        break;
    case 3:     /* unknown keys, a line without '=', binary noise, a key twice: the known ones still read */
        PutS("foo=1\nvisiblewilds=2\nwildzzz=9\n\x01\x02junk\n=\nwildaggro=3\nwildaggro=2\nvisiblewilds=3 \n", 1);
        Expect(3, 0, 2);
        break;
    case 4:     /* out of range */
        PutS("visiblewilds=9\nwildsil=7\nwildaggro=12\n", 1);
        Expect(0, 0, 0);
        break;
    case 5:     /* negative and huge numbers, empty values */
        PutS("visiblewilds=-1\nwildsil=-1\nwildaggro=99999999999\n", 1);
        Expect(0, 0, 0);
        PutS("visiblewilds=\nwildsil=\nwildaggro=\n", 1);
        break;
    case 6:     /* an empty file and one of blank lines only: the defaults (the file is read once, so the second case is its own) */
        PutS("", 0);
        Expect(0, 0, 0);
        break;
    case 7:
        PutS("", 1);
        Expect(0, 0, 0);
        break;
    case 8:     /* a file longer than the block, with the keys after a long junk line: all are read */
        memset(junk, 'x', sizeof(junk));
        memcpy(junk + 2100, "\nwildsil=1\nwildaggro=2\nvisiblewilds=2\n", 39);
        Put(junk, 2139, 0);
        Expect(2, 1, 2);
        break;
    case 9:     /* every setter, then the next process reads it back (case 10) */
        remove(CTR_SETTINGS_PATH);
        CtrSettings_SetWildDensity(3);
        CtrSettings_SetWildSilhouette(1);
        CtrSettings_SetWildAggro(2);
        Expect(3, 1, 2);
        break;
    case 10:
        Expect(3, 1, 2);
        break;
    case 11:    /* an old file's NORMAL, rewritten by the new code, stays NORMAL (and not a second migration: 2 is 2) */
        PutS("visiblewilds=1\n", 1);
        Expect(2, 0, 0);
        CtrSettings_SetWildAggro(1);        /* any change rewrites the file in the new format */
        break;
    case 12:
        Expect(2, 0, 1);
        break;
    case 13:    /* FEW written by the new code reads back as FEW, even with the other two at 0 */
        remove(CTR_SETTINGS_PATH);
        CtrSettings_SetWildDensity(1);
        Expect(1, 0, 0);
        break;
    case 14:
        Expect(1, 0, 0);
        break;
    case 20:    /* the file cut after `cut` bytes (no padding): never a wrong density, never a changed other value */
        Put(kFull, (size_t)cut, 0);
        {
            int d = CtrSettings_WildDensity();

            if (d != 0 && d != 1)
            {
                printf("FAIL: a file cut at %d reads density %d\n", cut, d);
                return 1;
            }
            if (CtrSettings_WildSilhouette() != 0 && CtrSettings_WildSilhouette() != 1)
                return 1;
            if (CtrSettings_WildAggro() != 0 && CtrSettings_WildAggro() != 4)
            {
                printf("FAIL: a file cut at %d reads aggro %d\n", cut, CtrSettings_WildAggro());
                return 1;
            }
            if (d == 1 && (CtrSettings_WildSilhouette() != 1 || CtrSettings_WildAggro() != 4))
            {
                printf("FAIL: a file cut at %d reads FEW without its companion keys\n", cut);
                return 1;
            }
        }
        break;
    default:
        return 1;
    }
    return 0;
}

int main(int argc, char **argv)
{
    char cmd[512];

    if (argc >= 2)
        return Case(atoi(argv[1]), argc >= 3 ? atoi(argv[2]) : 0);
    for (int n = 0; n <= 14; ++n)
    {
        snprintf(cmd, sizeof(cmd), "\"%s\" %d", argv[0], n);
        if (system(cmd) != 0)
        {
            printf("FAIL: case %d\n", n);
            return 1;
        }
    }
    for (int cut = 0; cut <= (int)strlen(kFull); ++cut)
    {
        snprintf(cmd, sizeof(cmd), "\"%s\" 20 %d", argv[0], cut);
        if (system(cmd) != 0)
        {
            printf("FAIL: cut %d\n", cut);
            return 1;
        }
    }
    remove(CTR_SETTINGS_PATH);
    puts("wilds_settings_test ok");
    return 0;
}
