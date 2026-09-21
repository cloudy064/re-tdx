/* tdx_auction_json.c - JSONL rendering for the call-auction series. */
#include "tdx_auction_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* The argument must be a string literal.  This macro sizes it with sizeof, and a
 * computed expression decays to a pointer, so `cond ? "true" : "false"` would
 * copy sizeof(char*) - 1 bytes - which is how "true" once came out as
 * "true\0fa".  Pass an expression to tdx_buf_append_printf instead. */
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

static int append_date(tdx_buf *out, const char *date, tdx_error *err) {
    if (!date || !*date)
        return APPEND_LITERAL(out, err, "null");
    return tdx_format_json_string(out, date, err);
}

int tdx_auction_format_point(tdx_buf *out, const tdx_auction_point *point, int market_id,
                             const char *code, const char *trading_date, tdx_error *err) {
    char label[12];
    if (!out || !point) {
        tdx_error_set(err, "auction point rendering needs a buffer and a point");
        return TDX_ERR;
    }
    if (tdx_auction_time_label(point->time_seconds, label, sizeof(label)) != TDX_OK) {
        tdx_error_set(err, "auction point has time %d seconds", point->time_seconds);
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"auction_point\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (append_security_id(out, market_id, code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"trading_date\":") != TDX_OK)
        return TDX_ERR;
    if (append_date(out, trading_date, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"index\":%zu,\"time\":\"%s\",\"time_seconds\":%d,"
                              "\"price\":%.4f,\"matched_volume_hand\":%lld,"
                              "\"matched_amount_yuan\":%.2f,\"unmatched_signed_hand\":%lld,"
                              "\"unmatched_volume_hand\":%lld,\"unmatched_direction\":\"%s\","
                              "\"reserved\":%d}",
                              point->index, label, point->time_seconds, point->price,
                              (long long)point->matched_volume_hand, point->matched_amount_yuan,
                              (long long)point->unmatched_signed_hand,
                              (long long)point->unmatched_volume_hand,
                              tdx_auction_direction_text(point->unmatched_direction_raw),
                              point->reserved) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

static int append_segment(tdx_buf *out, const char *key, const tdx_auction_segment *segment,
                          tdx_error *err) {
    char start_label[12];
    char end_label[12];
    char peak_label[12];

    if (APPEND_LITERAL(out, err, ",\"") != TDX_OK ||
        tdx_buf_append(out, key, strlen(key), err) != TDX_OK ||
        APPEND_LITERAL(out, err, "\":{\"has_points\":") != TDX_OK)
        return TDX_ERR;
    /* Two separate literals, never a ternary: APPEND_LITERAL sizes its argument
     * with sizeof, and a computed expression decays to a pointer, which would
     * copy sizeof(char*) - 1 bytes instead of the text. */
    if (segment->has_points) {
        if (APPEND_LITERAL(out, err, "true") != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "false") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"point_count\":%zu", segment->point_count) != TDX_OK)
        return TDX_ERR;
    if (!segment->has_points)
        return APPEND_LITERAL(out, err, "}");

    (void)tdx_auction_time_label(segment->start_time_seconds, start_label, sizeof(start_label));
    (void)tdx_auction_time_label(segment->end_time_seconds, end_label, sizeof(end_label));
    (void)tdx_auction_time_label(segment->max_unmatched_time_seconds, peak_label,
                                 sizeof(peak_label));
    if (tdx_buf_append_printf(out, err,
                              ",\"start_time\":\"%s\",\"end_time\":\"%s\","
                              "\"first_price\":%.4f,\"last_price\":%.4f,\"min_price\":%.4f,"
                              "\"max_price\":%.4f,\"last_matched_volume_hand\":%lld,"
                              "\"last_matched_amount_yuan\":%.2f,"
                              "\"last_unmatched_signed_hand\":%lld,"
                              "\"last_unmatched_volume_hand\":%lld,"
                              "\"last_unmatched_direction\":\"%s\","
                              "\"max_unmatched_volume_hand\":%lld,\"max_unmatched_time\":\"%s\","
                              "\"unmatched_direction_flips\":%zu,"
                              "\"matched_volume_monotonic_violations\":%zu}",
                              start_label, end_label, segment->first_price, segment->last_price,
                              segment->min_price, segment->max_price,
                              (long long)segment->last_matched_volume_hand,
                              segment->last_matched_amount_yuan,
                              (long long)segment->last_unmatched_signed_hand,
                              (long long)segment->last_unmatched_volume_hand,
                              tdx_auction_direction_text(segment->last_unmatched_direction_raw),
                              (long long)segment->max_unmatched_volume_hand, peak_label,
                              segment->unmatched_direction_flips,
                              segment->matched_volume_monotonic_violations) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}

int tdx_auction_format_summary(tdx_buf *out, const tdx_auction_series *series,
                               const tdx_auction_summary *summary, int market_id,
                               const char *code, const char *trading_date, const char *endpoint,
                               tdx_error *err) {
    if (!out || !series || !summary) {
        tdx_error_set(err, "auction summary rendering needs a buffer, a series and a summary");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"auction_summary\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (append_security_id(out, market_id, code, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"trading_date\":") != TDX_OK)
        return TDX_ERR;
    if (append_date(out, trading_date, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"command\":\"0x056A\",\"endpoint\":") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"selector\":%u,\"start_raw\":%u,\"limit\":%u",
                              (unsigned)series->selector, (unsigned)series->start_raw,
                              (unsigned)series->limit) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"point_count\":%zu", summary->point_count) != TDX_OK)
        return TDX_ERR;
    if (append_segment(out, "opening", &summary->opening, err) != TDX_OK)
        return TDX_ERR;
    if (append_segment(out, "closing", &summary->closing, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"largest_gap_seconds\":") != TDX_OK)
        return TDX_ERR;
    if (summary->has_largest_gap) {
        if (tdx_buf_append_printf(out, err, "%d", summary->largest_gap_seconds) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"reserved_nonzero_points\":%zu}",
                              summary->reserved_nonzero_points) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
