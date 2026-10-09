/* BIOS copy / fill / LZ77 semantics. The Ref_* functions are the original
 * byte-for-byte contract (kept here only as the test oracle); the production
 * functions must produce identical buffers, including canary bytes around the
 * destination, for every case. */
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void CpuSet(const void *src, void *dest, uint32_t control);
void CpuFastSet(const void *src, void *dest, uint32_t control);
void LZ77UnCompWram(const void *src, void *dest);
void LZ77UnCompVram(const void *src, void *dest);

enum { FIXED = 0x01000000, W32 = 0x04000000, MASK = 0x001fffff };

static void Ref_CpuSet(const void *src, void *dest, uint32_t control)
{
    const uint8_t *from = src; uint8_t *to = dest;
    uint32_t count = control & MASK;
    unsigned width = (control & W32) ? 4 : 2;
    if (count == 0) return;
    if (control & FIXED) {
        uint32_t value = 0;
        memcpy(&value, from, width);
        for (uint32_t i = 0; i < count; ++i) memcpy(to + (size_t)i * width, &value, width);
    } else memmove(to, from, (size_t)count * width);
}

static void Ref_CpuFastSet(const void *src, void *dest, uint32_t control)
{
    const uint8_t *from = src; uint8_t *to = dest;
    uint32_t count = ((control & MASK) + 7) & ~7u;
    if (count == 0) return;
    if (control & FIXED) {
        uint32_t value; memcpy(&value, from, 4);
        for (uint32_t i = 0; i < count; ++i) memcpy(to + (size_t)i * 4, &value, 4);
    } else memmove(to, from, (size_t)count * 4);
}

static void Ref_Lz77(const void *src, void *dest)
{
    const uint8_t *from = src; uint8_t *to = dest;
    if (from[0] != 0x10) return;
    uint32_t length = from[1] | (from[2] << 8) | ((uint32_t)from[3] << 16);
    if (length > 0x1000000) return;
    from += 4;
    uint32_t written = 0;
    while (written < length) {
        uint8_t flags = *from++;
        for (unsigned bit = 0; bit < 8 && written < length; ++bit, flags <<= 1) {
            if (!(flags & 0x80)) { to[written++] = *from++; continue; }
            uint8_t high = *from++, low = *from++;
            uint32_t count = (high >> 4) + 3;
            uint32_t distance = (((uint32_t)high & 15) << 8) + low + 1;
            if (distance > written) return;
            while (count-- && written < length) { to[written] = to[written - distance]; ++written; }
        }
    }
}

static uint32_t sRng = 0x1234abcd;
static uint32_t Rand(void) { sRng = sRng * 1664525u + 1013904223u; return sRng >> 8; }

#define POOL 16384
#define PAD 64

/* Run ref and production on identical memory images; compare everything. */
typedef void (*SetFn)(const void *, void *, uint32_t);
static void CompareSet(SetFn ref, SetFn prod, unsigned srcOff, unsigned dstOff, uint32_t control, int overlap)
{
    static uint8_t a[POOL * 2], b[POOL * 2];
    for (unsigned i = 0; i < sizeof(a); ++i) a[i] = (uint8_t)(Rand() >> 3);
    memcpy(b, a, sizeof(a));
    /* overlap: both pointers in the same region (offsets chosen by caller);
     * otherwise source in the first half and destination in the second. */
    uint8_t *sa = a + (overlap ? 0 : 0) + srcOff, *da = a + (overlap ? 0 : POOL) + dstOff;
    uint8_t *sb = b + srcOff, *db = b + (overlap ? 0 : POOL) + dstOff;
    ref(sa, da, control);
    prod(sb, db, control);
    if (memcmp(a, b, sizeof(a)) != 0) {
        fprintf(stderr, "mismatch control=%08x srcOff=%u dstOff=%u overlap=%d\n", control, srcOff, dstOff, overlap);
        abort();
    }
}

