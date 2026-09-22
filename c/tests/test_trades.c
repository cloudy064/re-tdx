/* test_trades.c - 0x0FC5/0x0FC6 L1 trade details.
 *
 * The reply fixtures are the bytes a live public node actually returned for
 * SZ000623; they are kept verbatim so the record walk is pinned against the
 * peer's real encoding rather than against the encoder I would otherwise write
 * to match my own decoder.  See output/zst_trades_probe_evidence.txt.
 *
 * The 0x0FC6 fixture is fully accounted for: 2 header bytes, 4 price-base bytes
 * and ten records whose varints add up to the remaining 74 bytes, which is what
 * the parser's "no trailing bytes" rule checks. */
#include <stdio.h>
#include <string.h>

#include "tdx_trades.h"
#include "tdx_trades_json.h"
#include "render_check.h"

static int failures = 0;

#define CHECK(condition, ...)                                                        \
    do {                                                                             \
        if (!(condition)) {                                                          \
            printf("FAIL %s:%d: ", __FILE__, __LINE__);                              \
            printf(__VA_ARGS__);                                                     \
            printf("\n");                                                            \
            failures++;                                                              \
        }                                                                            \
    } while (0)

/* Live 0x0FC6 reply for SZ000623 on 20260612, start=0 count=10. */
static const uint8_t history_reply[] = {
    0x0a, 0x00, 0x8f, 0xc2, 0x85, 0x41, 0x80, 0x03, 0x8b, 0x1b, 0x17, 0x03, 0x00, 0x00,
    0x80, 0x03, 0x00, 0x20, 0x05, 0x00, 0x00, 0x80, 0x03, 0x00, 0x33, 0x07, 0x00, 0x00,
    0x80, 0x03, 0x00, 0x2a, 0x09, 0x00, 0x00, 0x80, 0x03, 0x00, 0x0d, 0x03, 0x00, 0x00,
    0x80, 0x03, 0x00, 0x23, 0x04, 0x00, 0x00, 0x80, 0x03, 0x00, 0x16, 0x04, 0x00, 0x00,
    0x80, 0x03, 0x00, 0x83, 0x02, 0x04, 0x00, 0x00, 0x81, 0x03, 0x41, 0x12, 0x03, 0x01,
    0x00, 0x84, 0x03, 0x01, 0xb4, 0x12, 0x91, 0x02, 0x02, 0x00};

/* Live 0x0FC5 reply for SZ000623, start=0 count=10. */
static const uint8_t today_reply[] = {
    0x0a, 0x00, 0x80, 0x03, 0xb0, 0x1b, 0x11, 0x0a, 0x01, 0x00, 0x80, 0x03, 0x01, 0x14,
    0x09, 0x00, 0x00, 0x80, 0x03, 0x00, 0xb7, 0x01, 0x0f, 0x00, 0x00, 0x80, 0x03, 0x00,
    0x14, 0x07, 0x01, 0x00, 0x80, 0x03, 0x41, 0x1a, 0x06, 0x01, 0x00, 0x80, 0x03, 0x01,
    0x03, 0x02, 0x01, 0x00, 0x81, 0x03, 0x00, 0x02, 0x01, 0x01, 0x00, 0x84, 0x03, 0x42,
    0x89, 0x11, 0x83, 0x02, 0x02, 0x00, 0x8e, 0x03, 0x00, 0x01, 0x01, 0x05, 0x00, 0x92,
    0x03, 0x00, 0x03, 0x01, 0x05, 0x00};

/* --- pure helpers ----------------------------------------------------- */

