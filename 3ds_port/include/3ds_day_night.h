#ifndef FIRERED_3DS_DAY_NIGHT_H
#define FIRERED_3DS_DAY_NIGHT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The colour of the outdoors by the hour of the 3DS clock. FireRed has no
 * clock of its own, so nothing in the game reads this: the compositor draws
 * the field's layers and sprites through it on the GPU (3ds_video.c,
 * DayNightEnv, with CtrDayNight_Grade below), which leaves the game's
 * palettes, the weather and the fades exactly as the game wrote them. Window
 * glass on the tiles listed in 3ds_day_night_lights.h is lit instead of
 * darkened, by an overlay drawn after its layer. The
 * hours follow Merrp's day/night system for pokeemerald (dawn, day, dusk,
 * night, blended through the minutes in between); the colours are this
 * port's own. Shared with the host test.
 */
typedef struct
{
    uint32_t second;            /* of the day, 0 to 86400 */
    uint8_t r, g, b;            /* what each channel is multiplied by, 255 = unchanged */
} CtrDayNightKey;

#define CTR_DAY_NIGHT_DAY_SECONDS 86400u

static const CtrDayNightKey kCtrDayNightKeys[] = {
    {     0u,  92, 107, 168},   /* night */
    { 18000u,  92, 107, 168},   /* 05:00, night until here */
    { 23400u, 255, 214, 219},   /* 06:30, dawn */
    { 32400u, 255, 255, 255},   /* 09:00, day */
    { 63000u, 255, 255, 255},   /* 17:30, day until here */
    { 68400u, 255, 184, 153},   /* 19:00, dusk */
    { 75600u,  92, 107, 168},   /* 21:00, night */
    { 86400u,  92, 107, 168},
};

/* The tint at a second of the day as 0xRRGGBB; 0xffffff is the plain day. */
static inline uint32_t CtrDayNight_Tint(uint32_t second)
{
    const unsigned count = sizeof(kCtrDayNightKeys) / sizeof(kCtrDayNightKeys[0]);
    unsigned i = 1;

    second %= CTR_DAY_NIGHT_DAY_SECONDS;
    while (i < count - 1 && kCtrDayNightKeys[i].second <= second)
        ++i;
    const CtrDayNightKey *a = &kCtrDayNightKeys[i - 1], *b = &kCtrDayNightKeys[i];
    uint32_t span = b->second - a->second, at = second - a->second;
    uint32_t r = a->r + (int32_t)(b->r - a->r) * (int32_t)at / (int32_t)span;
    uint32_t g = a->g + (int32_t)(b->g - a->g) * (int32_t)at / (int32_t)span;
    uint32_t bl = a->b + (int32_t)(b->b - a->b) * (int32_t)at / (int32_t)span;
    return (r << 16) | (g << 8) | bl;
}

/* The darkest the tint gets (the night's red channel): the windows are fully lit there. */
#define CTR_DAY_NIGHT_DARKEST 92u
/* Lamplight, BGR555: (255, 197, 90). */
#define CTR_DAY_NIGHT_LAMP ((11u << 10) | (24u << 5) | 31u)

/* How lit the windows are under a tint: 0 by day, 256 at night, by its darkest channel. */
static inline unsigned CtrDayNight_Lights(uint32_t tint)
{
    unsigned r = (tint >> 16) & 255, g = (tint >> 8) & 255, b = tint & 255;
    unsigned low = r < g ? r : g;

    low = low < b ? low : b;
    if (low <= CTR_DAY_NIGHT_DARKEST) return 256;
    return (255 - low) * 256 / (255 - CTR_DAY_NIGHT_DARKEST);
}

/* The night's own grade: a quarter of the way to grey, darker mid-tones
 * (gamma 1.3, from 5 bits to 8) and moonlight blue, so the night keeps its
 * contrast instead of the flat multiply going muddy. */
