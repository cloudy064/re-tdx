/* tdx_auction_json.h - JSONL rendering for the call-auction series. */
#ifndef TDX_AUCTION_JSON_H
#define TDX_AUCTION_JSON_H

#include <stddef.h>

#include "tdx_bytes.h"
#include "tdx_error.h"
#include "tdx_auction.h"

#ifdef __cplusplus
extern "C" {
#endif

/* One auction point, without a trailing newline. */
int tdx_auction_format_point(tdx_buf *out, const tdx_auction_point *point, int market_id,
                             const char *code, const char *trading_date, tdx_error *err);

/* The trailing summary event, without a trailing newline. */
int tdx_auction_format_summary(tdx_buf *out, const tdx_auction_series *series,
                               const tdx_auction_summary *summary, int market_id,
                               const char *code, const char *trading_date, const char *endpoint,
                               tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_AUCTION_JSON_H */
