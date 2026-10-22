#ifndef FIRERED_3DS_DAY_NIGHT_H
#define FIRERED_3DS_DAY_NIGHT_H

#include <stdbool.h>
#include <stdint.h>

/*
 * The colour of the outdoors by the hour of the 3DS clock. FireRed has no
 * clock of its own, so nothing in the game reads this: the compositor
 * converts the map's and the sprites' palettes through it when it turns them
 * into texels (3ds_video.c, DayNight), which leaves the game's palettes, the
 * weather and the fades exactly as the game wrote them. Window glass on the
 * tiles listed in 3ds_day_night_lights.h is lit instead of darkened. The
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

/* A GBA colour (BGR555) times the tint, still BGR555. */
static inline uint16_t CtrDayNight_Shade(uint16_t color, uint32_t tint)
{
    unsigned r = color & 31, g = (color >> 5) & 31, b = (color >> 10) & 31;

    r = (r * ((tint >> 16) & 255) + 127) / 255;
    g = (g * ((tint >> 8) & 255) + 127) / 255;
    b = (b * (tint & 255) + 127) / 255;
    return (uint16_t)(r | (g << 5) | (b << 10));
}

/* How lit the windows are under a tint: 0 by day, 256 at night, by its darkest channel. */
static inline unsigned CtrDayNight_Lights(uint32_t tint)
{
    unsigned r = (tint >> 16) & 255, g = (tint >> 8) & 255, b = tint & 255;
    unsigned low = r < g ? r : g;

    low = low < b ? low : b;
    if (low <= CTR_DAY_NIGHT_DARKEST) return 256;
    return (255 - low) * 256 / (255 - CTR_DAY_NIGHT_DARKEST);
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

#endif
