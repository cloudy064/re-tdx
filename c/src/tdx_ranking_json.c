/* tdx_ranking_json.c - JSONL rendering for the 0x054B category ranking. */
#include "tdx_ranking_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* The argument must be a string literal: sizeof sizes it, and a computed expression decays
 * to a pointer, so `cond ? "a" : "b"` would copy sizeof(char*) - 1 bytes. */
#define RANK_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))

int tdx_ranking_format(tdx_buf *out, const tdx_ranking_record *record, size_t index,
                       uint16_t category, uint16_t sort, tdx_error *err) {
    const char *sort_name;

    if (!out || !record) {
        tdx_error_set(err, "rendering a ranking record needs a buffer and a record");
        return TDX_ERR;
    }
    sort_name = tdx_ranking_sort_name(sort);
    if (RANK_LITERAL(out, err, "{\"type\":\"ranking\",\"index\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              "%zu,\"security_id\":\"%s\",\"market_id\":%d,"
                              "\"code\":\"%s\",\"category\":%u,\"sort\":%u,"
                              "\"sort_name\":",
                              index, record->security_id, record->market_id, record->code,
                              category, sort) != TDX_OK)
        return TDX_ERR;
    if (sort_name) {
        if (tdx_format_json_string(out, sort_name, err) != TDX_OK)
            return TDX_ERR;
    } else if (RANK_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"last_price\":%.4f,\"pre_close_price\":%.4f,"
                              "\"open_price\":%.4f,\"high_price\":%.4f,\"low_price\":%.4f,"
                              "\"amount\":%.2f,\"total_hand\":%lld,\"current_hand\":%lld,"
                              "\"bid1_price\":%.4f,\"ask1_price\":%.4f,"
                              "\"bid1_volume_hand\":%lld,\"ask1_volume_hand\":%lld,"
                              "\"open_amount_yuan\":%.2f,\"rise_speed\":%.2f,"
                              "\"short_turnover\":%.2f,\"two_minute_amount\":%.4f,"
                              "\"opening_rush\":%.2f,\"volume_rise_speed\":%.4f,"
                              "\"depth\":%.4f,\"inside_dish\":%lld,\"outer_disc\":%lld,"
                              "\"active1\":%u,\"active2\":%u,\"status_raw\":%u,"
                              "\"extra_pair_hex\":\"%s\",\"extra_meta_hex\":\"%s\"}",
                              record->last_price, record->pre_close_price, record->open_price,
                              record->high_price, record->low_price, record->amount,
                              (long long)record->total_hand, (long long)record->current_hand,
                              record->bid1_price, record->ask1_price,
                              (long long)record->bid1_volume_hand,
                              (long long)record->ask1_volume_hand, record->open_amount_yuan,
                              record->rise_speed, record->short_turnover,
                              record->two_minute_amount, record->opening_rush,
                              record->volume_rise_speed, record->depth,
                              (long long)record->inside_dish, (long long)record->outer_disc,
                              record->active1, record->active2, record->status_or_sort_raw,
                              record->extra_pair_hex, record->extra_meta_hex) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_ranking_format_summary(tdx_buf *out, uint16_t category, uint16_t sort, int ascending,
                               size_t records, size_t pages, const char *endpoint,
                               tdx_error *err) {
    const char *sort_name;

    if (!out) {
        tdx_error_set(err, "the ranking summary needs a buffer");
        return TDX_ERR;
    }
    sort_name = tdx_ranking_sort_name(sort);
    if (RANK_LITERAL(out, err, "{\"type\":\"ranking_summary\",\"category\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, "%u,\"sort\":%u,\"sort_name\":", category, sort) !=
        TDX_OK)
        return TDX_ERR;
    if (sort_name) {
        if (tdx_format_json_string(out, sort_name, err) != TDX_OK)
            return TDX_ERR;
    } else if (RANK_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"ascending\":%s,\"records\":%zu,\"pages\":%zu,"
                                        "\"endpoint\":",
                              ascending ? "true" : "false", records, pages) != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (RANK_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    return tdx_buf_push(out, '}', err);
}