static void test_helpers(void) {
    char text[24];
    char label[8];
    tdx_error error;
    error.message[0] = '\0';

    CHECK(strcmp((tdx_trades_side_text(0, text, sizeof(text)), text), "buy") == 0, "0 is buy");
    CHECK(strcmp((tdx_trades_side_text(1, text, sizeof(text)), text), "sell") == 0, "1 is sell");
    CHECK(strcmp((tdx_trades_side_text(2, text, sizeof(text)), text), "neutral") == 0,
          "2 is neutral");
    CHECK(strcmp((tdx_trades_side_text(5, text, sizeof(text)), text), "status_5") == 0,
          "an unknown status must be reported as status_N, got %s", text);

    CHECK(tdx_trades_price_divisor("000623") == 100, "an ordinary stock scales by 100");
    CHECK(tdx_trades_price_divisor("600000") == 100, "a Shanghai stock scales by 100");
    CHECK(tdx_trades_price_divisor("102218") == 1000, "a bond scales by 1000");
    CHECK(tdx_trades_price_divisor("123456") == 1000, "10..12 scale by 1000");
    CHECK(tdx_trades_price_divisor("159915") == 1000, "an exchange fund scales by 1000");
    CHECK(tdx_trades_price_divisor("204001") == 100, "204 is not in the thousand list");
    CHECK(tdx_trades_price_divisor(NULL) == 100, "a null code falls back to 100");

    CHECK(tdx_trades_time_label(0, label, sizeof(label)) == TDX_OK && strcmp(label, "00:00") == 0,
          "midnight, got %s", label);
    CHECK(tdx_trades_time_label(9 * 60 + 25, label, sizeof(label)) == TDX_OK &&
              strcmp(label, "09:25") == 0,
          "the auction, got %s", label);
    CHECK(tdx_trades_time_label(15 * 60, label, sizeof(label)) == TDX_OK &&
              strcmp(label, "15:00") == 0,
          "the close, got %s", label);
    CHECK(tdx_trades_time_label(14 * 60 + 56, label, sizeof(label)) == TDX_OK &&
              strcmp(label, "14:56") == 0,
          "a continuous-session minute, got %s", label);
    CHECK(tdx_trades_time_label(0, label, 3) == TDX_ERR, "a short label buffer must fail");
}

static void test_date_validation(void) {
    tdx_error error;
    error.message[0] = '\0';
    CHECK(tdx_trades_date_valid("20260612", &error) == TDX_OK, "a real date: %s", error.message);
    CHECK(tdx_trades_date_valid("20260229", &error) == TDX_ERR,
          "2026 is not a leap year, 29 February must fail");
    CHECK(tdx_trades_date_valid("20240229", &error) == TDX_OK, "2024 is a leap year: %s",
          error.message);
    CHECK(tdx_trades_date_valid("20260230", &error) == TDX_ERR, "30 February must fail");
    CHECK(tdx_trades_date_valid("20261301", &error) == TDX_ERR, "month 13 must fail");
    CHECK(tdx_trades_date_valid("2026-06-12", &error) == TDX_ERR,
          "a dashed date must fail here; the CLI normalises first");
    CHECK(tdx_trades_date_valid("", &error) == TDX_ERR, "an empty date must fail");
}

/* --- requests --------------------------------------------------------- */

