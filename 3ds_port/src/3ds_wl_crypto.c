/* SHA-256, HMAC, PBKDF2, ChaCha20, Poly1305 and the ChaCha20-Poly1305 AEAD, from their specifications (3ds_wl_crypto.h). */
#include <string.h>

#include "3ds_wl_crypto.h"

/* ---------------------------------------------------------------- SHA-256 */
typedef struct {
    uint32_t h[8];
    uint8_t block[64];
    size_t blockLen;
    uint64_t total;
} Sha;

static const uint32_t kK[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5, 0xd807aa98, 0x12835b01, 0x243185be,
    0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174, 0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa,
    0x5cb0a9dc, 0x76f988da, 0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967, 0x27b70a85,
    0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85, 0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070, 0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f,
    0x682e6ff3, 0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2};

#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static void ShaInit(Sha *s)
{
    static const uint32_t init[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a, 0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};

    memcpy(s->h, init, sizeof(init));
    s->blockLen = 0;
    s->total = 0;
}

static void ShaBlock(Sha *s, const uint8_t *p)
{
    uint32_t w[64], a, b, c, d, e, f, g, h;

    for (int i = 0; i < 16; ++i)
        w[i] = (uint32_t)p[i * 4] << 24 | (uint32_t)p[i * 4 + 1] << 16 | (uint32_t)p[i * 4 + 2] << 8 | p[i * 4 + 3];
    for (int i = 16; i < 64; ++i)
    {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);

        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    a = s->h[0]; b = s->h[1]; c = s->h[2]; d = s->h[3]; e = s->h[4]; f = s->h[5]; g = s->h[6]; h = s->h[7];
    for (int i = 0; i < 64; ++i)
    {
        uint32_t S1 = ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25), ch = (e & f) ^ (~e & g);
        uint32_t t1 = h + S1 + ch + kK[i] + w[i];
        uint32_t S0 = ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22), maj = (a & b) ^ (a & c) ^ (b & c);
        uint32_t t2 = S0 + maj;

        h = g; g = f; f = e; e = d + t1; d = c; c = b; b = a; a = t1 + t2;
    }
    s->h[0] += a; s->h[1] += b; s->h[2] += c; s->h[3] += d; s->h[4] += e; s->h[5] += f; s->h[6] += g; s->h[7] += h;
}

static void ShaUpdate(Sha *s, const void *data, size_t len)
{
    const uint8_t *p = data;

    s->total += len;
    while (len)
    {
        size_t n = 64 - s->blockLen;

        if (n > len)
            n = len;
        memcpy(s->block + s->blockLen, p, n);
        s->blockLen += n;
        p += n;
        len -= n;
        if (s->blockLen == 64)
        {
            ShaBlock(s, s->block);
            s->blockLen = 0;
        }
    }
}

static void ShaFinish(Sha *s, uint8_t out[32])
{
    uint64_t bits = s->total * 8;
    uint8_t pad = 0x80, zero = 0, len[8];

    ShaUpdate(s, &pad, 1);
    while (s->blockLen != 56)
        ShaUpdate(s, &zero, 1);
    for (int i = 0; i < 8; ++i)
        len[i] = (uint8_t)(bits >> (56 - 8 * i));
    ShaUpdate(s, len, 8);
    for (int i = 0; i < 8; ++i)
    {
        out[i * 4] = (uint8_t)(s->h[i] >> 24);
        out[i * 4 + 1] = (uint8_t)(s->h[i] >> 16);
        out[i * 4 + 2] = (uint8_t)(s->h[i] >> 8);
        out[i * 4 + 3] = (uint8_t)s->h[i];
    }
}

void WlSha256(const void *data, size_t len, uint8_t out[32])
{
    Sha s;

    ShaInit(&s);
    ShaUpdate(&s, data, len);
    ShaFinish(&s, out);
}

/* ---------------------------------------------------------------- HMAC, PBKDF2 */
typedef struct {
    Sha inner, outer;
} Hmac;

static void HmacInit(Hmac *h, const void *key, size_t keyLen)
{
    uint8_t k[64] = {0}, pad[64];

    if (keyLen > 64)
        WlSha256(key, keyLen, k);
    else
        memcpy(k, key, keyLen);
    for (int i = 0; i < 64; ++i)
        pad[i] = (uint8_t)(k[i] ^ 0x36);
    ShaInit(&h->inner);
    ShaUpdate(&h->inner, pad, 64);
    for (int i = 0; i < 64; ++i)
        pad[i] = (uint8_t)(k[i] ^ 0x5c);
    ShaInit(&h->outer);
    ShaUpdate(&h->outer, pad, 64);
}

