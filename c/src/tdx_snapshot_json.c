/* tdx_snapshot_json.c - JSONL rendering for the 0x054C snapshot feed. */
#include "tdx_snapshot_json.h"

#include <stdio.h>
#include <string.h>

#include "tdx_format.h"

#define APPEND_LITERAL(buf, err, text) tdx_buf_append((buf), (text), sizeof(text) - 1, (err))
/* The argument must be a string literal.  This macro sizes it with sizeof, and a
 * computed expression decays to a pointer, so `cond ? "true" : "false"` would
 * copy sizeof(char*) - 1 bytes.  Pass an expression to tdx_buf_append_printf. */

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

int tdx_snapshot_format(tdx_buf *out, const tdx_snapshot *snapshot, tdx_error *err) {
    char identity[16];

    if (!out || !snapshot) {
        tdx_error_set(err, "snapshot rendering needs a buffer and a record");
        return TDX_ERR;
    }
    snprintf(identity, sizeof(identity), "%s%s", upper_prefix(snapshot->security.market_id),
             snapshot->security.code);
    if (APPEND_LITERAL(out, err, "{\"type\":\"snapshot\",\"security_id\":") != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, identity, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"market_id\":%d,\"code\":\"%s\",\"active\":%u,"
                              "\"last_price\":%.6f,\"pre_close_price\":%.6f,"
                              "\"open_price\":%.6f,\"high_price\":%.6f,\"low_price\":%.6f,"
                              "\"total_hand\":%lld,\"current_hand\":%lld,\"amount\":%.6f,"
                              "\"inside_dish\":%lld,\"outer_disc\":%lld,"
                              "\"auction_imbalance_hand_raw\":%lld,"
                              "\"open_amount_yuan\":%.6f,\"update_time_raw\":%lld,"
                              "\"auxiliary_price_delta_raw\":%lld,\"tail_bytes\":%zu",
                              snapshot->security.market_id, snapshot->security.code,
                              (unsigned)snapshot->active, snapshot->last, snapshot->previous,
                              snapshot->open, snapshot->high, snapshot->low,
                              (long long)snapshot->total_hand,
                              (long long)snapshot->current_hand, snapshot->amount,
                              (long long)snapshot->inside, (long long)snapshot->outside,
                              (long long)snapshot->auction_imbalance_hand,
                              snapshot->open_amount, (long long)snapshot->time_raw,
                              (long long)snapshot->auxiliary_price_delta_raw,
                              snapshot->tail_size) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"fund_iopv\":") != TDX_OK)
        return TDX_ERR;
    if (snapshot->has_fund_iopv) {
        if (tdx_buf_append_printf(out, err, "%.6f", snapshot->fund_iopv) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    return tdx_buf_push(out, '}', err);
}

void tdx_snapshot_tally_init(tdx_snapshot_tally *tally) {
    if (!tally)
        return;
    memset(tally, 0, sizeof(*tally));
}

void tdx_snapshot_tally_add(tdx_snapshot_tally *tally, const tdx_snapshot *records,
                            size_t count) {
    size_t index;
    if (!tally || !records)
        return;
    for (index = 0; index < count; ++index) {
        tally->record_count++;
        if (records[index].has_fund_iopv)
            tally->records_with_fund_iopv++;
        if (records[index].tail_size)
            tally->records_with_tail++;
        tally->amount_sum += records[index].amount;
        tally->total_hand_sum += (double)records[index].total_hand;
    }
}

int tdx_snapshot_format_summary(tdx_buf *out, const tdx_snapshot_tally *tally, size_t requested,
                                const char *endpoint, tdx_error *err) {
    if (!out || !tally) {
        tdx_error_set(err, "snapshot summary rendering needs a buffer and a tally");
        return TDX_ERR;
    }
    if (APPEND_LITERAL(out, err, "{\"type\":\"snapshot_summary\",\"command\":\"0x054C\"") !=
        TDX_OK)
        return TDX_ERR;
    if (APPEND_LITERAL(out, err, ",\"endpoint\":") != TDX_OK)
        return TDX_ERR;
    if (endpoint && *endpoint) {
        if (tdx_format_json_string(out, endpoint, err) != TDX_OK)
            return TDX_ERR;
    } else if (APPEND_LITERAL(out, err, "null") != TDX_OK) {
        return TDX_ERR;
    }
    if (tdx_buf_append_printf(out, err,
                              ",\"requested\":%zu,\"record_count\":%zu,"
                              "\"records_with_fund_iopv\":%zu,\"records_with_tail\":%zu,"
                              "\"amount_sum\":%.6f,\"total_hand_sum\":%.0f}",
                              requested, tally->record_count, tally->records_with_fund_iopv,
                              tally->records_with_tail, tally->amount_sum,
                              tally->total_hand_sum) != TDX_OK)
        return TDX_ERR;
    return TDX_OK;
}
