/* tdx_md5.h - RFC 1321 MD5, used to verify a downloaded resource.
 *
 * The 0x02C5 file probe returns the server's MD5 of the resource, and the C++
 * tool refuses a download whose digest does not match.  Keeping that check on
 * this side means a truncated or reordered chunk stream cannot be mistaken for a
 * complete file.  MD5 is used here purely as a transport checksum, matching the
 * peer; it is not a security primitive and must not be used as one. */
#ifndef TDX_MD5_H
#define TDX_MD5_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_MD5_DIGEST_SIZE 16
#define TDX_MD5_HEX_SIZE 33

typedef struct tdx_md5 {
    uint32_t state[4];
    uint64_t bits; /* message length in bits */
    uint8_t block[64];
    size_t filled;
} tdx_md5;

void tdx_md5_init(tdx_md5 *context);
void tdx_md5_update(tdx_md5 *context, const void *data, size_t size);
void tdx_md5_final(tdx_md5 *context, uint8_t digest[TDX_MD5_DIGEST_SIZE]);

/* Lower-case hex, NUL terminated. */
void tdx_md5_hex(const uint8_t digest[TDX_MD5_DIGEST_SIZE], char out[TDX_MD5_HEX_SIZE]);

/* One-shot convenience. */
void tdx_md5_hex_of(const void *data, size_t size, char out[TDX_MD5_HEX_SIZE]);

#ifdef __cplusplus
}
#endif

#endif /* TDX_MD5_H */
