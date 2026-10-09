/* ARM11 replacements for the GBA BIOS memory services used by FireRed.
 * The game owns its loop; these services operate on its virtual GBA banks. */
#include <stdint.h>
#include <string.h>

#include "gba_shadow.h"
#include "3ds_perf.h"
#include "3ds_assets.h"

enum {
    RESET_PALETTE = 0x04,
    RESET_VRAM = 0x08,
    RESET_OAM = 0x10,
    RESET_SIO_REGS = 0x20,
    RESET_SOUND_REGS = 0x40,
    RESET_REGS = 0x80,
    CPU_SRC_FIXED = 0x01000000,
    CPU_32BIT = 0x04000000,
    CPU_COUNT_MASK = 0x001fffff,
};

void RegisterRamReset(uint32_t flags)
{
    /* The ARM11 C runtime initializes game EWRAM/IWRAM globals at process
     * start. A later soft reset needs a separate, explicit game-RAM reset. */
    if (flags & RESET_PALETTE)
        memset(gGbaShadow.pltt, 0, sizeof(gGbaShadow.pltt));
    if (flags & RESET_VRAM)
        memset(gGbaShadow.vram, 0, sizeof(gGbaShadow.vram));
    if (flags & RESET_OAM)
        memset(gGbaShadow.oam, 0, sizeof(gGbaShadow.oam));
    if (flags & RESET_REGS)
    {
        uint8_t keyLo = gGbaShadow.regs[0x130];
        uint8_t keyHi = gGbaShadow.regs[0x131];
        memset(gGbaShadow.regs, 0, sizeof(gGbaShadow.regs));
        gGbaShadow.regs[0x130] = keyLo;
        gGbaShadow.regs[0x131] = keyHi;
        /* BIOS RESET_REGS restores both affine BGs to 1:1 sampling.
         * Games may use these defaults without calling BgAffineSet.
         * This is a reset value, not a fallback for singular matrices. */
        for (unsigned base = 0x20; base <= 0x30; base += 0x10)
        {
            gGbaShadow.regs[base + 1] = 1; /* PA = 0x0100 */
            gGbaShadow.regs[base + 7] = 1; /* PD = 0x0100 */
        }
    }
    else
    {
        if (flags & RESET_SOUND_REGS)
            memset(gGbaShadow.regs + 0x060, 0, 0x050);
        if (flags & RESET_SIO_REGS)
        {
            memset(gGbaShadow.regs + 0x120, 0, 0x010);
            memset(gGbaShadow.regs + 0x134, 0, 0x02c);
        }
    }
}

/* Fill `units` copies of the `width`-byte value at `to`, for any alignment.
 * The first few units are written directly, then the filled prefix is copied
 * onto the bytes after it, doubling each time (the copies never overlap). */
static void FillUnits(uint8_t *to, const uint8_t *value, unsigned width, size_t units)
{
    size_t bytes = units * width, done = units < 8 ? units : 8;
    if (value[0] == value[1] && (width == 2 || (value[1] == value[2] && value[2] == value[3])))
    {
        memset(to, value[0], bytes);
        return;
    }
    if (width == 4)
        for (size_t i = 0; i < done; ++i)
            memcpy(to + i * 4, value, 4);
    else
        for (size_t i = 0; i < done; ++i)
            memcpy(to + i * 2, value, 2);
    size_t filled = done * width;
    while (filled < bytes)
    {
        size_t chunk = bytes - filled < filled ? bytes - filled : filled;
        memcpy(to + filled, to, chunk);
        filled += chunk;
    }
}

/* memmove semantics, memcpy speed when the ranges are disjoint. */
static void CopyBytes(uint8_t *to, const uint8_t *from, size_t bytes)
{
    if (to + bytes <= from || from + bytes <= to)
        memcpy(to, from, bytes);
    else
        memmove(to, from, bytes);
}

