/* tdx_bytes.h - growable little-endian byte buffer.
 *
 * All protocol wire structures in this project are little-endian; the append
 * helpers below keep that convention in one place instead of scattering
 * shifts through the encoders. */
#ifndef TDX_BYTES_H
#define TDX_BYTES_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct tdx_buf {
    uint8_t *data;
    size_t len;
    size_t cap;
} tdx_buf;

/* Zeroes the struct.  Safe to call on an already initialised buffer only
 * after tdx_buf_free; it does not release memory. */
void tdx_buf_init(tdx_buf *buf);
void tdx_buf_free(tdx_buf *buf);

/* Ensures room for extra additional bytes beyond buf->len. */
int tdx_buf_reserve(tdx_buf *buf, size_t extra, tdx_error *err);

int tdx_buf_push(tdx_buf *buf, uint8_t value, tdx_error *err);
int tdx_buf_append(tdx_buf *buf, const void *data, size_t size, tdx_error *err);
int tdx_buf_append_zeros(tdx_buf *buf, size_t count, tdx_error *err);

/* Resets the length but keeps the allocation, so a formatter can reuse
 * the same buffer for every record of a round. */
void tdx_buf_clear(tdx_buf *buf);

/* Appends formatted text, growing the buffer as needed. */
int tdx_buf_append_printf(tdx_buf *buf, tdx_error *err, const char *format, ...)
#if defined(__GNUC__) && !defined(__MINGW32__)
    __attribute__((format(printf, 3, 4)))
#endif
    ;
int tdx_buf_append_u16le(tdx_buf *buf, uint16_t value, tdx_error *err);
int tdx_buf_append_u32le(tdx_buf *buf, uint32_t value, tdx_error *err);

/* Reads a fixed-width little-endian value from an explicitly sized view. */
uint16_t tdx_u16le(const uint8_t *data);
uint32_t tdx_u32le(const uint8_t *data);
int32_t tdx_i32le(const uint8_t *data);
float tdx_f32le(const uint8_t *data);

#ifdef __cplusplus
}
#endif

#endif /* TDX_BYTES_H */
