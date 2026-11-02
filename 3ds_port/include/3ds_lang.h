#ifndef CTR_LANG_H
#define CTR_LANG_H

/* The language of the lower screen's own words, chosen when the port is built: one .3dsx per language, made from that
 * language's FireRed (make TWIN_LANG=es builds the Spanish one). The game's own text comes from the ROM; this only
 * picks the port's words. CTR_TR("English", "Spanish") is a string literal, so it also works in static tables and as
 * a printf format. Both are UTF-8; 3ds_poketouch.c draws the accents, Ñ, ¿ and ¡ with FireRed's font. */
#ifndef CTR_LANG_ES
#define CTR_LANG_ES 0
#endif

#if CTR_LANG_ES
#define CTR_TR(en, es) es
#else
#define CTR_TR(en, es) en
#endif

#endif