void CpuSet(const void *src, void *dest, uint32_t control)
{
    const uint8_t *from = src;
    uint8_t *to = dest;
    uint32_t count = control & CPU_COUNT_MASK;
    unsigned width = (control & CPU_32BIT) ? 4 : 2;
    if (count == 0)
        return;
    CTR_ASSET_READ(src, (control & CPU_SRC_FIXED) ? width : (size_t)count * width);
    uint64_t perfStart = CtrPerf_Begin();
    CTR_PERF_TRANSFER(dest, (size_t)count * width);
    if (control & CPU_SRC_FIXED)
    {
        uint8_t value[4] = {0, 0, 0, 0};
        memcpy(value, from, width);
        FillUnits(to, value, width, count);
    }
    else
        CopyBytes(to, from, (size_t)count * width);
    CtrPerf_End(PERF_DMA, perfStart);
}

void CpuFastSet(const void *src, void *dest, uint32_t control)
{
    const uint8_t *from = src;
    uint8_t *to = dest;
    uint32_t count = (control & CPU_COUNT_MASK) + 7;
    count &= ~7u; /* BIOS transfers whole eight-word blocks. */
    if (count == 0)
        return;
    CTR_ASSET_READ(src, (control & CPU_SRC_FIXED) ? 4 : (size_t)count * 4);
    uint64_t perfStart = CtrPerf_Begin();
    CTR_PERF_TRANSFER(dest, (size_t)count * 4);
    if (control & CPU_SRC_FIXED)
    {
        uint8_t value[4];
        memcpy(value, from, sizeof(value));
        FillUnits(to, value, 4, count);
    }
    else
        CopyBytes(to, from, (size_t)count * 4);
    CtrPerf_End(PERF_DMA, perfStart);
}

static void DecompressLz77(const void *src, void *dest)
{
    CTR_ASSET_READ(src, 1);
    const uint8_t *from = src;
    uint8_t *to = dest;
    if (from[0] != 0x10)
        return;
    uint32_t length = (uint32_t)from[1] | ((uint32_t)from[2] << 8)
                    | ((uint32_t)from[3] << 16);
    if (length > 0x1000000)
        return;
    uint64_t perfStart=CtrPerf_Begin();
    from += 4;
    /* Pointer form of the byte loop: one flag byte, then eight tokens, each a
     * literal or a 3-18 byte copy from up to 4 KiB behind. A copy may overlap
     * its own output (distance < count repeats the pattern), so it is made
     * forward byte by byte; the bounds are checked once per token. */
    uint8_t *out = to;
    uint8_t *const end = to + length;
    while (out < end)
    {
        unsigned flags = *from++;
        if (flags == 0 && end - out >= 8)
        {
            memcpy(out, from, 8); /* eight literals */
            out += 8;
            from += 8;
            continue;
        }
        for (unsigned bit = 0; bit < 8 && out < end; ++bit, flags <<= 1)
        {
            if (!(flags & 0x80))
            {
                *out++ = *from++;
                continue;
            }
            unsigned high = from[0];
            unsigned distance = ((high & 15u) << 8) + from[1] + 1;
            size_t count = (high >> 4) + 3;
            from += 2;
            if (distance > (size_t)(out - to))
            {
                CtrPerf_End(PERF_DECOMPRESS,perfStart);
                return;
            }
            if (count > (size_t)(end - out))
                count = (size_t)(end - out);
            const uint8_t *ref = out - distance;
            do
                *out++ = *ref++;
            while (--count);
        }
    }
    CTR_PERF_TRANSFER(dest,(size_t)(out - to));
    CtrPerf_End(PERF_DECOMPRESS,perfStart);
}

void LZ77UnCompWram(const void *src, void *dest)
{
    DecompressLz77(src, dest);
}

void LZ77UnCompVram(const void *src, void *dest)
{
    /* Shadow VRAM is RAM; the decoded bytes have the same final layout as
     * 16-bit BIOS writes, including a trailing odd byte. */
    DecompressLz77(src, dest);
}
