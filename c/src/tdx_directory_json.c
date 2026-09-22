/* Security-directory JSON schema shared by CLI and library callers. */
#include "tdx_directory_json.h"
#include "tdx_json_write.h"
#include <stdio.h>

int tdx_directory_format_security(tdx_buf *out, const tdx_security *security,
                                   tdx_error *err) {
    static const char *upper[3] = {"SZ", "SH", "BJ"};
    static const char *lower[3] = {"sz", "sh", "bj"};
    int id;
    char code[8];
    if (!out || !security) {
        tdx_error_set(err, "security JSON needs a buffer and a record");
        return TDX_ERR;
    }
    id = (security->market_id >= 0 && security->market_id <= 2) ? security->market_id : 0;
    snprintf(code, sizeof(code), "%s", security->code);
    if (tdx_buf_append_printf(out, err,
          "{\"security_id\":\"%s%s\",\"market\":\"%s\",\"code\":", upper[id], code, lower[id]) != TDX_OK ||
        tdx_format_json_string(out, code, err) != TDX_OK ||
        tdx_buf_append(out, ",\"name\":", 8, err) != TDX_OK ||
        tdx_format_json_string(out, security->name, err) != TDX_OK ||
        tdx_buf_append(out, ",\"category\":", 12, err) != TDX_OK ||
        tdx_format_json_string(out, security->category, err) != TDX_OK ||
        tdx_buf_append(out, ",\"board\":", 9, err) != TDX_OK ||
        tdx_format_json_string(out, security->board, err) != TDX_OK ||
        tdx_buf_append_printf(out, err,
          ",\"multiple\":%u,\"decimal\":%u,\"previous_close_price\":%.6f}",
          (unsigned)security->multiple, (unsigned)security->decimal,
          security->previous_close_price) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
