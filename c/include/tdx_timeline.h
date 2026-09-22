/* tdx_timeline.h - intraday time-share series over 0x0537 (today) / 0x0FB4 (one date).
 *
 * Unlike every other module here this one is not a port: 0x0537 and 0x0FB4 are
 * listed as confirmed in doc/02-engine/07-useful-live-features.md but have no
 * implementation anywhere in the tree, so the wire format was recovered from the
 * live server.  The recovery is recorded in output/timeline_probe_evidence.txt
 * and pinned by test_timeline.c against the captured reply.
 *
 * Request, 12 bytes:
 *   u8  market
 *   u8  0
 *   code[6]
 *   [8..11] not interpreted by the server
 * The last four bytes are documented as ignored because sending
 * 00 00 00 00 and 04 27 35 01 (a date) produced byte-identical replies; this
 * build writes zeros.  The reply is always the whole session, newest last:
 * there is no paging and no start offset.
 *
 * Reply:
 *   u16 count          up to one point per session minute
 *   u16 reserved       observed zero
 *   per point, three varints:
 *     price offset     point 0 carries the session base in 1/100 yuan;
 *                      every later point carries its offset FROM that base,
 *                      not a delta against the previous point
 *     average offset   same shape, 1/10000 yuan
 *     volume           that minute's volume in lots
 * so price[i] = (base_price + offset[i]) / 100, with offset[0] reading as 0.
 *
 * That "base plus offset" shape is the one trap here: reading the fields as
 * per-point deltas makes the price drift away from the real series, which is
 * exactly what a first pass at this file did.
 *
 * Verified on 240/240 points of a live reply: price matched the same day's
 * 0x052D 1m closes exactly, the average matched the running VWAP to 1.5e-3 yuan,
 * and the volumes summed to the day total in lots.  See
 * output/timeline_probe_evidence.txt. */
#ifndef TDX_TIMELINE_H
#define TDX_TIMELINE_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_TIMELINE_TODAY 0x0537
#define TDX_CMD_TIMELINE_HISTORY 0x0FB4

#define TDX_TIMELINE_REQUEST_SIZE 12
#define TDX_TIMELINE_POINTS_MAX 1024 /* a session is 240; the cap is a sanity bound */
#define TDX_TIMELINE_SESSION_POINTS 240

typedef struct tdx_timeline_point {
    size_t index;
    int minute_of_day; /* derived: the wire carries no time, only an order */
    double price;
    double average_price; /* running VWAP at this minute */
    int64_t volume_hand;
    int64_t price_offset_raw;
    int64_t average_offset_raw;
} tdx_timeline_point;

typedef struct tdx_timeline {
    tdx_timeline_point *points;
    size_t count;
    size_t capacity;
    double base_price;   /* point 0's price, 1/100 yuan on the wire */
    double base_average; /* point 0's average, 1/10000 yuan on the wire */
    int64_t reserved;    /* the reply's second header word */
} tdx_timeline;

/* Initialize fresh storage. Does not release an existing allocation: use free
 * before reinitializing a populated timeline. free also resets all fields. */
void tdx_timeline_init(tdx_timeline *timeline);
void tdx_timeline_free(tdx_timeline *timeline);

/* The label for point `index` of a session: 09:31..11:30 then 13:01..15:00. */
int tdx_timeline_minute_of_day(size_t index);

int tdx_timeline_build_request(int market_id, const char *code, tdx_buf *out, tdx_error *err);

/* out must be initialized and empty. The caller owns any allocated points,
 * including partial results on failure, and must free them before parsing again. */
int tdx_timeline_parse(const uint8_t *payload, size_t size, tdx_timeline *out, tdx_error *err);

/* --- session-bound ---------------------------------------------------- */

int tdx_timeline_fetch(tdx_connection *connection, int market_id, const char *code,
                       int history, const char *date, tdx_timeline *out, tdx_error *err);

int tdx_timeline_fetch_from_pool(const tdx_endpoint_pool *pool, int timeout_ms, int market_id,
                                 const char *code, int history, const char *date,
                                 tdx_timeline *out, char *endpoint_used,
                                 size_t endpoint_used_size, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_TIMELINE_H */
