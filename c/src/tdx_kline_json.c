/* tdx_kline_json.c - JSONL rendering for 0x052D K-lines. */
#include "tdx_kline_json.h"

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

int tdx_kline_format_bar(tdx_buf *out, const tdx_kline_bar *bar, int market_id,
                         const char *code, const char *period_name, tdx_error *err) {
    if (!out || !bar) {
        tdx_error_set(err, "K-line bar rendering needs a buffer and a bar");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"kline\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (append_security_id(out, market_id, code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"period\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, period_name ? period_name : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"date\":%d,\"time\":\"%02d:%02d\",\"open\":%.3f,"
                              "\"high\":%.3f,\"low\":%.3f,\"close\":%.3f,"
                              "\"volume\":%lld,\"amount\":%.4f",
                              bar->date, bar->hour, bar->minute, bar->open, bar->high, bar->low,
                              bar->close, (long long)bar->volume, bar->amount) != TDX_OK)
        return TDX_ERR;
    if (bar->has_extras) {
        if (tdx_buf_append_printf(out, err, ",\"extra_1\":%u,\"extra_2\":%u",
                                  (unsigned)bar->extra_1, (unsigned)bar->extra_2) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, ",\"extra_1\":null,\"extra_2\":null") != TDX_OK) {
        return TDX_ERR;
    }
    return APPEND_LITERAL(out, err, "}");
}

int tdx_kline_format_summary(tdx_buf *out, const tdx_kline_series *series, int market_id,
                             const char *code, const char *period_name, uint16_t start,
                             const char *endpoint, tdx_error *err) {
    double high = 0.0;
    double low = 0.0;
    double volume = 0.0;
    double amount = 0.0;
    size_t index;
    int have_range = 0;

    if (!out || !series) {
        tdx_error_set(err, "K-line summary rendering needs a buffer and a series");
        return TDX_ERR;
    }
    for (index = 0; index < series->count; ++index) {
        const tdx_kline_bar *bar = &series->bars[index];
        if (!have_range) {
            high = bar->high;
            low = bar->low;
            have_range = 1;
        } else {
            if (bar->high > high)
                high = bar->high;
            if (bar->low < low)
                low = bar->low;
        }
        volume += (double)bar->volume;
        amount += bar->amount;
    }

    if (APPEND_LITERAL(out, err, "{\"type\":\"kline_summary\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (append_security_id(out, market_id, code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"period\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, period_name ? period_name : "", err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"period_id\":%u,\"index_mode\":%s,\"endpoint\":",
                              (unsigned)series->period_id,
                              series->index_mode ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"start\":%u,\"pages\":%zu,\"page_size\":%u,"
                              "\"bar_count\":%zu,\"reached_end\":%s",
                              (unsigned)start, series->pages, (unsigned)series->page_size,
                              series->count, series->reached_end ? "true" : "false") != TDX_OK)
        return TDX_ERR;
    if (series->count) {
        const tdx_kline_bar *first = &series->bars[0];
        const tdx_kline_bar *last = &series->bars[series->count - 1];
        if (tdx_buf_append_printf(out, err,
                                  ",\"first_date\":%d,\"first_time\":\"%02d:%02d\","
                                  "\"last_date\":%d,\"last_time\":\"%02d:%02d\"",
                                  first->date, first->hour, first->minute, last->date,
                                  last->hour, last->minute) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err,
                              ",\"first_date\":null,\"first_time\":null,"
                              "\"last_date\":null,\"last_time\":null") != TDX_OK) {
        return TDX_ERR;
    }
    if (have_range) {
        if (tdx_buf_append_printf(out, err,
                                  ",\"high\":%.3f,\"low\":%.3f,\"volume\":%.0f,"
                                  "\"amount\":%.4f}",
                                  high, low, volume, amount) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err,
                              ",\"high\":null,\"low\":null,\"volume\":0,\"amount\":0}") !=
               TDX_OK) {
        return TDX_ERR;
    }
    return TDX_OK;
}