static void test_requests(void) {
    tdx_buf request;
    tdx_error error;
    error.message[0] = '\0';
    tdx_buf_init(&request);

    CHECK(tdx_trades_build_today_request(0, "000623", 0, 1800, &request, &error) == TDX_OK,
          "today build: %s", error.message);
    CHECK(request.len == 12, "the 0x0FC5 body is %zu bytes, expected 12", request.len);
    if (request.len == 12) {
        CHECK(request.data[0] == 0, "market at +0");
        CHECK(request.data[1] == 0, "the reserved byte at +1 must be zero");
        CHECK(memcmp(request.data + 2, "000623", 6) == 0, "the code sits at +2");
        CHECK(tdx_u16le(request.data + 8) == 0, "start at +8");
        CHECK(tdx_u16le(request.data + 10) == 1800, "count at +10");
    }

    CHECK(tdx_trades_build_history_request(1, "600000", "20260612", 10, 2000, &request,
                                           &error) == TDX_OK,
          "history build: %s", error.message);
    CHECK(request.len == 16, "the 0x0FC6 body is %zu bytes, expected 16", request.len);
    if (request.len == 16) {
        CHECK(tdx_u32le(request.data) == 20260612u, "the date is a u32 at +0, got %u",
              (unsigned)tdx_u32le(request.data));
        CHECK(tdx_u16le(request.data + 4) == 1, "market at +4");
        CHECK(memcmp(request.data + 6, "600000", 6) == 0, "the code sits at +6");
        CHECK(tdx_u16le(request.data + 12) == 10, "start at +12");
        CHECK(tdx_u16le(request.data + 14) == 2000, "count at +14");
    }

    CHECK(tdx_trades_build_today_request(0, "000623", 0, 0, &request, &error) == TDX_ERR,
          "a zero count must be refused");
    CHECK(tdx_trades_build_today_request(3, "000623", 0, 10, &request, &error) == TDX_ERR,
          "market 3 must be refused");
    CHECK(tdx_trades_build_today_request(0, "00062", 0, 10, &request, &error) == TDX_ERR,
          "a five-digit code must be refused");
    CHECK(tdx_trades_build_today_request(0, "00062a", 0, 10, &request, &error) == TDX_ERR,
          "a non-numeric code must be refused");
    CHECK(tdx_trades_build_history_request(0, "000623", "2026061", 0, 10, &request, &error) ==
              TDX_ERR,
          "a seven-digit date must be refused");
    tdx_buf_free(&request);
}

/* --- replies ---------------------------------------------------------- */

static void test_parse_history(void) {
    tdx_trade_page page;
    tdx_error error;
    error.message[0] = '\0';
    tdx_trades_page_init(&page);

    CHECK(tdx_trades_parse_history(history_reply, sizeof(history_reply), 0, 100, &page,
                                   &error) == TDX_OK,
          "history parse: %s", error.message);
    CHECK(page.count == 10, "ten records, got %zu", page.count);
    CHECK(page.has_price_base == 1, "the price base must be reported");
    CHECK(page.price_base > 16.719 && page.price_base < 16.721, "price base %.6f",
          page.price_base);
    if (page.count == 10) {
        const tdx_trade_tick *first = &page.ticks[0];
        const tdx_trade_tick *sell = &page.ticks[8];
        const tdx_trade_tick *close = &page.ticks[9];
        CHECK(first->time_minutes == 14 * 60 + 56, "first minute %d", first->time_minutes);
        CHECK(first->price == 17.39, "first price %.4f", first->price);
        CHECK(first->price_acc_raw == 1739, "first accumulated price %lld",
              (long long)first->price_acc_raw);
        CHECK(first->price_delta_raw == 1739, "the first delta is already absolute, got %lld",
              (long long)first->price_delta_raw);
        CHECK(first->volume_hand == 23 && first->order_count == 3, "first volume %lld orders %lld",
              (long long)first->volume_hand, (long long)first->order_count);
        CHECK(first->status_raw == 0, "first status %lld", (long long)first->status_raw);
        CHECK(first->index == 0 && first->absolute_index == 0, "first index %zu/%zu",
              first->index, first->absolute_index);

        CHECK(sell->time_minutes == 14 * 60 + 57, "the sell record minute %d",
              sell->time_minutes);
        CHECK(sell->price == 17.38, "a negative delta must subtract, got %.4f", sell->price);
        CHECK(sell->price_delta_raw == -1, "the sell delta %lld",
              (long long)sell->price_delta_raw);
        CHECK(sell->status_raw == 1, "the sell status %lld", (long long)sell->status_raw);

        CHECK(close->time_minutes == 15 * 60, "the closing record minute %d",
              close->time_minutes);
        CHECK(close->price == 17.39, "the closing price %.4f", close->price);
        CHECK(close->volume_hand == 1204, "the closing auction volume %lld",
              (long long)close->volume_hand);
        CHECK(close->order_count == 145, "the closing order count %lld",
              (long long)close->order_count);
        CHECK(close->status_raw == 2, "the closing auction is neutral, got %lld",
              (long long)close->status_raw);
        CHECK(close->absolute_index == 9, "the closing absolute index %zu",
              close->absolute_index);
        /* 1204 lots at 17.39 is 2,093,756 yuan. */
        CHECK(close->amount_yuan > 2093755.0 && close->amount_yuan < 2093757.0,
              "the closing amount %.2f", close->amount_yuan);
    }
    tdx_trades_page_free(&page);
}

