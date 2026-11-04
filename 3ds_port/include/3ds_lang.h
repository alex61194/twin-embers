#ifndef CTR_LANG_H
#define CTR_LANG_H

/* The language of the lower screen's own words, chosen when the port is built: one .3dsx per language, made from that
 * language's FireRed (make TWIN_LANG=es builds the Spanish one, make TWIN_LANG=de builds German). The game's own text
 * comes from the ROM; this only picks the port's words. CTR_TR("English", "Spanish", "German") is a string literal,
 * so it also works in static tables and as a printf format. UTF-8; 3ds_poketouch.c draws the accents, umlauts,
 * and special characters with FireRed's font. */
#ifndef CTR_LANG_ES
#define CTR_LANG_ES 0
#endif

#ifndef CTR_LANG_DE
#define CTR_LANG_DE 0
#endif

#define CTR_TR_GLUE(a, b) a b
#define CTR_TR_PICK(_1, _2, _3, NAME, ...) NAME

#if CTR_LANG_DE
#define CTR_TR_2(en, es) en
#define CTR_TR_3(en, es, de) de
#define CTR_TR(...) CTR_TR_GLUE(CTR_TR_PICK, (__VA_ARGS__, CTR_TR_3, CTR_TR_2))(__VA_ARGS__)
#elif CTR_LANG_ES
#define CTR_TR_2(en, es) es
#define CTR_TR_3(en, es, de) es
#define CTR_TR(...) CTR_TR_GLUE(CTR_TR_PICK, (__VA_ARGS__, CTR_TR_3, CTR_TR_2))(__VA_ARGS__)
#else
#define CTR_TR_2(en, es) en
#define CTR_TR_3(en, es, de) en
#define CTR_TR(...) CTR_TR_GLUE(CTR_TR_PICK, (__VA_ARGS__, CTR_TR_3, CTR_TR_2))(__VA_ARGS__)
#endif

#endif
