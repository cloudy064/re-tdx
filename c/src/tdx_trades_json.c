/* tdx_trades_json.c - JSONL rendering for the trade feed. */
#include "tdx_trades_json.h"

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

/* Appends "SZ000623" as a JSON string. */
static int append_security_id(tdx_buf *out, int market_id, const char *code,
                              tdx_error *err) {
    char identity[16];
    snprintf(identity, sizeof(identity), "%s%s", upper_prefix(market_id), code ? code : "");
    return tdx_format_json_string(out, identity, err);
}

static int append_date(tdx_buf *out, const char *date, tdx_error *err) {
    if (!date || !*date)
        return APPEND_LITERAL(out, err, "null");
    return tdx_format_json_string(out, date, err);
}

int tdx_trades_format_tick(tdx_buf *out, const tdx_trade_tick *tick, int market_id,
                          const char *code, const char *date, tdx_error *err) {
    char label[8];
    char side[24];

    if (!out || !tick) {
        tdx_error_set(err, "trade tick rendering needs a buffer and a tick");
        return TDX_ERR;
    }
    if (tdx_trades_time_label(tick->time_minutes, label, sizeof(label)) != TDX_OK) {
        tdx_error_set(err, "trade tick has minute-of-day %d", tick->time_minutes);
        return TDX_ERR;
    }
    tdx_trades_side_text(tick->status_raw, side, sizeof(side));

    if (APPEND_LITERAL(out, err, "{\"type\":\"trade\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (append_security_id(out, market_id, code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"trading_date\":") != TDX_OK)
        return TDX_ERR;
    if (append_date(out, date, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"index\":%zu,\"absolute_index\":%zu,\"time\":\"%s\","
                              "\"time_minutes\":%d,\"price\":%.6f,\"volume_hand\":%lld,"
                              "\"amount_yuan\":%.4f,\"order_count\":%lld,\"side\":\"%s\","
                              "\"status_raw\":%lld,\"price_delta_raw\":%lld,"
                              "\"price_acc_raw\":%lld,\"tail_raw\":%lld}",
                              tick->index, tick->absolute_index, label, tick->time_minutes,
                              tick->price, (long long)tick->volume_hand, tick->amount_yuan,
                              (long long)tick->order_count, side, (long long)tick->status_raw,
                              (long long)tick->price_delta_raw,
                              (long long)tick->price_acc_raw, (long long)tick->tail_raw) !=
        TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_trades_format_summary(tdx_buf *out, const tdx_trade_series *series,
                              const tdx_trade_summary *summary, int market_id, const char *code,
                              const char *date, const char *endpoint, tdx_error *err) {
    char first_label[8];
    char last_label[8];
    size_t index;

    if (!out || !series || !summary) {
        tdx_error_set(err, "trade summary rendering needs a buffer, a series and a summary");
        return TDX_ERR;
    }
    if (summary->has_times) {
        (void)tdx_trades_time_label(summary->first_time_minutes, first_label,
                                    sizeof(first_label));
        (void)tdx_trades_time_label(summary->last_time_minutes, last_label,
                                    sizeof(last_label));
    } else {
        snprintf(first_label, sizeof(first_label), "null");
        snprintf(last_label, sizeof(last_label), "null");
    }

    if (APPEND_LITERAL(out, err, "{\"type\":\"trade_summary\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (append_security_id(out, market_id, code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"trading_date\":") != TDX_OK)
        return TDX_ERR;
    if (append_date(out, date, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"command\":\"%s\",\"endpoint\":",
                              series->history ? "0x0FC6" : "0x0FC5") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"pages\":%zu,\"page_size\":%u,\"price_divisor\":%d,"
                              "\"price_base\":",
                              series->pages, (unsigned)series->page_size,
                              series->price_divisor) != TDX_OK)
        return TDX_ERR;
    if (series->has_price_base) {
        if (tdx_buf_append_printf(out, err, "%.6f", series->price_base) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"tick_count\":%zu,\"minute_count\":%zu,"
                              "\"volume_hand\":%lld,\"order_count\":%lld,"
                              "\"amount_yuan\":%.4f,\"vwap\":",
                              summary->tick_count, summary->minute_count,
                              (long long)summary->volume_hand,
                              (long long)summary->order_count, summary->amount_yuan) != TDX_OK)
        return TDX_ERR;
    if (summary->volume_hand) {
        if (tdx_buf_append_printf(out, err, "%.6f", summary->vwap) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }

    if (tdx_buf_append_printf(out, err, ",\"first_time\":") != TDX_OK)
        return TDX_ERR;
    if (summary->has_times) {
        if (tdx_format_json_string(out, first_label, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"last_time\":") != TDX_OK)
        return TDX_ERR;
    if (summary->has_times) {
        if (tdx_format_json_string(out, last_label, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }

    if (tdx_buf_append_printf(out, err, ",\"buy_volume_hand\":%lld,\"sell_volume_hand\":%lld,"
                                        "\"neutral_volume_hand\":%lld,\"buy_amount_yuan\":%.4f,"
                                        "\"sell_amount_yuan\":%.4f,"
                                        "\"neutral_amount_yuan\":%.4f,\"status_counts\":{",
                              (long long)summary->buy_volume_hand,
                              (long long)summary->sell_volume_hand,
                              (long long)summary->neutral_volume_hand,
                              summary->buy_amount_yuan, summary->sell_amount_yuan,
                              summary->neutral_amount_yuan) != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < summary->status_count; ++index) {
        if (tdx_buf_append_printf(out, err, "%s\"%lld\":%zu", index ? "," : "",
                                  (long long)summary->status_counts[index].status,
                                  summary->status_counts[index].count) != TDX_OK)
            return TDX_ERR;
    }
    /* Close status_counts, then the summary object itself. */
    if (APPEND_LITERAL(out, err, "}}") != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