static void test_parse_today(void) {
    tdx_trade_page page;
    tdx_error error;
    size_t index;
    error.message[0] = '\0';
    tdx_trades_page_init(&page);

    CHECK(tdx_trades_parse_today(today_reply, sizeof(today_reply), 0, 100, &page, &error) ==
              TDX_OK,
          "today parse: %s", error.message);
    CHECK(page.count == 10, "ten records, got %zu", page.count);
    CHECK(page.has_price_base == 0, "0x0FC5 carries no price base");
    for (index = 0; index < page.count; ++index) {
        CHECK(page.ticks[index].time_minutes < 24 * 60, "record %zu minute %d", index,
              page.ticks[index].time_minutes);
        CHECK(page.ticks[index].price > 0, "record %zu price %.4f", index,
              page.ticks[index].price);
        CHECK(page.ticks[index].volume_hand >= 0, "record %zu volume %lld", index,
              (long long)page.ticks[index].volume_hand);
    }
    if (page.count == 10) {
        /* i=0 is a sell at 14:56, i=7 is the 15:00 call auction, and i=8/i=9 are
         * the after-hours fixed-price prints at 15:10 and 15:14 with status 5. */
        CHECK(page.ticks[0].time_minutes == 14 * 60 + 56, "first minute %d",
              page.ticks[0].time_minutes);
        CHECK(page.ticks[0].price == 17.76, "first price %.4f", page.ticks[0].price);
        CHECK(page.ticks[0].volume_hand == 17 && page.ticks[0].order_count == 10,
              "first volume %lld orders %lld", (long long)page.ticks[0].volume_hand,
              (long long)page.ticks[0].order_count);
        CHECK(page.ticks[0].status_raw == 1, "first status %lld",
              (long long)page.ticks[0].status_raw);
        CHECK(page.ticks[7].time_minutes == 15 * 60, "the auction minute %d",
              page.ticks[7].time_minutes);
        CHECK(page.ticks[7].volume_hand == 1097 && page.ticks[7].order_count == 131,
              "the auction volume %lld orders %lld", (long long)page.ticks[7].volume_hand,
              (long long)page.ticks[7].order_count);
        CHECK(page.ticks[7].status_raw == 2, "the auction is neutral, got %lld",
              (long long)page.ticks[7].status_raw);
        CHECK(page.ticks[9].time_minutes == 15 * 60 + 14, "last minute %d",
              page.ticks[9].time_minutes);
        CHECK(page.ticks[9].status_raw == 5, "last status %lld",
              (long long)page.ticks[9].status_raw);
    }
    tdx_trades_page_free(&page);
}

