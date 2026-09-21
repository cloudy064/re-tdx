/* tdx_trades_json.h - JSONL rendering for the 0x0FC5/0x0FC6 trade feed.
 *
 * One line per tick, then one summary line.  The summary is a separate object
 * type rather than a wrapper so the tick stream can be piped and processed
 * before the totals arrive, and so `tail -1` is the summary. */
#ifndef TDX_TRADES_JSON_H
#define TDX_TRADES_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_trades.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One tick, without a trailing newline. */
int tdx_trades_format_tick(tdx_buf *out, const tdx_trade_tick *tick, int market_id,
                           const char *code, const char *date, tdx_error *err);

/* The trailing summary event, without a trailing newline. */
int tdx_trades_format_summary(tdx_buf *out, const tdx_trade_series *series,
                              const tdx_trade_summary *summary, int market_id, const char *code,
                              const char *date, const char *endpoint, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_TRADES_JSON_H */
