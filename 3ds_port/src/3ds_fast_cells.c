/* See 3ds_fast_cells.h. Adapted from ZallaxDev's pokeemerald-3Ds-dualscreen (FastInit/FastQuad/FastCells, MIT). */
#include <3ds.h>
#include <citro3d.h>
#include <stdio.h>

#include "3ds_fast_cells.h"
#include "3ds_log.h"

#define FAST_QUADS 8192

typedef struct
{
    int16_t x, y;
    float s, t;
} FastVertex;

static FastVertex *sVertices;
static uint16_t *sIndices;
static unsigned sUsed;
static bool sReady;

bool CtrFastCells_Init(void)
{
    FILE *off = fopen("sdmc:/3ds/twinembers/fastcells.off", "rb");

    if (off != NULL)
    {
        fclose(off);
        CtrLog_Write(CTR_LOG_VIDEO, "VIDEO: fastcells.off, layer cells through Citro2D");
        return false;
    }
    sVertices = linearAlloc(FAST_QUADS * 4 * sizeof(FastVertex));
    sIndices = linearAlloc(FAST_QUADS * 6 * sizeof(uint16_t));
    if (sVertices == NULL || sIndices == NULL)
    {
        linearFree(sVertices);
        linearFree(sIndices);
        sVertices = NULL;
        sIndices = NULL;
        CtrLog_Write(CTR_LOG_ERROR, "VIDEO: no linear memory for layer cells; through Citro2D");
        return false;
    }
    for (unsigned q = 0; q < FAST_QUADS; ++q)
    {
        uint16_t *index = sIndices + q * 6, base = (uint16_t)(q * 4);

        index[0] = base;
        index[1] = base + 1;
        index[2] = base + 2;
        index[3] = base + 2;
        index[4] = base + 1;
        index[5] = base + 3;
    }
    sReady = true;
    return true;
}

void CtrFastCells_FrameBegin(void)
{
    sUsed = 0;
}

bool CtrFastCells_Room(unsigned count)
{
    return sReady && count <= FAST_QUADS - sUsed;
}

/* One cell's square: corners top-left, top-right, bottom-left, bottom-right, with the texcoords Citro2D gives an
 * atlas slot (DrawSlotTinted in 3ds_video.c). */
static void Quad(FastVertex *v, int x, int y, int slot, bool flipX, bool flipY, unsigned atlasSize)
{
    unsigned perRow = atlasSize / 8;
    unsigned tileX = ((unsigned)slot % perRow) * 8, tileY = ((unsigned)slot / perRow) * 8;
    float s0 = tileX / (float)atlasSize, s1 = (tileX + 8) / (float)atlasSize;
    float t0 = 1.0f - tileY / (float)atlasSize, t1 = 1.0f - (tileY + 8) / (float)atlasSize;

    if (flipX) { float swap = s0; s0 = s1; s1 = swap; }
    if (flipY) { float swap = t0; t0 = t1; t1 = swap; }
    v[0] = (FastVertex){(int16_t)x, (int16_t)y, s0, t0};
    v[1] = (FastVertex){(int16_t)(x + 8), (int16_t)y, s1, t0};
    v[2] = (FastVertex){(int16_t)x, (int16_t)(y + 8), s0, t1};
    v[3] = (FastVertex){(int16_t)(x + 8), (int16_t)(y + 8), s1, t1};
}

static void DrawRange(unsigned first, unsigned count)
{
    if (count)
        C3D_DrawElements(GPU_TRIANGLES, (int)count * 6, C3D_UNSIGNED_SHORT, sIndices + first * 6);
}

void CtrFastCells_Draw(const uint16_t *cells, unsigned count, const int16_t *slot, const uint16_t *entry,
                       unsigned columns, unsigned atlasSize)
{
    unsigned tiles = 0, first = sUsed, shift = (unsigned)__builtin_ctz(columns);
    C3D_AttrInfo savedAttr, *attr;
    C3D_BufInfo savedBuf, *buf;
    C3D_TexEnv savedEnv, *env;

    if (!CtrFastCells_Room(count) || count == 0)
        return;
    /* Tiles first, then the empty cells after them. columns is 32 or 64: a mask and a shift, no division (the ARM11
     * has no divider). */
    for (unsigned i = 0; i < count; ++i)
        if (slot[cells[i]] >= 0) ++tiles;
    for (unsigned i = 0, t = 0, e = tiles; i < count; ++i)
    {
        unsigned cell = cells[i];
        int x = (int)(cell & (columns - 1)) * 8, y = (int)(cell >> shift) * 8;

        if (slot[cell] >= 0)
            Quad(sVertices + (first + t++) * 4, x, y, slot[cell], entry[cell] & 1024, entry[cell] & 2048, atlasSize);
        else
            Quad(sVertices + (first + e++) * 4, x, y, 0, false, false, atlasSize);
    }
    sUsed += count;

    attr = C3D_GetAttrInfo();
    savedAttr = *attr;
    AttrInfo_Init(attr);
    AttrInfo_AddLoader(attr, 0, GPU_SHORT, 2);
    AttrInfo_AddLoader(attr, 1, GPU_FLOAT, 2);
    AttrInfo_AddFixed(attr, 2);
    AttrInfo_AddFixed(attr, 3);
    C3D_FixedAttribSet(2, 0.0f, 0.0f, 0.0f, 0.0f);
    C3D_FixedAttribSet(3, 255.0f, 255.0f, 255.0f, 255.0f);
    buf = C3D_GetBufInfo();
    savedBuf = *buf;
    BufInfo_Init(buf);
    BufInfo_Add(buf, sVertices, sizeof(FastVertex), 2, 0x10);
    C3D_AlphaTest(false, GPU_ALWAYS, 0);
    C3D_AlphaBlend(GPU_BLEND_ADD, GPU_BLEND_ADD, GPU_ONE, GPU_ZERO, GPU_ONE, GPU_ZERO);

    env = C3D_GetTexEnv(0);
    savedEnv = *env;
    C3D_TexEnvInit(env);
    C3D_TexEnvSrc(env, C3D_Both, GPU_TEXTURE0, 0, 0);
    C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
    DrawRange(first, tiles);
    if (count > tiles)
    {
        env = C3D_GetTexEnv(0);
        C3D_TexEnvInit(env);
        C3D_TexEnvSrc(env, C3D_Both, GPU_CONSTANT, 0, 0);
        C3D_TexEnvFunc(env, C3D_Both, GPU_REPLACE);
        C3D_TexEnvColor(env, 0);
        DrawRange(first + tiles, count - tiles);
    }

    /* Citro2D's layout, buffer and stage back, as it left them. */
    *C3D_GetTexEnv(0) = savedEnv;
    C3D_SetAttrInfo(&savedAttr);
    C3D_SetBufInfo(&savedBuf);
}
