#ifndef TDX_LIMIT_JSON_H
#define TDX_LIMIT_JSON_H
#include "tdx_bytes.h"
#include "tdx_limit.h"
#include "tdx_quote.h"
#ifdef __cplusplus
extern "C" {
#endif
int tdx_limit_format_rules(tdx_buf *out, const tdx_limit_rules *rules, tdx_error *err);
int tdx_limit_format_prices(tdx_buf *out, const tdx_code *security, double previous_close,
                             const tdx_limit_prices *prices, tdx_error *err);
#ifdef __cplusplus
}
#endif
#endif
