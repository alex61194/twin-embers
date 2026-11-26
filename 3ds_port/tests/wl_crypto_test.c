/* 3ds_wl_crypto.c: the RFC 8439 test vectors and the known SHA-256 answer asserted here; with the argument "dump" it prints what
 * it makes of a spread of lengths for tools/test_wl_crypto.py to compare with Python's hashlib and with an independent Python
 * ChaCha20, Poly1305 and AEAD. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "3ds_wl_crypto.h"

static void Hex(const char *label, unsigned len, const uint8_t *p, size_t n)
{
    printf("%s %u ", label, len);
    for (size_t i = 0; i < n; ++i)
        printf("%02x", p[i]);
    printf("%c", 10);
}

static void Pattern(uint8_t *p, size_t n, unsigned seed)
{
    for (size_t i = 0; i < n; ++i)
        p[i] = (uint8_t)(i * 7 + 3 + seed * 31);
}

static int Equal(const uint8_t *a, const char *hex, size_t n)
{
    for (size_t i = 0; i < n; ++i)
    {
        unsigned v;

        if (sscanf(hex + 2 * i, "%2x", &v) != 1 || a[i] != v)
            return 0;
    }
    return 1;
}

static void Vectors(void)
{
    uint8_t out[160], tag[16], key[32], nonce[12], aad[12];
    static const char text[] = "Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.";
    uint8_t ct[sizeof(text)], back[sizeof(text)];

    WlSha256("abc", 3, out);
    assert(Equal(out, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad", 32));
    WlSha256("", 0, out);
    assert(Equal(out, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855", 32));

    /* RFC 8439 2.4.2: ChaCha20 */
    for (int i = 0; i < 32; ++i)
        key[i] = (uint8_t)i;
    memset(nonce, 0, sizeof(nonce));
    nonce[7] = 0x4a;
    WlChaCha20Xor(key, nonce, 1, (const uint8_t *)"Ladies and Gentlemen of the class of '99: If I could offer you only one tip for the future, sunscreen would be it.", out,
                  114);
    assert(Equal(out, "6e2e359a2568f98041ba0728dd0d6981", 16));

    /* RFC 8439 2.5.2: Poly1305 */
    {
        static const uint8_t pk[32] = {0x85, 0xd6, 0xbe, 0x78, 0x57, 0x55, 0x6d, 0x33, 0x7f, 0x44, 0x52, 0xfe, 0x42, 0xd5, 0x06, 0xa8,
                                       0x01, 0x03, 0x80, 0x8a, 0xfb, 0x0d, 0xb2, 0xfd, 0x4a, 0xbf, 0xf6, 0xaf, 0x41, 0x49, 0xf5, 0x1b};

        WlPoly1305(pk, (const uint8_t *)"Cryptographic Forum Research Group", 34, tag);
        assert(Equal(tag, "a8061dc1305136c6c22b8baf0c0127a9", 16));
    }

    /* RFC 8439 2.8.2: the AEAD */
    for (int i = 0; i < 32; ++i)
        key[i] = (uint8_t)(0x80 + i);
    nonce[0] = 0x07; nonce[1] = nonce[2] = nonce[3] = 0;
    for (int i = 0; i < 8; ++i)
        nonce[4 + i] = (uint8_t)(0x40 + i);
    aad[0] = 0x50; aad[1] = 0x51; aad[2] = 0x52; aad[3] = 0x53;
    for (int i = 0; i < 8; ++i)
        aad[4 + i] = (uint8_t)(0xc0 + i);
    WlAeadSeal(key, nonce, aad, 12, (const uint8_t *)text, 114, ct, tag);
    assert(Equal(ct, "d31a8d34648e60db7b86afbc53ef7ec2", 16));
    assert(Equal(tag, "1ae10b594f09e26a7e902ecbd0600691", 16));
    assert(WlAeadOpen(key, nonce, aad, 12, ct, 114, tag, back) && !memcmp(back, text, 114));
    ct[5] ^= 1;
    assert(!WlAeadOpen(key, nonce, aad, 12, ct, 114, tag, back));      /* a changed byte */
    ct[5] ^= 1;
    tag[0] ^= 0x80;
    assert(!WlAeadOpen(key, nonce, aad, 12, ct, 114, tag, back));      /* a changed tag */
    tag[0] ^= 0x80;
    aad[0] ^= 1;
    assert(!WlAeadOpen(key, nonce, aad, 12, ct, 114, tag, back));      /* a changed header */
    aad[0] ^= 1;
    key[31] ^= 1;
    assert(!WlAeadOpen(key, nonce, aad, 12, ct, 114, tag, back));      /* another key */
    key[31] ^= 1;
    {
        uint8_t inplace[114];

        memcpy(inplace, text, 114);
        WlAeadSeal(key, nonce, aad, 12, inplace, 114, inplace, tag);   /* in place gives the same */
        assert(!memcmp(inplace, ct, 114) && Equal(tag, "1ae10b594f09e26a7e902ecbd0600691", 16));
    }
}

static void Dump(void)
{
    uint8_t in[256], key[32], nonce[12], out[256], tag[16], aad[40], mac[32];

    Pattern(key, 32, 1);
    Pattern(nonce, 12, 2);
    Pattern(aad, sizeof(aad), 3);
    for (unsigned len = 0; len <= 200; len += (len < 70 ? 1 : 13))
    {
        Pattern(in, len, 4);
        WlSha256(in, len, mac);
        Hex("sha256", len, mac, 32);
        WlHmacSha256(in, len, in, len, mac);
        Hex("hmac", len, mac, 32);
        WlChaCha20Xor(key, nonce, 7, in, out, len);
        Hex("chacha", len, out, len);
        WlPoly1305(key, in, len, tag);
        Hex("poly", len, tag, 16);
        WlAeadSeal(key, nonce, aad, len % 41, in, len, out, tag);
        Hex("aeadct", len, out, len);
        Hex("aeadtag", len, tag, 16);
    }
    for (unsigned iterations = 1; iterations <= 3; ++iterations)
    {
        WlPbkdf2Sha256("password", 8, "salt", 4, iterations, out, 40);
        Hex("pbkdf2", iterations, out, 40);
    }
    WlPbkdf2Sha256("a-long-room-code", 16, "TwinEmbers", 10, 4096, out, 32);
    Hex("pbkdf2", 4096, out, 32);
}

int main(int argc, char **argv)
{
    if (argc >= 2 && !strcmp(argv[1], "dump"))
    {
        Dump();
        return 0;
    }
    Vectors();
    puts("wl_crypto_test ok");
    return 0;
}
