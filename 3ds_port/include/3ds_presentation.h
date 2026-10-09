#ifndef CTR_PRESENTATION_H
#define CTR_PRESENTATION_H
#include <stdbool.h>

/* Coordinates here belong to the GPU, never to FireRed's camera or UI. */
typedef struct
{
    int width, height, textureHeight, integerScale;
    float x, y, scale;
} CtrPresentation;

static inline CtrPresentation CtrPresentation_Layout(void)
{
    return (CtrPresentation){400, 240, 256, 1, 0, 0, 1};
}
#endif