/* The HMAC of `msg` (and `extra`, which may be empty) with the key set up in `h`, which stays usable. */
static void HmacRun(const Hmac *h, const void *msg, size_t msgLen, const void *extra, size_t extraLen, uint8_t out[32])
{
    Sha inner = h->inner, outer = h->outer;
    uint8_t digest[32];

    ShaUpdate(&inner, msg, msgLen);
    if (extraLen)
        ShaUpdate(&inner, extra, extraLen);
    ShaFinish(&inner, digest);
    ShaUpdate(&outer, digest, 32);
    ShaFinish(&outer, out);
}

void WlHmacSha256(const void *key, size_t keyLen, const void *msg, size_t msgLen, uint8_t out[32])
{
    Hmac h;

    HmacInit(&h, key, keyLen);
    HmacRun(&h, msg, msgLen, NULL, 0, out);
}

void WlPbkdf2Sha256(const void *password, size_t passwordLen, const void *salt, size_t saltLen, uint32_t iterations, uint8_t *out,
                    size_t outLen)
{
    Hmac h;
    uint32_t block = 1;

    HmacInit(&h, password, passwordLen);
    while (outLen)
    {
        uint8_t index[4] = {(uint8_t)(block >> 24), (uint8_t)(block >> 16), (uint8_t)(block >> 8), (uint8_t)block};
        uint8_t u[32], t[32];
        size_t n = outLen < 32 ? outLen : 32;

        HmacRun(&h, salt, saltLen, index, 4, u);
        memcpy(t, u, 32);
        for (uint32_t i = 1; i < iterations; ++i)
        {
            HmacRun(&h, u, 32, NULL, 0, u);
            for (int k = 0; k < 32; ++k)
                t[k] ^= u[k];
        }
        memcpy(out, t, n);
        out += n;
        outLen -= n;
        ++block;
    }
}

/* ---------------------------------------------------------------- ChaCha20 */
static uint32_t Le32(const uint8_t *p)
{
    return (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
}

static void PutLe32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)v;
    p[1] = (uint8_t)(v >> 8);
    p[2] = (uint8_t)(v >> 16);
    p[3] = (uint8_t)(v >> 24);
}

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define QR(a, b, c, d) \
    a += b; d ^= a; d = ROL(d, 16); \
    c += d; b ^= c; b = ROL(b, 12); \
    a += b; d ^= a; d = ROL(d, 8); \
    c += d; b ^= c; b = ROL(b, 7)

static void ChaChaBlock(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter, uint8_t out[64])
{
    uint32_t in[16], x[16];

    in[0] = 0x61707865; in[1] = 0x3320646e; in[2] = 0x79622d32; in[3] = 0x6b206574;
    for (int i = 0; i < 8; ++i)
        in[4 + i] = Le32(key + 4 * i);
    in[12] = counter;
    for (int i = 0; i < 3; ++i)
        in[13 + i] = Le32(nonce + 4 * i);
    memcpy(x, in, sizeof(x));
    for (int round = 0; round < 10; ++round)
    {
        QR(x[0], x[4], x[8], x[12]); QR(x[1], x[5], x[9], x[13]); QR(x[2], x[6], x[10], x[14]); QR(x[3], x[7], x[11], x[15]);
        QR(x[0], x[5], x[10], x[15]); QR(x[1], x[6], x[11], x[12]); QR(x[2], x[7], x[8], x[13]); QR(x[3], x[4], x[9], x[14]);
    }
    for (int i = 0; i < 16; ++i)
        PutLe32(out + 4 * i, x[i] + in[i]);
}

void WlChaCha20Xor(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter, const uint8_t *in, uint8_t *out, size_t len)
{
    uint8_t block[64];

    while (len)
    {
        size_t n = len < 64 ? len : 64;

        ChaChaBlock(key, nonce, counter++, block);
        for (size_t i = 0; i < n; ++i)
            out[i] = (uint8_t)(in[i] ^ block[i]);
        in += n;
        out += n;
        len -= n;
    }
}

