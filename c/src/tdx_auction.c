/* tdx_auction.c - 0x056A call-auction point series, ported from the C++ tool. */
#include "tdx_auction.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tdx_trades.h" /* tdx_trades_read_server_date, the shared 0x0004 reader */

const char *tdx_auction_direction_text(int direction_raw) {
    if (direction_raw > 0)
        return "buy";
    if (direction_raw < 0)
        return "sell";
    return "balanced";
}

int tdx_auction_time_label(int time_seconds, char *out, size_t out_size) {
    int written;
    if (!out || out_size == 0)
        return TDX_ERR;
    if (time_seconds < 0 || time_seconds >= 24 * 3600) {
        return TDX_ERR;
    }
    written = snprintf(out, out_size, "%02d:%02d:%02d", time_seconds / 3600,
                       (time_seconds / 60) % 60, time_seconds % 60);
    if (written < 0 || (size_t)written >= out_size)
        return TDX_ERR;
    return TDX_OK;
}

void tdx_auction_series_init(tdx_auction_series *series) {
    if (!series)
        return;
    memset(series, 0, sizeof(*series));
}

void tdx_auction_series_free(tdx_auction_series *series) {
    if (!series)
        return;
    free(series->points);
    memset(series, 0, sizeof(*series));
}

static int point_push(tdx_auction_series *series, const tdx_auction_point *point,
                      tdx_error *err) {
    if (series->count == series->capacity) {
        size_t wanted = series->capacity ? series->capacity * 2 : 128;
        tdx_auction_point *grown =
            (tdx_auction_point *)realloc(series->points, wanted * sizeof(*grown));
        if (!grown) {
            tdx_error_set(err, "out of memory growing the auction series to %zu points", wanted);
            return TDX_ERR;
        }
        series->points = grown;
        series->capacity = wanted;
    }
    series->points[series->count++] = *point;
    return TDX_OK;
}