static void test_parse_rejects(void) {
    tdx_trade_page page;
    tdx_error error;
    uint8_t body[64];
    error.message[0] = '\0';

    tdx_trades_page_init(&page);
    {
        static const uint8_t empty_page[2] = {0x00, 0x00};
        CHECK(tdx_trades_parse_today(empty_page, sizeof(empty_page), 0, 100, &page, &error) ==
                  TDX_OK,
              "a page with zero records is legal, its walk just stops");
        CHECK(page.count == 0, "no records, got %zu", page.count);
    }
    tdx_trades_page_free(&page);

    tdx_trades_page_init(&page);
    CHECK(tdx_trades_parse_today(history_reply, 1, 0, 100, &page, &error) == TDX_ERR,
          "a one-byte body must be rejected");
    tdx_trades_page_free(&page);

    tdx_trades_page_init(&page);
    CHECK(tdx_trades_parse_history(history_reply, 5, 0, 100, &page, &error) == TDX_ERR,
          "a five-byte history body must be rejected");
    tdx_trades_page_free(&page);

    /* Truncated mid-record. */
    tdx_trades_page_init(&page);
    CHECK(tdx_trades_parse_history(history_reply, sizeof(history_reply) - 4, 0, 100, &page,
                                   &error) == TDX_ERR,
          "a truncated record must be rejected");
    tdx_trades_page_free(&page);

    /* A body that carries more bytes than its declared record count consumes. */
    {
        uint8_t trimmed[sizeof(history_reply)];
        memcpy(trimmed, history_reply, sizeof(trimmed));
        trimmed[0] = 9; /* one record short of what the body actually holds */
        tdx_trades_page_init(&page);
        CHECK(tdx_trades_parse_history(trimmed, sizeof(trimmed), 0, 100, &page, &error) ==
                  TDX_ERR,
              "unconsumed bytes must be rejected");
        CHECK(strstr(error.message, "trailing") != NULL, "the error must say why: %s",
              error.message);
        tdx_trades_page_free(&page);
    }

    /* An impossible minute of day. */
    memset(body, 0, sizeof(body));
    body[0] = 1; /* one record */
    body[1] = 0;
    body[2] = (uint8_t)(25 * 60 & 0xFF); /* 1500 minutes */
    body[3] = (uint8_t)(25 * 60 >> 8);
    tdx_trades_page_init(&page);
    CHECK(tdx_trades_parse_today(body, sizeof(body), 0, 100, &page, &error) == TDX_ERR,
          "minute 1500 must be rejected");
    CHECK(strstr(error.message, "minute-of-day") != NULL, "the error must name the field: %s",
          error.message);
    tdx_trades_page_free(&page);

    /* A varint that runs off the end. */
    memset(body, 0, sizeof(body));
    body[0] = 1;
    body[1] = 0;
    body[2] = 0x80; /* 00:00 */
    body[3] = 0x03;
    body[4] = 0x80; /* a varint with the continuation bit set and nothing after */
    tdx_trades_page_init(&page);
    CHECK(tdx_trades_parse_today(body, 5, 0, 100, &page, &error) == TDX_ERR,
          "a dangling varint must be rejected");
    tdx_trades_page_free(&page);
}

/* --- summary ---------------------------------------------------------- */

