/* tdx_timeline.c - 0x0537/0x0FB4 intraday time-share series.
 *
 * Recovered from the live server rather than ported; the evidence and the exact
 * arithmetic are in output/timeline_probe_evidence.txt and the header. */
#include "tdx_timeline.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Price and average arrive as offsets from point 0, in 1/100 and 1/10000 yuan. */
#define TDX_TIMELINE_PRICE_SCALE 100.0
#define TDX_TIMELINE_AVERAGE_SCALE 10000.0

void tdx_timeline_init(tdx_timeline *timeline) {
    if (!timeline)
        return;
    memset(timeline, 0, sizeof(*timeline));
}

void tdx_timeline_free(tdx_timeline *timeline) {
    if (!timeline)
        return;
    free(timeline->points);
    memset(timeline, 0, sizeof(*timeline));
}

/* A session is 09:31..11:30 then 13:01..15:00, so the morning holds the first
 * 120 points and the lunch break is simply skipped.  The 0x052D 1m bars use the
 * same labels, which is how the mapping was confirmed. */
int tdx_timeline_minute_of_day(size_t index) {
    if (index < 120)
        return 9 * 60 + 31 + (int)index;
    return 13 * 60 + 1 + (int)(index - 120);
}

static int point_push(tdx_timeline *timeline, const tdx_timeline_point *point,
                      tdx_error *err) {
    if (timeline->count == timeline->capacity) {
        size_t wanted = timeline->capacity ? timeline->capacity * 2 : 256;
        tdx_timeline_point *grown =
            (tdx_timeline_point *)realloc(timeline->points, wanted * sizeof(*grown));
        if (!grown) {
            tdx_error_set(err, "out of memory growing the timeline to %zu points", wanted);
            return TDX_ERR;
        }
        timeline->points = grown;
        timeline->capacity = wanted;
    }
    timeline->points[timeline->count++] = *point;
    return TDX_OK;
}

int tdx_timeline_build_request(int market_id, const char *code, tdx_buf *out, tdx_error *err) {
    uint8_t body[TDX_TIMELINE_REQUEST_SIZE];
    size_t length;
    size_t index;

    if (!out) {
        tdx_error_set(err, "0x0537 needs an output buffer");
        return TDX_ERR;
    }
    if (market_id < 0 || market_id > 2) {
        tdx_error_set(err, "timeline market must be 0, 1 or 2, got %d", market_id);
        return TDX_ERR;
    }
    if (!code || (length = strlen(code)) != 6) {
        tdx_error_set(err, "timeline needs a six-character code");
        return TDX_ERR;
    }
    for (index = 0; index < length; ++index)
        if (code[index] < '0' || code[index] > '9') {
            tdx_error_set(err, "timeline code must be six digits, got %s", code);
            return TDX_ERR;
        }
    memset(body, 0, sizeof(body));
    body[0] = (uint8_t)market_id;
    /* body[1] stays zero, matching the trade-detail request's reserved byte. */
    memcpy(body + 2, code, 6);
    /* body[8..11] is not interpreted by the server; see the header. */
    tdx_buf_clear(out);
    return tdx_buf_append(out, body, sizeof(body), err);
}

