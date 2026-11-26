#ifndef CTR_WL_CRYPTO_H
#define CTR_WL_CRYPTO_H

/*
 * The small set of standard primitives the internet play needs to seal frames between consoles, written from the
 * published specifications (nothing is copied from another project) and checked in tests/wl_crypto_test.c against the
 * RFC 8439 test vector and, through tools/test_wl_crypto.py, against Python's hashlib and an independent Python
 * implementation:
 *
 *   SHA-256 (FIPS 180-4), HMAC-SHA-256 (RFC 2104), PBKDF2-HMAC-SHA-256 (RFC 8018),
 *   ChaCha20 and Poly1305 and their AEAD construction (RFC 8439).
 *
 * No state outside the arguments, no allocation, no dependency on the game or the console's libraries.
 */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

void WlSha256(const void *data, size_t len, uint8_t out[32]);
void WlHmacSha256(const void *key, size_t keyLen, const void *msg, size_t msgLen, uint8_t out[32]);
void WlPbkdf2Sha256(const void *password, size_t passwordLen, const void *salt, size_t saltLen, uint32_t iterations,
                    uint8_t *out, size_t outLen);

/* ChaCha20: `out` may be `in`. The 32-bit block counter starts at `counter`. */
void WlChaCha20Xor(const uint8_t key[32], const uint8_t nonce[12], uint32_t counter, const uint8_t *in, uint8_t *out, size_t len);
void WlPoly1305(const uint8_t key[32], const uint8_t *msg, size_t len, uint8_t tag[16]);

/* ChaCha20-Poly1305 AEAD: `out` (len bytes) and `tag` are written; `out` may be `in`. */
void WlAeadSeal(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, size_t aadLen, const uint8_t *in, size_t len,
                uint8_t *out, uint8_t tag[16]);
/* False (and nothing useful in `out`) when the tag does not match: the frame was changed, or sealed with another key. */
bool WlAeadOpen(const uint8_t key[32], const uint8_t nonce[12], const uint8_t *aad, size_t aadLen, const uint8_t *in, size_t len,
                const uint8_t tag[16], uint8_t *out);

#endif