/* ---------------------------------------------------------------- Poly1305 (26-bit limbs) */
typedef struct {
    uint32_t r[5], h[5], pad[4];
    uint8_t buffer[16];
    size_t used;
} Poly;

static void PolyInit(Poly *p, const uint8_t key[32])
{
    uint32_t t0 = Le32(key), t1 = Le32(key + 4), t2 = Le32(key + 8), t3 = Le32(key + 12);

    p->r[0] = t0 & 0x3ffffff;
    p->r[1] = ((t0 >> 26) | (t1 << 6)) & 0x3ffff03;
    p->r[2] = ((t1 >> 20) | (t2 << 12)) & 0x3ffc0ff;
    p->r[3] = ((t2 >> 14) | (t3 << 18)) & 0x3f03fff;
    p->r[4] = (t3 >> 8) & 0x00fffff;
    memset(p->h, 0, sizeof(p->h));
    for (int i = 0; i < 4; ++i)
        p->pad[i] = Le32(key + 16 + 4 * i);
    p->used = 0;
}

static void PolyBlock(Poly *p, const uint8_t m[16], uint32_t hibit)
{
    uint32_t r0 = p->r[0], r1 = p->r[1], r2 = p->r[2], r3 = p->r[3], r4 = p->r[4];
    uint32_t s1 = r1 * 5, s2 = r2 * 5, s3 = r3 * 5, s4 = r4 * 5;
    uint32_t h0 = p->h[0], h1 = p->h[1], h2 = p->h[2], h3 = p->h[3], h4 = p->h[4];
    uint64_t d0, d1, d2, d3, d4;
    uint32_t c;

    h0 += Le32(m) & 0x3ffffff;
    h1 += (Le32(m + 3) >> 2) & 0x3ffffff;
    h2 += (Le32(m + 6) >> 4) & 0x3ffffff;
    h3 += (Le32(m + 9) >> 6) & 0x3ffffff;
    h4 += (Le32(m + 12) >> 8) | hibit;
    d0 = (uint64_t)h0 * r0 + (uint64_t)h1 * s4 + (uint64_t)h2 * s3 + (uint64_t)h3 * s2 + (uint64_t)h4 * s1;
    d1 = (uint64_t)h0 * r1 + (uint64_t)h1 * r0 + (uint64_t)h2 * s4 + (uint64_t)h3 * s3 + (uint64_t)h4 * s2;
    d2 = (uint64_t)h0 * r2 + (uint64_t)h1 * r1 + (uint64_t)h2 * r0 + (uint64_t)h3 * s4 + (uint64_t)h4 * s3;
    d3 = (uint64_t)h0 * r3 + (uint64_t)h1 * r2 + (uint64_t)h2 * r1 + (uint64_t)h3 * r0 + (uint64_t)h4 * s4;
    d4 = (uint64_t)h0 * r4 + (uint64_t)h1 * r3 + (uint64_t)h2 * r2 + (uint64_t)h3 * r1 + (uint64_t)h4 * r0;
    c = (uint32_t)(d0 >> 26); h0 = (uint32_t)d0 & 0x3ffffff;
    d1 += c; c = (uint32_t)(d1 >> 26); h1 = (uint32_t)d1 & 0x3ffffff;
    d2 += c; c = (uint32_t)(d2 >> 26); h2 = (uint32_t)d2 & 0x3ffffff;
    d3 += c; c = (uint32_t)(d3 >> 26); h3 = (uint32_t)d3 & 0x3ffffff;
    d4 += c; c = (uint32_t)(d4 >> 26); h4 = (uint32_t)d4 & 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;
    p->h[0] = h0; p->h[1] = h1; p->h[2] = h2; p->h[3] = h3; p->h[4] = h4;
}

static void PolyUpdate(Poly *p, const uint8_t *data, size_t len)
{
    while (len)
    {
        size_t n = 16 - p->used;

        if (n > len)
            n = len;
        memcpy(p->buffer + p->used, data, n);
        p->used += n;
        data += n;
        len -= n;
        if (p->used == 16)
        {
            PolyBlock(p, p->buffer, 1u << 24);
            p->used = 0;
        }
    }
}

