#ifndef FIRERED_3DS_DAY_NIGHT_H
#define FIRERED_3DS_DAY_NIGHT_H

#include <stdint.h>

/*
 * The colour of the outdoors by the hour of the 3DS clock. FireRed has no
 * clock of its own, so nothing in the game reads this: the compositor
 * multiplies the field layers and its sprites by it (3ds_video.c, DayNight),
 * which leaves the palettes, the weather and the fades exactly as the game
 * wrote them. The hours follow Merrp's day/night system for pokeemerald
 * (dawn, day, dusk, night, blended through the minutes in between); the
 * colours are this port's own. Shared with the host test.
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

#endif
