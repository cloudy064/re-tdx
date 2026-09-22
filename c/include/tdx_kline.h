/* tdx_kline.h - multi-period K-lines over 0x052D.
 *
 * One command serves every period; the period is a u16 request field rather than
 * a separate command.  Ported from the verified C++ implementation
 * (native/src/market/minute_download.cpp).
 *
 * Request, 42 bytes, little endian:
 *   [0..1]   u16 market id
 *   [2..7]   six ASCII code characters, zero padded when shorter
 *   [8..9]   u16 period id
 *   [10..11] u16 period parameter (always 1)
 *   [12..13] u16 start record
 *   [14..15] u16 count, 1..800
 *   [16..41] zero
 *
 * Reply:
 *   u16 count, then per record
 *     intraday period: u16 LC1 date word, u16 minute of day
 *     daily and slower: u32 date as YYYYMMDD
 *     varint open  delta against the PREVIOUS RECORD's close
 *     varint close delta against this record's open
 *     varint high  delta against this record's open
 *     varint low   delta against this record's open
 *     u32 volume, wire encoded
 *     u32 amount, wire encoded
 *     index mode only: u16 extra_1, u16 extra_2 (advance/decline breadth)
 *
 * Three things that are easy to get wrong and are therefore pinned by tests:
 *   1. Prices arrive as milli-units, so the scale is 1000, not the 100/1000
 *      divisor the quote and trade families use.
 *   2. The deltas chain across records in ARRIVAL order, and the server sends
 *      newest first, so the chronological sort has to happen after decoding.
 *   3. The LC1 date word packs year/month/day into a u16 and only covers
 *      2004 onwards. */
#ifndef TDX_KLINE_H
#define TDX_KLINE_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_KLINE 0x052D

#define TDX_KLINE_PAGE_SIZE_MAX 800
#define TDX_KLINE_PAGES_MAX 20
#define TDX_KLINE_START_MAX 65535
#define TDX_KLINE_REQUEST_SIZE 42
#define TDX_KLINE_PERIOD_PARAMETER 1

typedef struct tdx_kline_period {
    const char *name; /* canonical: time, 1m, 5m, 15m, 30m, 60m, day, week, month */
    uint16_t id;
    int intraday;
} tdx_kline_period;

/* Accepts time/timeline/1m/5m/15m/30m/60m/1h/day/1d/daily/week/1w/weekly/
 * month/1mo/monthly.  "time" and "1m" share period id 7, which is why the
 * canonical name is reported back rather than the text the user typed. */
int tdx_kline_period_parse(const char *text, tdx_kline_period *out, tdx_error *err);

/* Public TDX block indexes live on the Shanghai market in this wire family. */
int tdx_kline_is_block_index_code(const char *code);

/* Unpacks the LC1 date word: year = value / 2048 + 2004. */
int tdx_kline_lc1_date(uint16_t word, int *out_date, tdx_error *err);

typedef struct tdx_kline_bar {
    int date; /* YYYYMMDD */
    int hour;
    int minute;
    double open;
    double high;
    double low;
    double close;
    double amount;
    int64_t volume;
    uint16_t extra_1; /* index mode only: advancing / declining counts */
    uint16_t extra_2;
    int has_extras;
} tdx_kline_bar;

typedef struct tdx_kline_page {
    tdx_kline_bar *bars;
    size_t count;
    size_t capacity;
} tdx_kline_page;

void tdx_kline_page_init(tdx_kline_page *page);
void tdx_kline_page_free(tdx_kline_page *page);

typedef struct tdx_kline_series {
    tdx_kline_bar *bars; /* chronological after tdx_kline_sort */
    size_t count;
    size_t capacity;
    size_t pages;
    uint16_t page_size;
    uint16_t period_id;
    int index_mode;
    int reached_end; /* the server returned fewer bars than were asked for */
} tdx_kline_series;

void tdx_kline_series_init(tdx_kline_series *series);
void tdx_kline_series_free(tdx_kline_series *series);

/* --- framing ---------------------------------------------------------- */

int tdx_kline_build_request(int market_id, const char *code, uint16_t period_id,
                            uint16_t start, uint16_t count, tdx_buf *out, tdx_error *err);

/* Refuses a body with unconsumed trailing bytes, like every other parser here. */
int tdx_kline_parse(const uint8_t *payload, size_t size, uint16_t period_id, int index_mode,
                    tdx_kline_page *out, tdx_error *err);

/* Stable sort by date, hour, minute. */
void tdx_kline_sort(tdx_kline_bar *bars, size_t count);

/* --- session-bound ---------------------------------------------------- */

/* Walks up to max_pages pages and sorts the result chronologically.  The walk
 * stops early when a short page arrives, which is how the server signals the
 * oldest available record. */
int tdx_kline_fetch(tdx_connection *connection, int market_id, const char *code,
                    const tdx_kline_period *period, int index_mode, uint16_t start,
                    uint16_t page_size, size_t max_pages, tdx_kline_series *out,
                    tdx_error *err);

int tdx_kline_fetch_from_pool(const tdx_endpoint_pool *pool, int timeout_ms, int market_id,
                              const char *code, const tdx_kline_period *period,
                              int index_mode, uint16_t start, uint16_t page_size,
                              size_t max_pages, tdx_kline_series *out, char *endpoint_used,
                              size_t endpoint_used_size, tdx_error *err);

#ifdef __cplusplus
}
#endif

#endif /* TDX_KLINE_H */