static void test_summary(void) {
    tdx_trade_series series;
    tdx_trade_summary summary;
    tdx_trade_page page;
    tdx_error error;
    size_t index;

    error.message[0] = '\0';
    tdx_trades_series_init(&series);
    tdx_trades_page_init(&page);
    CHECK(tdx_trades_parse_history(history_reply, sizeof(history_reply), 0, 100, &page,
                                   &error) == TDX_OK,
          "history parse: %s", error.message);
    series.ticks = page.ticks;
    series.count = page.count;
    series.capacity = page.capacity;
    series.pages = 1;
    series.page_size = 10;
    series.price_divisor = 100;
    series.history = 1;
    series.has_price_base = page.has_price_base;
    series.price_base = page.price_base;

    tdx_trades_summarize(&series, &summary);
    CHECK(summary.tick_count == 10, "tick count %zu", summary.tick_count);
    CHECK(summary.minute_count == 3, "the fixture spans three minutes, got %zu",
          summary.minute_count);
    CHECK(summary.has_times == 1, "times must be reported");
    CHECK(summary.first_time_minutes == 14 * 60 + 56, "first minute %d",
          summary.first_time_minutes);
    CHECK(summary.last_time_minutes == 15 * 60, "last minute %d", summary.last_time_minutes);
    CHECK(summary.first_price == 17.39 && summary.last_price == 17.39, "prices %.4f..%.4f",
          summary.first_price, summary.last_price);
    CHECK(summary.low_price == 17.38, "low %.4f", summary.low_price);
    CHECK(summary.high_price == 17.39, "high %.4f", summary.high_price);
    /* one sell of 18 lots, one neutral close of 1204, the rest buy */
    CHECK(summary.sell_volume_hand == 18, "sell volume %lld",
          (long long)summary.sell_volume_hand);
    CHECK(summary.neutral_volume_hand == 1204, "neutral volume %lld",
          (long long)summary.neutral_volume_hand);
    CHECK(summary.volume_hand == summary.buy_volume_hand + summary.sell_volume_hand +
                                     summary.neutral_volume_hand,
          "the three sides must add up to the total");
    CHECK(summary.amount_yuan > 0, "amount %.2f", summary.amount_yuan);
    CHECK(summary.vwap > 17.38 && summary.vwap < 17.39, "vwap %.6f", summary.vwap);
    CHECK(summary.status_count == 3, "three distinct statuses, got %zu", summary.status_count);
    if (summary.status_count == 3) {
        CHECK(summary.status_counts[0].status == 0, "buckets must be sorted, first is %lld",
              (long long)summary.status_counts[0].status);
        CHECK(summary.status_counts[1].status == 1, "second bucket %lld",
              (long long)summary.status_counts[1].status);
        CHECK(summary.status_counts[2].status == 2, "third bucket %lld",
              (long long)summary.status_counts[2].status);
        CHECK(summary.status_counts[0].count == 8, "eight buy prints, got %zu",
              summary.status_counts[0].count);
    }

    /* An empty series must not invent prices. */
    memset(&series, 0, sizeof(series));
    tdx_trades_summarize(&series, &summary);
    CHECK(summary.tick_count == 0 && summary.has_price_range == 0 && summary.has_times == 0,
          "an empty series stays empty");
    CHECK(summary.volume_hand == 0, "volume 0");
    for (index = 0; index < 1; ++index)
        (void)index;

    tdx_trades_page_free(&page);
}

/* --- JSON rendering --------------------------------------------------- */

/* Counts structural braces outside string literals.  This is the check that
 * would have caught a summary object that never closed. */
static int braces_balanced(const char *text) {
    int depth = 0;
    int in_string = 0;
    int escaped = 0;
    const char *cursor;
    for (cursor = text; *cursor; ++cursor) {
        if (in_string) {
            if (escaped)
                escaped = 0;
            else if (*cursor == '\\')
                escaped = 1;
            else if (*cursor == '"')
                in_string = 0;
            continue;
        }
        if (*cursor == '"')
            in_string = 1;
        else if (*cursor == '{')
            depth++;
        else if (*cursor == '}') {
            depth--;
            if (depth < 0)
                return 0;
        }
    }
    return !in_string && depth == 0;
}

