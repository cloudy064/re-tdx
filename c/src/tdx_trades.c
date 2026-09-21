/* tdx_trades.c - 0x0FC5/0x0FC6 L1 trade details, ported from the C++ tool. */
#include "tdx_trades.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* --- helpers ---------------------------------------------------------- */

size_t tdx_trades_side_text(int64_t status_raw, char *out, size_t out_size) {
    const char *text;
    int written;
    if (!out || out_size == 0)
        return 0;
    if (status_raw == 0)
        text = "buy";
    else if (status_raw == 1)
        text = "sell";
    else if (status_raw == 2)
        text = "neutral";
    else
        text = NULL;
    if (text) {
        written = snprintf(out, out_size, "%s", text);
    } else {
        written = snprintf(out, out_size, "status_%lld", (long long)status_raw);
    }
    if (written < 0)
        return 0;
    return (size_t)written < out_size ? (size_t)written : out_size - 1;
}

int tdx_trades_price_divisor(const char *code) {
    static const char *const thousand[] = {"10", "11", "12", "15", "16", "50",
                                           "51", "52", "53", "56", "58"};
    size_t index;
    if (!code)
        return 100;
    for (index = 0; index < sizeof(thousand) / sizeof(thousand[0]); ++index)
        if (strncmp(code, thousand[index], 2) == 0)
            return 1000;
    return 100;
}

static int leap_year(int year) {
    return year % 400 == 0 || (year % 4 == 0 && year % 100 != 0);
}