static const uint32_t kCounts[] = {1, 2, 3, 4, 5, 7, 8, 9, 15, 16, 17, 31, 32, 33, 63, 100, 255, 256, 1000, 2047};

static void TestCpuSetCopyFill(void)
{
    for (unsigned w = 0; w < 2; ++w) {
        uint32_t wide = w ? W32 : 0;
        for (unsigned c = 0; c < sizeof(kCounts) / 4; ++c)
            for (unsigned so = 0; so < 4; so += (w ? 1 : 2))
                for (unsigned d = 0; d < 4; d += (w ? 1 : 2)) {
                    CompareSet(Ref_CpuSet, CpuSet, so, d, wide | kCounts[c], 0);
                    CompareSet(Ref_CpuSet, CpuSet, so, d, FIXED | wide | kCounts[c], 0);
                }
        CompareSet(Ref_CpuSet, CpuSet, 0, 0, wide | 0, 0);           /* count 0 */
        CompareSet(Ref_CpuSet, CpuSet, 0, 0, FIXED | wide | 0, 0);
    }
}

static void TestCpuSetFillValues(void)
{
    static const uint32_t values[] = {0, 0xffffffffu, 0x01010101u, 0xa5a5a5a5u, 0x00ff00ffu, 0x12345678u, 0x80000001u, 0x0000ffffu};
    for (unsigned v = 0; v < sizeof(values) / 4; ++v)
        for (unsigned w = 0; w < 2; ++w)
            for (unsigned c = 0; c < sizeof(kCounts) / 4; ++c)
                for (unsigned d = 0; d < 4; d += (w ? 1 : 2)) {
                    static uint8_t a[POOL], b[POOL];
                    uint32_t value = values[v];
                    memset(a, 0x5c, sizeof(a)); memcpy(b, a, sizeof(a));
                    uint32_t control = FIXED | (w ? W32 : 0) | kCounts[c];
                    Ref_CpuSet(&value, a + PAD + d, control);
                    CpuSet(&value, b + PAD + d, control);
                    assert(memcmp(a, b, sizeof(a)) == 0);
                }
}

static void TestCpuSetOverlap(void)
{
    /* destination overlapping the source from above and from below, 16 and 32 bit */
    static const unsigned shifts[] = {2, 4, 6, 8, 14, 32, 100};
    for (unsigned w = 0; w < 2; ++w)
        for (unsigned s = 0; s < sizeof(shifts) / 4; ++s)
            for (unsigned c = 0; c < sizeof(kCounts) / 4; ++c) {
                uint32_t control = (w ? W32 : 0) | kCounts[c];
                if (w && (shifts[s] & 3)) continue;
                CompareSet(Ref_CpuSet, CpuSet, 0, shifts[s], control, 1);      /* dst above src */
                CompareSet(Ref_CpuSet, CpuSet, shifts[s], 0, control, 1);      /* src above dst */
                CompareSet(Ref_CpuSet, CpuSet, 0, 0, control, 1);              /* identical */
            }
    /* a fixed source inside the destination range */
    for (unsigned w = 0; w < 2; ++w)
        for (unsigned c = 0; c < sizeof(kCounts) / 4; ++c)
            for (unsigned inside = 0; inside < 3; ++inside)
                CompareSet(Ref_CpuSet, CpuSet, inside * 4, 0, FIXED | (w ? W32 : 0) | kCounts[c], 1);
}