static const uint8_t kCtrDayNightGamma[32] = {
    0, 3, 7, 12, 18, 24, 30, 37, 44, 51, 59, 66, 74, 82, 91, 99,
    108, 117, 126, 135, 144, 154, 163, 173, 183, 193, 203, 213, 223, 234, 244, 255,
};
static const uint8_t kCtrDayNightMoon[3] = {82, 102, 194};

static inline uint16_t CtrDayNight_Moon(uint16_t color)
{
    unsigned c[3] = {color & 31u, (color >> 5) & 31u, (color >> 10) & 31u};
    unsigned lum = (c[0] * 77 + c[1] * 150 + c[2] * 29) >> 8;
    uint16_t out = 0;

    for (unsigned i = 0; i < 3; ++i)
    {
        unsigned v = (c[i] * 3 + lum + 2) >> 2;

        v = (kCtrDayNightGamma[v] * kCtrDayNightMoon[i] + 127) / 255;
        out |= (uint16_t)(((v + 4) >> 3 > 31 ? 31 : (v + 4) >> 3) << (i * 5));
    }
    return out;
}

/* A GBA colour (BGR555) under the hour, still BGR555: the hour's colour
 * multiplied in, going over to the night's grade as the night deepens
 * (squared, so a dusk stays warm). */
static inline uint16_t CtrDayNight_Shade(uint16_t color, uint32_t tint)
{
    unsigned r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;
    unsigned night = CtrDayNight_Lights(tint);
    uint16_t moon, out = 0;

    r = (r * ((tint >> 16) & 255) + 127) / 255;
    g = (g * ((tint >> 8) & 255) + 127) / 255;
    b = (b * (tint & 255) + 127) / 255;
    night = night * night / 256;
    if (!night) return (uint16_t)(r | (g << 5) | (b << 10));
    moon = CtrDayNight_Moon(color);
    out |= (uint16_t)((r * (256 - night) + (moon & 31) * night + 128) >> 8);
    out |= (uint16_t)(((g * (256 - night) + ((moon >> 5) & 31) * night + 128) >> 8) << 5);
    out |= (uint16_t)(((b * (256 - night) + ((moon >> 10) & 31) * night + 128) >> 8) << 10);
    return out;
}

/*
 * The hour as the GPU applies it (3ds_video.c, DayNightEnv): two texture
 * combiner stages after Citro2D's own, the way pokeemerald-3Ds-dualscreen
 * tints its overworld, so a new hour changes two constants and re-decodes
 * nothing. Stage 4 takes the colour a little towards its green (keep/255 of
 * the colour stays), stage 5 multiplies by k. The night's grade above
 * (CtrDayNight_Shade) has a gamma no combiner can do; this is its least
 * squares fit over the field palettes (FireRed's General and Pallet Town, at
 * 23:00): 0.144 of the way to green, times (77, 96, 180), off by 4.7/255 on
 * average. Through the evening it goes from the plain multiply over to that
 * fit as the night deepens (squared, as Shade does). white (0-256) is how far
 * the layer is faded or brightened towards white, which has to stay white.
 */
typedef struct
{
    uint8_t keep;
    uint8_t k[3];
} CtrDayNightGrade;

static const uint8_t kCtrDayNightGradeMoon[3] = {77, 96, 180};
#define CTR_DAY_NIGHT_GRADE_GREEN 37u

static inline CtrDayNightGrade CtrDayNight_Grade(uint32_t tint, unsigned white)
{
    unsigned night = CtrDayNight_Lights(tint);
    CtrDayNightGrade grade;

    night = night * night / 256;
    if (white > 256) white = 256;
    grade.keep = (uint8_t)(255 - (CTR_DAY_NIGHT_GRADE_GREEN * night * (256 - white) >> 16));
    for (unsigned i = 0; i < 3; ++i)
    {
        unsigned hour = (tint >> (16 - i * 8)) & 255;
        unsigned k = (hour * (256 - night) + kCtrDayNightGradeMoon[i] * night) >> 8;

        grade.k[i] = (uint8_t)(k + ((255 - k) * white >> 8));
    }
    return grade;
}

/* What the GPU makes of a GBA colour under a grade, as BGR555 (the backdrop,
 * which is a clear colour and not drawn through the combiners). */