int tdx_auction_build_request(int market_id, const char *code, uint32_t selector,
                              uint32_t start_raw, uint32_t limit, tdx_buf *out,
                              tdx_error *err) {
    size_t index;

    if (!out) {
        tdx_error_set(err, "0x056A needs an output buffer");
        return TDX_ERR;
    }
    if (market_id < 0 || market_id > 2) {
        tdx_error_set(err, "auction market must be 0, 1 or 2, got %d", market_id);
        return TDX_ERR;
    }
    if (!code || strlen(code) != 6) {
        tdx_error_set(err, "auction needs a six-digit code");
        return TDX_ERR;
    }
    for (index = 0; index < 6; ++index)
        if (code[index] < '0' || code[index] > '9') {
            tdx_error_set(err, "auction code must be six digits, got %s", code);
            return TDX_ERR;
        }
    if (limit < 1 || limit > TDX_AUCTION_LIMIT_MAX) {
        tdx_error_set(err, "auction limit must be in 1..%u", (unsigned)TDX_AUCTION_LIMIT_MAX);
        return TDX_ERR;
    }
    tdx_buf_clear(out);
    /* market, reserved, code, constant, selector, constant, start, limit */
    if (tdx_buf_push(out, (uint8_t)market_id, err) != TDX_OK ||
        tdx_buf_push(out, 0, err) != TDX_OK ||
        tdx_buf_append(out, code, 6, err) != TDX_OK ||
        tdx_buf_append_u32le(out, 0, err) != TDX_OK ||
        tdx_buf_append_u32le(out, selector, err) != TDX_OK ||
        tdx_buf_append_u32le(out, 0, err) != TDX_OK ||
        tdx_buf_append_u32le(out, start_raw, err) != TDX_OK ||
        tdx_buf_append_u32le(out, limit, err) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_auction_parse(const uint8_t *payload, size_t size, uint32_t selector, uint32_t start_raw,
                      uint32_t limit, tdx_auction_series *out, tdx_error *err) {
    uint16_t count;
    size_t index;

    if (!payload || !out) {
        tdx_error_set(err, "auction reply needs a body and an output series");
        return TDX_ERR;
    }
    if (size < 2) {
        tdx_error_set(err, "auction reply is %zu bytes, expected at least 2", size);
        return TDX_ERR;
    }
    count = tdx_u16le(payload);
    if (size != 2 + (size_t)count * TDX_AUCTION_RECORD_SIZE) {
        tdx_error_set(err,
                      "auction reply length mismatch: %zu bytes for %u records of %d bytes",
                      size, (unsigned)count, TDX_AUCTION_RECORD_SIZE);
        return TDX_ERR;
    }
    out->selector = selector;
    out->start_raw = start_raw;
    out->limit = limit;

    for (index = 0; index < (size_t)count; ++index) {
        const uint8_t *record = payload + 2 + index * TDX_AUCTION_RECORD_SIZE;
        tdx_auction_point point;
        int64_t signed_unmatched;
        memset(&point, 0, sizeof(point));
        point.index = index;
        point.minute_of_day = (int)tdx_u16le(record);
        point.reserved = record[14];
        point.second = record[15];
        if (point.minute_of_day >= 24 * 60 || point.second >= 60) {
            tdx_error_set(err, "auction record %zu has time %02d:%02d:%02d", index,
                          point.minute_of_day / 60, point.minute_of_day % 60, point.second);
            return TDX_ERR;
        }
        {
            /* The wire carries a float32; widen it explicitly rather than
             * memcpy-ing four bytes into a double's low half. */
            float raw_price;
            memcpy(&raw_price, record + 2, sizeof(raw_price));
            point.price = (double)raw_price;
        }
        if (!isfinite(point.price) || point.price < 0) {
            tdx_error_set(err, "auction record %zu has price %g", index, point.price);
            return TDX_ERR;
        }
        point.matched_volume_hand = (int64_t)tdx_u32le(record + 6);
        signed_unmatched = (int64_t)tdx_i32le(record + 10);
        point.unmatched_signed_hand = signed_unmatched;
        point.unmatched_volume_hand =
            signed_unmatched < 0 ? -signed_unmatched : signed_unmatched;
        point.unmatched_direction_raw =
            signed_unmatched > 0 ? 1 : (signed_unmatched < 0 ? -1 : 0);
        point.time_seconds = point.minute_of_day * 60 + point.second;
        point.matched_amount_yuan =
            point.price * (double)point.matched_volume_hand * 100.0;
        if (point_push(out, &point, err) != TDX_OK)
            return TDX_ERR;
    }
    return TDX_OK;
}

/* Aggregates one segment.  The per-segment invariants it reports (unmatched
 * direction flips, matched volume that went backwards) are the useful part:
 * they are how a caller spots a malformed or surprising auction without
 * re-deriving anything. */
static void summarize_segment(const tdx_auction_series *series, int opening,
                              tdx_auction_segment *out) {
    const tdx_auction_point *previous = NULL;
    int previous_direction = 0;
    size_t index;

    memset(out, 0, sizeof(*out));
    for (index = 0; index < series->count; ++index) {
        const tdx_auction_point *point = &series->points[index];
        if ((point->minute_of_day < 12 * 60) != opening)
            continue;
        if (!previous) {
            out->has_points = 1;
            out->start_time_seconds = point->time_seconds;
            out->first_price = point->price;
            out->min_price = point->price;
            out->max_price = point->price;
            out->max_unmatched_volume_hand = point->unmatched_volume_hand;
            out->max_unmatched_time_seconds = point->time_seconds;
        } else {
            if (point->price < out->min_price)
                out->min_price = point->price;
            if (point->price > out->max_price)
                out->max_price = point->price;
            if (point->unmatched_volume_hand > out->max_unmatched_volume_hand) {
                out->max_unmatched_volume_hand = point->unmatched_volume_hand;
                out->max_unmatched_time_seconds = point->time_seconds;
            }
            /* Compared against the previous point of THIS segment, which is what
             * the peer does by building one vector per segment. */
            if (point->matched_volume_hand < previous->matched_volume_hand)
                out->matched_volume_monotonic_violations++;
        }
        if (point->unmatched_direction_raw != 0) {
            if (previous_direction != 0 && previous_direction != point->unmatched_direction_raw)
                out->unmatched_direction_flips++;
            previous_direction = point->unmatched_direction_raw;
        }
        out->point_count++;
        out->end_time_seconds = point->time_seconds;
        out->last_price = point->price;
        out->last_matched_volume_hand = point->matched_volume_hand;
        out->last_matched_amount_yuan = point->matched_amount_yuan;
        out->last_unmatched_signed_hand = point->unmatched_signed_hand;
        out->last_unmatched_volume_hand = point->unmatched_volume_hand;
        out->last_unmatched_direction_raw = point->unmatched_direction_raw;
        previous = point;
    }
}

void tdx_auction_summarize(const tdx_auction_series *series, tdx_auction_summary *out) {
    size_t index;

    if (!out)
        return;
    memset(out, 0, sizeof(*out));
    if (!series)
        return;
    out->point_count = series->count;
    summarize_segment(series, 1, &out->opening);
    summarize_segment(series, 0, &out->closing);
    for (index = 0; index < series->count; ++index) {
        if (series->points[index].reserved != 0)
            out->reserved_nonzero_points++;
        if (index > 0) {
            int gap = series->points[index].time_seconds - series->points[index - 1].time_seconds;
            if (!out->has_largest_gap || gap > out->largest_gap_seconds) {
                out->has_largest_gap = 1;
                out->largest_gap_seconds = gap;
                out->gap_after_time_seconds = series->points[index - 1].time_seconds;
                out->gap_before_time_seconds = series->points[index].time_seconds;
            }
        }
    }
}

int tdx_auction_fetch(tdx_connection *connection, int market_id, const char *code,
                      uint32_t selector, uint32_t start_raw, uint32_t limit,
                      tdx_auction_series *out, tdx_error *err) {
    tdx_buf request;
    tdx_buf response;
    int result = TDX_ERR;

    if (!connection || !out) {
        tdx_error_set(err, "auction fetch needs a session and an output series");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    if (tdx_auction_build_request(market_id, code, selector, start_raw, limit, &request, err) !=
        TDX_OK)
        goto done;
    if (tdx_connection_call(connection, TDX_CMD_AUCTION, request.data, request.len, &response,
                            err) != TDX_OK)
        goto done;
    result = tdx_auction_parse(response.data, response.len, selector, start_raw, limit, out, err);

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return result;
}

int tdx_auction_fetch_from_pool(const tdx_endpoint_pool *pool, int timeout_ms, int market_id,
                                const char *code, uint32_t selector, uint32_t start_raw,
                                uint32_t limit, tdx_auction_series *out, char *endpoint_used,
                                size_t endpoint_used_size, char server_date[9], tdx_error *err) {
    tdx_connection connection;
    size_t attempt;
    int result = TDX_ERR;

    if (!pool || !pool->count || !out) {
        tdx_error_set(err, "auction fetch needs an endpoint pool and a series");
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
        (void)tdx_trades_read_server_date(&connection, date_buffer, &step);
        ok = tdx_auction_fetch(&connection, market_id, code, selector, start_raw, limit, out,
                               &step) == TDX_OK;
        tdx_connection_close(&connection);
        if (!ok) {
            tdx_auction_series_free(out);
            tdx_auction_series_init(out);
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
