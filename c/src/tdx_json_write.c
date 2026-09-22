/* Shared byte-oriented JSON string writer. */
#include "tdx_json_write.h"

#include <string.h>

int tdx_format_json_string(tdx_buf *out, const char *text, tdx_error *err) {
    return tdx_format_json_string_n(out, text, text ? strlen(text) : 0, err);
}

int tdx_format_json_string_n(tdx_buf *out, const char *text, size_t size,
                             tdx_error *err) {
    const unsigned char *cursor = (const unsigned char *)text;
    size_t index;
    if (!out || (!text && size)) {
        tdx_error_set(err, "JSON string needs a buffer and a valid byte view");
        return TDX_ERR;
    }
    if (tdx_buf_push(out, '"', err) != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < size; ++index, ++cursor) {
        switch (*cursor) {
        case '"':
            if (tdx_buf_append(out, "\\\"", 2, err) != TDX_OK)
                return TDX_ERR;
            break;
        case '\\':
            if (tdx_buf_append(out, "\\\\", 2, err) != TDX_OK)
                return TDX_ERR;
            break;
        case '\n':
            if (tdx_buf_append(out, "\\n", 2, err) != TDX_OK)
                return TDX_ERR;
            break;
        case '\r':
            if (tdx_buf_append(out, "\\r", 2, err) != TDX_OK)
                return TDX_ERR;
            break;
        case '\t':
            if (tdx_buf_append(out, "\\t", 2, err) != TDX_OK)
                return TDX_ERR;
            break;
        default:
            if (*cursor < 0x20) {
                if (tdx_buf_append_printf(out, err, "\\u%04x", (unsigned)*cursor) != TDX_OK)
                    return TDX_ERR;
            } else if (tdx_buf_push(out, (uint8_t)*cursor, err) != TDX_OK) {
                return TDX_ERR;
            }
            break;
        }
    }
    return tdx_buf_push(out, '"', err);
}
