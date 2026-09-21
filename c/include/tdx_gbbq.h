/* tdx_gbbq.h - the local encrypted GBBQ rights-and-dividend file.
 *
 * The C++ reference reads this file as an alternative to asking the server over
 * 0x000F: TdxW keeps its whole-market corporate-action history locally at
 * <root>/T0002/hq_cache/gbbq, and the two sources describe the same events.  That
 * makes this the only place in the project where two INDEPENDENT sources for one
 * data set can be diffed against each other, which is why it is worth the cipher.
 *
 * File layout:
 *   u32 count
 *   count records of 29 bytes, in the SAME layout as an 0x000F record, except
 *   that the first 24 bytes are encrypted and the last 5 are clear
 *
 * The count must satisfy size == 4 + count * 29 exactly; the live file does
 * (5,607,734 bytes and 193,370 records), and any other size is refused rather than
 * read optimistically.
 *
 * The cipher
 * ----------
 * Sixteen Feistel rounds over each 8-byte block, keyed by a 4168-byte state table
 * that the reference embeds as base64.  The state splits into
 *   [0x000, 0x004)  initial xor word
 *   [0x004, 0x044)  sixteen round words, consumed from 0x040 downwards
 *   [0x044, 0x048)  one more xor word
 *   [0x048, 0x448)  table indexed by byte 3 of the working word
 *   [0x448, 0x848)  table indexed by byte 2
 *   [0x848, 0xC48)  table indexed by byte 1
 *   [0xC48, 0x1048) table indexed by byte 0
 * and the four tables tile the rest of the blob exactly, which is the check that
 * the table offsets are right: 4168 bytes is 0x1048.
 *
 * The state is generated into gbbq_cipher_state.h from the reference by
 * output/make_gbbq_cipher_state.py rather than transcribed. */
#ifndef TDX_GBBQ_H
#define TDX_GBBQ_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_capital.h"
#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_GBBQ_HEADER_SIZE 4
#define TDX_GBBQ_RECORD_SIZE 29
#define TDX_GBBQ_ENCRYPTED_SIZE 24
#define TDX_GBBQ_CIPHER_STATE_BYTES 4168
/* A count above this is treated as a damaged header rather than a huge file. */
#define TDX_GBBQ_RECORD_LIMIT 2000000u

/* Decodes the embedded base64 into a 4168-byte state.  Exposed so a test can
 * check the blob instead of trusting it. */
int tdx_gbbq_decode_cipher_state(uint8_t *out, size_t capacity, size_t *out_size,
                                 tdx_error *err);

/* Decrypts one 29-byte record: 24 encrypted bytes in three 8-byte blocks, then 5
 * clear bytes copied through. */
int tdx_gbbq_decrypt_record(const uint8_t *state, const uint8_t *encrypted, size_t size,
                            uint8_t *clear, tdx_error *err);

/* The whole file: header, every record, decrypted and validated.  Records that
 * name a security other than the wanted one are still validated, so a damaged file
 * is reported rather than silently filtered. */
int tdx_gbbq_parse(const uint8_t *data, size_t size, const tdx_code *wanted,
                   tdx_capital_record *out, size_t capacity, size_t *out_count,
                   size_t *source_count, tdx_error *err);

/* Reads the file from disk and parses it.  A NULL or empty path uses
 * <root>/T0002/hq_cache/gbbq. */
int tdx_gbbq_load(const char *path, const char *root, const tdx_code *wanted,
                  tdx_capital_record *out, size_t capacity, size_t *out_count,
                  size_t *source_count, tdx_error *err);

/* The default location the reference uses, for callers that want to report it. */
int tdx_gbbq_default_path(const char *root, char *out, size_t capacity, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_GBBQ_H */
