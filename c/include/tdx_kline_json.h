/* tdx_kline_json.h - JSONL rendering for the 0x052D K-line feed.
 *
 * One line per bar, then one summary line, matching the trade-detail feed so a
 * consumer only has to learn one shape per family. */
#ifndef TDX_KLINE_JSON_H
#define TDX_KLINE_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_kline.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One bar, without a trailing newline. */
int tdx_kline_format_bar(tdx_buf *out, const tdx_kline_bar *bar, int market_id,
                         const char *code, const char *period_name, tdx_error *err);

/* The trailing summary event, without a trailing newline. */
int tdx_kline_format_summary(tdx_buf *out, const tdx_kline_series *series, int market_id,
                             const char *code, const char *period_name, uint16_t start,
                             const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_KLINE_JSON_H */