int tdx_timeline_parse(const uint8_t *payload, size_t size, tdx_timeline *out, tdx_error *err) {
    uint16_t count;
    size_t offset = 4;
    size_t index;
    int64_t base_price_raw;
    int64_t base_average_raw;

    if (!payload || !out) {
        tdx_error_set(err, "timeline reply needs a body and an output");
        return TDX_ERR;
    }
    if (size < 4) {
        tdx_error_set(err, "timeline reply is %zu bytes, expected at least 4", size);
        return TDX_ERR;
    }
    count = tdx_u16le(payload);
    out->reserved = (int64_t)tdx_u16le(payload + 2);
    if (count > TDX_TIMELINE_POINTS_MAX) {
        tdx_error_set(err, "timeline reply declares %u points, above the %d cap", (unsigned)count,
                      TDX_TIMELINE_POINTS_MAX);
        return TDX_ERR;
    }
    if (count == 0)
        return TDX_OK;

    /* Point 0 carries the session base in both fields; later points carry offsets
     * from it, so the base has to be read before the loop can resolve anything. */
    base_price_raw = 0;
    base_average_raw = 0;
    for (index = 0; index < (size_t)count; ++index) {
        tdx_timeline_point point;
        int64_t price_raw;
        int64_t average_raw;
        int64_t volume_raw;

        memset(&point, 0, sizeof(point));
        if (tdx_varint_decode(payload, size, &offset, &price_raw, err) != TDX_OK ||
            tdx_varint_decode(payload, size, &offset, &average_raw, err) != TDX_OK ||
            tdx_varint_decode(payload, size, &offset, &volume_raw, err) != TDX_OK)
            return TDX_ERR;
        if (index == 0) {
            base_price_raw = price_raw;
            base_average_raw = average_raw;
            out->base_price = (double)base_price_raw / TDX_TIMELINE_PRICE_SCALE;
            out->base_average = (double)base_average_raw / TDX_TIMELINE_AVERAGE_SCALE;
        }
        if (volume_raw < 0) {
            tdx_error_set(err, "timeline point %zu has a negative volume", index);
            return TDX_ERR;
        }
        point.index = index;
        point.minute_of_day = tdx_timeline_minute_of_day(index);
        point.price_offset_raw = price_raw;
        point.average_offset_raw = average_raw;
        point.price = index == 0 ? (double)price_raw / TDX_TIMELINE_PRICE_SCALE
                                 : (double)(base_price_raw + price_raw) / TDX_TIMELINE_PRICE_SCALE;
        point.average_price =
            index == 0
                ? (double)average_raw / TDX_TIMELINE_AVERAGE_SCALE
                : (double)(base_average_raw + average_raw) / TDX_TIMELINE_AVERAGE_SCALE;
        point.volume_hand = volume_raw;
        if (!isfinite(point.price) || point.price < 0 || !isfinite(point.average_price) ||
            point.average_price < 0) {
            tdx_error_set(err, "timeline point %zu decodes to price %.6f / average %.6f", index,
                          point.price, point.average_price);
            return TDX_ERR;
        }
        if (point_push(out, &point, err) != TDX_OK)
            return TDX_ERR;
    }
    if (offset != size) {
        tdx_error_set(err, "timeline reply has %zu trailing bytes after %u points", size - offset,
                      (unsigned)count);
        return TDX_ERR;
    }
    return TDX_OK;
}

int tdx_timeline_fetch(tdx_connection *connection, int market_id, const char *code, int history,
                       const char *date, tdx_timeline *out, tdx_error *err) {
    tdx_buf request;
    tdx_buf response;
    int result = TDX_ERR;

    (void)date; /* only the historical command would use it, and that shape is open */
    if (!connection || !out) {
        tdx_error_set(err, "timeline fetch needs a session and an output");
        return TDX_ERR;
    }
    if (history) {
        /* The historical command's request shape is not recovered yet: every
         * candidate body tried so far is answered with silence.  Fail loudly
         * instead of sending a guess and reporting an empty series as success. */
        tdx_error_set(err,
                      "0x0FB4 (historical time-share) request shape is not recovered yet; "
                      "use the today command or the 1m K-line for a past session");
        return TDX_ERR;
    }
    tdx_buf_init(&request);
    tdx_buf_init(&response);
    if (tdx_timeline_build_request(market_id, code, &request, err) != TDX_OK)
        goto done;
    if (tdx_connection_call(connection, TDX_CMD_TIMELINE_TODAY, request.data, request.len,
                            &response, err) != TDX_OK)
        goto done;
    result = tdx_timeline_parse(response.data, response.len, out, err);

done:
    tdx_buf_free(&request);
    tdx_buf_free(&response);
    return result;
}

int tdx_timeline_fetch_from_pool(const tdx_endpoint_pool *pool, int timeout_ms, int market_id,
                                 const char *code, int history, const char *date,
                                 tdx_timeline *out, char *endpoint_used,
                                 size_t endpoint_used_size, tdx_error *err) {
    tdx_connection connection;
    size_t attempt;
    int result = TDX_ERR;

    (void)date;
    if (!pool || !pool->count || !out) {
        tdx_error_set(err, "timeline fetch needs an endpoint pool and an output");
        return TDX_ERR;
    }
    memset(&connection, 0, sizeof(connection));
    connection.socket_handle = (intptr_t)-1;
    for (attempt = 0; attempt < pool->count; ++attempt) {
        char address[80];
        tdx_error step;
        int ok;

        tdx_endpoint_address(&pool->items[attempt], address, sizeof(address));
        step.message[0] = '\0';
        if (tdx_connection_open(&connection, &pool->items[attempt], timeout_ms, &step) != TDX_OK)
            continue;
        ok = tdx_timeline_fetch(&connection, market_id, code, history, date, out, &step) == TDX_OK;
        tdx_connection_close(&connection);
        if (!ok) {
            tdx_timeline_free(out);
            tdx_timeline_init(out);
            *err = step;
            continue;
        }
        if (endpoint_used && endpoint_used_size)
            snprintf(endpoint_used, endpoint_used_size, "%s", address);
        result = TDX_OK;
        break;
    }
    return result;
}
