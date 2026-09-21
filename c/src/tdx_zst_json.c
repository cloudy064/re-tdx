/* tdx_zst_json.c - JSON rendering of a replayed zst snapshot. */
#include "tdx_zst_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

/* Literal appends carry their own length, so a miscounted byte count cannot
 * silently truncate the object. */
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

int tdx_zst_format_diff_names(tdx_buf *out, tdx_zst_diff_mask mask, tdx_error *err) {
    const char *names[16];
    const size_t count = tdx_zst_diff_names(mask, names, sizeof(names) / sizeof(names[0]));
    size_t index;
    if (tdx_buf_push(out, '[', err) != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < count; ++index) {
        if (index && tdx_buf_push(out, ',', err) != TDX_OK)
            return TDX_ERR;
        if (tdx_format_json_string(out, names[index], err) != TDX_OK)
            return TDX_ERR;
    }
    return tdx_buf_push(out, ']', err);
}

/* Appends a number, or the null literal when the stream never delivered it. */
static int append_optional(tdx_buf *out, int present, const char *format, double value,
                           tdx_error *err) {
    if (!present)
        return APPEND_LITERAL(out, err, "null");
    return tdx_buf_append_printf(out, err, format, value);
}

static int append_field(tdx_buf *out, int present, const char *key, const char *format,
                        double value, tdx_error *err) {
    if (tdx_buf_append_printf(out, err, ",\"%s\":", key) != TDX_OK)
        return TDX_ERR;
    return append_optional(out, present, format, value, err);
}

static int append_ladder(tdx_buf *out, const tdx_zst_book_level *levels, unsigned present,
                         tdx_error *err) {
    int level;
    int first = 1;
    if (tdx_buf_push(out, '[', err) != TDX_OK)
        return TDX_ERR;
    for (level = 0; level < TDX_ZST_LEVELS; ++level) {
        if (!(present & (1u << level)))
            continue;
        if (tdx_buf_append_printf(out, err, "%s{\"price\":%.6f,\"volume\":%.0f}",
                                  first ? "" : ",", levels[level].price,
                                  levels[level].volume) != TDX_OK)
            return TDX_ERR;
        first = 0;
    }
    return tdx_buf_push(out, ']', err);
}

int tdx_zst_format_snapshot(tdx_buf *out, const tdx_zst_snapshot *snapshot, int market_id,
                            tdx_zst_diff_mask changed, int include_raw_tags,
                            tdx_error *err) {
    char security_id[16];
    unsigned present;
    size_t index;
    int first = 1;

    if (!out || !snapshot) {
        tdx_error_set(err, "snapshot rendering needs a buffer and a snapshot");
        return TDX_ERR;
    }
    present = snapshot->present;
    snprintf(security_id, sizeof(security_id), "%s%s", upper_prefix(market_id),
             snapshot->code);

    if (APPEND_LITERAL(out, err, "{\"type\":\"zst_snapshot\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, security_id, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"record\":%zu,\"sequence\":%zu",
                              snapshot->record_index, snapshot->update_index) != TDX_OK)
        return TDX_ERR;
    if (present & TDX_ZST_HAVE_TIME) {
        if (tdx_buf_append_printf(out, err, ",\"time\":\"%02d:%02d:%02d\"",
                                  snapshot->time_hhmmss / 10000,
                                  (snapshot->time_hhmmss / 100) % 100,
                                  snapshot->time_hhmmss % 100) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, ",\"time\":null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err, ",\"time_hhmmss\":") != TDX_OK)
        return TDX_ERR;
    if (present & TDX_ZST_HAVE_TIME) {
        if (tdx_buf_append_printf(out, err, "%d", snapshot->time_hhmmss) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }

    if (APPEND_LITERAL(out, err, ",\"changed\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_zst_format_diff_names(out, changed, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"changed_tags\":[") != TDX_OK)
        return TDX_ERR;
    for (index = 0; index < snapshot->changed_id_count; ++index) {
        if (tdx_buf_append_printf(out, err, "%s\"%s\"", first ? "" : ",",
                                  snapshot->changed_ids[index]) != TDX_OK)
            return TDX_ERR;
        first = 0;
    }
    if (tdx_buf_append_printf(out, err,
                              "],\"merged_tags\":%zu,\"record_tags\":%zu,\"dropped_tags\":%zu",
                              snapshot->field_count, snapshot->changed_id_count,
                              snapshot->dropped_fields) != TDX_OK)
        return TDX_ERR;

    if (append_field(out, (present & TDX_ZST_HAVE_LAST) != 0, "last_price", "%.6f",
                     snapshot->last_price, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_PREVIOUS_CLOSE) != 0, "pre_close_price",
                     "%.6f", snapshot->previous_close, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_OPEN) != 0, "open_price", "%.6f",
                     snapshot->open_price, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_HIGH) != 0, "high_price", "%.6f",
                     snapshot->high_price, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_LOW) != 0, "low_price", "%.6f",
                     snapshot->low_price, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_LIMIT_UP) != 0, "limit_up_price", "%.6f",
                     snapshot->limit_up, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_LIMIT_DOWN) != 0, "limit_down_price", "%.6f",
                     snapshot->limit_down, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_TRADE_COUNT) != 0, "trade_count", "%.0f",
                     snapshot->trade_count, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_VOLUME) != 0, "volume", "%.0f",
                     snapshot->volume, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_AMOUNT) != 0, "amount", "%.4f",
                     snapshot->amount, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_AVERAGE_PRICE) != 0, "average_price", "%.6f",
                     snapshot->average_price, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_PE_RATIO) != 0, "pe_ratio", "%.6f",
                     snapshot->pe_ratio, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_AVERAGE_BID) != 0, "aggregate_bid_price",
                     "%.6f", snapshot->average_bid, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_TOTAL_BID) != 0, "aggregate_bid_volume",
                     "%.0f", snapshot->total_bid, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_AVERAGE_ASK) != 0, "aggregate_ask_price",
                     "%.6f", snapshot->average_ask, err) != TDX_OK)
        return TDX_ERR;
    if (append_field(out, (present & TDX_ZST_HAVE_TOTAL_ASK) != 0, "aggregate_ask_volume",
                     "%.0f", snapshot->total_ask, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"phase\":") != TDX_OK)
        return TDX_ERR;
    if (present & TDX_ZST_HAVE_PHASE) {
        if (tdx_format_json_string(out, snapshot->phase, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }

    if (APPEND_LITERAL(out, err, ",\"buy_levels\":") != TDX_OK)
        return TDX_ERR;
    if (append_ladder(out, snapshot->bids, snapshot->bid_levels, err) != TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"sell_levels\":") != TDX_OK)
        return TDX_ERR;
    if (append_ladder(out, snapshot->asks, snapshot->ask_levels, err) != TDX_OK)
        return TDX_ERR;

    if (include_raw_tags && snapshot->fields) {
        if (APPEND_LITERAL(out, err, ",\"fields\":{") != TDX_OK)
            return TDX_ERR;
        for (index = 0; index < snapshot->field_count; ++index) {
            const tdx_zst_field *field = &snapshot->fields[index];
            if (tdx_buf_append_printf(out, err, "%s\"%s\":", index ? "," : "", field->id) !=
                TDX_OK)
                return TDX_ERR;
            if (tdx_format_json_string(out, field->value, err) != TDX_OK)
                return TDX_ERR;
        }
        if (tdx_buf_push(out, '}', err) != TDX_OK)
            return TDX_ERR;
    }
    return tdx_buf_push(out, '}', err);
}