static void TestCpuFastSet(void)
{
    static const uint32_t counts[] = {1, 2, 7, 8, 9, 15, 16, 17, 24, 31, 32, 33, 64, 100, 256, 1000, 2047};
    for (unsigned c = 0; c < sizeof(counts) / 4; ++c) {
        for (unsigned a = 0; a < 4; a += 4) {
            CompareSet(Ref_CpuFastSet, CpuFastSet, a, a, counts[c], 0);
            CompareSet(Ref_CpuFastSet, CpuFastSet, a, a, FIXED | counts[c], 0);
        }
        CompareSet(Ref_CpuFastSet, CpuFastSet, 0, 4, counts[c], 0);
        CompareSet(Ref_CpuFastSet, CpuFastSet, 4, 0, FIXED | counts[c], 0);
        for (unsigned sh = 4; sh <= 128; sh += 28) {
            CompareSet(Ref_CpuFastSet, CpuFastSet, 0, sh, counts[c], 1);
            CompareSet(Ref_CpuFastSet, CpuFastSet, sh, 0, counts[c], 1);
        }
        CompareSet(Ref_CpuFastSet, CpuFastSet, 0, 0, FIXED | counts[c], 1);
    }
    CompareSet(Ref_CpuFastSet, CpuFastSet, 0, 0, 0, 0);
    CompareSet(Ref_CpuFastSet, CpuFastSet, 0, 0, FIXED, 0);
    /* rounding: 1..8 words move one 8-word block, 9 moves two */
    static uint32_t src[32], dst[32];
    for (unsigned i = 0; i < 32; ++i) src[i] = 0x1000 + i;
    memset(dst, 0, sizeof(dst)); CpuFastSet(src, dst, 1);
    assert(dst[7] == src[7] && dst[8] == 0);
    memset(dst, 0, sizeof(dst)); CpuFastSet(src, dst, 9);
    assert(dst[15] == src[15] && dst[16] == 0);
    uint32_t fill = 0xdeadbeef; memset(dst, 0, sizeof(dst)); CpuFastSet(&fill, dst, FIXED | 9);
    assert(dst[15] == fill && dst[16] == 0);
    fill = 0; memset(dst, 0xff, sizeof(dst)); CpuFastSet(&fill, dst, FIXED | 8);
    assert(dst[7] == 0 && dst[8] == 0xffffffffu);
}

/* ---- LZ77 fixtures: a token list encoded as a GBA type-0x10 stream ---- */
typedef struct { uint8_t *p; size_t n; } Buf;
typedef struct { int match; unsigned count, distance; uint8_t lit; } Tok;

static size_t Encode(const Tok *t, unsigned n, uint32_t length, uint8_t *out)
{
    size_t o = 0;
    out[o++] = 0x10; out[o++] = length & 255; out[o++] = (length >> 8) & 255; out[o++] = length >> 16;
    for (unsigned i = 0; i < n; i += 8) {
        size_t flagAt = o++;
        out[flagAt] = 0;
        for (unsigned b = 0; b < 8 && i + b < n; ++b) {
            const Tok *k = &t[i + b];
            if (k->match) {
                out[flagAt] |= 0x80 >> b;
                out[o++] = (uint8_t)(((k->count - 3) << 4) | ((k->distance - 1) >> 8));
                out[o++] = (uint8_t)((k->distance - 1) & 255);
            } else out[o++] = k->lit;
        }
    }
    memset(out + o, 0xee, 64); /* never read by valid streams */
    return o;
}

static void CheckLz(const Tok *t, unsigned n, uint32_t length, const char *name, int expectFull)
{
    static uint8_t in[1 << 17], a[1 << 16], b[1 << 16], c[1 << 16];
    Encode(t, n, length, in);
    memset(a, 0x77, sizeof(a)); memcpy(b, a, sizeof(a)); memcpy(c, a, sizeof(a));
    Ref_Lz77(in, a); LZ77UnCompWram(in, b); LZ77UnCompVram(in, c);
    if (memcmp(a, b, sizeof(a)) || memcmp(a, c, sizeof(a))) { fprintf(stderr, "LZ77 mismatch: %s\n", name); abort(); }
    if (expectFull) assert(a[length - 1] != 0x77 || length == 0);
    if (!expectFull) { /* malformed: output stops at the bad token, rest untouched */ }
}

