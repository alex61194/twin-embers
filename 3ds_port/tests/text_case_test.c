/* The lower screen's game names in proper case (include/3ds_text_case.h). Pure C, no game data. */
#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "3ds_text_case.h"

static void is(const char *in, const char *want)
{
    char buf[64];

    snprintf(buf, sizeof(buf), "%s", in);
    CtrTextCase(buf);
    if (strcmp(buf, want)) {
        fprintf(stderr, "\"%s\" gave \"%s\", wanted \"%s\"\n", in, buf, want);
        assert(0);
    }
    CtrTextCase(buf);               /* idempotent */
    assert(!strcmp(buf, want));
}

int main(void)
{
    /* Plain ASCII names. */
    is("CHARIZARD", "Charizard");
    is("POTION", "Potion");
    is("FLAMETHROWER", "Flamethrower");
    is("FIRE", "Fire");
    is("VIRIDIAN FOREST", "Viridian Forest");
    is("X", "X");
    is("", "");
    is("???", "???");
    /* Word starts: space, hyphen, period, slash. */
    is("HO-OH", "Ho-Oh");
    is("MR. MIME", "Mr. Mime");
    is("S.S. TICKET", "S.S. Ticket");
    is("EXP. ALL", "Exp. All");
    is("X SP. DEF", "X Sp. Def");
    is("SAND-ATTACK", "Sand-Attack");
    is("FARFETCH'D", "Farfetch'd");
    is("WATER'S EDGE", "Water's Edge");
    is("PORYGON2", "Porygon2");
    is("ROUTE 12", "Route 12");
    /* Acronyms and short codes stay. */
    is("HP", "HP");
    is("PP UP", "PP Up");
    is("HP UP", "HP Up");
    is("PC", "PC");
    is("TM01", "TM01");
    is("HM05", "HM05");
    is("TM CASE", "TM Case");
    is("TM01 FOCUS PUNCH", "TM01 Focus Punch");
    is("B1F", "B1F");
    is("5F", "5F");
    /* The e acute of POKe is small in the game's caps: it neither blocks nor starts a word. */
    is("POK\xC3\xA9MON", "Pok\xC3\xA9mon");
    is("POK\xC3\xA9 BALL", "Pok\xC3\xA9 Ball");
    is("POK\xC3\xA9" "DEX", "Pok\xC3\xA9" "dex");
    /* Latin-1 letters as UTF-8. */
    is("\xC3\x89TOILE", "\xC3\x89toile");           /* a capital E acute starts the word */
    is("ZUBAT \xC3\x89LECTRIQUE", "Zubat \xC3\x89lectrique");
    is("MA\xC3\x91" "ANA", "Ma\xC3\xB1" "ana");     /* N tilde */
    is("\xC3\x9C" "BER", "\xC3\x9C" "ber");
    is("GR\xC3\x9C" "N", "Gr\xC3\xBC" "n");
    /* FireRed's own codes for the European letters ("\x01" + code). */
    is("\x01\x06" "CLAIR", "\x01\x06" "clair");     /* E acute (0x06) first */
    is("MA\x01\x14" "ANA", "Ma\x01\x29" "ana");     /* N tilde 0x14 -> 0x29 */
    is("\x01\xF5" "NIDORAN", "\x01\xF5" "Nidoran");  /* a sign is no letter: the name still starts the word */
    is("NIDORAN\x01\xF6", "Nidoran\x01\xF6");        /* the gender sign is kept */
    /* Mixed-case text is left as it is: German names, nicknames, sentences. */
    is("Pikachu", "Pikachu");
    is("Gl\xC3\xBCrndrache", "Gl\xC3\xBCrndrache");
    is("RedBull", "RedBull");
    is("Lv50", "Lv50");
    is("CHARIZARD  Lv50", "CHARIZARD  Lv50");
    is("mr. mime", "mr. mime");
    is("Fire", "Fire");
    is("NAME: ", "Name: ");
    is("IDNo.", "IDNo.");
    /* A lowercase accented letter blocks it, an uppercase one alone does not. */
    is("PIK\xC3\xA0", "PIK\xC3\xA0");
    is("PIK\x01\x16", "PIK\x01\x16");
    /* Japanese codes and other multibyte marks. */
    is("\x02\x20\x02\x21", "\x02\x20\x02\x21");
    is("A\xE2\x80\x99S", "A\xE2\x80\x99s");
    is("\xC2\xA5", "\xC2\xA5");
    /* Digits and symbols only. */
    is("123", "123");
    is("--", "--");
    /* Never longer, never past the NUL: the bytes after it are untouched, a cut escape is safe. */
    {
        char buf[8] = {'A', 'B', 'C', 0, 'D', 'E', 'F', 'G'};

        CtrTextCase(buf);
        assert(!strcmp(buf, "Abc") && buf[4] == 'D' && buf[7] == 'G');
        char cut[4] = {'A', (char)0xC3, 0, 'Z'};
        CtrTextCase(cut);
        assert(cut[3] == 'Z' && cut[2] == 0);
        char cut2[4] = {'A', 0x01, 0, 'Z'};
        CtrTextCase(cut2);
        assert(cut2[3] == 'Z');
        CtrTextCase(NULL);
    }
    /* Status words, Party tags and the "BALL" tail arrive in the game's raw ALL CAPS or already cased: both must match
     * (the Summary's "PSN" vs "Psn", the Party's "NOT ABLE" vs "Not able", "Great Ball" vs "BALL"). */
    assert(CtrTextCase_Eq("PSN", "Psn") && CtrTextCase_Eq("Psn", "PSN") && CtrTextCase_Eq("PKR", "Pkr"));
    assert(CtrTextCase_Eq("NOT ABLE", "Not able") && CtrTextCase_Eq("LEARNED", "Learned") && CtrTextCase_Eq("KENNT ATT.", "Kennt Att."));
    assert(CtrTextCase_Eq("", "") && !CtrTextCase_Eq("PSN", "Psnx") && !CtrTextCase_Eq("PSN", "Prz") && !CtrTextCase_Eq("A", ""));
    assert(!CtrTextCase_Eq("Ã", "Ã¤"));               /* only ASCII letters fold */
    assert(CtrTextCase_EqN("NO USE: a TM", "No use", 6) && !CtrTextCase_EqN("NO US", "No use", 6));
    {
        char ball[] = "Great Ball", mb[] = "Meisterball";

        assert(CtrTextCase_Eq(ball + strlen(ball) - 4, "BALL") && CtrTextCase_Eq(mb + strlen(mb) - 4, "BALL"));
        assert(!CtrTextCase_Eq("Great Bell" + 6, "BALL"));
    }
    puts("text case: ok");
    return 0;
}
