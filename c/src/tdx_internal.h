/* tdx_internal.h - helpers shared between translation units, not installed. */
#ifndef TDX_INTERNAL_H
#define TDX_INTERNAL_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_error.h"

/* Decodes GB18030 (the client sends host names in that code page) into UTF-8.
 * Returns TDX_OK or TDX_ERR; out is always NUL terminated on success. */
int tdx_decode_gb18030(const uint8_t *data, size_t size, char *out,
                       size_t out_size, size_t *out_length, tdx_error *err);

/* Case-insensitive comparison over ASCII. */
int tdx_ascii_casecmp(const char *left, const char *right);

/* Trims ASCII whitespace in place and returns the new start pointer. */
char *tdx_trim(char *text);

#endif /* TDX_INTERNAL_H */
