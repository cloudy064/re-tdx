/* tdx_format.c - the single JSON rendering path. */
#include "tdx_format.h"

#include <stdio.h>
#include <string.h>

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

int tdx_format_depth(tdx_buf *out, const tdx_depth *depth, tdx_error *err) {
    char id[16];
    size_t level;

    if (!depth) {
        tdx_error_set(err, "depth record is null");
        return TDX_ERR;
    }
    snprintf(id, sizeof(id), "%s%s", upper_prefix(depth->security.market_id),
             depth->security.code);
    if (tdx_buf_append_printf(out, err,
                              "{\"security_id\":\"%s\",\"active\":%u,\"last_price\":%.6f,"
                              "\"pre_close_price\":%.6f,\"open_price\":%.6f,"
                              "\"high_price\":%.6f,\"low_price\":%.6f,"
                              "\"total_hand\":%lld,\"current_hand\":%lld,"
                              "\"amount\":%.6f,\"inside_dish\":%lld,\"outer_disc\":%lld,"
                              "\"auction_imbalance_hand_raw\":%lld,"
                              "\"open_amount_yuan\":%.6f,\"update_time_raw\":%u,"
                              "\"status_raw\":%lld,\"tail_bytes\":%zu,\"buy_levels\":[",
                              id, (unsigned)depth->active, depth->last, depth->previous,
                              depth->open, depth->high, depth->low,
                              (long long)depth->total_hand,
                              (long long)depth->current_hand, depth->amount,
                              (long long)depth->inside, (long long)depth->outside,
                              (long long)depth->auction_imbalance_hand,
                              depth->open_amount, (unsigned)depth->update_time,
                              (long long)depth->status, depth->tail_size) != TDX_OK)
        return TDX_ERR;
    for (level = 0; level < TDX_DEPTH_LEVELS; ++level) {
        if (tdx_buf_append_printf(out, err,
                                  "%s{\"price\":%.6f,\"volume_hand\":%lld}",
                                  level ? "," : "", depth->buys[level].price,
                                  (long long)depth->buys[level].volume_hand) != TDX_OK)
            return TDX_ERR;
    }
    if (tdx_buf_append(out, "],\"sell_levels\":[", 17, err) != TDX_OK)
        return TDX_ERR;
    for (level = 0; level < TDX_DEPTH_LEVELS; ++level) {
        if (tdx_buf_append_printf(out, err,
                                  "%s{\"price\":%.6f,\"volume_hand\":%lld}",
                                  level ? "," : "", depth->sells[level].price,
                                  (long long)depth->sells[level].volume_hand) != TDX_OK)
            return TDX_ERR;
    }
    return tdx_buf_append(out, "]}", 2, err);
}

int tdx_format_diff_names(tdx_buf *out, tdx_diff_mask mask, tdx_error *err) {
    const char *names[8];
    const size_t count = tdx_diff_names(mask, names, 8);
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

static int format_depth_event(tdx_buf *out, const char *type, const char *counter_name, int64_t sequence,
                           const tdx_depth *depth, tdx_diff_mask mask,
                           uint64_t updates, tdx_error *err) {
    char id[16];

    if (!depth) {
        tdx_error_set(err, "event record is null");
        return TDX_ERR;
    }
    snprintf(id, sizeof(id), "%s%s", upper_prefix(depth->security.market_id),
             depth->security.code);
    if (tdx_buf_append(out, "{\"type\":", 8, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_json_string(out, type, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err,
                              ",\"%s\":%lld,\"security_id\":\"%s\",\"changed\":",
                              counter_name, (long long)sequence, id) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_diff_names(out, mask, err) != TDX_OK)
        return TDX_ERR;
    if (tdx_buf_append_printf(out, err, ",\"updates\":%llu,\"record\":",
                              (unsigned long long)updates) != TDX_OK)
        return TDX_ERR;
    if (tdx_format_depth(out, depth, err) != TDX_OK)
        return TDX_ERR;
    return tdx_buf_push(out, '}', err);
}

int tdx_format_depth_event(tdx_buf *out, const char *type, int64_t sequence,
                           const tdx_depth *depth, tdx_diff_mask mask,
                           uint64_t updates, tdx_error *err) {
    return format_depth_event(out, type, "sequence", sequence, depth, mask, updates, err);
}

int tdx_format_depth_round_event(tdx_buf *out, const char *type, size_t round,
                                 const tdx_depth *depth, tdx_diff_mask mask,
                                 uint64_t updates, tdx_error *err) {
    if (round > (uint64_t)INT64_MAX) {
        tdx_error_set(err, "watch round exceeds the event counter range");
        return TDX_ERR;
    }
    return format_depth_event(out, type, "round", (int64_t)round, depth, mask, updates, err);
}

static int format_heartbeat_event(tdx_buf *out, const char *counter_name, int64_t sequence,
                                   size_t subscribed, uint64_t total_events, tdx_error *err) {
    return tdx_buf_append_printf(out, err,
                                 "{\"type\":\"heartbeat\",\"%s\":%lld,"
                                 "\"subscribed\":%zu,\"total_events\":%llu}",
                                 counter_name, (long long)sequence, subscribed,
                                 (unsigned long long)total_events);
}

int tdx_format_heartbeat_event(tdx_buf *out, int64_t sequence, size_t subscribed,
                               uint64_t total_events, tdx_error *err) {
    return format_heartbeat_event(out, "sequence", sequence, subscribed, total_events, err);
}

int tdx_format_heartbeat_round_event(tdx_buf *out, size_t round, size_t subscribed,
                                     uint64_t total_events, tdx_error *err) {
    if (round > (uint64_t)INT64_MAX) {
        tdx_error_set(err, "watch round exceeds the event counter range");
        return TDX_ERR;
    }
    return format_heartbeat_event(out, "round", (int64_t)round, subscribed, total_events, err);
}
