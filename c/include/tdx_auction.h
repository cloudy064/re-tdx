/* tdx_auction.h - call-auction point series over 0x056A.
 *
 * Ported from the verified C++ implementation (native/src/market/auction.cpp).
 *
 * The auction is the one part of a session nothing else here exposes: the
 * snapshot and the K-line give you the auction's outcome, and only this command
 * gives the series of virtual match prices, matched volume and unmatched
 * imbalance that produced it.
 *
 * Request, 28 bytes:
 *   u8  market
 *   u8  0
 *   code[6]
 *   u32 0            constant
 *   u32 selector     0 = opening auction only, non-zero = opening and closing
 *   u32 0            constant
 *   u32 start_raw    first record
 *   u32 limit        1..5000
 *
 * Reply: u16 count, then count fixed 16-byte records:
 *   [0..1]   u16 minute of day
 *   [2..5]   f32 virtual match price
 *   [6..9]   u32 matched volume, lots
 *   [10..13] i32 unmatched lots, SIGNED: positive means the buy side is left
 *            over, negative means the sell side
 *   [14]     reserved, observed zero
 *   [15]     u8 second
 *
 * The reply length must be exactly 2 + count * 16; anything else is rejected
 * rather than partially read. */
#ifndef TDX_AUCTION_H
#define TDX_AUCTION_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_AUCTION 0x056A

#define TDX_AUCTION_REQUEST_SIZE 28
#define TDX_AUCTION_RECORD_SIZE 16
#define TDX_AUCTION_LIMIT_MAX 5000u

typedef struct tdx_auction_point {
    size_t index;
    int minute_of_day;
    int second;
    int time_seconds; /* minute_of_day * 60 + second */
    double price;
    int64_t matched_volume_hand;
    double matched_amount_yuan; /* price * matched_volume_hand * 100 */
    int64_t unmatched_signed_hand;
    int64_t unmatched_volume_hand; /* absolute value of the field above */
    int unmatched_direction_raw;   /* 1 buy, -1 sell, 0 balanced */
    int reserved;                  /* byte 14, observed zero on every sample */
} tdx_auction_point;

typedef struct tdx_auction_series {
    tdx_auction_point *points;
    size_t count;
    size_t capacity;
    uint32_t selector;
    uint32_t start_raw;
    uint32_t limit;
} tdx_auction_series;

void tdx_auction_series_init(tdx_auction_series *series);
void tdx_auction_series_free(tdx_auction_series *series);

/* One auction segment.  has_points is 0 when the segment is absent, which is the
 * normal case for the closing side while selector is 0. */
typedef struct tdx_auction_segment {
    int has_points;
    size_t point_count;
    int start_time_seconds;
    int end_time_seconds;
    double first_price;
    double last_price;
    double min_price;
    double max_price;
    int64_t last_matched_volume_hand;
    double last_matched_amount_yuan;
    int64_t last_unmatched_signed_hand;
    int64_t last_unmatched_volume_hand;
    int last_unmatched_direction_raw;
    int64_t max_unmatched_volume_hand;
    int max_unmatched_time_seconds;
    size_t unmatched_direction_flips;
    size_t matched_volume_monotonic_violations;
} tdx_auction_segment;

typedef struct tdx_auction_summary {
    size_t point_count;
    tdx_auction_segment opening; /* points before noon */
    tdx_auction_segment closing; /* points from noon on */
    int has_largest_gap;
    int largest_gap_seconds;
    int gap_after_time_seconds;
    int gap_before_time_seconds;
    size_t reserved_nonzero_points;
} tdx_auction_summary;

/* "buy" / "sell" / "balanced". */
const char *tdx_auction_direction_text(int direction_raw);

/* HH:MM:SS from a seconds-since-midnight value. */
int tdx_auction_time_label(int time_seconds, char *out, size_t out_size);

/* --- framing ---------------------------------------------------------- */

int tdx_auction_build_request(int market_id, const char *code, uint32_t selector,
                              uint32_t start_raw, uint32_t limit, tdx_buf *out, tdx_error *err);

int tdx_auction_parse(const uint8_t *payload, size_t size, uint32_t selector,
                      uint32_t start_raw, uint32_t limit, tdx_auction_series *out,
                      tdx_error *err);

void tdx_auction_summarize(const tdx_auction_series *series, tdx_auction_summary *out);

/* --- session-bound ---------------------------------------------------- */

int tdx_auction_fetch(tdx_connection *connection, int market_id, const char *code,
                      uint32_t selector, uint32_t start_raw, uint32_t limit,
                      tdx_auction_series *out, tdx_error *err);

int tdx_auction_fetch_from_pool(const tdx_endpoint_pool *pool, int timeout_ms, int market_id,
                                const char *code, uint32_t selector, uint32_t start_raw,
                                uint32_t limit, tdx_auction_series *out, char *endpoint_used,
                                size_t endpoint_used_size, char server_date[9], tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_AUCTION_H */
