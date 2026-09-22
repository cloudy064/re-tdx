/* JSON string rendering shared by protocol resources and domain renderers. */
#ifndef TDX_JSON_WRITE_H
#define TDX_JSON_WRITE_H

#include "tdx_bytes.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Append an escaped JSON string literal. Inputs are UTF-8. */
int tdx_format_json_string(tdx_buf *out, const char *text, tdx_error *err);
/* Explicit byte view; preserves embedded NUL as a JSON escape. */
int tdx_format_json_string_n(tdx_buf *out, const char *text, size_t size,
                             tdx_error *err);

#ifdef __cplusplus
}
#endif
#endif