static void TestLz77(void)
{
    static Tok t[8192];
    unsigned n;

    n = 0; for (unsigned i = 0; i < 100; ++i) t[n++] = (Tok){0, 0, 0, (uint8_t)(i * 7 + 1)};
    CheckLz(t, n, 100, "literals only", 1);

    n = 0; t[n++] = (Tok){0, 0, 0, 'A'}; t[n++] = (Tok){1, 18, 1, 0};
    CheckLz(t, n, 19, "distance 1 expansion", 1);
    {   /* exact content: A x19 */
        static uint8_t in[256], out[64]; Encode(t, n, 19, in); memset(out, 0, sizeof(out));
        LZ77UnCompWram(in, out);
        for (unsigned i = 0; i < 19; ++i) assert(out[i] == 'A');
        assert(out[19] == 0);
    }

    n = 0; for (unsigned i = 0; i < 4; ++i) t[n++] = (Tok){0, 0, 0, (uint8_t)('a' + i)};
    t[n++] = (Tok){1, 18, 2, 0}; t[n++] = (Tok){1, 3, 3, 0}; t[n++] = (Tok){1, 10, 4, 0};
    CheckLz(t, n, 4 + 18 + 3 + 10, "overlapping short distances", 1);

    /* long matches at the maximum distance (4096) */
    n = 0; for (unsigned i = 0; i < 4096; ++i) t[n++] = (Tok){0, 0, 0, (uint8_t)(Rand())};
    t[n++] = (Tok){1, 18, 4096, 0}; t[n++] = (Tok){1, 18, 4095, 0}; t[n++] = (Tok){1, 3, 1, 0};
    CheckLz(t, n, 4096 + 18 + 18 + 3, "distance 4096", 1);

    /* patterns generated recursively from the output itself */
    n = 0; t[n++] = (Tok){0, 0, 0, 1}; t[n++] = (Tok){0, 0, 0, 2}; t[n++] = (Tok){0, 0, 0, 3};
    unsigned len = 3;
    for (unsigned i = 0; i < 300; ++i) {
        unsigned d = 1 + (Rand() % (len < 4096 ? len : 4096)), c = 3 + Rand() % 16;
        t[n++] = (Tok){1, c, d, 0}; len += c;
        if (i % 5 == 0) { t[n++] = (Tok){0, 0, 0, (uint8_t)Rand()}; len++; }
    }
    CheckLz(t, n, len, "recursive random mix", 1);

    /* destination length ends mid-match and mid-token-group */
    for (uint32_t cut = 1; cut < len; cut += 37) CheckLz(t, n, cut, "exact destination length", 1);
    CheckLz(t, n, 0, "zero length", 0);

    /* malformed: reference before the start of the output */
    n = 0; t[n++] = (Tok){0, 0, 0, 'x'}; t[n++] = (Tok){1, 5, 2, 0}; t[n++] = (Tok){0, 0, 0, 'y'};
    CheckLz(t, n, 8, "distance beyond output (first match)", 0);
    n = 0; t[n++] = (Tok){1, 3, 1, 0};
    CheckLz(t, n, 3, "match as first token", 0);
    n = 0; for (unsigned i = 0; i < 20; ++i) t[n++] = (Tok){0, 0, 0, (uint8_t)i};
    t[n++] = (Tok){1, 10, 21, 0}; t[n++] = (Tok){0, 0, 0, 9};
    CheckLz(t, n, 40, "distance one beyond", 0);

    /* wrong type byte and oversize header: nothing is written */
    {
        static uint8_t in[16] = {0x11, 4, 0, 0, 0, 1, 2, 3, 4}, a[16], b[16];
        memset(a, 0x55, 16); memcpy(b, a, 16);
        Ref_Lz77(in, a); LZ77UnCompWram(in, b); assert(!memcmp(a, b, 16) && a[0] == 0x55);
    }
}

int main(void)
{
    TestCpuSetCopyFill();
    TestCpuSetFillValues();
    TestCpuSetOverlap();
    TestCpuFastSet();
    TestLz77();
    puts("PASS bios semantics: CpuSet/CpuFastSet/LZ77 match the original contract");
    return 0;
}
