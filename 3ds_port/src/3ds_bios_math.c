/* Portable replacements for the GBA BIOS math and affine calls. */
#include <math.h>
#include <stdint.h>
#include <string.h>

#define ANGLE_SCALE (6.28318530717958647692 / 65536.0)

typedef struct {
    int32_t texX, texY;
    int16_t scrX, scrY, sx, sy;
    uint16_t alpha;
} BgAffineSrc;

typedef struct {
    int16_t pa, pb, pc, pd;
    int32_t dx, dy;
} BgAffineDst;

typedef struct {
    int16_t xScale, yScale;
    uint16_t rotation;
} ObjAffineSrc;

static int16_t Fixed16(double value)
{
    return (int16_t)(int32_t)lrint(value);
}

int32_t Div(int32_t numerator, int32_t denominator)
{
    return denominator ? numerator / denominator : 0;
}

uint16_t Sqrt(uint32_t value)
{
    uint32_t result = (uint32_t)sqrt((double)value);
    return (uint16_t)result;
}

uint16_t ArcTan2(int16_t x, int16_t y)
{
    double radians = atan2((double)y, (double)x);
    return (uint16_t)(int32_t)lrint(radians / ANGLE_SCALE);
}

void ObjAffineSet(const ObjAffineSrc *src, void *dest, int32_t count, int32_t offset)
{
    uint8_t *out = dest;
    if (offset < 2 || count <= 0)
        return;
    for (int32_t i = 0; i < count; ++i, out += 4 * offset)
    {
        double s = sin(src[i].rotation * ANGLE_SCALE);
        double c = cos(src[i].rotation * ANGLE_SCALE);
        int16_t values[4] = {
            Fixed16(src[i].xScale * c), Fixed16(-src[i].xScale * s),
            Fixed16(src[i].yScale * s), Fixed16(src[i].yScale * c),
        };
        for (int j = 0; j < 4; ++j)
            memcpy(out + j * offset, &values[j], sizeof(values[j]));
    }
}

void BgAffineSet(const BgAffineSrc *src, BgAffineDst *dest, int32_t count)
{
    for (int32_t i = 0; i < count; ++i)
    {
        double s = sin(src[i].alpha * ANGLE_SCALE);
        double c = cos(src[i].alpha * ANGLE_SCALE);
        dest[i].pa = Fixed16(src[i].sx * c);
        dest[i].pb = Fixed16(-src[i].sx * s);
        dest[i].pc = Fixed16(src[i].sy * s);
        dest[i].pd = Fixed16(src[i].sy * c);
        dest[i].dx = src[i].texX - dest[i].pa * src[i].scrX - dest[i].pb * src[i].scrY;
        dest[i].dy = src[i].texY - dest[i].pc * src[i].scrX - dest[i].pd * src[i].scrY;
    }
}