static void test_json_rendering(void) {
    tdx_trade_series series;
    tdx_trade_summary summary;
    tdx_trade_page page;
    tdx_buf line;
    tdx_error error;
    char text[4096];
    size_t copy;
    size_t index;

    error.message[0] = '\0';
    tdx_trades_series_init(&series);
    tdx_trades_page_init(&page);
    tdx_buf_init(&line);
    CHECK(tdx_trades_parse_history(history_reply, sizeof(history_reply), 0, 100, &page,
                                   &error) == TDX_OK,
          "history parse: %s", error.message);
    series.ticks = page.ticks;
    series.count = page.count;
    series.capacity = page.capacity;
    series.pages = 1;
    series.page_size = 2000;
    series.price_divisor = 100;
    series.history = 1;
    series.has_price_base = 1;
    series.price_base = page.price_base;
    tdx_trades_summarize(&series, &summary);

    CHECK(tdx_trades_format_tick(&line, &series.ticks[0], 0, "000623", "20260612", &error) ==
              TDX_OK,
          "tick render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the tick object must be balanced: %s", text);
    {
        /* Balanced is not the same as parseable; the project's own parser decides. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
    CHECK(strstr(text, "\"type\":\"trade\"") != NULL, "the tick type");
    CHECK(strstr(text, "\"security_id\":\"SZ000623\"") != NULL, "the identity");
    CHECK(strstr(text, "\"trading_date\":\"20260612\"") != NULL, "the date");
    CHECK(strstr(text, "\"time\":\"14:56\"") != NULL, "the time label");
    CHECK(strstr(text, "\"price\":17.390000") != NULL, "the price");
    CHECK(strstr(text, "\"side\":\"buy\"") != NULL, "the side");

    tdx_buf_clear(&line);
    CHECK(tdx_trades_format_summary(&line, &series, &summary, 0, "000623", "20260612",
                                    "1.2.3.4:7709", &error) == TDX_OK,
          "summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "the summary object must be balanced (length %zu)", line.len);
    {
        /* Balanced is not the same as parseable; the project's own parser decides. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
    CHECK(text[0] == '{' && text[line.len - 1] == '}', "the summary must end with '}': ...%s",
          line.len > 20 ? text + line.len - 20 : text);
    CHECK(strstr(text, "\"type\":\"trade_summary\"") != NULL, "the summary type");
    CHECK(strstr(text, "\"command\":\"0x0FC6\"") != NULL, "the command");
    CHECK(strstr(text, "\"endpoint\":\"1.2.3.4:7709\"") != NULL, "the endpoint");
    CHECK(strstr(text, "\"pages\":1") != NULL, "the page count");
    /* The base is a float32 on the wire, so 16.72 arrives as 16.719999 and is
     * reported with that precision rather than silently rounded. */
    CHECK(strstr(text, "\"price_base\":16.719999") != NULL, "the price base: %s", text);
    CHECK(strstr(text, "\"tick_count\":10") != NULL, "the tick count");
    CHECK(strstr(text, "\"minute_count\":3") != NULL, "the minute count");
    CHECK(strstr(text, "\"status_counts\":{\"0\":8,\"1\":1,\"2\":1}") != NULL,
          "the status buckets: %s", text);

    /* An empty series must still render a valid object with null timestamps. */
    memset(&series, 0, sizeof(series));
    tdx_trades_summarize(&series, &summary);
    tdx_buf_clear(&line);
    CHECK(tdx_trades_format_summary(&line, &series, &summary, 0, "000623", NULL, NULL,
                                    &error) == TDX_OK,
          "empty summary render: %s", error.message);
    copy = line.len < sizeof(text) - 1 ? line.len : sizeof(text) - 1;
    memcpy(text, line.data, copy);
    text[copy] = '\0';
    CHECK(braces_balanced(text), "an empty summary must still be balanced: %s", text);
    {
        /* Balanced is not the same as parseable; the project's own parser decides. */
        char reason[192];
        CHECK(render_parses(text, reason, sizeof(reason)),
              "and it parses as JSON: %s\n    %s", reason, text);
    }
    CHECK(strstr(text, "\"trading_date\":null") != NULL, "a missing date renders null");
    CHECK(strstr(text, "\"first_time\":null") != NULL, "missing times render null");
    CHECK(strstr(text, "\"vwap\":null") != NULL, "a zero-volume vwap renders null");

    for (index = 0; index < series.count; ++index)
        (void)index;
    tdx_buf_free(&line);
    tdx_trades_page_free(&page);
}

int main(void) {
    test_helpers();
    test_date_validation();
    test_requests();
    test_parse_history();
    test_parse_today();
    test_parse_rejects();
    test_summary();
    test_json_rendering();

    if (failures) {
        printf("%d trades check(s) failed\n", failures);
        return 1;
    }
    printf("trades checks passed\n");
    return 0;
}