static inline uint16_t CtrDayNight_Graded(uint16_t color, CtrDayNightGrade grade)
{
    unsigned c[3], out = 0;

    for (unsigned i = 0; i < 3; ++i)
    {
        c[i] = (color >> (i * 5)) & 31;
        c[i] = (c[i] << 3) | (c[i] >> 2);
    }
    for (unsigned i = 0; i < 3; ++i)
    {
        unsigned v = (c[i] * grade.keep + c[1] * (255u - grade.keep) + 127) / 255;

        v = (v * grade.k[i] + 127) / 255;
        out |= ((v * 31 + 127) / 255) << (i * 5);
    }
    return (uint16_t)out;
}

/* Window glass: a light blue. Which tiles are windows is the lights table's
 * business; this only keeps a window's frame and sill out of the light. */
static inline bool CtrDayNight_Glass(uint16_t color)
{
    unsigned r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;

    r = (r << 3) | (r >> 2);
    g = (g << 3) | (g >> 2);
    b = (b << 3) | (b >> 2);
    return b >= 170 && b >= r + 50 && g >= 120;
}

/* A glass colour under the tint, taken towards lamplight by lights/256. */
static inline uint16_t CtrDayNight_Lit(uint16_t color, uint32_t tint, unsigned lights)
{
    uint16_t shaded = CtrDayNight_Shade(color, tint);
    uint16_t out = 0;

    for (unsigned shift = 0; shift < 15; shift += 5)
    {
        unsigned from = (shaded >> shift) & 31, to = (CTR_DAY_NIGHT_LAMP >> shift) & 31;
        out |= (uint16_t)(((from * (256 - lights) + to * lights + 128) >> 8) << shift);
    }
    return out;
}

/* A lights table entry (3ds_day_night_lights.h) of a building open all night. */
#define CTR_LIGHT_PUBLIC (1u << 10)
/* A lights table entry whose glass is not blue (grey panes, dark arches): the
 * word after it is the mask of its glass colour indices, lit whatever their
 * colour. CTR_LIGHT_GLASS writes both words. */
#define CTR_LIGHT_MASKED (1u << 11)
#define CTR_LIGHT_GLASS(entry, mask) ((entry) | CTR_LIGHT_MASKED), (uint16_t)(mask)

/*
 * Not every window is lit all night. Each lit window (the light table's
 * houses; public buildings stay lit) has its own hours from a hash of where
 * it stands: lights on between 18:30 and 20:00, off one by one between 21:45
 * and 02:15, about one in eight on all night, and about a third on again
 * from 05:00 to 06:12 until the day puts them out. The same window keeps the
 * same hours every night, so walking past does not make them flicker.
 */
static inline uint32_t CtrDayNight_WindowHash(int x, int y, unsigned map)
{
    uint32_t v = (uint32_t)x * 73856093u ^ (uint32_t)y * 19349663u ^ (map + 1u) * 83492791u;

    v ^= v >> 13;
    v *= 0x5bd1e995u;
    v ^= v >> 15;
    v *= 0x27d4eb2du;
    return v ^ (v >> 16);
}

static inline bool CtrDayNight_WindowOn(uint32_t hash, uint32_t second)
{
    uint32_t noon = (second % CTR_DAY_NIGHT_DAY_SECONDS + 43200u) % CTR_DAY_NIGHT_DAY_SECONDS;
    uint32_t on = 23400u + (hash & 255u) * 5400u / 256u;              /* 18:30 + up to 90 min */
    unsigned stay = (hash >> 8) & 255u;
    uint32_t off = stay >= 224u ? CTR_DAY_NIGHT_DAY_SECONDS             /* all night */
        : 35100u + stay * 16200u / 224u;                                /* 21:45 + up to 270 min */

    if (noon >= on && noon < off) return true;
    /* Early risers: from 05:00 plus up to 72 min, until noon. */
    return ((hash >> 16) & 255u) < 90u && noon >= 61200u + ((hash >> 24) & 255u) * 4320u / 256u;
}

#endif
