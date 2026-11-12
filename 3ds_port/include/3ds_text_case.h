#ifndef CTR_TEXT_CASE_H
#define CTR_TEXT_CASE_H

#include <stddef.h>
#include <string.h>

/*
 * The lower screen's game names in proper case. FireRed's own text is ALL CAPS ("CHARIZARD", "MR. MIME"); the lower
 * screen shows "Charizard", "Mr. Mime". Done at display time on the UTF-8 the bridge (ctr_poketouch.c ToUtf8) gives:
 *   - ASCII letters; "\xC3\xXX" Latin-1 letters; FireRed's own codes for the European letters ("\x01" + code, the
 *     lower screen draws them as they are); a Japanese name ("\x02" + code) is left alone.
 *   - Only a string with no lowercase letter at all is changed (the e acute of POKe does not count: the game's caps
 *     keep it small). Mixed text (a German name, a nickname, a sentence) is never touched, so it is idempotent.
 *   - Each word (split at space, hyphen, period, slash): first letter upper, the rest lower. HP, PP, PC, TM, HM and
 *     TM01 / HM01 style codes stay as they are; a letter after a digit ("1F", "B1F") stays upper.
 * The text keeps its length: the change is in place and never writes past the terminating NUL.
 */

/* 1 upper, -1 lower, 0 neither (digits, marks, a lower letter that is not cased such as the sharp s is lower). */
static inline int CtrTextCase_Kind(unsigned char c)
{
    if (c >= 'A' && c <= 'Z')
        return 1;
    return c >= 'a' && c <= 'z' ? -1 : 0;
}

/* FireRed's code (after "\x01") of a European letter: 1 upper, -1 lower, 0 none; `other` its other case. */
static inline int CtrTextCase_Code(unsigned char c, unsigned char *other)
{
    if ((c >= 0x01 && c <= 0x14 && c != 0x0A)) {
        *other = (unsigned char)(c + 0x15);
        return 1;
    }
    if (c == 0x5A) {
        *other = 0x6F;
        return 1;
    }
    if (c >= 0x16 && c <= 0x29 && c != 0x1F) {
        *other = (unsigned char)(c - 0x15);
        return -1;
    }
    if (c == 0x6F) {
        *other = 0x5A;
        return -1;
    }
    *other = c;
    return c == 0x15 ? -1 : 0;      /* the sharp s */
}

/* Latin-1 letters as UTF-8 (the second byte of "\xC3\xXX"): 1 upper, -1 lower. */
static inline int CtrTextCase_Latin(unsigned char c)
{
    if (c >= 0x80 && c <= 0x9E && c != 0x97)
        return 1;
    return c >= 0xA0 && c != 0xB7 ? -1 : 0;     /* 0xB7 is the division sign */
}

static inline int CtrTextCase_IsSep(unsigned char c)
{
    return c == ' ' || c == '-' || c == '.' || c == '/' || c == '\n';
}

/* A word is kept as it is when it is one of the short codes, alone or followed by digits. */
static inline int CtrTextCase_IsCode(const char *w, size_t n)
{
    static const char *const codes[] = {"HP", "PP", "PC", "TM", "HM"};
    size_t k = 0;

    while (k < n && w[k] >= 'A' && w[k] <= 'Z')
        ++k;
    for (size_t i = 0; i < n; ++i)      /* only letters then digits */
        if (i >= k && (w[i] < '0' || w[i] > '9'))
            return 0;
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); ++i)
        if (k == 2 && w[0] == codes[i][0] && w[1] == codes[i][1])
            return 1;
    return 0;
}

static inline void CtrTextCase(char *s)
{
    unsigned char *p = (unsigned char *)s;
    size_t i, start, upper = 0;

    if (s == NULL)
        return;
    /* Only a string that is all upper case: one lower letter (e acute aside), or kana, leaves it alone. */
    for (i = 0; p[i]; ++i) {
        unsigned char o;

        if (p[i] == 0x02)
            return;
        if (p[i] == 0x01 && p[i + 1]) {
            int k = CtrTextCase_Code(p[i + 1], &o);

            if (k < 0 && p[i + 1] != 0x1B)
                return;
            upper += k > 0;
            ++i;
        } else if (p[i] == 0xC3 && p[i + 1]) {
            int k = CtrTextCase_Latin(p[i + 1]);

            if (k < 0 && p[i + 1] != 0xA9)
                return;
            upper += k > 0;
            ++i;
        } else if (p[i] >= 0x80) {
            continue;                   /* another multibyte character: a mark, not a letter */
        } else {
            int k = CtrTextCase_Kind(p[i]);

            if (k < 0)
                return;
            upper += k > 0;
        }
    }
    if (!upper)
        return;
    for (start = 0; p[start];) {
        size_t end = start;
        int first = 1, digit = 0;

        while (p[end] && !CtrTextCase_IsSep(p[end]))
            ++end;
        if (!CtrTextCase_IsCode(s + start, end - start)) {
            for (i = start; i < end; ++i) {
                unsigned char o;

                if (p[i] == 0x01 && p[i + 1]) {
                    int k = CtrTextCase_Code(p[i + 1], &o);

                    if (k > 0 && !first && !digit)
                        p[i + 1] = o;
                    first = first && k == 0;
                    ++i;
                } else if (p[i] == 0xC3 && p[i + 1]) {
                    int k = CtrTextCase_Latin(p[i + 1]);

                    if (k > 0 && !first && !digit)
                        p[i + 1] = (unsigned char)(p[i + 1] + 0x20);
                    first = first && k == 0;
                    ++i;
                } else if (p[i] >= 0x80) {
                    continue;
                } else if (p[i] >= '0' && p[i] <= '9') {
                    digit = 1;
                    first = 0;
                } else if (CtrTextCase_Kind(p[i]) > 0) {
                    if (!first && !digit)
                        p[i] = (unsigned char)(p[i] + 0x20);
                    first = 0;
                }
            }
        }
        start = end;
        while (p[start] && CtrTextCase_IsSep(p[start]))
            ++start;
    }
}

#endif
