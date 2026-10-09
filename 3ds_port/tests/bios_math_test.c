#include <assert.h>
#include <stdint.h>
#include <string.h>

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

extern int32_t Div(int32_t, int32_t);
extern uint16_t Sqrt(uint32_t);
extern uint16_t ArcTan2(int16_t, int16_t);
extern void ObjAffineSet(const ObjAffineSrc *, void *, int32_t, int32_t);
extern void BgAffineSet(const BgAffineSrc *, BgAffineDst *, int32_t);

int main(void)
{
    assert(Div(-9, 2) == -4);
    assert(Sqrt(65536) == 256);
    assert(ArcTan2(1, 0) == 0);
    assert(ArcTan2(0, 1) == 16384);
    BgAffineSrc bg = { .texX = 1000, .texY = 2000, .scrX = 4,
                       .scrY = 5, .sx = 256, .sy = 256 };
    BgAffineDst out = {0};
    BgAffineSet(&bg, &out, 1);
    assert(out.pa == 256 && out.pb == 0 && out.pc == 0 && out.pd == 256);
    assert(out.dx == 1000 - 1024 && out.dy == 2000 - 1280);
    ObjAffineSrc obj = { .xScale = 256, .yScale = 256 };
    int16_t matrix[4] = {0};
    ObjAffineSet(&obj, matrix, 1, 2);
    assert(matrix[0] == 256 && matrix[1] == 0);
    assert(matrix[2] == 0 && matrix[3] == 256);
    return 0;
}
