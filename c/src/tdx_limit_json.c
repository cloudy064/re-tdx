#include "tdx_limit_json.h"
#include "tdx_json_write.h"
#include <math.h>
#include <string.h>

#define LITERAL(out, err, text) tdx_buf_append(out, text, sizeof(text) - 1, err)
static int fixed_text(tdx_buf *out, const char *text, size_t capacity, tdx_error *err) {
    const char *end = (const char *)memchr(text, '\0', capacity);
    return tdx_format_json_string_n(out, text, end ? (size_t)(end - text) : capacity, err);
}
static int number4(tdx_buf *out, double value, tdx_error *err) {
    return isfinite(value) ? tdx_buf_append_printf(out, err, "%.4f", value)
                           : LITERAL(out, err, "null");
}
int tdx_limit_format_rules(tdx_buf *out, const tdx_limit_rules *rules, tdx_error *err) {
    if (!out || !rules) {
        tdx_error_set(err, "rendering limit rules needs a buffer and rules");
        return TDX_ERR;
    }
    if (LITERAL(out, err, "{\"type\":\"limit_rules\",\"source\":") != TDX_OK ||
        fixed_text(out, rules->source, sizeof(rules->source), err) != TDX_OK ||
        tdx_buf_append_printf(out, err, ",\"sz_st_10_date\":%d,\"sh_st_10_date\":%d,\"chinext_rate\":",
                              rules->sz_st_10_date, rules->sh_st_10_date) != TDX_OK ||
        number4(out, rules->chinext_rate, err) != TDX_OK ||
        LITERAL(out, err, ",\"star_rate\":") != TDX_OK ||
        number4(out, rules->star_rate, err) != TDX_OK ||
        LITERAL(out, err, ",\"beijing_rate\":") != TDX_OK ||
        number4(out, rules->beijing_rate, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}
int tdx_limit_format_prices(tdx_buf *out, const tdx_code *security, double previous_close,
                             const tdx_limit_prices *prices, tdx_error *err) {
    if (!out || !security || !prices) {
        tdx_error_set(err, "rendering limit prices needs a buffer, security and prices");
        return TDX_ERR;
    }
    if (LITERAL(out, err, "{\"type\":\"limit_prices\",\"code\":") != TDX_OK ||
        fixed_text(out, security->code, sizeof(security->code), err) != TDX_OK ||
        tdx_buf_append_printf(out, err, ",\"market_id\":%d,\"security_class\":%d,\"previous_close\":",
                              security->market_id, prices->security_class) != TDX_OK ||
        number4(out, previous_close, err) != TDX_OK ||
        LITERAL(out, err, ",\"rate\":") != TDX_OK || number4(out, prices->rate, err) != TDX_OK ||
        LITERAL(out, err, ",\"upper\":") != TDX_OK || number4(out, prices->upper, err) != TDX_OK ||
        LITERAL(out, err, ",\"lower\":") != TDX_OK || number4(out, prices->lower, err) != TDX_OK ||
        LITERAL(out, err, ",\"source\":") != TDX_OK ||
        fixed_text(out, prices->source, sizeof(prices->source), err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}