static void PolyFinish(Poly *p, uint8_t tag[16])
{
    uint32_t h0, h1, h2, h3, h4, c, g0, g1, g2, g3, g4, mask;
    uint64_t f;

    if (p->used)
    {
        p->buffer[p->used] = 1;
        memset(p->buffer + p->used + 1, 0, 15 - p->used);
        PolyBlock(p, p->buffer, 0);
    }
    h0 = p->h[0]; h1 = p->h[1]; h2 = p->h[2]; h3 = p->h[3]; h4 = p->h[4];
    c = h1 >> 26; h1 &= 0x3ffffff;
    h2 += c; c = h2 >> 26; h2 &= 0x3ffffff;
    h3 += c; c = h3 >> 26; h3 &= 0x3ffffff;
    h4 += c; c = h4 >> 26; h4 &= 0x3ffffff;
    h0 += c * 5; c = h0 >> 26; h0 &= 0x3ffffff;
    h1 += c;
    g0 = h0 + 5; c = g0 >> 26; g0 &= 0x3ffffff;
    g1 = h1 + c; c = g1 >> 26; g1 &= 0x3ffffff;
    g2 = h2 + c; c = g2 >> 26; g2 &= 0x3ffffff;
    g3 = h3 + c; c = g3 >> 26; g3 &= 0x3ffffff;
    g4 = h4 + c - (1u << 26);
    mask = (g4 >> 31) - 1;              /* all ones when h >= 2^130 - 5: take h - p */
    g0 &= mask; g1 &= mask; g2 &= mask; g3 &= mask; g4 &= mask;
    mask = ~mask;
    h0 = (h0 & mask) | g0; h1 = (h1 & mask) | g1; h2 = (h2 & mask) | g2; h3 = (h3 & mask) | g3; h4 = (h4 & mask) | g4;
    h0 = h0 | (h1 << 26);
    h1 = (h1 >> 6) | (h2 << 20);
    h2 = (h2 >> 12) | (h3 << 14);
    h3 = (h3 >> 18) | (h4 << 8);
    f = (uint64_t)h0 + p->pad[0]; PutLe32(tag, (uint32_t)f);
    f = (uint64_t)h1 + p->pad[1] + (f >> 32); PutLe32(tag + 4, (uint32_t)f);
    f = (uint64_t)h2 + p->pad[2] + (f >> 32); PutLe32(tag + 8, (uint32_t)f);
    f = (uint64_t)h3 + p->pad[3] + (f >> 32); PutLe32(tag + 12, (uint32_t)f);
}

void WlPoly1305(const uint8_t key[32], const uint8_t *msg, size_t len, uint8_t tag[16])
{
    Poly p;

    PolyInit(&p, key);
    PolyUpdate(&p, msg, len);
    PolyFinish(&p, tag);
}

/* ---------------------------------------------------------------- AEAD */
static void AeadTag(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, size_t aadLen, const uint8_t *ct, size_t len,
                    uint8_t tag[16])
{
    static const uint8_t zero[16] = {0};
    uint8_t otk[64], lengths[16];
    Poly p;

    ChaChaBlock(key, nonce, 0, otk);
    PolyInit(&p, otk);
    PolyUpdate(&p, aad, aadLen);
    PolyUpdate(&p, zero, (16 - aadLen % 16) % 16);
    PolyUpdate(&p, ct, len);
    PolyUpdate(&p, zero, (16 - len % 16) % 16);
    for (int i = 0; i < 8; ++i)
    {
        lengths[i] = (uint8_t)((uint64_t)aadLen >> (8 * i));
        lengths[8 + i] = (uint8_t)((uint64_t)len >> (8 * i));
    }
    PolyUpdate(&p, lengths, 16);
    PolyFinish(&p, tag);
}

void WlAeadSeal(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, size_t aadLen, const uint8_t *in, size_t len,
                uint8_t *out, uint8_t tag[16])
{
    WlChaCha20Xor(key, nonce, 1, in, out, len);
    AeadTag(key, nonce, aad, aadLen, out, len, tag);
}

bool WlAeadOpen(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, size_t aadLen, const uint8_t *in, size_t len,
                const uint8_t tag[16], uint8_t *out)
{
    uint8_t expected[16];
    unsigned diff = 0;

    AeadTag(key, nonce, aad, aadLen, in, len, expected);
    for (int i = 0; i < 16; ++i)
        diff |= (unsigned)(expected[i] ^ tag[i]);
    if (diff)
        return false;
    WlChaCha20Xor(key, nonce, 1, in, out, len);
    return true;
}
