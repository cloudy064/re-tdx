/* tdx_timeline_json.c - JSONL rendering for the time-share series. */
#include "tdx_timeline_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

#define APPEND_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

static const char *upper_prefix(int market_id) {
    switch (market_id) {
    case 1:
        return "SH";
    case 2:
        return "BJ";
    default:
        return "SZ";
    }
}

static int append_security_id(tdx_buf *out, int market_id, const char *code, tdx_error *err) {
    char identity[16];
    snprintf(identity, sizeof(identity), "%s%s", upper_prefix(market_id), code ? code : "");
    return tdx_format_json_string(out, identity, err);
}

int tdx_timeline_format_point(tdx_buf *out, const tdx_timeline *timeline,
                              const tdx_timeline_point *point, int market_id, const char *code,
                              tdx_error *err) {
    if (!out || !timeline || !point) {
        tdx_error_set(err, "timeline point rendering needs a buffer, a series and a point");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"minute_point\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (append_security_id(out, market_id, code, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"index\":%zu,\"time\":\"%02d:%02d\",\"minute_of_day\":%d,"
                              "\"price\":%.4f,\"average_price\":%.5f,\"volume_hand\":%lld}",
                              point->index, point->minute_of_day / 60, point->minute_of_day % 60,
                              point->minute_of_day, point->price, point->average_price,
                              (long long)point->volume_hand) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_timeline_format_summary(tdx_buf *out, const tdx_timeline *timeline, int market_id,
                                const char *code, const char *endpoint, tdx_error *err) {
    double high = 0.0;
    double low = 0.0;
    double volume = 0.0;
    size_t index;
    int have_range = 0;

    if (!out || !timeline) {
        tdx_error_set(err, "timeline summary rendering needs a buffer and a series");
        return TDX_ERR;
    }
    for (index = 0; index < timeline->count; ++index) {
        const tdx_timeline_point *point = &timeline->points[index];
        if (!have_range) {
            high = point->price;
            low = point->price;
            have_range = 1;
        } else {
            if (point->price > high)
                high = point->price;
            if (point->price < low)
                low = point->price;
        }
        volume += (double)point->volume_hand;
    }

    if (APPEND_LITERAL(out, err, "{\"type\":\"timeline_summary\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (append_security_id(out, market_id, code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"command\":\"0x0537\",\"endpoint\":") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"point_count\":%zu,\"base_price\":%.4f,"
                                        "\"base_average_price\":%.5f,\"volume_hand\":%.0f",
                              timeline->count, timeline->base_price, timeline->base_average,
                              volume) != TDX_OK)
        return TDX_ERR;
    if (timeline->count) {
        const tdx_timeline_point *first = &timeline->points[0];
        const tdx_timeline_point *last = &timeline->points[timeline->count - 1];
        if (tdx_buf_append_printf(out, err,
                                  ",\"first_time\":\"%02d:%02d\",\"last_time\":\"%02d:%02d\","
                                  "\"open_price\":%.4f,\"last_price\":%.4f,"
                                  "\"high\":%.4f,\"low\":%.4f,\"vwap\":%.5f}",
                                  first->minute_of_day / 60, first->minute_of_day % 60,
                                  last->minute_of_day / 60, last->minute_of_day % 60,
                                  first->price, last->price, high, low,
                                  last->average_price) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err,
                              ",\"first_time\":null,\"last_time\":null,\"open_price\":null,"
                              "\"last_price\":null,\"high\":null,\"low\":null,"
                              "\"vwap\":null}") != TDX_OK) {
        return TDX_ERR;
    }
    return TDX_OK;
}
