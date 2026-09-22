/* tdx_trades.h - L1 trade details (成交明细) over 0x0FC5 / 0x0FC6.
 *
 * Scope note, because the word "逐笔" is ambiguous in this domain: these two
 * commands are the public L1 trade-detail feed, and their time resolution is a
 * MINUTE, not a second.  The exchange-level second-by-second tape is an L2
 * product served by other commands and gated behind an entitlement.  The C++
 * research tool states the same boundary in its own help text:
 *   "公开 L1 成交明细，时间精度为分钟；不是 Level2 秒级逐笔成交或委托"
 *
 * Wire layout, ported byte for byte from the verified C++ implementation:
 *
 *   request  0x0FC5  u8 market, 0x00, code[6], u16 start, u16 count   (12 bytes)
 *            0x0FC6  u32 date, u16 market, code[6], u16 start, u16 count (16 bytes)
 *   reply    0x0FC5  u16 count, then that many records
 *            0x0FC6  u16 count, f32 price base, then that many records
 *   record   u16 minute_of_day, then five varints:
 *              price delta, volume in lots, order count, status, tail
 *
 * The price accumulator starts at zero for every page and the first record's
 * delta is already the absolute price, so a page is self contained: page N+1
 * repeats the same base value rather than continuing from page N.  Verified on
 * the live server: the first record of start=0 and of start=10 both decode to
 * 17.39 for 000623 on 20260612.
 *
 * status 0 = buy, 1 = sell, 2 = neutral (matches the 15:00 call auction), and
 * anything else is reported as status_N rather than being guessed.
 *
 * Paging walks the cursor forward and the pages come back newest first, so the
 * series is reversed once at the end to read chronologically. */
#ifndef TDX_TRADES_H
#define TDX_TRADES_H

#include <stddef.h>
#include <stdint.h>

#include "tdx_bytes.h"
#include "tdx_endpoint.h"
#include "tdx_error.h"
#include "tdx_quote.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TDX_CMD_TRADES_TODAY 0x0FC5
#define TDX_CMD_TRADES_HISTORY 0x0FC6
#define TDX_CMD_HEARTBEAT 0x0004

#define TDX_TRADES_DATE_LENGTH 8
#define TDX_TRADES_MAX_PAGES 100
#define TDX_TRADES_PAGE_SIZE_TODAY 1800
#define TDX_TRADES_PAGE_SIZE_HISTORY 2000

typedef struct tdx_trade_tick {
    size_t index;          /* position inside its own page */
    size_t absolute_index; /* start + index across the whole paging walk */
    int time_minutes;      /* minute of the day, exchange local time */
    double price;
    int64_t volume_hand;
    int64_t order_count;
    int64_t status_raw;
    int64_t price_delta_raw;
    int64_t price_acc_raw;
    int64_t tail_raw;
    double amount_yuan; /* price * volume_hand * 100 */
} tdx_trade_tick;

typedef struct tdx_trade_page {
    tdx_trade_tick *ticks;
    size_t count;
    size_t capacity;
    int has_price_base;
    double price_base;
} tdx_trade_page;

void tdx_trades_page_init(tdx_trade_page *page);
void tdx_trades_page_free(tdx_trade_page *page);

typedef struct tdx_trade_series {
    tdx_trade_tick *ticks;
    size_t count;
    size_t capacity;
    size_t pages;
    uint16_t page_size;
    int price_divisor;
    int has_price_base;
    double price_base;
    int history; /* 1 when the series came from 0x0FC6 */
} tdx_trade_series;

void tdx_trades_series_init(tdx_trade_series *series);
void tdx_trades_series_free(tdx_trade_series *series);

typedef struct tdx_trade_summary {
    size_t tick_count;
    size_t minute_count;
    int64_t volume_hand;
    int64_t order_count;
    double amount_yuan;
    double vwap;
    int has_price_range;
    double first_price;
    double last_price;
    double high_price;
    double low_price;
    int first_time_minutes;
    int last_time_minutes;
    int has_times;
    int64_t buy_volume_hand;
    int64_t sell_volume_hand;
    int64_t neutral_volume_hand;
    double buy_amount_yuan;
    double sell_amount_yuan;
    double neutral_amount_yuan;
    /* one bucket per distinct status value, ascending */
    struct {
        int64_t status;
        size_t count;
    } status_counts[8];
    size_t status_count;
} tdx_trade_summary;

/* --- pure helpers ----------------------------------------------------- */

/* "buy" / "sell" / "neutral" / "status_N".  Returns the text length. */
size_t tdx_trades_side_text(int64_t status_raw, char *out, size_t out_size);

/* Wire decimal policy: bonds and exchange funds carry one extra digit. */
int tdx_trades_price_divisor(const char *code);

/* Accepts YYYYMMDD (the C++ tool also accepts YYYY-MM-DD; the CLI normalises
 * first).  Validates the calendar, not just the digit count. */
int tdx_trades_date_valid(const char *date, tdx_error *err);

int tdx_trades_time_label(int minutes, char *out, size_t out_size);

/* --- framing ---------------------------------------------------------- */

int tdx_trades_build_today_request(int market_id, const char *code, uint16_t start,
                                   uint16_t count, tdx_buf *out, tdx_error *err);
int tdx_trades_build_history_request(int market_id, const char *code, const char *date,
                                     uint16_t start, uint16_t count, tdx_buf *out,
                                     tdx_error *err);

/* Parses one reply body.  price_divisor comes from tdx_trades_price_divisor.
 * Both refuse a body with unconsumed trailing bytes, matching the peer
 * implementation: a short read must not look like a clean page. */
int tdx_trades_parse_today(const uint8_t *payload, size_t size, uint16_t start,
                           int price_divisor, tdx_trade_page *out, tdx_error *err);
int tdx_trades_parse_history(const uint8_t *payload, size_t size, uint16_t start,
                             int price_divisor, tdx_trade_page *out, tdx_error *err);

/* --- session-bound ---------------------------------------------------- */

/* 0x0004 returns the server's trading date at body offset 6.  Only the today
 * command needs it, because its own reply carries no date. */
int tdx_trades_read_server_date(tdx_connection *connection, char out[TDX_TRADES_DATE_LENGTH + 1],
                                tdx_error *err);

/* Pages until an empty page, then reverses into chronological order. */
int tdx_trades_fetch(tdx_connection *connection, int market_id, const char *code,
                     const char *date, uint16_t page_size, size_t max_pages,
                     tdx_trade_series *out, tdx_error *err);

/* Same, opening one of the pool's endpoints and closing it again. */
int tdx_trades_fetch_from_pool(const tdx_endpoint_pool *pool, int timeout_ms, int market_id,
                               const char *code, const char *date, uint16_t page_size,
                               size_t max_pages, tdx_trade_series *out, char *endpoint_used,
                               size_t endpoint_used_size,
                               char server_date[TDX_TRADES_DATE_LENGTH + 1], tdx_error *err);

/* Aggregates a whole series, plus the per-minute bucket count. */
void tdx_trades_summarize(const tdx_trade_series *series, tdx_trade_summary *out);

#ifdef __cplusplus
}
#endif

#endif /* TDX_TRADES_H */