int tdx_trades_date_valid(const char *date, tdx_error *err) {
    static const int month_days[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    int year;
    int month;
    int day;
    int maximum;
    size_t index;

    if (!date || strlen(date) != TDX_TRADES_DATE_LENGTH) {
        tdx_error_set(err, "trading date must be eight digits, YYYYMMDD");
        return TDX_ERR;
    }
    for (index = 0; index < TDX_TRADES_DATE_LENGTH; ++index)
        if (date[index] < '0' || date[index] > '9') {
            tdx_error_set(err, "trading date must be eight digits, YYYYMMDD");
            return TDX_ERR;
        }
    year = (date[0] - '0') * 1000 + (date[1] - '0') * 100 + (date[2] - '0') * 10 +
           (date[3] - '0');
    month = (date[4] - '0') * 10 + (date[5] - '0');
    day = (date[6] - '0') * 10 + (date[7] - '0');
    if (year < 1990 || year > 2200 || month < 1 || month > 12) {
        tdx_error_set(err, "trading date %s is not a usable date", date);
        return TDX_ERR;
    }
    maximum = month_days[month - 1];
    if (month == 2 && leap_year(year))
        ++maximum;
    if (day < 1 || day > maximum) {
        tdx_error_set(err, "trading date %s is not a usable date", date);
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_trades_time_label(int minutes, char *out, size_t out_size) {
    int written;
    if (!out || out_size == 0)
        return TDX_ERR;
    written = snprintf(out, out_size, "%02d:%02d", minutes / 60, minutes % 60);
    if (written < 0 || (size_t)written >= out_size)
        return TDX_ERR;
    return TDX_OK;
}

/* --- page and series storage ------------------------------------------ */

void tdx_trades_page_init(tdx_trade_page *page) {
    if (!page)
        return;
    memset(page, 0, sizeof(*page));
}

void tdx_trades_page_free(tdx_trade_page *page) {
    if (!page)
        return;
    free(page->ticks);
    memset(page, 0, sizeof(*page));
}

void tdx_trades_series_init(tdx_trade_series *series) {
    if (!series)
        return;
    memset(series, 0, sizeof(*series));
}

void tdx_trades_series_free(tdx_trade_series *series) {
    if (!series)
        return;
    free(series->ticks);
    memset(series, 0, sizeof(*series));
}

static int page_push(tdx_trade_page *page, const tdx_trade_tick *tick, tdx_error *err) {
    if (page->count == page->capacity) {
        size_t wanted = page->capacity ? page->capacity * 2 : 64;
        tdx_trade_tick *grown =
            (tdx_trade_tick *)realloc(page->ticks, wanted * sizeof(*grown));
        if (!grown) {
            tdx_error_set(err, "out of memory growing the trade page to %zu ticks", wanted);
            return TDX_ERR;
        }
        page->ticks = grown;
        page->capacity = wanted;
    }
    page->ticks[page->count++] = *tick;
    return TDX_OK;
}

static int series_append(tdx_trade_series *series, const tdx_trade_tick *ticks, size_t count,
                         tdx_error *err) {
    if (series->count + count > series->capacity) {
        size_t wanted = series->capacity ? series->capacity : 256;
        tdx_trade_tick *grown;
        while (wanted < series->count + count)
            wanted *= 2;
        grown = (tdx_trade_tick *)realloc(series->ticks, wanted * sizeof(*grown));
        if (!grown) {
            tdx_error_set(err, "out of memory growing the trade series to %zu ticks", wanted);
            return TDX_ERR;
        }
        series->ticks = grown;
        series->capacity = wanted;
    }
    memcpy(series->ticks + series->count, ticks, count * sizeof(*ticks));
    series->count += count;
    return TDX_OK;
}

/* --- requests --------------------------------------------------------- */

static int validate_security(int market_id, const char *code, tdx_error *err) {
    size_t index;
    if (market_id < 0 || market_id > 2) {
        tdx_error_set(err, "trade market must be 0, 1 or 2, got %d", market_id);
        return TDX_ERR;
    }
    if (!code || strlen(code) != 6) {
        tdx_error_set(err, "trade security needs a six-digit code");
        return TDX_ERR;
    }
    for (index = 0; index < 6; ++index)
        if (code[index] < '0' || code[index] > '9') {
            tdx_error_set(err, "trade security code must be six digits, got %s", code);
            return TDX_ERR;
        }
    return TDX_OK;
}

int tdx_trades_build_today_request(int market_id, const char *code, uint16_t start,
                                  uint16_t count, tdx_buf *out, tdx_error *err) {
    if (!out) {
        tdx_error_set(err, "0x0FC5 needs an output buffer");
        return TDX_ERR;
    }
    if (count == 0) {
        tdx_error_set(err, "0x0FC5 needs a positive count");
        return TDX_ERR;
    }
    if (validate_security(market_id, code, err) != TDX_OK)
        return TDX_ERR;
    tdx_buf_clear(out);
    if (tdx_buf_push(out, (uint8_t)market_id, err) != TDX_OK ||
        tdx_buf_push(out, 0, err) != TDX_OK ||
        tdx_buf_append(out, code, 6, err) != TDX_OK ||
        tdx_buf_append_u16le(out, start, err) != TDX_OK ||
        tdx_buf_append_u16le(out, count, err) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_trades_build_history_request(int market_id, const char *code, const char *date,
                                    uint16_t start, uint16_t count, tdx_buf *out,
                                    tdx_error *err) {
    uint32_t numeric = 0;
    size_t index;
    if (!out) {
        tdx_error_set(err, "0x0FC6 needs an output buffer");
        return TDX_ERR;
    }
    if (count == 0) {
        tdx_error_set(err, "0x0FC6 needs a positive count");
        return TDX_ERR;
    }
    if (validate_security(market_id, code, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_trades_date_valid(date, err) != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < TDX_TRADES_DATE_LENGTH; ++index)
        numeric = numeric * 10u + (uint32_t)(date[index] - '0');
    tdx_buf_clear(out);
    if (tdx_buf_append_u32le(out, numeric, err) != TDX_OK ||
        tdx_buf_append_u16le(out, (uint16_t)market_id, err) != TDX_OK ||
        tdx_buf_append(out, code, 6, err) != TDX_OK ||
        tdx_buf_append_u16le(out, start, err) != TDX_OK ||
        tdx_buf_append_u16le(out, count, err) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

/* --- records ---------------------------------------------------------- */

/* Shared record walk.  offset points just past the reply header. */
static int parse_records(const uint8_t *payload, size_t size, size_t *offset, uint16_t count,
                         uint16_t start, int price_divisor, tdx_trade_page *out,
                         tdx_error *err) {
    int64_t price_acc = 0;
    size_t index;

    for (index = 0; index < (size_t)count; ++index) {
        tdx_trade_tick tick;
        memset(&tick, 0, sizeof(tick));
        if (*offset + 2 > size) {
            tdx_error_set(err, "trade record %zu has no time field", index + 1);
            return TDX_ERR;
        }
        tick.index = index;
        tick.absolute_index = (size_t)start + index;
        tick.time_minutes = (int)tdx_u16le(payload + *offset);
        *offset += 2;
        if (tick.time_minutes >= 24 * 60) {
            tdx_error_set(err, "trade record %zu has minute-of-day %d", index + 1,
                          tick.time_minutes);
            return TDX_ERR;
        }
        if (tdx_varint_decode(payload, size, offset, &tick.price_delta_raw, err) != TDX_OK ||
            tdx_varint_decode(payload, size, offset, &tick.volume_hand, err) != TDX_OK ||
            tdx_varint_decode(payload, size, offset, &tick.order_count, err) != TDX_OK ||
            tdx_varint_decode(payload, size, offset, &tick.status_raw, err) != TDX_OK ||
            tdx_varint_decode(payload, size, offset, &tick.tail_raw, err) != TDX_OK)
            return TDX_ERR;
        if (tick.volume_hand < 0 || tick.order_count < 0) {
            tdx_error_set(err, "trade record %zu has a negative volume or order count",
                          index + 1);
            return TDX_ERR;
        }
        if ((tick.price_delta_raw > 0 && price_acc > INT64_MAX - tick.price_delta_raw) ||
            (tick.price_delta_raw < 0 && price_acc < INT64_MIN - tick.price_delta_raw)) {
            tdx_error_set(err, "trade record %zu overflows the price accumulator", index + 1);
            return TDX_ERR;
        }
        price_acc += tick.price_delta_raw;
        tick.price_acc_raw = price_acc;
        tick.price = (double)price_acc / (double)price_divisor;
        if (!isfinite(tick.price) || tick.price < 0) {
            tdx_error_set(err, "trade record %zu decodes to price %.6f", index + 1, tick.price);
            return TDX_ERR;
        }
        tick.amount_yuan = tick.price * (double)tick.volume_hand * 100.0;
        if (page_push(out, &tick, err) != TDX_OK)
            return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_trades_parse_today(const uint8_t *payload, size_t size, uint16_t start,
                          int price_divisor, tdx_trade_page *out, tdx_error *err) {
    size_t offset = 2;
    uint16_t count;
    if (!payload || !out) {
        tdx_error_set(err, "0x0FC5 reply needs a body and an output page");
        return TDX_ERR;
    }
    if (size < 2) {
        tdx_error_set(err, "0x0FC5 reply is %zu bytes, expected at least 2", size);
        return TDX_ERR;
    }
    count = tdx_u16le(payload);
    if (parse_records(payload, size, &offset, count, start, price_divisor, out, err) != TDX_OK)
        return TDX_ERR;
    if (offset != size) {
        tdx_error_set(err, "0x0FC5 reply has %zu trailing bytes after %u records",
                      size - offset, (unsigned)count);
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_trades_parse_history(const uint8_t *payload, size_t size, uint16_t start,
                            int price_divisor, tdx_trade_page *out, tdx_error *err) {
    size_t offset = 6;
    uint16_t count;
    float base;
    if (!payload || !out) {
        tdx_error_set(err, "0x0FC6 reply needs a body and an output page");
        return TDX_ERR;
    }
    if (size < 6) {
        tdx_error_set(err, "0x0FC6 reply is %zu bytes, expected at least 6", size);
        return TDX_ERR;
    }
    count = tdx_u16le(payload);
    memcpy(&base, payload + 2, sizeof(base));
    if (!isfinite((double)base)) {
        tdx_error_set(err, "0x0FC6 reply carries a non-finite price base");
        return TDX_ERR;
    }
    out->has_price_base = 1;
    out->price_base = (double)base;
    if (parse_records(payload, size, &offset, count, start, price_divisor, out, err) != TDX_OK)
        return TDX_ERR;
    if (offset != size) {
        tdx_error_set(err, "0x0FC6 reply has %zu trailing bytes after %u records",
                      size - offset, (unsigned)count);
        return TDX_ERR;
    }
    return TDX_OK;
}

/* --- session-bound ---------------------------------------------------- */

int tdx_trades_read_server_date(tdx_connection *connection,
                               char out[TDX_TRADES_DATE_LENGTH + 1], tdx_error *err) {
    tdx_buf response;
    uint32_t raw;
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "0x0004 needs a session and an output");
        return TDX_ERR;
    }
    out[0] = '\0';
    tdx_buf_init(&response);
    if (tdx_connection_call(connection, TDX_CMD_HEARTBEAT, NULL, 0, &response, err) != TDX_OK)
        goto done;
    if (response.len < 10) {
        tdx_error_set(err, "0x0004 reply is %zu bytes, expected at least 10", response.len);
        goto done;
    }
    raw = tdx_u32le(response.data + 6);
    snprintf(out, TDX_TRADES_DATE_LENGTH + 1, "%08u", (unsigned)raw);
    if (tdx_trades_date_valid(out, err) != TDX_OK)
        goto done;
    result = TDX_OK;

done:
    tdx_buf_free(&response);
    return result;
}

int tdx_trades_fetch(tdx_connection *connection, int market_id, const char *code,
                     const char *date, uint16_t page_size, size_t max_pages,
                     tdx_trade_series *out, tdx_error *err) {
    tdx_trade_page *pages = NULL;
    size_t page_count = 0;
    size_t page_capacity = 0;
    uint32_t cursor = 0;
    const int history = date && *date;
    int price_divisor;
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "trade fetch needs a session and an output series");
        return TDX_ERR;
    }
    if (page_size == 0) {
        tdx_error_set(err, "trade page size must be positive");
        return TDX_ERR;
    }
    if (max_pages < 1 || max_pages > TDX_TRADES_MAX_PAGES) {
        tdx_error_set(err, "trade max pages must be in 1..%d", TDX_TRADES_MAX_PAGES);
        return TDX_ERR;
    }
    if (tdx_trades_date_valid(date, err) != TDX_OK && history)
        return TDX_ERR;
    price_divisor = tdx_trades_price_divisor(code);

    for (;;) {
        tdx_trade_page page;
        tdx_buf request;
        tdx_buf response;
        int parsed;

        if (cursor > 0xFFFFu) {
            tdx_error_set(err, "trade cursor %u exceeds the 16-bit request field",
                          (unsigned)cursor);
            goto done;
        }
        tdx_trades_page_init(&page);
        tdx_buf_init(&request);
        tdx_buf_init(&response);
        if (history) {
            if (tdx_trades_build_history_request(market_id, code, date, (uint16_t)cursor,
                                                 page_size, &request, err) != TDX_OK)
                parsed = TDX_ERR;
            else if (tdx_connection_call(connection, TDX_CMD_TRADES_HISTORY, request.data,
                                         request.len, &response, err) != TDX_OK)
                parsed = TDX_ERR;
            else
                parsed = tdx_trades_parse_history(response.data, response.len,
                                                  (uint16_t)cursor, price_divisor, &page, err);
        } else {
            if (tdx_trades_build_today_request(market_id, code, (uint16_t)cursor, page_size,
                                               &request, err) != TDX_OK)
                parsed = TDX_ERR;
            else if (tdx_connection_call(connection, TDX_CMD_TRADES_TODAY, request.data,
                                         request.len, &response, err) != TDX_OK)
                parsed = TDX_ERR;
            else
                parsed = tdx_trades_parse_today(response.data, response.len,
                                                (uint16_t)cursor, price_divisor, &page, err);
        }
        tdx_buf_free(&request);
        tdx_buf_free(&response);
        if (parsed != TDX_OK) {
            tdx_trades_page_free(&page);
            goto done;
        }
        if (page.count == 0) {
            tdx_trades_page_free(&page);
            break;
        }
        if (page_count >= max_pages) {
            tdx_error_set(err, "trades need more than the %zu page safety limit", max_pages);
            tdx_trades_page_free(&page);
            goto done;
        }
        if (page_count == page_capacity) {
            size_t wanted = page_capacity ? page_capacity * 2 : 8;
            tdx_trade_page *grown =
                (tdx_trade_page *)realloc(pages, wanted * sizeof(*grown));
            if (!grown) {
                tdx_error_set(err, "out of memory growing the page list to %zu", wanted);
                tdx_trades_page_free(&page);
                goto done;
            }
            pages = grown;
            page_capacity = wanted;
        }
        pages[page_count++] = page;
        cursor += (uint32_t)page.count;
        if (cursor > 0xFFFFu) {
            tdx_error_set(err, "trade paging exceeded the 16-bit cursor at %u ticks",
                          (unsigned)cursor);
            goto done;
        }
    }

    out->pages = page_count;
    out->page_size = page_size;
    out->price_divisor = price_divisor;
    out->history = history;
    /* The server hands pages back newest first, so walking the page list
     * backwards yields chronological order. */
    {
        size_t index;
        for (index = page_count; index > 0; --index) {
            const tdx_trade_page *page = &pages[index - 1];
            if (!out->has_price_base && page->has_price_base) {
                out->has_price_base = 1;
                out->price_base = page->price_base;
            }
            if (series_append(out, page->ticks, page->count, err) != TDX_OK)
                goto done;
        }
    }
    result = TDX_OK;

done:
    if (pages) {
        size_t index;
        for (index = 0; index < page_count; ++index)
            tdx_trades_page_free(&pages[index]);
        free(pages);
    }
    return result;
}

int tdx_trades_fetch_from_pool(const tdx_endpoint_pool *pool, int timeout_ms, int market_id,
                               const char *code, const char *date, uint16_t page_size,
                               size_t max_pages, tdx_trade_series *out, char *endpoint_used,
                               size_t endpoint_used_size,
                               char server_date[TDX_TRADES_DATE_LENGTH + 1], tdx_error *err) {
    tdx_connection connection;
    size_t attempt;
    int result = TDX_ERR;

    if (!pool || !pool->count || !out) {
        tdx_error_set(err, "trade fetch needs an endpoint pool and a series");
        return TDX_ERR;
    }
    if (server_date)
        server_date[0] = '\0';
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    for (attempt = 0; attempt < pool->count; ++attempt) {
        char address[80];
        char date_buffer[TDX_TRADES_DATE_LENGTH + 1];
        tdx_error step;
        int ok;

        tdx_endpoint_address(&pool->items[attempt], address, sizeof(address));
        step.message[0] = '\0';
        if (tdx_connection_open(&connection, &pool->items[attempt], timeout_ms, &step) != TDX_OK)
            continue;
        date_buffer[0] = '\0';
        if (!date || !*date)
            (void)tdx_trades_read_server_date(&connection, date_buffer, &step);
        ok = tdx_trades_fetch(&connection, market_id, code, date, page_size, max_pages, out,
                              &step) == TDX_OK;
        tdx_connection_close(&connection);
        if (!ok) {
            /* A failed walk may have left the series half filled; restart it. */
            tdx_trades_series_free(out);
            tdx_trades_series_init(out);
            *err = step;
            continue;
        }
        if (server_date)
            snprintf(server_date, TDX_TRADES_DATE_LENGTH + 1, "%s", date_buffer);
        if (endpoint_used && endpoint_used_size)
            snprintf(endpoint_used, endpoint_used_size, "%s", address);
        result = TDX_OK;
        break;
    }
    return result;
}

/* --- aggregation ------------------------------------------------------ */

void tdx_trades_summarize(const tdx_trade_series *series, tdx_trade_summary *out) {
    size_t index;
    int64_t previous_minute = -1;

    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!series)
        return;
    for (index = 0; index < series->count; ++index) {
        const tdx_trade_tick *tick = &series->ticks[index];
        if (!out->has_price_range) {
            out->first_price = tick->price;
            out->high_price = tick->price;
            out->low_price = tick->price;
            out->has_price_range = 1;
        } else {
            if (tick->price > out->high_price)
                out->high_price = tick->price;
            if (tick->price < out->low_price)
                out->low_price = tick->price;
        }
        out->last_price = tick->price;
        out->volume_hand += tick->volume_hand;
        out->order_count += tick->order_count;
        out->amount_yuan += tick->amount_yuan;
        if (tick->status_raw == 0) {
            out->buy_volume_hand += tick->volume_hand;
            out->buy_amount_yuan += tick->amount_yuan;
        } else if (tick->status_raw == 1) {
            out->sell_volume_hand += tick->volume_hand;
            out->sell_amount_yuan += tick->amount_yuan;
        } else if (tick->status_raw == 2) {
            out->neutral_volume_hand += tick->volume_hand;
            out->neutral_amount_yuan += tick->amount_yuan;
        }
        if (previous_minute != tick->time_minutes) {
            previous_minute = tick->time_minutes;
            out->minute_count++;
        }
        {
            size_t slot;
            for (slot = 0; slot < out->status_count; ++slot)
                if (out->status_counts[slot].status == tick->status_raw)
                    break;
            if (slot < out->status_count) {
                out->status_counts[slot].count++;
            } else if (out->status_count < sizeof(out->status_counts) /
                                             sizeof(out->status_counts[0])) {
                out->status_counts[out->status_count].status = tick->status_raw;
                out->status_counts[out->status_count].count = 1;
                out->status_count++;
            }
        }
    }
    out->tick_count = series->count;
    if (series->count) {
        out->has_times = 1;
        out->first_time_minutes = series->ticks[0].time_minutes;
        out->last_time_minutes = series->ticks[series->count - 1].time_minutes;
        if (out->volume_hand)
            out->vwap = out->amount_yuan / (double)out->volume_hand / 100.0;
    }
    /* Keep the status buckets in ascending order so the JSON is deterministic. */
    for (index = 0; index + 1 < out->status_count; ++index) {
        size_t other;
        for (other = index + 1; other < out->status_count; ++other) {
            if (out->status_counts[other].status < out->status_counts[index].status) {
                int64_t status = out->status_counts[index].status;
                size_t count = out->status_counts[index].count;
                out->status_counts[index] = out->status_counts[other];
                out->status_counts[other].status = status;
                out->status_counts[other].count = count;
            }
        }
    }
}
